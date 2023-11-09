import neuron.api as api
from neuron.config import *
from neuron.common import *
from neuron.error import *
from prometheus_client.parser import text_string_to_metric_families


class TestMetrics:
    @description(given="neuron start", when="get global metrics", then="success")
    def test_get_global_metrics(self):
        resp = api.get_metrics(category="global")
        assert 200 == resp.status_code
        flag_version = False
        flag_kernel = False
        flag_machine = False
        flag_clib = False
        for family in text_string_to_metric_families(resp.content.decode('utf-8')):
            for sample in family.samples:
                if sample.name == "os_info" and "version" in sample.labels:
                    flag_version = True
                if sample.name == "os_info" and "kernel" in sample.labels:
                    flag_kernel = True
                if sample.name == "os_info" and "machine" in sample.labels:
                    flag_machine = True
                if sample.name == "os_info" and "clib" in sample.labels:
                    flag_clib = True

        assert flag_version
        assert flag_kernel
        assert flag_machine
        assert flag_clib
