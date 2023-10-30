import neuron.api as api
import neuron.error as error
import neuron.config as config
from neuron.common import *

tcp_port = random_port()
rtu_port = random_port()


@pytest.fixture(params=[('modbus-tcp', config.PLUGIN_MODBUS_TCP),
                        ('modbus-rtu', config.PLUGIN_MODBUS_RTU)], scope='class')
def param(request):
    return request.param


@pytest.fixture(autouse=True, scope='class')
def simulator_setup_teardown(param):
    if param[1] == config.PLUGIN_MODBUS_TCP:
        p = process.start_simulator(
            ['./modbus_simulator', 'tcp', f'{tcp_port}', 'ip_v4'])
    else:
        p = process.start_simulator(
            ['./modbus_simulator', 'rtu', f'{rtu_port}', 'ip_v4'])

    response = api.add_node(node=param[0], plugin=param[1])
    assert 200 == response.status_code

    response = api.add_group(node=param[0], group='group')
    assert 200 == response.status_code

    if param[1] == config.PLUGIN_MODBUS_TCP:
        response = api.modbus_tcp_node_setting(
            node=param[0], port=tcp_port)
    else:
        response = api.modbus_rtu_node_setting(
            node=param[0], port=rtu_port)

    yield
    process.stop_simulator(p)


hold_bit = [{"name": "hold_bit", "address": "1!400001.15",
             "attribute": config.NEU_TAG_ATTRIBUTE_READ, "type": config.NEU_TYPE_BIT}]
hold_int16 = [{"name": "hold_int16", "address": "1!400001",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_INT16}]
hold_uint16 = [{"name": "hold_uint16", "address": "1!400002",
                "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_UINT16}]
hold_int32 = [{"name": "hold_int32", "address": "1!400003",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_INT32}]
hold_uint32 = [{"name": "hold_uint32", "address": "1!400015",
                "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_UINT32}]
hold_float = [{"name": "hold_float", "address": "1!400017",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_FLOAT}]
hold_string = [{"name": "hold_string", "address": "1!400020.10",
                "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_STRING}]
hold_bytes = [{"name": "hold_bytes", "address": "1!400040.10",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_BYTES}]

coil_bit_1 = [{"name": "coil_bit_1", "address": "1!000001",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_BIT}]
coil_bit_2 = [{"name": "coil_bit_2", "address": "1!000002",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_BIT}]
coil_bit_3 = [{"name": "coil_bit_3", "address": "1!000003",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_BIT}]
coil_bit_4 = [{"name": "coil_bit_4", "address": "1!000005",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_BIT}]
coil_bit_5 = [{"name": "coil_bit_5", "address": "1!000010",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_BIT}]

input_bit_1 = [{"name": "input_bit_1", "address": "1!100001",
                "attribute": config.NEU_TAG_ATTRIBUTE_READ, "type": config.NEU_TYPE_BIT}]
input_bit_2 = [{"name": "input_bit_2", "address": "1!100002",
                "attribute": config.NEU_TAG_ATTRIBUTE_READ, "type": config.NEU_TYPE_BIT}]
input_bit_3 = [{"name": "input_bit_3", "address": "1!100003",
                "attribute": config.NEU_TAG_ATTRIBUTE_READ, "type": config.NEU_TYPE_BIT}]
input_bit_4 = [{"name": "input_bit_4", "address": "1!100005",
                "attribute": config.NEU_TAG_ATTRIBUTE_READ, "type": config.NEU_TYPE_BIT}]
input_bit_5 = [{"name": "input_bit_5", "address": "1!100010",
                "attribute": config.NEU_TAG_ATTRIBUTE_READ, "type": config.NEU_TYPE_BIT}]

input_register_bit = [{"name": "input_register_bit", "address": "1!30101.15",
                       "attribute": config.NEU_TAG_ATTRIBUTE_READ, "type": config.NEU_TYPE_BIT}]
input_register_int16 = [{"name": "input_register_int16", "address": "1!30101",
                         "attribute": config.NEU_TAG_ATTRIBUTE_READ, "type": config.NEU_TYPE_INT16}]
input_register_uint16 = [{"name": "input_register_uint16", "address": "1!30102",
                          "attribute": config.NEU_TAG_ATTRIBUTE_READ, "type": config.NEU_TYPE_UINT16}]
input_register_int32 = [{"name": "input_register_int32", "address": "1!30103",
                         "attribute": config.NEU_TAG_ATTRIBUTE_READ, "type": config.NEU_TYPE_INT32}]
input_register_uint32 = [{"name": "input_register_uint32", "address": "1!30115",
                          "attribute": config.NEU_TAG_ATTRIBUTE_READ, "type": config.NEU_TYPE_UINT32}]
input_register_float = [{"name": "input_register_float", "address": "1!30117",
                         "attribute": config.NEU_TAG_ATTRIBUTE_READ, "type": config.NEU_TYPE_FLOAT}]
input_register_string = [{"name": "input_register_string", "address": "1!30120.10",
                          "attribute": config.NEU_TAG_ATTRIBUTE_READ, "type": config.NEU_TYPE_STRING}]
input_register_bytes = [{"name": "input_register_bytes", "address": "1!30130.10",
                         "attribute": config.NEU_TAG_ATTRIBUTE_READ, "type": config.NEU_TYPE_BYTES}]

hold_int16_s = [{"name": "hold_int16_s", "address": "1!400001",
                 "attribute": config.NEU_TAG_ATTRIBUTE_RW_STATIC, "type": config.NEU_TYPE_INT16, "value": 1}]
hold_uint16_s = [{"name": "hold_uint16_s", "address": "1!400002",
                  "attribute": config.NEU_TAG_ATTRIBUTE_RW_STATIC, "type": config.NEU_TYPE_UINT16, "value": 1}]
hold_int32_s = [{"name": "hold_int32_s", "address": "1!400003",
                 "attribute": config.NEU_TAG_ATTRIBUTE_RW_STATIC, "type": config.NEU_TYPE_INT32, "value": 1}]
hold_uint32_s = [{"name": "hold_uint32_s", "address": "1!400015",
                  "attribute": config.NEU_TAG_ATTRIBUTE_RW_STATIC, "type": config.NEU_TYPE_UINT32, "value": 1}]
hold_float_s = [{"name": "hold_float_s", "address": "1!400017",
                 "attribute": config.NEU_TAG_ATTRIBUTE_RW_STATIC, "type": config.NEU_TYPE_FLOAT, "value": 1.0}]
hold_string_s = [{"name": "hold_string_s", "address": "1!400020.10",
                  "attribute": config.NEU_TAG_ATTRIBUTE_RW_STATIC, "type": config.NEU_TYPE_STRING, "value": "a"}]

hold_int16_decimal = [{"name": "hold_int16_decimal", "address": "1!40103",
                       "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_INT16, "decimal": 0.1}]
hold_uint16_decimal = [{"name": "hold_uint16_decimal", "address": "1!400105",
                        "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_UINT16, "decimal": 0.2}]
hold_int32_decimal = [{"name": "hold_int32_decimal", "address": "1!400199",
                       "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_INT32, "decimal": 0.3}]
hold_uint32_decimal = [{"name": "hold_uint32_decimal", "address": "1!400115",
                        "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_UINT32, "decimal": 0.3}]
hold_float_decimal = [{"name": "hold_float_decimal", "address": "1!400119",
                       "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_FLOAT, "decimal": 0.5}]
hold_double_decimal = [{"name": "hold_double_decimal", "address": "1!4000127",
                        "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_DOUBLE, "decimal": 0.6}]

hold_float_precision = [{"name": "hold_float_precision", "address": "1!4000235",
                         "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_FLOAT, "precision": 3}]
hold_double_precision = [{"name": "hold_double_precision", "address": "1!4000240",
                          "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_DOUBLE, "precision": 3}]


class TestModbus:

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

        api.add_tags_check(
            node=param[0], group='group', tags=input_register_bit)
        api.add_tags_check(
            node=param[0], group='group', tags=input_register_int16)
        api.add_tags_check(
            node=param[0], group='group', tags=input_register_uint16)
        api.add_tags_check(
            node=param[0], group='group', tags=input_register_int32)
        api.add_tags_check(
            node=param[0], group='group', tags=input_register_uint32)
        api.add_tags_check(
            node=param[0], group='group', tags=input_register_float)
        api.add_tags_check(
            node=param[0], group='group', tags=input_register_string)
        api.add_tags_check(
            node=param[0], group='group', tags=input_register_bytes)

        api.add_tags_check(node=param[0], group='group', tags=coil_bit_1)
        api.add_tags_check(node=param[0], group='group', tags=coil_bit_2)
        api.add_tags_check(node=param[0], group='group', tags=coil_bit_3)
        api.add_tags_check(node=param[0], group='group', tags=coil_bit_4)
        api.add_tags_check(node=param[0], group='group', tags=coil_bit_5)

        api.add_tags_check(node=param[0], group='group', tags=input_bit_1)
        api.add_tags_check(node=param[0], group='group', tags=input_bit_2)
        api.add_tags_check(node=param[0], group='group', tags=input_bit_3)
        api.add_tags_check(node=param[0], group='group', tags=input_bit_4)
        api.add_tags_check(node=param[0], group='group', tags=input_bit_5)

    @description(given="created modbus node", when="add static tags", then="add success")
    def test_add_static_tags(self, param):
        api.add_tags_check(node=param[0], group='group', tags=hold_int16_s)
        api.add_tags_check(node=param[0], group='group', tags=hold_uint16_s)
        api.add_tags_check(node=param[0], group='group', tags=hold_int32_s)
        api.add_tags_check(node=param[0], group='group', tags=hold_uint32_s)
        api.add_tags_check(node=param[0], group='group', tags=hold_float_s)
        api.add_tags_check(node=param[0], group='group', tags=hold_string_s)

    @description(given="created modbus node", when="add wrong tags", then="add failed")
    def test_add_wrong_tags(self, param):
        wrong_tag1 = [{"name": "winput_bit_1", "address": "1!100001",
                       "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_BIT}]
        wrong_tag2 = [{"name": "winput_bit_1", "address": "1!100001",
                       "attribute": config.NEU_TAG_ATTRIBUTE_READ, "type": config.NEU_TYPE_INT16}]
        wrong_tag3 = [{"name": "whold_bytes_1", "address": "1!400001.9",
                       "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_BYTES}]
        wrong_tag4 = [{"name": "whold_string_1", "address": "1!400001.130",
                       "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_STRING}]
        wrong_tag5 = [{"name": "whold_bytes_2", "address": "1!400001.130",
                       "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_BYTES}]

        response = api.add_tags(node=param[0], group='group', tags=wrong_tag1)
        assert 200 == response.status_code
        assert error.NEU_ERR_TAG_ATTRIBUTE_NOT_SUPPORT == response.json()[
            'error']

        response = api.add_tags(node=param[0], group='group', tags=wrong_tag2)
        assert 200 == response.status_code
        assert error.NEU_ERR_TAG_TYPE_NOT_SUPPORT == response.json()[
            'error']

        response = api.add_tags(node=param[0], group='group', tags=wrong_tag3)
        assert 200 == response.status_code
        assert error.NEU_ERR_TAG_ADDRESS_FORMAT_INVALID == response.json()[
            'error']

        response = api.add_tags(node=param[0], group='group', tags=wrong_tag4)
        assert 200 == response.status_code
        assert error.NEU_ERR_TAG_ADDRESS_FORMAT_INVALID == response.json()[
            'error']

        response = api.add_tags(node=param[0], group='group', tags=wrong_tag5)
        assert 200 == response.status_code
        assert error.NEU_ERR_TAG_ADDRESS_FORMAT_INVALID == response.json()[
            'error']

    @description(given="created modbus node and tags", when="write/read tags", then="write/read success")
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
        api.write_tag_check(
            node=param[0], group='group', tag=coil_bit_1[0]['name'], value=1)
        api.write_tag_check(
            node=param[0], group='group', tag=coil_bit_2[0]['name'], value=0)
        api.write_tag_check(
            node=param[0], group='group', tag=coil_bit_3[0]['name'], value=1)
        api.write_tag_check(
            node=param[0], group='group', tag=coil_bit_4[0]['name'], value=1)
        api.write_tag_check(
            node=param[0], group='group', tag=coil_bit_5[0]['name'], value=0)

        time.sleep(0.30)
        assert -123 == api.read_tag(
            node=param[0], group='group', tag=hold_int16[0]['name'])
        assert 123 == api.read_tag(
            node=param[0], group='group', tag=hold_uint16[0]['name'])
        assert -1234 == api.read_tag(
            node=param[0], group='group', tag=hold_int32[0]['name'])
        assert 1234 == api.read_tag(
            node=param[0], group='group', tag=hold_uint32[0]['name'])
        assert compare_float(13.4121, api.read_tag(
            node=param[0], group='group', tag=hold_float[0]['name']))
        assert 'hello' == api.read_tag(
            node=param[0], group='group', tag=hold_string[0]['name'])
        bytes = api.read_tag(
            node=param[0], group='group', tag=hold_bytes[0]['name'])
        assert len(bytes) == 10
        assert bytes == [0x1, 0x2, 0x3, 0x4, 0, 0, 0, 0, 0, 0]

        assert 1 == api.read_tag(
            node=param[0], group='group', tag=coil_bit_1[0]['name'])
        assert 0 == api.read_tag(
            node=param[0], group='group', tag=coil_bit_2[0]['name'])
        assert 1 == api.read_tag(
            node=param[0], group='group', tag=coil_bit_3[0]['name'])
        assert 1 == api.read_tag(
            node=param[0], group='group', tag=coil_bit_4[0]['name'])
        assert 0 == api.read_tag(
            node=param[0], group='group', tag=coil_bit_5[0]['name'])

    @description(given="created modbus node and tags", when="read tags", then="read success")
    def test_read_tags(self, param):
        assert 0 == api.read_tag(
            node=param[0], group='group', tag=input_register_bit[0]['name'])
        assert 0 == api.read_tag(
            node=param[0], group='group', tag=input_register_uint16[0]['name'])
        assert 0 == api.read_tag(
            node=param[0], group='group', tag=input_register_int16[0]['name'])
        assert 0 == api.read_tag(
            node=param[0], group='group', tag=input_register_uint32[0]['name'])
        assert 0 == api.read_tag(
            node=param[0], group='group', tag=input_register_int32[0]['name'])
        compare_float(0, api.read_tag(
            node=param[0], group='group', tag=input_register_float[0]['name']))
        assert '' == api.read_tag(
            node=param[0], group='group', tag=input_register_string[0]['name'])
        bytes = api.read_tag(
            node=param[0], group='group', tag=input_register_bytes[0]['name'])
        assert len(bytes) == 10
        assert bytes == [0, 0, 0, 0, 0, 0, 0, 0, 0, 0]

        assert 0 == api.read_tag(
            node=param[0], group='group', tag=input_bit_1[0]['name'])
        assert 0 == api.read_tag(
            node=param[0], group='group', tag=input_bit_2[0]['name'])
        assert 0 == api.read_tag(
            node=param[0], group='group', tag=input_bit_3[0]['name'])
        assert 0 == api.read_tag(
            node=param[0], group='group', tag=input_bit_4[0]['name'])
        assert 0 == api.read_tag(
            node=param[0], group='group', tag=input_bit_5[0]['name'])

    @description(given="created modbus node and tags", when="read tags synchronously", then="read fail")
    def test_read_tags_sync(self, param):
        assert error.NEU_ERR_PLUGIN_NOT_SUPPORT_READ_SYNC == api.read_tag_error(
            node=param[0], group='group', tag=input_register_bit[0]['name'], sync=True)
        assert error.NEU_ERR_PLUGIN_NOT_SUPPORT_READ_SYNC == api.read_tag_error(
            node=param[0], group='group', tag=input_register_uint16[0]['name'], sync=True)
        assert error.NEU_ERR_PLUGIN_NOT_SUPPORT_READ_SYNC == api.read_tag_error(
            node=param[0], group='group', tag=input_register_int16[0]['name'], sync=True)
        assert error.NEU_ERR_PLUGIN_NOT_SUPPORT_READ_SYNC == api.read_tag_error(
            node=param[0], group='group', tag=input_register_uint32[0]['name'], sync=True)
        assert error.NEU_ERR_PLUGIN_NOT_SUPPORT_READ_SYNC == api.read_tag_error(
            node=param[0], group='group', tag=input_register_int32[0]['name'], sync=True)
        assert error.NEU_ERR_PLUGIN_NOT_SUPPORT_READ_SYNC == api.read_tag_error(
            node=param[0], group='group', tag=input_register_string[0]['name'], sync=True)

        assert error.NEU_ERR_PLUGIN_NOT_SUPPORT_READ_SYNC == api.read_tag_error(
            node=param[0], group='group', tag=input_bit_1[0]['name'], sync=True)
        assert error.NEU_ERR_PLUGIN_NOT_SUPPORT_READ_SYNC == api.read_tag_error(
            node=param[0], group='group', tag=input_bit_2[0]['name'], sync=True)
        assert error.NEU_ERR_PLUGIN_NOT_SUPPORT_READ_SYNC == api.read_tag_error(
            node=param[0], group='group', tag=input_bit_3[0]['name'], sync=True)
        assert error.NEU_ERR_PLUGIN_NOT_SUPPORT_READ_SYNC == api.read_tag_error(
            node=param[0], group='group', tag=input_bit_4[0]['name'], sync=True)
        assert error.NEU_ERR_PLUGIN_NOT_SUPPORT_READ_SYNC == api.read_tag_error(
            node=param[0], group='group', tag=input_bit_5[0]['name'], sync=True)

    @description(given="created modbus node, a tag with same name/address that in different groups", when="read tags", then="read success")
    def test_read_tag_in_diff_group(self, param):
        response = api.add_group(node=param[0], group='group1')
        assert 200 == response.status_code

        api.add_tags_check(node=param[0], group='group1', tags=hold_int16)
        api.write_tag_check(
            node=param[0], group='group', tag=hold_int16[0]['name'], value=222)
        time.sleep(0.30)
        assert 222 == api.read_tag(
            node=param[0], group='group', tag=hold_int16[0]['name'])
        assert 222 == api.read_tag(
            node=param[0], group='group1', tag=hold_int16[0]['name'])

    @description(given="created modbus node, a tag with same name diff address that in different groups", when="read tags", then="read success")
    def test_read_tag_in_diff_address_group(self, param):
        t_tag = [{"name": "t_tag", "address": "1!400031",
                  "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_INT16}]

        api.add_tags_check(node=param[0], group='group1', tags=t_tag)
        api.write_tag_check(
            node=param[0], group='group', tag=hold_int16[0]['name'], value=333)
        api.write_tag_check(
            node=param[0], group='group1', tag=t_tag[0]['name'], value=444)
        time.sleep(0.30)
        assert 333 == api.read_tag(
            node=param[0], group='group', tag=hold_int16[0]['name'])
        assert 444 == api.read_tag(
            node=param[0], group='group1', tag=t_tag[0]['name'])

    @description(given="created modbus node/tags", when="update tags", then="update success")
    def test_update_tag(self, param):
        up_tag = [{"name": "up_tag", "address": "1!400031",
                  "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_INT16}]
        api.add_tags_check(node=param[0], group='group', tags=up_tag)

        up_tag[0]['address'] = '2!400041'
        up_tag[0]['type'] = config.NEU_TYPE_FLOAT
        response = api.update_tags(
            node=param[0], group='group', tags=up_tag)
        assert 200 == response.status_code
        assert error.NEU_ERR_SUCCESS == response.json()['error']

    @description(given="created modbus node/tags", when="update tag with unmatch address/type", then="update failed")
    def test_update_wrong_tag(self, param):
        up_tag = [{"name": "up_tag", "address": "1!400031",
                  "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_INT16}]
        up_tag[0]['address'] = '2!400041'
        up_tag[0]['type'] = config.NEU_TYPE_BIT
        response = api.update_tags(
            node=param[0], group='group', tags=up_tag)
        assert 200 == response.status_code
        assert error.NEU_ERR_TAG_ATTRIBUTE_NOT_SUPPORT == response.json()[
            'error']

    @description(given="created modbus node", when="write tag that donot exist in neuron", then="write failed")
    def test_write_not_exist_tag(self, param):
        response = api.write_tag(
            node=param[0], group='group', tag='not_exist', value=1)
        assert 404 == response.status_code
        assert error.NEU_ERR_TAG_NOT_EXIST == response.json()['error']

        response = api.write_tag(
            node=param[0], group='groupxx', tag='not_exist', value=1)
        assert 404 == response.status_code
        assert error.NEU_ERR_GROUP_NOT_EXIST == response.json()['error']

        response = api.write_tag(
            node="nonode", group='group', tag='not_exist', value=1)
        assert 404 == response.status_code
        assert error.NEU_ERR_NODE_NOT_EXIST == response.json()['error']

    @description(given="created modbus node/tag", when="write only read tag", then="write failed")
    def test_write_only_read_tag(self, param):
        r_tag = [{"name": "r_tag", "address": "1!400031",
                  "attribute": config.NEU_TAG_ATTRIBUTE_READ, "type": config.NEU_TYPE_INT16}]
        api.add_tags_check(node=param[0], group='group', tags=r_tag)

        response = api.write_tag(
            node=param[0], group='group', tag=r_tag[0]['name'], value=1)
        assert 200 == response.status_code
        assert error.NEU_ERR_PLUGIN_TAG_NOT_ALLOW_WRITE == response.json()[
            'error']

    @description(given="created modbus node/tag", when="node stopped and read/write tag", then="read/write failed")
    def test_read_write_tag_stopped_node(self, param):
        response = api.node_ctl(node=param[0], ctl=config.NEU_CTL_STOP)
        assert 200 == response.status_code

        error_code = api.read_tag_error(node=param[0], group='group',
                                        tag=hold_int16[0]['name'])
        assert error.NEU_ERR_PLUGIN_NOT_RUNNING == error_code

        response = api.write_tag(
            node=param[0], group='group', tag=hold_int16[0]['name'], value=1)
        assert 200 == response.status_code
        assert error.NEU_ERR_PLUGIN_NOT_RUNNING == response.json()['error']

        response = api.node_ctl(node=param[0], ctl=config.NEU_CTL_START)
        assert 200 == response.status_code

    @description(given="created modbus node", when="add decimal", then="add success")
    def test_add_decimal_tag(self, param):
        api.add_tags_check(
            node=param[0], group='group', tags=hold_int16_decimal)
        api.add_tags_check(
            node=param[0], group='group', tags=hold_uint16_decimal)
        api.add_tags_check(
            node=param[0], group='group', tags=hold_int32_decimal)
        api.add_tags_check(
            node=param[0], group='group', tags=hold_uint32_decimal)
        api.add_tags_check(
            node=param[0], group='group', tags=hold_float_decimal)
        api.add_tags_check(
            node=param[0], group='group', tags=hold_double_decimal)

    @description(given="created modbus node/tag", when="read/write decimal tag", then="read/write success")
    def test_read_write_decimal_tag(self, param):
        api.write_tag_check(
            node=param[0], group='group', tag=hold_int16_decimal[0]['name'], value=11)
        api.write_tag_check(
            node=param[0], group='group', tag=hold_uint16_decimal[0]['name'], value=23)
        api.write_tag_check(
            node=param[0], group='group', tag=hold_int32_decimal[0]['name'], value=123)
        api.write_tag_check(
            node=param[0], group='group', tag=hold_uint32_decimal[0]['name'], value=123)
        api.write_tag_check(
            node=param[0], group='group', tag=hold_float_decimal[0]['name'], value=121.314)
        api.write_tag_check(
            node=param[0], group='group', tag=hold_double_decimal[0]['name'], value=513.11)

        time.sleep(0.3)
        assert 11 == api.read_tag(
            node=param[0], group='group', tag=hold_int16_decimal[0]['name'])
        assert 23 == api.read_tag(
            node=param[0], group='group', tag=hold_uint16_decimal[0]['name'])
        assert 123 == api.read_tag(
            node=param[0], group='group', tag=hold_int32_decimal[0]['name'])
        assert 123 == api.read_tag(
            node=param[0], group='group', tag=hold_uint32_decimal[0]['name'])
        compare_float(121.314, api.read_tag(
            node=param[0], group='group', tag=hold_float_decimal[0]['name']))
        assert 513.11 == api.read_tag(
            node=param[0], group='group', tag=hold_double_decimal[0]['name'])

    @description(given="created modbus node", when="add precision tag", then="add success")
    def test_add_precision_tag(self, param):
        api.add_tags_check(
            node=param[0], group='group', tags=hold_float_precision)
        api.add_tags_check(
            node=param[0], group='group', tags=hold_double_precision)

    @description(given="created modbus node/tag", when="read/write precision tag", then="read/write success")
    def test_read_write_precision_tag(self, param):
        api.write_tag_check(
            node=param[0], group='group', tag=hold_float_precision[0]['name'], value=121.3149121)
        api.write_tag_check(
            node=param[0], group='group', tag=hold_double_precision[0]['name'], value=513.1279112)
        time.sleep(0.3)
        compare_float(121.314, api.read_tag(
            node=param[0], group='group', tag=hold_float_precision[0]['name']))
        compare_float(513.127, api.read_tag(
            node=param[0], group='group', tag=hold_double_precision[0]['name']))
