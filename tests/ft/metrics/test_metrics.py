import neuron.api as api
from neuron.config import *
from neuron.common import *
from neuron.error import *
from prometheus_client.parser import text_string_to_metric_families

def assert_metrics(resp_content, expected_metrics):
    metrics_found = {metric: False for metric in expected_metrics}

    for family in text_string_to_metric_families(resp_content):
        for sample in family.samples:
            metric_name = sample.name
            expected_value, extra_labels = expected_metrics.get(metric_name, (None, {}))
            if all(sample.labels.get(k) == v for k, v in extra_labels.items()):
                assert sample.value == expected_value, f"Metric {metric_name} expected {expected_value} but got {sample.value}"
                metrics_found[metric_name] = True

    missing_metrics = [name for name, found in metrics_found.items() if not found]
    assert not missing_metrics, f"Missing metrics: {', '.join(missing_metrics)}"

def assert_global_metrics(resp_content, expected_metrics):
    found_metrics = set()
    labels_found = {metric: set() for metric in expected_metrics if expected_metrics[metric]}

    for family in text_string_to_metric_families(resp_content):
        for sample in family.samples:
            metric_name = sample.name
            found_metrics.add(metric_name)
            if expected_metrics.get(metric_name):
                labels_found[metric_name].update(sample.labels.keys())
    
    missing_metrics = set(expected_metrics.keys()) - found_metrics
    print(f"Missing metrics: {missing_metrics}")
    assert not missing_metrics, f"Missing metrics: {', '.join(missing_metrics)}"

    if 'os_info' in expected_metrics and expected_metrics['os_info']:
        missing_labels = set(expected_metrics['os_info']) - labels_found['os_info']
        print(f"Missing labels for os_info: {missing_labels}")
        assert not missing_labels, f"Missing labels for os_info: {', '.join(missing_labels)}"

    for metric, expected_labels in expected_metrics.items():
        if expected_labels and metric != 'os_info':
            print(f"Incorrect labels for {metric}: {labels_found[metric]} vs {expected_labels}")
            assert labels_found[metric] == set(expected_labels), f"Incorrect labels for {metric}: {labels_found[metric]} vs {expected_labels}"

def get_metric_value(resp_content, metric_name):
    for family in text_string_to_metric_families(resp_content):
        for sample in family.samples:
            if sample.name == metric_name:
                return sample.value
    raise ValueError(f"Metric {metric_name} not found")

class TestMetrics:
    @description(given="neuron is started",
                 when="get global metrics and the metrics conform to the Prometheus specification",
                 then="success")
    def test_global_prometheus_specification(self):
        time.sleep(1)
        resp = api.get_metrics(category="global")
        assert 200 == resp.status_code

        expected_metrics = {
            "os_info": ["version", "kernel", "machine", "clib"],
            "cpu_percent": [],
            "cpu_cores_total": [],
            "mem_total_bytes_total": [],
            "mem_used_bytes": [],
            "mem_cache_bytes": [],
            "rss_bytes": [],
            "disk_size_gibibytes_total": [],
            "disk_used_gibibytes": [],
            "disk_avail_gibibytes": [],
            "core_dumped": [],
            "uptime_seconds_total": [],
            "north_nodes_total": [],
            "north_running_nodes_total": [],
            "north_disconnected_nodes_total": [],
            "south_nodes_total": [],
            "south_running_nodes_total": [],
            "south_disconnected_nodes_total": []
        }

        assert_global_metrics(resp.content.decode('utf-8'), expected_metrics)
        uptime_seconds_total = get_metric_value(resp.content.decode('utf-8'), 'uptime_seconds_total')
        assert 0 < uptime_seconds_total < 1000

    @description(given="neuron is started",
                 when="a MQTT node is created and the metrics conform to the Prometheus specification",
                 then="success")
    def test_app_prometheus_specification(self):
        response = api.add_node(node='mqtt', plugin=PLUGIN_MQTT)
        assert 200 == response.status_code

        resp = api.get_metrics(category="app", node='mqtt')
        assert 200 == resp.status_code

        expected_metrics = {
            "node_type": (2, {}),
            "link_state": (0, {}),
            "running_state": (1, {}),
            "send_msgs_total": (0, {}),
            "send_msg_errors_total": (0, {}),
            "recv_msgs_total": (0, {}),
            "cached_msgs": (0, {}),
            "last_5s_trans_data_msgs": (0, {}),
            "last_30s_trans_data_msgs": (0, {}),
            "last_60s_trans_data_msgs": (0, {}),
            "last_5s_send_bytes": (0, {}),
            "last_30s_send_bytes": (0, {}),
            "last_60s_send_bytes": (0, {}),
            "last_5s_recv_bytes": (0, {}),
            "last_30s_recv_bytes": (0, {}),
            "last_60s_recv_bytes": (0, {}),
            "last_5s_recv_msgs": (0, {}),
            "last_30s_recv_msgs": (0, {}),
            "last_60s_recv_msgs": (0, {}),
            "last_60s_disconnections": (0, {}),
            "last_600s_disconnections": (0, {}),
            "last_1800s_disconnections": (0, {}),
        }

        assert_metrics(resp.content.decode('utf-8'), expected_metrics)

    @description(given="neuron is started",
                 when="a Modbus node is created and the metrics conform to the Prometheus specification",
                 then="success")
    def test_driver_prometheus_specification(self):
        response = api.add_node(node='modbus', plugin=PLUGIN_MODBUS_TCP)
        assert 200 == response.status_code

        response = api.add_group(node='modbus', group='group')
        assert 200 == response.status_code

        resp = api.get_metrics(category="driver", node='modbus')
        assert 200 == resp.status_code

        expected_metrics = {
            "node_type": (1, {}),
            "link_state": (0, {}),
            "running_state": (1, {}),
            "last_rtt_ms": (9999, {}),
            "send_bytes": (0, {}),
            "recv_bytes": (0, {}),
            "tag_reads_total": (0, {}),
            "tag_read_errors_total": (0, {}),
            "tags_total": (0, {}),
            "group_tags_total": (0, {"group": "group", "node": "modbus"}),
            "group_last_error_code": (0, {"group": "group", "node": "modbus"}),
            "group_last_error_timestamp_ms": (0, {"group": "group", "node": "modbus"}),
            "group_last_send_msgs": (0, {"group": "group", "node": "modbus"}),
            "group_last_timer_ms": (0, {"group": "group", "node": "modbus"})
        }

        assert_metrics(resp.content.decode('utf-8'), expected_metrics)

        resp = api.get_metrics(category="driver")
        assert 200 == resp.status_code
        assert_metrics(resp.content.decode('utf-8'), expected_metrics)