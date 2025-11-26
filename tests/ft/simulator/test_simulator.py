import json
from pathlib import Path
import socket
import time

import neuron.api as api
import neuron.error as error
import neuron.config as config
from neuron.common import *


def _update_build_neuron_json(port):
    roots = [Path("build"), Path(__file__).parents[3] / "build"]
    for path in (r / s / "neuron.json" for r in roots for s in ["config", ""]):
        if path.exists():
            with path.open("r+", encoding="utf-8") as f:
                data = json.load(f)
                sim = data.get("modbus_simulator") or {}
                sim["port"] = int(port)
                if not sim.get("ip") or sim["ip"] == "0.0.0.0":
                    sim["ip"] = "127.0.0.1"
                data["modbus_simulator"] = sim
                f.seek(0)
                json.dump(data, f, ensure_ascii=False, indent=1)
                f.truncate()
            return
    assert False, "neuron.json not found"

class TestSimulator:

    def _check_status(self, running=True, port=None, tag_count=None):
        st = api.simulator_status().json()
        assert set(st.keys()) >= {"running", "port", "tag_count", "error"}
        assert st["error"] == 0
        assert st["running"] == (1 if running else 0)
        if port is not None:
            assert st["port"] == port
        if tag_count is not None:
            assert st["tag_count"] == tag_count
        return st

    @description(given="load neuron.json",
                 when="start and stop simulator",
                 then="status reflects running and correct port")
    def test_start_stop_status(self, class_setup_and_teardown):
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.bind(("127.0.0.1", 0))
            sim_port = s.getsockname()[1]
        _update_build_neuron_json(sim_port)
        class_setup_and_teardown.restart()

        api.simulator_start()
        self._check_status(running=True, port=sim_port, tag_count=0)

        api.simulator_stop()
        self._check_status(running=False, port=sim_port)

    @description(given="set three tags using full name/address strings",
                 when="list tags",
                 then="returned tags")
    def test_set_config_and_list_tags(self, class_setup_and_teardown):
        api.simulator_start()

        tags = [
            {"type": "sine",   "name": "t1", "address": "1!400001"},
            {"type": "square", "name": "t3", "address": "1!400005"},
            {"type": "saw",    "name": "t2", "address": "1!400010"},
        ]
        api.simulator_set_config(tags)

        lt = api.simulator_list_tags().json()
        assert "tags" in lt
        assert isinstance(lt["tags"], list) and len(lt["tags"]) == 3
        names = [t["name"] for t in lt["tags"]]
        addrs = [t["address"] for t in lt["tags"]]
        types = [t["type"] for t in lt["tags"]]
        assert addrs == ["1!400001", "1!400005", "1!400010"]
        assert names == ["t1", "t3", "t2"]
        assert types == ["sine", "square", "saw"]

    @description(given="simulator configured and running",
                 when="export configuration",
                 then="exports as attachment with expected structure and correct port")
    def test_export_attachment(self):
        st = api.simulator_status().json()
        port = st.get("port", 0)
        resp = api.simulator_export()
        assert 200 == resp.status_code
        dispo = resp.headers.get("Content-Disposition", "")
        assert "attachment;" in dispo
        assert "simulator_modbus_south.json" in dispo

        data = json.loads(resp.text)
        assert "nodes" in data and isinstance(data["nodes"], list)
        assert len(data["nodes"]) >= 1
        node0 = data["nodes"][0]
        assert "plugin" in node0 and node0["plugin"] == "Modbus TCP"
        assert "params" in node0 and isinstance(node0["params"], dict)
        assert node0["params"].get("port") == port
        assert "groups" in node0 and isinstance(node0["groups"], list) and len(node0["groups"]) >= 1
        assert node0["groups"][0].get("group") == "group1"

    @description(given="simulator started and tags configured",
                 when="restart neuron with new port in config",
                 then="enabled state and tags restored from DB, port updated from neuron.json")
    def test_persistence_after_restart(self, class_setup_and_teardown):
        api.simulator_start()
        tags = [{"type": "sine", "name": "p1", "address": "1!400001"},
                {"type": "sine", "name": "p2", "address": "1!400011"}]
        api.simulator_set_config(tags)

        st_before = self._check_status(running=True, tag_count=2)
        port_before = st_before["port"]

        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.bind(("127.0.0.1", 0))
            new_port = s.getsockname()[1]
        if new_port == port_before:
            new_port += 1
        
        _update_build_neuron_json(new_port)

        class_setup_and_teardown.restart()

        self._check_status(running=True, port=new_port, tag_count=2)
        
        lt = api.simulator_list_tags().json()
        assert len(lt["tags"]) == 2
        assert lt["tags"][0]["name"] == "p1"
        assert lt["tags"][1]["name"] == "p2"
