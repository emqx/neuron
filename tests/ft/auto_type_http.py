"""Isolated real-HTTP AUTO tests; run directly, never through the FT pytest fixtures.

    python3 tests/ft/auto_type_http.py --build build

Build neuron, plugin-auto-type and plugin-modbus-tcp first. This runner refuses to
share a running Neuron instance and creates its own temporary config, plugins,
logs and database.
The runtime is retained on failure (or with --keep-runtime).

Mock params replace the previous settings: hold (default true), concrete numeric
NEU_TYPE type (default INT16), value (default 123), error (default 0), update_mode
(update/meta/trace/im/im_f_m), pointer (STRING/BYTES wrapper), optional quality,
timestamp, source_timestamp, server_timestamp, record_path (append JSONL writes:
tag, actual type, union value, path).
All readable tags receive the selected sample; group_sync uses the same controls.
"""

import argparse
import json
import os
from pathlib import Path
import shutil
import signal
import socket
import sqlite3
import subprocess
import tempfile
import time
import unittest
import urllib.error
import urllib.request
from urllib.parse import urlencode


REPO = Path(__file__).resolve().parents[2]
BUILD = REPO / "build"
KEEP_RUNTIME = False
SAMPLE_ATTRS = ("timestamp", "quality", "source_timestamp", "server_timestamp")


class AutoTypeHTTP(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if subprocess.run(["pgrep", "-x", "neuron"], capture_output=True).returncode == 0:
            raise RuntimeError("Stop the existing Neuron instance before running AUTO HTTP tests")
        for artifact in (BUILD / "neuron", BUILD / "tests/plugins/libplugin-auto-type.so",
                         BUILD / "plugins/libplugin-modbus-tcp.so"):
            if not artifact.is_file():
                raise RuntimeError(f"Build required artifact first: {artifact}")
        cls.root = Path(tempfile.mkdtemp(prefix="neuron-auto-http-"))
        print(f"AUTO HTTP runtime: {cls.root}", flush=True)
        cls.process = None
        cls.token = None
        cls.failed = False
        cls.addClassCleanup(cls.cleanup)
        for directory in ("config", "plugins/schema", "persistence", "logs"):
            (cls.root / directory).mkdir(parents=True, exist_ok=True)
        for sql in (REPO / "persistence").glob("*.sql"):
            shutil.copy2(sql, cls.root / "config" / sql.name)
        for name in ("zlog.conf", "dev.conf"):
            shutil.copy2(REPO / name, cls.root / "config" / name)
        shutil.copy2(BUILD / "tests/plugins/libplugin-auto-type.so", cls.root / "plugins")
        shutil.copy2(REPO / "tests/plugins/auto_type/auto_type.json", cls.root / "plugins/schema")
        shutil.copy2(BUILD / "plugins/libplugin-modbus-tcp.so", cls.root / "plugins")
        shutil.copy2(BUILD / "plugins/schema/modbus-tcp.json", cls.root / "plugins/schema")
        plugins = ["libplugin-auto-type.so", "libplugin-modbus-tcp.so"]
        (cls.root / "config/default_plugins.json").write_text(json.dumps({"plugins": plugins}))
        with socket.socket() as sock:
            sock.bind(("127.0.0.1", 0))
            port = sock.getsockname()[1]
        cls.base = f"http://127.0.0.1:{port}/api/v2"
        (cls.root / "config/neuron.json").write_text(json.dumps({
            "ip": "127.0.0.1", "port": port, "disable_auth": 0,
            "sub_filter_error": 0,
        }))
        cls.log = (cls.root / "server.log").open("w")
        cls.start()

    @classmethod
    def start(cls):
        cls.token = None
        env = dict(os.environ)
        env["LD_LIBRARY_PATH"] = str(BUILD) + ":" + env.get("LD_LIBRARY_PATH", "/usr/local/lib")
        server_log = cls.root / "logs/neuron.log"
        log_offset = server_log.stat().st_size if server_log.exists() else 0
        cls.process = subprocess.Popen([
            str(BUILD / "neuron"), "--config_file", "config/neuron.json",
            "--config_dir", "config", "--plugin_dir", "plugins",
        ], cwd=cls.root, env=env, stdout=cls.log, stderr=subprocess.STDOUT)
        deadline = time.monotonic() + 30
        while time.monotonic() < deadline:
            if cls.process.poll() is not None:
                cls.failed = True
                raise RuntimeError(f"Neuron exited; see {cls.root / 'server.log'}")
            try:
                if not server_log.exists() or "manager start" not in server_log.read_text()[log_offset:]:
                    time.sleep(0.1)
                    continue
                status, body = cls.request("POST", "/login", {"name": "admin", "pass": "0000"})
                if status == 200 and "token" in body:
                    cls.token = body["token"]
                    return
            except OSError:
                pass
            time.sleep(0.1)
        cls.failed = True
        raise RuntimeError(f"Neuron readiness timeout; see {cls.root}")

    @classmethod
    def stop(cls):
        if cls.process is not None:
            if cls.process.poll() is None:
                cls.process.send_signal(signal.SIGTERM)
                try:
                    cls.process.wait(timeout=15)
                except subprocess.TimeoutExpired:
                    cls.process.kill()
                    cls.process.wait(timeout=5)
            cls.process = None

    @classmethod
    def cleanup(cls):
        cls.stop()
        if hasattr(cls, "log"):
            cls.log.close()
        if not cls.failed and not KEEP_RUNTIME:
            shutil.rmtree(cls.root)

    @classmethod
    def request(cls, method, path, body=None):
        headers = {"Content-Type": "application/json"}
        if cls.token:
            headers["Authorization"] = "Bearer " + cls.token
        req = urllib.request.Request(
            cls.base + path, method=method, headers=headers,
            data=None if body is None else json.dumps(body).encode())
        try:
            with urllib.request.urlopen(req, timeout=10) as response:
                return response.status, json.loads(response.read())
        except urllib.error.HTTPError as error:
            return error.code, json.loads(error.read())

    def http(self, method, path, body=None, *, status=200, error=None):
        actual_status, response = self.request(method, path, body)
        self.assertEqual(status, actual_status, (method, path, response))
        if error is not None:
            self.assertEqual(error, response.get("error"), response)
        return response

    def setUp(self):
        self.node = self._testMethodName
        self.record = self.root / (self.node + ".jsonl")
        self.http("POST", "/node", {"name": self.node, "plugin": "auto_type",
                                    "params": {"hold": True}}, error=0)
        self.addCleanup(self.finish_node)
        self.http("POST", "/group", {"node": self.node, "group": "g", "interval": 100}, error=0)
        state = self.http("GET", "/node/state?node=" + self.node)
        if state["running"] != 3:
            self.http("POST", "/node/ctl", {"node": self.node, "cmd": 0}, error=0)

    def finish_node(self):
        result = self._outcome.result
        if any(test is self or getattr(test, "test_case", None) is self
               for test, _ in result.failures + result.errors):
            type(self).failed = True
        self.http("DELETE", "/node", {"name": self.node}, error=0)

    def control(self, **params):
        settings = {"hold": False, "type": 3, "value": 123, "error": 0,
                    "update_mode": "update", "record_path": str(self.record)}
        settings.update(params)
        self.http("POST", "/node/setting", {"node": self.node, "params": settings}, error=0)

    def add_tags(self, *tags, group="g", method="POST", error=0, status=200):
        return self.http(method, "/tags", {"node": self.node, "group": group,
                                          "tags": list(tags)}, error=error, status=status)

    @staticmethod
    def tag(name="a", **fields):
        return dict({"name": name, "type": 0, "address": "sample",
                     "attribute": 3, "precision": 0, "decimal": 0}, **fields)

    def read(self, name="a", *, group="g", paginate=False, sync=False, query=None):
        payload = {"node": self.node, "group": group, "sync": sync}
        if query is not None:
            payload["query"] = query
        response = self.http("POST", "/read/paginate" if paginate else "/read", payload)
        tag = next(tag for tag in response["items" if paginate else "tags"]
                   if tag["name"] == name)
        self.assertIs(int, type(tag["resolved_type"]), tag)
        return tag

    def list_tags(self, **query):
        params = {"node": self.node, "group": "g"}
        params.update(query)
        return self.http("GET", "/tags?" + urlencode(params))["tags"]

    def await_types(self, expected):
        deadline = time.monotonic() + 8
        actual = None
        while time.monotonic() < deadline:
            tags = self.list_tags()
            self.assertTrue(all(type(tag["resolved_type"]) is int for tag in tags))
            actual = {tag["name"]: tag["resolved_type"] for tag in tags}
            if actual == expected:
                return tags
            time.sleep(0.05)
        self.fail(f"resolved types did not become {expected!r}: {actual!r}")

    def await_sample(self, value=None, error=None, attrs=None, resolved_type=None, **kwargs):
        deadline = time.monotonic() + 8
        actual = None
        while time.monotonic() < deadline:
            actual = self.read(**kwargs)
            sample_attrs = {name: actual[name] for name in SAMPLE_ATTRS if name in actual}
            matches = (actual.get("error") == error if error is not None else
                       "error" not in actual and actual.get("value") == value)
            if (matches and (attrs is None or sample_attrs == attrs) and
                    (resolved_type is None or actual["resolved_type"] == resolved_type)):
                return actual
            time.sleep(0.05)
        self.fail(f"sample did not become value={value!r}, error={error!r}, "
                  f"attrs={attrs!r}, resolved_type={resolved_type!r}: {actual}")

    def write(self, value, *, name="a", error=0):
        status, response = self.request("POST", "/write", {
            "node": self.node, "group": "g", "tag": name, "value": value})
        self.assertEqual(error, response.get("error"), (status, response))
        if not error:
            self.assertEqual(200, status)

    def records(self):
        if not self.record.exists():
            return []
        return [json.loads(line) for line in self.record.read_text().splitlines()]

    def metadata_is_auto(self):
        tags = self.http("GET", f"/tags?node={self.node}&group=g")["tags"]
        self.assertTrue(tags)
        self.assertTrue(all(tag["type"] == 0 for tag in tags), tags)
        exported = self.http("GET", "/global/drivers?name=" + self.node)
        self.assertTrue(all(tag["type"] == 0 for node in exported["nodes"]
                            for group in node["groups"] for tag in group["tags"]))
        self.assertTrue(all("resolved_type" not in tag for node in exported["nodes"]
                            for group in node["groups"] for tag in group["tags"]))
        global_config = self.http("GET", "/global/config")
        self.assertTrue(all("resolved_type" not in tag for group in global_config["tags"]
                            for tag in group["tags"]))
        with sqlite3.connect(f"file:{self.root}/persistence/sqlite.db?mode=ro", uri=True) as db:
            rows = db.execute("SELECT type FROM tags WHERE driver_name = ?", (self.node,)).fetchall()
        self.assertTrue(rows)
        self.assertTrue(all(row == (0,) for row in rows), rows)

    def test_resolution_writes_mismatch_recovery_and_restart(self):
        self.add_tags(self.tag(), self.tag("b"))
        self.control(hold=True)
        self.await_sample(error=3011)
        self.write(123, error=3011)
        self.assertEqual([], self.records())
        self.control()
        self.await_sample(123)
        self.await_sample(123, paginate=True)
        self.await_sample(123, sync=True)
        self.metadata_is_auto()
        self.write(321)
        self.assertEqual({"tag": "a", "type": 3, "value": 321, "path": "single"}, self.records()[-1])
        for value, code in ((40000, 3020), ("123", 3007)):
            self.write(value, error=code)
        self.assertEqual(1, len(self.records()))
        self.http("POST", "/write/tags", {"node": self.node, "group": "g", "tags": [
            {"tag": "a", "value": 111}, {"tag": "b", "value": 222}]}, error=0)
        self.assertEqual([("a", 3, 111, "batch"), ("b", 3, 222, "batch")],
                         [(r["tag"], r["type"], r["value"], r["path"]) for r in self.records()[-2:]])
        self.control(type=13, value="wrong")
        self.await_sample(error=3007)
        self.write(444)
        self.assertEqual(3, self.records()[-1]["type"])
        self.control(error=3002)
        self.await_sample(error=3002)
        self.control(value=124)
        self.await_sample(124)
        self.metadata_is_auto()
        self.control(hold=True)
        self.stop()
        self.start()
        self.await_sample(error=3011)
        self.write(1, error=3011)
        self.metadata_is_auto()
        self.control(type=13, value="new lifecycle")
        self.await_sample("new lifecycle")
        self.metadata_is_auto()

    def test_resolved_type_exports_roundtrip_units(self):
        for unit in ("°C", "", None):
            tags = [self.tag(), self.tag("explicit", type=3)]
            if unit is not None:
                tags[0]["unit"] = unit
            data = {
                "nodes": [{"name": self.node, "plugin": "auto_type"}],
                "groups": [{"driver": self.node, "group": "g", "interval": 100,
                            "tag_count": len(tags)}],
                "tags": [{"driver": self.node, "group": "g", "tags": tags}],
                "subscriptions": [], "settings": []}
            expected_units = {"a": unit or "", "explicit": ""}
            for cycle in range(2):
                self.http("PUT", "/global/config", data, error=0)
                listed = self.await_types({"a": 0, "explicit": 3})
                self.assertEqual(expected_units, {tag["name"]: tag["unit"] for tag in listed})
                data = self.http("GET", "/global/config")
                exported = next(group for group in data["tags"]
                                if group["driver"] == self.node and group["group"] == "g")["tags"]
                self.assertEqual(expected_units, {tag["name"]: tag["unit"] for tag in exported})
                self.assertEqual({"a": 0, "explicit": 3},
                                 {tag["name"]: tag["type"] for tag in exported})
                self.assertTrue(all("resolved_type" not in tag for tag in exported))
                drivers = self.http("GET", "/global/drivers?name=" + self.node)
                exported = drivers["nodes"][0]["groups"][0]["tags"]
                self.assertEqual(expected_units, {tag["name"]: tag["unit"] for tag in exported})
                self.assertTrue(all("resolved_type" not in tag for tag in exported))

    def test_resolved_type_lifecycle(self):
        self.add_tags(self.tag(), self.tag("explicit", type=3),
                      self.tag("write_only", type=5, attribute=2))
        self.control(hold=True)
        self.await_types({"a": 0, "explicit": 3, "write_only": 5})
        for paginate in (False, True):
            self.await_sample(error=3011, resolved_type=0, paginate=paginate)
            self.await_sample(error=3011, resolved_type=3, name="explicit", paginate=paginate)
        self.control()
        for paginate in (False, True):
            self.await_sample(123, resolved_type=3, paginate=paginate)
        self.await_sample(123, resolved_type=3, sync=True)
        self.await_types({"a": 3, "explicit": 3, "write_only": 5})
        self.control(hold=True)
        self.http("POST", "/node/ctl", {"node": self.node, "cmd": 1}, error=0)
        for paginate in (False, True):
            self.await_sample(error=3010, resolved_type=3, paginate=paginate)
        self.await_types({"a": 3, "explicit": 3, "write_only": 5})
        self.add_tags(self.tag(address="changed"), method="PUT")
        self.await_types({"a": 0, "explicit": 3, "write_only": 5})
        for paginate in (False, True):
            self.await_sample(error=3010, resolved_type=0, paginate=paginate)
        self.control(type=10, value=12.5)
        self.http("POST", "/node/ctl", {"node": self.node, "cmd": 0}, error=0)
        self.await_sample(12.5, resolved_type=10)
        self.await_types({"a": 10, "explicit": 3, "write_only": 5})
        for params, expected_error in (({"type": 10, "error": 3002}, 3002),
                                       ({"type": 3}, 3007)):
            self.control(**params)
            for paginate in (False, True):
                self.await_sample(error=expected_error, resolved_type=10, paginate=paginate)
            self.await_types({"a": 10, "explicit": 3, "write_only": 5})
        self.control(type=10, value=12.5)
        self.await_sample(12.5, resolved_type=10)
        self.control(hold=True)
        self.stop()
        self.start()
        self.await_sample(error=3011, resolved_type=0)
        self.await_types({"a": 0, "explicit": 3, "write_only": 5})
        self.control(type=13, value="new type", pointer=True)
        for paginate in (False, True):
            self.await_sample("new type", resolved_type=13, paginate=paginate)
        self.await_types({"a": 13, "explicit": 3, "write_only": 5})
        exported = self.http("GET", "/global/drivers?name=" + self.node)
        tags = exported["nodes"][0]["groups"][0]["tags"]
        self.assertEqual(0, next(tag for tag in tags if tag["name"] == "a")["type"])
        self.assertTrue(all("resolved_type" not in tag for tag in tags))

    def test_resolved_type_sort_filter_and_pages(self):
        self.add_tags(self.tag("a", address="z"),
                      self.tag("b", type=3, address="a", description="needle"),
                      self.tag("c", type=10, address="m"),
                      self.tag("w", type=5, attribute=2))
        self.control()
        self.await_sample(123, resolved_type=3)
        expected = {"a": 3, "b": 3, "c": 10, "w": 5}
        self.await_types(expected)
        for sort in ("+name", "-name", "+type", "-type", "+address"):
            tags = self.list_tags(sort_by=sort)
            self.assertEqual(expected, {tag["name"]: tag["resolved_type"] for tag in tags})
        self.assertEqual(["b"], [tag["name"] for tag in self.list_tags(name="needle")])
        self.assertEqual([], self.list_tags(name="not-present"))
        self.http("GET", "/tags?" + urlencode({"node": self.node, "group": "g",
                                               "sort_by": "invalid"}), status=400, error=1003)
        request = {"node": self.node, "group": "g"}
        for sort in ("+name", "-type", "+address"):
            full = self.http("POST", "/read/paginate", dict(request, query={
                "sort_by": sort, "currentPage": 1, "pageSize": 10}))["items"]
            self.assertEqual(expected, {tag["name"]: tag["resolved_type"] for tag in full})
            for index, tag in enumerate(full):
                page = self.http("POST", "/read/paginate", dict(request, query={
                    "sort_by": sort, "currentPage": index + 1, "pageSize": 1}))["items"]
                self.assertEqual([tag["name"]], [item["name"] for item in page])
                self.assertEqual(expected[tag["name"]], page[0]["resolved_type"])
        self.control(error=3002)
        for name in ("a", "b", "c"):
            self.await_sample(error=3002, name=name, resolved_type=expected[name])
        self.control(hold=True)
        full = self.http("POST", "/read/paginate", dict(request, query={
            "isError": True, "sort_by": "-type"}))["items"]
        self.assertEqual({name: expected[name] for name in ("a", "b", "c")},
                         {tag["name"]: tag["resolved_type"] for tag in full})
        for index, tag in enumerate(full):
            page = self.http("POST", "/read/paginate", dict(request, query={
                "isError": True, "sort_by": "-type", "currentPage": index + 1,
                "pageSize": 1}))["items"]
            self.assertEqual([tag], page)
        for is_error in (False, True):
            response = self.http("POST", "/read/paginate", dict(request, query={
                "isError": is_error, "currentPage": 99, "pageSize": 1}))
            self.assertEqual([], response["items"])

    def test_all_update_callbacks(self):
        for index, mode in enumerate(("update", "meta", "trace", "im", "im_f_m")):
            with self.subTest(mode=mode):
                self.control(hold=True, update_mode=mode)
                name = f"mode{index}"
                self.add_tags(self.tag(name))
                self.await_sample(error=3011, name=name)
                attrs = {"timestamp": 1700000000000, "quality": 192,
                         "source_timestamp": 1700000000123,
                         "server_timestamp": 1700000000456}
                self.control(update_mode=mode, **attrs)
                self.await_sample(123, name=name, attrs=attrs)
                self.await_sample(123, name=name, attrs=attrs, paginate=True)
                self.control(type=10, value=1.5, update_mode=mode)
                self.await_sample(error=3007, name=name)
                self.control(value=125, update_mode=mode)
                self.await_sample(125, name=name)

    def test_optional_sample_attrs(self):
        self.add_tags(self.tag(), self.tag("explicit", type=3))
        fields = {"timestamp": 1700000000000, "quality": 192,
                  "source_timestamp": 1700000000123, "server_timestamp": 1700000000456}
        for flags in range(16):
            attrs = {key: fields[key] for index, key in enumerate(SAMPLE_ATTRS)
                     if flags & (1 << index)}
            with self.subTest(flags=flags):
                self.control(**attrs)
                for name in ("a", "explicit"):
                    for paginate in (False, True):
                        self.await_sample(123, name=name, attrs=attrs, paginate=paginate)
                self.await_sample(123, attrs=attrs, sync=True)
        zeros = dict.fromkeys(SAMPLE_ATTRS, 0)
        self.control(**zeros)
        for paginate in (False, True):
            self.await_sample(123, attrs=zeros, paginate=paginate)
        self.control(error=3002, **fields)
        for paginate in (False, True):
            self.await_sample(error=3002, attrs=fields, paginate=paginate)
        self.control(error=3002)
        for paginate in (False, True):
            self.await_sample(error=3002, attrs={}, paginate=paginate)
        self.control()
        for paginate in (False, True):
            self.await_sample(123, attrs={}, paginate=paginate)

    def test_sample_attrs_pagination(self):
        self.add_tags(self.tag("a"), self.tag("b"), self.tag("c"))
        attrs = {"quality": 0, "source_timestamp": 1700000000123}
        self.control(**attrs)
        self.await_sample(123, attrs=attrs)
        self.control(hold=True)
        request = {"node": self.node, "group": "g"}
        full = self.http("POST", "/read/paginate", request)["items"]
        for index, expected in enumerate(full):
            page = self.http("POST", "/read/paginate", dict(request, query={
                "currentPage": index + 1, "pageSize": 1}))
            self.assertEqual([expected["name"]], [tag["name"] for tag in page["items"]])
            self.assertEqual(attrs, {key: page["items"][0][key] for key in SAMPLE_ATTRS
                                     if key in page["items"][0]})
        self.control(error=3002, **attrs)
        for name in ("a", "b", "c"):
            self.await_sample(error=3002, attrs=attrs, name=name)
        self.control(hold=True)
        errors = self.http("POST", "/read/paginate", dict(request, query={"isError": True}))
        self.assertEqual(3, errors["meta"]["total"])
        for index, expected in enumerate(errors["items"]):
            page = self.http("POST", "/read/paginate", dict(request, query={
                "isError": True, "currentPage": index + 1, "pageSize": 1}))
            self.assertEqual([expected], page["items"])
        for is_error in (False, True):
            empty = self.http("POST", "/read/paginate", dict(request, query={
                "isError": is_error, "currentPage": 8, "pageSize": 1}))
            self.assertEqual([], empty["items"])
        self.control()
        for name in ("a", "b", "c"):
            self.await_sample(123, attrs={}, name=name)
        empty = self.http("POST", "/read/paginate", dict(request, query={"isError": True}))
        self.assertEqual([], empty["items"])
        self.assertEqual(0, empty["meta"]["total"])

    def test_scaled_value_and_byte_order(self):
        self.add_tags(self.tag(decimal=0.5, bias=1), self.tag("big", address="sample#B"))
        self.control(value=256)
        self.await_sample(129, resolved_type=3)
        self.await_sample(129, resolved_type=3, paginate=True)
        self.await_types({"a": 3, "big": 3})
        self.await_sample(1, name="big", resolved_type=3)
        self.write(51)
        self.assertEqual(100, self.records()[-1]["value"])
        self.write(1, name="big")
        self.assertEqual(256, self.records()[-1]["value"])
        self.metadata_is_auto()

    def test_pointer_string_recovery(self):
        self.add_tags(self.tag())
        self.control(type=13, value="hello", pointer=True)
        self.await_sample("hello")
        self.await_sample("hello", paginate=True)
        self.write("world")
        self.assertEqual({"tag": "a", "type": 13, "value": "world", "path": "single"}, self.records()[-1])
        self.control(type=13, value="recover", pointer=True, error=3002)
        self.await_sample(error=3002)
        self.control(type=13, value="recover", pointer=True)
        self.await_sample("recover")
        self.control(type=3)
        self.await_sample(error=3007)
        self.control(type=13, value="back", pointer=False)
        self.await_sample("back")
        self.metadata_is_auto()

    def test_batch_validation_and_cross_group_writes(self):
        self.add_tags(self.tag())
        self.http("POST", "/group", {"node": self.node, "group": "g2", "interval": 100}, error=0)
        self.add_tags(self.tag("b"), group="g2")
        self.control(hold=True)
        self.await_sample(error=3011)
        self.await_sample(error=3011, name="b", group="g2")
        for endpoint, body in (
            ("/write/tags", {"node": self.node, "group": "g", "tags": [{"tag": "a", "value": 12}]}),
            ("/write/gtags", {"node": self.node, "groups": [
                {"group": "g", "tags": [{"tag": "a", "value": 12}]},
                {"group": "g2", "tags": [{"tag": "b", "value": 13}]}]}),
        ):
            with self.subTest(endpoint=endpoint):
                self.http("POST", endpoint, body, error=3011)
        self.assertEqual([], self.records())
        self.control()
        self.await_sample(123)
        self.await_sample(123, name="b", group="g2")
        for value, code in ((40000, 3020), ("123", 3007)):
            for endpoint, body in (
                ("/write/tags", {"node": self.node, "group": "g", "tags": [{"tag": "a", "value": value}]}),
                ("/write/gtags", {"node": self.node, "groups": [
                    {"group": "g", "tags": [{"tag": "a", "value": value}]}]}),
            ):
                with self.subTest(endpoint=endpoint, value=value):
                    self.http("POST", endpoint, body, status=400, error=code)
        self.assertEqual([], self.records())
        self.http("POST", "/write/gtags", {"node": self.node, "groups": [
            {"group": "g", "tags": [{"tag": "a", "value": 12}]},
            {"group": "g2", "tags": [{"tag": "b", "value": 13}]}]}, error=0)
        self.assertEqual({("a", 3, 12), ("b", 3, 13)},
                         {(r["tag"], r["type"], r["value"]) for r in self.records()})

    def test_reconfigure_resets_detection(self):
        self.add_tags(self.tag())
        self.control()
        self.await_sample(123)
        self.control(hold=True)
        self.add_tags(self.tag(address="changed"), method="PUT")
        self.await_sample(error=3011)
        self.write(123, error=3011)
        self.control(type=10, value=12.5)
        self.await_sample(12.5)
        self.write(2.5)
        self.assertEqual(10, self.records()[-1]["type"])
        self.assertEqual(2.5, self.records()[-1]["value"])
        self.metadata_is_auto()

    def test_auto_admission_and_explicit_type_regression(self):
        self.add_tags(self.tag(attribute=2), error=2204, status=400)
        self.add_tags(self.tag(type=3))
        self.control(value=-123)
        self.await_sample(-123)
        self.write(123)
        self.assertEqual(3, self.records()[-1]["type"])
        self.write(40000, error=3020)
        self.write("123", error=3007)
        self.http("POST", "/node", {"name": "modbus-auto-reject", "plugin": "Modbus TCP"}, error=0)
        try:
            self.http("POST", "/group", {"node": "modbus-auto-reject", "group": "g", "interval": 100}, error=0)
            self.http("POST", "/tags", {"node": "modbus-auto-reject", "group": "g",
                "tags": [self.tag(address="1!400001")]}, status=400, error=2204)
        finally:
            self.http("DELETE", "/node", {"name": "modbus-auto-reject"}, error=0)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, default=BUILD)
    parser.add_argument("--keep-runtime", action="store_true")
    args, remaining = parser.parse_known_args()
    BUILD = args.build.resolve()
    KEEP_RUNTIME = args.keep_runtime
    unittest.main(argv=[__file__] + remaining, verbosity=2)
