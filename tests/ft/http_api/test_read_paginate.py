import neuron.api as api
import neuron.error as error
import neuron.config as config
from neuron.common import *

tcp_port = random_port()


@pytest.fixture(params=[('modbus-tcp', config.PLUGIN_MODBUS_TCP)], scope='class')
def param(request):
    return request.param


@pytest.fixture(autouse=True, scope='class')
def simulator_setup_teardown(param):
    p = process.start_simulator(
        ['./modbus_simulator', 'tcp', f'{tcp_port}', 'ip_v4'])

    response = api.add_node(node=param[0], plugin=param[1])
    assert 200 == response.status_code

    response = api.add_group(node=param[0], group='group')
    assert 200 == response.status_code

    response = api.modbus_tcp_node_setting(
        node=param[0], interval=1, port=tcp_port)

    yield
    process.stop_simulator(p)


hold_bit = [{"name": "hold_bit", "address": "1!400001.15",
             "attribute": config.NEU_TAG_ATTRIBUTE_READ, "type": config.NEU_TYPE_BIT, "description": "123"}]
hold_int16 = [{"name": "hold_int16", "address": "1!400001",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_INT16, "description": "123"}]
hold_uint16 = [{"name": "hold_uint16", "address": "1!400002",
                "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_UINT16, "description": "456"}]
hold_int32 = [{"name": "hold_int32", "address": "1!400003",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_INT32, "description": "456"}]
hold_uint32 = [{"name": "hold_uint32", "address": "1!400015",
                "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_UINT32}]
hold_float = [{"name": "hold_float", "address": "1!400017",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_FLOAT}]
hold_string = [{"name": "hold_string", "address": "1!400020.10",
                "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_STRING}]
hold_bytes = [{"name": "hold_bytes", "address": "1!400040.10",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_BYTES}]


class TestReadPaginate:

    @description(given="created modbus node", when="add multiple tags", then="add success")
    def test_add_tags(self, param):
        api.add_tags_check(node=param[0], group='group', tags=hold_bit)
        api.add_tags_check(node=param[0], group='group', tags=hold_int16)
        api.add_tags_check(node=param[0], group='group', tags=hold_uint16)
        api.add_tags_check(node=param[0], group='group', tags=hold_int32)
        api.add_tags_check(node=param[0], group='group', tags=hold_uint32)
        api.add_tags_check(node=param[0], group='group', tags=hold_float)
        api.add_tags_check(node=param[0], group='group', tags=hold_string)
        api.add_tags_check(node=param[0], group='group', tags=hold_bytes)


    @description(given="created modbus node and tags", when="write/read tags without filter", then="write/read success")
    def test_write_read_tags(self, param):
        api.write_tag_check(
            node=param[0], group='group', tag=hold_int16[0]['name'], value=-123)
        api.write_tag_check(
            node=param[0], group='group', tag=hold_uint16[0]['name'], value=123)
        api.write_tag_check(
            node=param[0], group='group', tag=hold_int32[0]['name'], value=-1234)
        api.write_tag_check(
            node=param[0], group='group', tag=hold_uint32[0]['name'], value=1234)
        api.write_tag_check(
            node=param[0], group='group', tag=hold_float[0]['name'], value=13.4121)
        api.write_tag_check(
            node=param[0], group='group', tag=hold_string[0]['name'], value='hello')
        api.write_tag_check(
            node=param[0], group='group', tag=hold_bytes[0]['name'], value=[0x1, 0x2, 0x3, 0x4])

        time.sleep(0.3)
        response = api.read_tags_paginate(node=param[0], group='group')

        assert 0 == response.json()["meta"]["currentPage"]
        assert 0 == response.json()["meta"]["pageSize"]
        assert 8 == response.json()["meta"]["total"]

        assert 8 == len(response.json()["items"])

        assert "hold_bit"    == response.json()["items"][0]["name"]
        assert 1             == response.json()["items"][0]["value"]

        assert "hold_int16"  == response.json()["items"][1]["name"]
        assert -123          == response.json()["items"][1]["value"]

        assert "hold_uint16" == response.json()["items"][2]["name"]
        assert 123           == response.json()["items"][2]["value"]

        assert "hold_int32"  == response.json()["items"][3]["name"]
        assert -1234         == response.json()["items"][3]["value"]

        assert "hold_uint32" == response.json()["items"][4]["name"]
        assert 1234          == response.json()["items"][4]["value"]

        assert "hold_float"  == response.json()["items"][5]["name"]
        assert compare_float(13.4121, response.json()["items"][5]["value"])

        assert "hold_string" == response.json()["items"][6]["name"]
        assert 'hello'       == response.json()["items"][6]["value"]

        assert "hold_bytes"  == response.json()["items"][7]["name"]
        assert [0x1, 0x2, 0x3, 0x4, 0, 0, 0, 0, 0, 0] == response.json()["items"][7]["value"]

    @description(given="created modbus node and tags", when="read tags with filter(name)", then="read success")
    def test_read_tags_filter_name(self, param):
        response = api.read_tags_paginate(node=param[0], group='group', query= {"name": "int16"})

        assert 0 == response.json()["meta"]["currentPage"]
        assert 0 == response.json()["meta"]["pageSize"]
        assert 2 == response.json()["meta"]["total"]

        assert 2 == len(response.json()["items"])

        assert "hold_int16"  == response.json()["items"][0]["name"]
        assert -123          == response.json()["items"][0]["value"]
        assert "hold_uint16" == response.json()["items"][1]["name"]
        assert 123           == response.json()["items"][1]["value"]

    @description(given="created modbus node and tags", when="read tags with filter(desc)", then="read success")
    def test_read_tags_filter_desc(self, param):
        response = api.read_tags_paginate(node=param[0], group='group', query= {"description": "12"})

        assert 0 == response.json()["meta"]["currentPage"]
        assert 0 == response.json()["meta"]["pageSize"]
        assert 2 == response.json()["meta"]["total"]

        assert 2 == len(response.json()["items"])

        assert "hold_bit"   == response.json()["items"][0]["name"]
        assert 1            == response.json()["items"][0]["value"]
        assert "hold_int16" == response.json()["items"][1]["name"]
        assert -123         == response.json()["items"][1]["value"]

    @description(given="created modbus node and tags", when="read tags with paginate", then="read success")
    def test_read_tags_paginate(self, param):
        response = api.read_tags_paginate(node=param[0], group='group', query= {"currentPage": 1, "pageSize": 5})
        assert 1 == response.json()["meta"]["currentPage"]
        assert 5 == response.json()["meta"]["pageSize"]
        assert 8 == response.json()["meta"]["total"]
        assert 5 == len(response.json()["items"])

        response = api.read_tags_paginate(node=param[0], group='group', query= {"currentPage": 2, "pageSize": 5})
        assert 2 == response.json()["meta"]["currentPage"]
        assert 5 == response.json()["meta"]["pageSize"]
        assert 8 == response.json()["meta"]["total"]
        assert 3 == len(response.json()["items"])

        response = api.read_tags_paginate(node=param[0], group='group', query= {"currentPage": 3, "pageSize": 5})
        assert 3 == response.json()["meta"]["currentPage"]
        assert 5 == response.json()["meta"]["pageSize"]
        assert 8 == response.json()["meta"]["total"]
        assert 0 == len(response.json()["items"])

        response = api.read_tags_paginate(node=param[0], group='group', query= {"currentPage": 6, "pageSize": 5})
        assert 6 == response.json()["meta"]["currentPage"]
        assert 5 == response.json()["meta"]["pageSize"]
        assert 8 == response.json()["meta"]["total"]
        assert 0 == len(response.json()["items"])

        response = api.read_tags_paginate(node=param[0], group='group', query= {"currentPage": 0, "pageSize": 5})
        assert 0 == response.json()["meta"]["currentPage"]
        assert 5 == response.json()["meta"]["pageSize"]
        assert 8 == response.json()["meta"]["total"]
        assert 8 == len(response.json()["items"])

    @description(given="created modbus node and tags", when="read tags with filter(isError)", then="read success")
    def test_read_tags_filter_isError(self, param):
        response = api.read_tags_paginate(node=param[0], group='group', query= {"isError": True})

        assert 0 == response.json()["meta"]["currentPage"]
        assert 0 == response.json()["meta"]["pageSize"]
        assert 0 == response.json()["meta"]["total"]
        assert 0 == len(response.json()["items"])

        api.node_ctl_check(node='modbus-tcp', ctl=config.NEU_CTL_STOP)

        response = api.read_tags_paginate(node=param[0], group='group', query= {"isError": True})

        assert 0 == response.json()["meta"]["currentPage"]
        assert 0 == response.json()["meta"]["pageSize"]
        assert 8 == response.json()["meta"]["total"]
        assert 8 == len(response.json()["items"])

        api.node_ctl_check(node='modbus-tcp', ctl=config.NEU_CTL_START)
        time.sleep(0.3)

    @description(given="created modbus node and tags", when="read tags with filter(name&desc)", then="read success")
    def test_read_tags_filter_name_desc(self, param):
        response = api.read_tags_paginate(node=param[0], group='group', query= {"name": "16", "description": "123"})

        assert 0 == response.json()["meta"]["currentPage"]
        assert 0 == response.json()["meta"]["pageSize"]
        assert 1 == response.json()["meta"]["total"]
        assert 1 == len(response.json()["items"])
        assert "hold_int16" == response.json()["items"][0]["name"]
        assert -123         == response.json()["items"][0]["value"]

    @description(given="created modbus node and tags", when="read tags with filter(isError&name)", then="read success")
    def test_read_tags_filter_isError_name(self, param):
        api.node_ctl_check(node='modbus-tcp', ctl=config.NEU_CTL_STOP)

        response = api.read_tags_paginate(node=param[0], group='group', query= {"isError": True, "name": "16"})

        assert 0 == response.json()["meta"]["currentPage"]
        assert 0 == response.json()["meta"]["pageSize"]
        assert 2 == response.json()["meta"]["total"]
        assert 2 == len(response.json()["items"])
        assert "hold_int16"  == response.json()["items"][0]["name"]
        assert error.NEU_ERR_PLUGIN_NOT_RUNNING == response.json()["items"][0]["error"]
        assert "hold_uint16" == response.json()["items"][1]["name"]
        assert error.NEU_ERR_PLUGIN_NOT_RUNNING == response.json()["items"][1]["error"]

        api.node_ctl_check(node='modbus-tcp', ctl=config.NEU_CTL_START)
        time.sleep(0.3)

    @description(given="created modbus node and tags", when="read tags with filter(isError&desc)", then="read success")
    def test_read_tags_filter_isError_desc(self, param):
        api.node_ctl_check(node='modbus-tcp', ctl=config.NEU_CTL_STOP)

        response = api.read_tags_paginate(node=param[0], group='group', query= {"isError": True, "description": "12"})

        assert 0 == response.json()["meta"]["currentPage"]
        assert 0 == response.json()["meta"]["pageSize"]
        assert 2 == response.json()["meta"]["total"]
        assert 2 == len(response.json()["items"])
        assert "hold_bit"   == response.json()["items"][0]["name"]
        assert error.NEU_ERR_PLUGIN_NOT_RUNNING == response.json()["items"][0]["error"]
        assert "hold_int16" == response.json()["items"][1]["name"]
        assert error.NEU_ERR_PLUGIN_NOT_RUNNING == response.json()["items"][1]["error"]

        api.node_ctl_check(node='modbus-tcp', ctl=config.NEU_CTL_START)
        time.sleep(0.3)

    @description(given="created modbus node and tags", when="read tags with filter(isError&name&desc)", then="read success")
    def test_read_tags_filter_isError_name_desc(self, param):
        api.node_ctl_check(node='modbus-tcp', ctl=config.NEU_CTL_STOP)

        response = api.read_tags_paginate(node=param[0], group='group', query= {"isError": True, "name":"16", "description": "12"})

        assert 0 == response.json()["meta"]["currentPage"]
        assert 0 == response.json()["meta"]["pageSize"]
        assert 1 == response.json()["meta"]["total"]
        assert 1 == len(response.json()["items"])
        assert "hold_int16" == response.json()["items"][0]["name"]
        assert error.NEU_ERR_PLUGIN_NOT_RUNNING == response.json()["items"][0]["error"]

        api.node_ctl_check(node='modbus-tcp', ctl=config.NEU_CTL_START)
        time.sleep(0.3)

    @description(given="created modbus node and tags", when="read tags with paginate & filter(isError)", then="read success")
    def test_read_tags_paginate_filter_isError(self, param):
        api.node_ctl_check(node='modbus-tcp', ctl=config.NEU_CTL_STOP)

        response = api.read_tags_paginate(node=param[0], group='group', query= {"currentPage": 1, "pageSize": 5, "isError": True})
        assert 1 == response.json()["meta"]["currentPage"]
        assert 5 == response.json()["meta"]["pageSize"]
        assert 8 == response.json()["meta"]["total"]
        assert 5 == len(response.json()["items"])
        assert "hold_bit"    == response.json()["items"][0]["name"]
        assert error.NEU_ERR_PLUGIN_NOT_RUNNING == response.json()["items"][0]["error"]
        assert "hold_int16"  == response.json()["items"][1]["name"]
        assert error.NEU_ERR_PLUGIN_NOT_RUNNING == response.json()["items"][1]["error"]
        assert "hold_uint16" == response.json()["items"][2]["name"]
        assert error.NEU_ERR_PLUGIN_NOT_RUNNING == response.json()["items"][2]["error"]
        assert "hold_int32"  == response.json()["items"][3]["name"]
        assert error.NEU_ERR_PLUGIN_NOT_RUNNING == response.json()["items"][3]["error"]
        assert "hold_uint32" == response.json()["items"][4]["name"]
        assert error.NEU_ERR_PLUGIN_NOT_RUNNING == response.json()["items"][4]["error"]

        response = api.read_tags_paginate(node=param[0], group='group', query= {"currentPage": 2, "pageSize": 5, "isError": True})
        assert 2 == response.json()["meta"]["currentPage"]
        assert 5 == response.json()["meta"]["pageSize"]
        assert 8 == response.json()["meta"]["total"]
        assert 3 == len(response.json()["items"])
        assert "hold_float"  == response.json()["items"][0]["name"]
        assert error.NEU_ERR_PLUGIN_NOT_RUNNING == response.json()["items"][0]["error"]
        assert "hold_string" == response.json()["items"][1]["name"]
        assert error.NEU_ERR_PLUGIN_NOT_RUNNING == response.json()["items"][1]["error"]
        assert "hold_bytes"  == response.json()["items"][2]["name"]
        assert error.NEU_ERR_PLUGIN_NOT_RUNNING == response.json()["items"][2]["error"]

        api.node_ctl_check(node='modbus-tcp', ctl=config.NEU_CTL_START)
        time.sleep(0.3)