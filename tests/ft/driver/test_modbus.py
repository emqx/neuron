import subprocess
import select
import fcntl
import re
import os
import neuron.api as api
import neuron.error as error
import neuron.config as config
from neuron.common import *

tcp_port = random_port()
rtu_port = random_port()


def start_socat():
    socat_cmd = ["socat", "-d", "-d", "pty,raw,echo=0", "pty,raw,echo=0"]
    socat_proc = subprocess.Popen(
        socat_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE
    )
    
    fl = fcntl.fcntl(socat_proc.stderr.fileno(), fcntl.F_GETFL)
    fcntl.fcntl(socat_proc.stderr.fileno(), fcntl.F_SETFL, fl | os.O_NONBLOCK)

    pty_regex = re.compile(r'N PTY is (/dev/pts/\d+)')
    dev1 = dev2 = None

    while not dev1 or not dev2:
        ready = select.select([socat_proc.stderr], [], [], 5)
        if ready[0]:
            line = socat_proc.stderr.readline()
            match = pty_regex.search(line.decode('utf-8'))
            if match:
                if not dev1:
                    dev1 = match.group(1)
                else:
                    dev2 = match.group(1)

    return socat_proc, dev1, dev2


@pytest.fixture(params=[('modbus-tcp', config.PLUGIN_MODBUS_TCP),
                        ('modbus-rtu', config.PLUGIN_MODBUS_RTU),
                        ('modbus-rtu-tty', config.PLUGIN_MODBUS_RTU)], scope='class')
def param(request):
    return request.param


@pytest.fixture(autouse=True, scope='class')
def simulator_setup_teardown(param):
    if param[0] == 'modbus-tcp':
        p = process.start_simulator(
            ['./modbus_simulator', 'tcp', f'{tcp_port}', 'ip_v4'])
    elif param[0] == 'modbus-rtu':
        p = process.start_simulator(
            ['./modbus_simulator', 'rtu', f'{rtu_port}', 'ip_v4'])
    elif param[0] == 'modbus-rtu-tty':
        socat_proc, modbus_neuron_dev, modbus_simulator_dev = start_socat()
        print(modbus_neuron_dev)
        print(modbus_simulator_dev)
        p = process.start_simulator(
            ['./modbus_tty_simulator', '-e', '--link', f'{modbus_simulator_dev}'])

    response = api.add_node(node=param[0], plugin=param[1])
    assert 200 == response.status_code

    response = api.add_group(node=param[0], group='group')
    assert 200 == response.status_code

    if param[0] == 'modbus-tcp':
        response = api.modbus_tcp_node_setting(
            node=param[0], interval=1, port=tcp_port)
    elif param[0] == 'modbus-rtu':
        response = api.modbus_rtu_node_setting(
            node=param[0], interval=1, port=rtu_port)
    elif param[0] == 'modbus-rtu-tty':
        api.update_group(param[0], group='group', interval=100)
        response = api.modbus_rtu_node_setting(
            node=param[0], interval=1, device=modbus_neuron_dev, link=0)

    yield
    process.stop_simulator(p)
    if param[0] == 'modbus-rtu-tty':
        socat_proc.terminate()
        time.sleep(2)
        api.node_ctl_check(node=param[0], ctl=1)


hold_bit = [{"name": "hold_bit", "address": "1!400001.15",
             "attribute": config.NEU_TAG_ATTRIBUTE_READ_SUBSCRIBE, "type": config.NEU_TYPE_BIT}]
hold_int16 = [{"name": "hold_int16", "address": "1!400001",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW_SUBSCRIBE, "type": config.NEU_TYPE_INT16}]
hold_uint16 = [{"name": "hold_uint16", "address": "1!400002",
                "attribute": config.NEU_TAG_ATTRIBUTE_RW_SUBSCRIBE, "type": config.NEU_TYPE_UINT16}]
hold_int32 = [{"name": "hold_int32", "address": "1!400003",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW_SUBSCRIBE, "type": config.NEU_TYPE_INT32}]
hold_uint32 = [{"name": "hold_uint32", "address": "1!400015",
                "attribute": config.NEU_TAG_ATTRIBUTE_RW_SUBSCRIBE, "type": config.NEU_TYPE_UINT32}]
hold_int64 = [{"name": "hold_int64", "address": "1!400005",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW_SUBSCRIBE, "type": config.NEU_TYPE_INT64}]
hold_uint64 = [{"name": "hold_uint64", "address": "1!400009",
                "attribute": config.NEU_TAG_ATTRIBUTE_RW_SUBSCRIBE, "type": config.NEU_TYPE_UINT64}]
hold_float = [{"name": "hold_float", "address": "1!400017",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW_SUBSCRIBE, "type": config.NEU_TYPE_FLOAT}]
hold_string = [{"name": "hold_string", "address": "1!400020.10",
                "attribute": config.NEU_TAG_ATTRIBUTE_RW_SUBSCRIBE, "type": config.NEU_TYPE_STRING}]
hold_bytes = [{"name": "hold_bytes", "address": "1!400040.10",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW_SUBSCRIBE, "type": config.NEU_TYPE_BYTES}]
hold_double = [{"name": "hold_double", "address": "1!400050",
                "attribute": config.NEU_TAG_ATTRIBUTE_RW_SUBSCRIBE, "type": config.NEU_TYPE_DOUBLE}]

hold_bit_0 = [{"name": "hold_bit_0", "address": "1!400001.0",
             "attribute": config.NEU_TAG_ATTRIBUTE_READ_SUBSCRIBE, "type": config.NEU_TYPE_BIT}]
hold_bit_1 = [{"name": "hold_bit_1", "address": "1!400001.1",
             "attribute": config.NEU_TAG_ATTRIBUTE_READ_SUBSCRIBE, "type": config.NEU_TYPE_BIT}]
hold_bit_2 = [{"name": "hold_bit_2", "address": "1!400001.2",
             "attribute": config.NEU_TAG_ATTRIBUTE_READ_SUBSCRIBE, "type": config.NEU_TYPE_BIT}]
hold_bit_3 = [{"name": "hold_bit_3", "address": "1!400001.3",
             "attribute": config.NEU_TAG_ATTRIBUTE_READ_SUBSCRIBE, "type": config.NEU_TYPE_BIT}]
hold_bit_4 = [{"name": "hold_bit_4", "address": "1!400001.4",
             "attribute": config.NEU_TAG_ATTRIBUTE_READ_SUBSCRIBE, "type": config.NEU_TYPE_BIT}]
hold_bit_5 = [{"name": "hold_bit_5", "address": "1!400001.5",
             "attribute": config.NEU_TAG_ATTRIBUTE_READ_SUBSCRIBE, "type": config.NEU_TYPE_BIT}]
hold_bit_6 = [{"name": "hold_bit_6", "address": "1!400001.6",
             "attribute": config.NEU_TAG_ATTRIBUTE_READ_SUBSCRIBE, "type": config.NEU_TYPE_BIT}]
hold_bit_7 = [{"name": "hold_bit_7", "address": "1!400001.7",
             "attribute": config.NEU_TAG_ATTRIBUTE_READ_SUBSCRIBE, "type": config.NEU_TYPE_BIT}]
hold_bit_8 = [{"name": "hold_bit_8", "address": "1!400001.8",
             "attribute": config.NEU_TAG_ATTRIBUTE_READ_SUBSCRIBE, "type": config.NEU_TYPE_BIT}]
hold_bit_9 = [{"name": "hold_bit_9", "address": "1!400001.9",
             "attribute": config.NEU_TAG_ATTRIBUTE_READ_SUBSCRIBE, "type": config.NEU_TYPE_BIT}]
hold_bit_10 = [{"name": "hold_bit_10", "address": "1!400001.10",
             "attribute": config.NEU_TAG_ATTRIBUTE_READ_SUBSCRIBE, "type": config.NEU_TYPE_BIT}]
hold_bit_11 = [{"name": "hold_bit_11", "address": "1!400001.11",
             "attribute": config.NEU_TAG_ATTRIBUTE_READ_SUBSCRIBE, "type": config.NEU_TYPE_BIT}]
hold_bit_12 = [{"name": "hold_bit_12", "address": "1!400001.12",
             "attribute": config.NEU_TAG_ATTRIBUTE_READ_SUBSCRIBE, "type": config.NEU_TYPE_BIT}]
hold_bit_13 = [{"name": "hold_bit_13", "address": "1!400001.13",
             "attribute": config.NEU_TAG_ATTRIBUTE_READ_SUBSCRIBE, "type": config.NEU_TYPE_BIT}]
hold_bit_14 = [{"name": "hold_bit_14", "address": "1!400001.14",
             "attribute": config.NEU_TAG_ATTRIBUTE_READ_SUBSCRIBE, "type": config.NEU_TYPE_BIT}]

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
                "attribute": config.NEU_TAG_ATTRIBUTE_READ, "type": config.NEU_TYPE_BIT, "description": "bit_1"}]
input_bit_2 = [{"name": "input_bit_2", "address": "1!100002",
                "attribute": config.NEU_TAG_ATTRIBUTE_READ, "type": config.NEU_TYPE_BIT, "description": "bit_2"}]
input_bit_3 = [{"name": "input_bit_3", "address": "1!100003",
                "attribute": config.NEU_TAG_ATTRIBUTE_READ, "type": config.NEU_TYPE_BIT, "description": "bit_3"}]
input_bit_4 = [{"name": "input_bit_4", "address": "1!100005",
                "attribute": config.NEU_TAG_ATTRIBUTE_READ, "type": config.NEU_TYPE_BIT, "description": "bit_4"}]
input_bit_5 = [{"name": "input_bit_5", "address": "1!100010",
                "attribute": config.NEU_TAG_ATTRIBUTE_READ, "type": config.NEU_TYPE_BIT, "description": "bit_5"}]

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

hold_int16_s = [{"name": "hold_int16_s", "address": "1!400801",
                 "attribute": config.NEU_TAG_ATTRIBUTE_RW_STATIC, "type": config.NEU_TYPE_INT16, "value": 1}]
hold_uint16_s = [{"name": "hold_uint16_s", "address": "1!400802",
                  "attribute": config.NEU_TAG_ATTRIBUTE_RW_STATIC, "type": config.NEU_TYPE_UINT16, "value": 1}]
hold_int32_s = [{"name": "hold_int32_s", "address": "1!400803",
                 "attribute": config.NEU_TAG_ATTRIBUTE_RW_STATIC, "type": config.NEU_TYPE_INT32, "value": 1}]
hold_uint32_s = [{"name": "hold_uint32_s", "address": "1!400815",
                  "attribute": config.NEU_TAG_ATTRIBUTE_RW_STATIC, "type": config.NEU_TYPE_UINT32, "value": 1}]
hold_int64_s = [{"name": "hold_int64_s", "address": "1!400823",
                 "attribute": config.NEU_TAG_ATTRIBUTE_RW_STATIC, "type": config.NEU_TYPE_INT64, "value": 1}]
hold_uint64_s = [{"name": "hold_uint64_s", "address": "1!400835",
                  "attribute": config.NEU_TAG_ATTRIBUTE_RW_STATIC, "type": config.NEU_TYPE_UINT64, "value": 1}]
hold_float_s = [{"name": "hold_float_s", "address": "1!400847",
                 "attribute": config.NEU_TAG_ATTRIBUTE_RW_STATIC, "type": config.NEU_TYPE_FLOAT, "value": 1.0}]
hold_double_s = [{"name": "hold_double_s", "address": "1!400857",
                 "attribute": config.NEU_TAG_ATTRIBUTE_RW_STATIC, "type": config.NEU_TYPE_DOUBLE, "value": 1.0}]
hold_string_s = [{"name": "hold_string_s", "address": "1!400870.2",
                  "attribute": config.NEU_TAG_ATTRIBUTE_RW_STATIC, "type": config.NEU_TYPE_STRING, "value": "a"}]

hold_int16_decimal = [{"name": "hold_int16_decimal", "address": "1!40103",
                       "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_INT16, "decimal": 0.1}]
hold_int16_decimal_w = [{"name": "hold_int16_decimal_w", "address": "1!40103",
                       "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_INT16, "decimal": 0.1}]
hold_int16_decimal_neg = [{"name": "int16_decimal_neg", "address": "1!40140",
                       "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_INT16, "decimal": -1}]
hold_uint16_decimal = [{"name": "hold_uint16_decimal", "address": "1!400105",
                        "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_UINT16, "decimal": 0.2}]
hold_int32_decimal = [{"name": "hold_int32_decimal", "address": "1!400199",
                       "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_INT32, "decimal": 0.3}]
hold_uint32_decimal = [{"name": "hold_uint32_decimal", "address": "1!400115",
                        "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_UINT32, "decimal": 0.3}]
hold_float_decimal = [{"name": "hold_float_decimal", "address": "1!400119",
                       "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_FLOAT, "decimal": 0.5}]
hold_float_decimal_neg = [{"name": "float_decimal_neg", "address": "1!400150",
                           "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_FLOAT, "decimal": -1}]
hold_double_decimal = [{"name": "hold_double_decimal", "address": "1!4000127",
                        "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_DOUBLE, "decimal": 0.6}]

hold_float_precision = [{"name": "hold_float_precision", "address": "1!4000235",
                         "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_FLOAT, "precision": 3}]
hold_double_precision = [{"name": "hold_double_precision", "address": "1!4000240",
                          "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_DOUBLE, "precision": 3}]

hold_int16_m1 = [{"name": "hold_int16_m1", "address": "1!400301",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_INT16}]
hold_int16_m2 = [{"name": "hold_int16_m2", "address": "1!400302",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_INT16}]
hold_int16_m3 = [{"name": "hold_int16_m3", "address": "1!400303",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_INT16}]
hold_int16_m4 = [{"name": "hold_int16_m4", "address": "1!400304",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_INT16}]
hold_int16_m5 = [{"name": "hold_int16_m5", "address": "1!400305",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_INT16}]
hold_int16_m6 = [{"name": "hold_int16_m6", "address": "1!400307",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_INT16}]
hold_int16_m7 = [{"name": "hold_int16_m7", "address": "1!400309",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_INT16}]
hold_int16_m8 = [{"name": "hold_int16_m8", "address": "1!400311",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_INT16}]
hold_int16_m9 = [{"name": "hold_int16_m9", "address": "1!400313",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_INT16}]
hold_int16_m10 = [{"name": "hold_int16_m10", "address": "1!400315",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_INT16}]
hold_int16_m11 = [{"name": "hold_int16_m11", "address": "1!400317",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_INT16}]
hold_int16_m12 = [{"name": "hold_int16_m12", "address": "1!400319",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_INT16}]
hold_int16_m13 = [{"name": "hold_int16_m13", "address": "1!400321",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_INT16}]
hold_int16_m14 = [{"name": "hold_int16_m14", "address": "1!400323",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_INT16}]
hold_int16_m15 = [{"name": "hold_int16_m15", "address": "1!400325",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_INT16}]

coil_bit_m1 = [{"name": "coil_bit_m1", "address": "1!000101",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_BIT}]
coil_bit_m2 = [{"name": "coil_bit_m2", "address": "1!000102",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_BIT}]
coil_bit_m3 = [{"name": "coil_bit_m3", "address": "1!000103",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_BIT}]
coil_bit_m4 = [{"name": "coil_bit_m4", "address": "1!000104",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_BIT}]
coil_bit_m5 = [{"name": "coil_bit_m5", "address": "1!000105",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_BIT}]
coil_bit_m6 = [{"name": "coil_bit_m6", "address": "1!000107",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_BIT}]
coil_bit_m7 = [{"name": "coil_bit_m7", "address": "1!000109",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_BIT}]
coil_bit_m8 = [{"name": "coil_bit_m8", "address": "1!000111",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_BIT}]
coil_bit_m9 = [{"name": "coil_bit_m9", "address": "1!000113",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_BIT}]
coil_bit_m10 = [{"name": "coil_bit_m10", "address": "1!000115",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_BIT}]
coil_bit_m11 = [{"name": "coil_bit_m11", "address": "1!000117",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_BIT}]
coil_bit_m12 = [{"name": "coil_bit_m12", "address": "1!000119",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_BIT}]
coil_bit_m13 = [{"name": "coil_bit_m13", "address": "1!000121",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_BIT}]
coil_bit_m14 = [{"name": "coil_bit_m14", "address": "1!000123",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_BIT}]
coil_bit_m15 = [{"name": "coil_bit_m15", "address": "1!000125",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_BIT}]

hold_int16_retry_1 = [{"name": "hold_int16_retry_1", "address": "1!408000",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_INT16}]
hold_int16_retry_2 = [{"name": "hold_int16_retry_2", "address": "1!408001",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_INT16}]

hold_int16_B = [{"name": "hold_int16_B", "address": "1!400401#B",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_INT16}]
hold_int16_L = [{"name": "hold_int16_L", "address": "1!400402#L",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_INT16}]
hold_int32_LL = [{"name": "hold_int32_LL", "address": "1!400403#LL",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_INT32}]
hold_int32_LB = [{"name": "hold_int32_LB", "address": "1!400405#LB",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_INT32}]
hold_int32_BL = [{"name": "hold_int32_BL", "address": "1!400407#BL",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_INT32}]
hold_int32_BB = [{"name": "hold_int32_BB", "address": "1!400409#BB",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_INT32}]
hold_uint16_B = [{"name": "hold_uint16_B", "address": "1!400411#B",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_UINT16}]
hold_uint16_L = [{"name": "hold_uint16_L", "address": "1!400412#L",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_UINT16}]
hold_uint32_LL = [{"name": "hold_uint32_LL", "address": "1!400413#LL",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_UINT32}]
hold_uint32_LB = [{"name": "hold_uint32_LB", "address": "1!400415#LB",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_UINT32}]
hold_uint32_BL = [{"name": "hold_uint32_BL", "address": "1!400417#BL",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_UINT32}]
hold_uint32_BB = [{"name": "hold_uint32_BB", "address": "1!400419#BB",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_UINT32}]
hold_float_LL = [{"name": "hold_float_LL", "address": "1!400421#LL",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_FLOAT}]
hold_float_LB = [{"name": "hold_float_LB", "address": "1!400423#LB",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_FLOAT}]
hold_float_BL = [{"name": "hold_float_BL", "address": "1!400425#BL",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_FLOAT}]
hold_float_BB = [{"name": "hold_float_BB", "address": "1!400427#BB",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_FLOAT}]
hold_int64_B = [{"name": "hold_int64_B", "address": "1!400429#B",
                 "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_INT64}]
hold_int64_L = [{"name": "hold_int64_L", "address": "1!400433#L",
                 "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_INT64}]
hold_uint64_B = [{"name": "hold_uint64_B", "address": "1!400437#B",
                  "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_UINT64}]
hold_uint64_L = [{"name": "hold_uint64_L", "address": "1!400441#L",
                  "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_UINT64}]
hold_double_B = [{"name": "hold_double_B", "address": "1!400445#B",
                  "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_DOUBLE}]
hold_double_L = [{"name": "hold_double_L", "address": "1!400449#L",
                  "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_DOUBLE}]
hold_string_H = [{"name": "hold_string_H", "address": "1!400710.4H",
                "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_STRING}]
hold_string_L = [{"name": "hold_string_L", "address": "1!400710.4L",
                "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_STRING}]

hold_string_l1 = [{"name": "hold_string_l1", "address": "1!400501.127",
                "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_STRING}]
hold_string_l2 = [{"name": "hold_string_l2", "address": "1!400628.127",
                "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_STRING}]

hold_int16_device_err = [{"name": "hold_int16_device_err", "address": "1!409000",
               "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_INT16}]

hold_uint16_utf8 = [{"name": "hold_uint16_utf8", "address": "1!401100",
                "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_UINT16}]
hold_string_utf8 = [{"name": "hold_string_utf8", "address": "1!401100.4",
                "attribute": config.NEU_TAG_ATTRIBUTE_RW, "type": config.NEU_TYPE_STRING}]

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

        api.add_tags_check(node=param[0], group='group', tags=hold_bit_0)
        api.add_tags_check(node=param[0], group='group', tags=hold_bit_1)
        api.add_tags_check(node=param[0], group='group', tags=hold_bit_2)
        api.add_tags_check(node=param[0], group='group', tags=hold_bit_3)
        api.add_tags_check(node=param[0], group='group', tags=hold_bit_4)
        api.add_tags_check(node=param[0], group='group', tags=hold_bit_5)
        api.add_tags_check(node=param[0], group='group', tags=hold_bit_6)
        api.add_tags_check(node=param[0], group='group', tags=hold_bit_7)
        api.add_tags_check(node=param[0], group='group', tags=hold_bit_8)
        api.add_tags_check(node=param[0], group='group', tags=hold_bit_9)
        api.add_tags_check(node=param[0], group='group', tags=hold_bit_10)
        api.add_tags_check(node=param[0], group='group', tags=hold_bit_11)
        api.add_tags_check(node=param[0], group='group', tags=hold_bit_12)
        api.add_tags_check(node=param[0], group='group', tags=hold_bit_13)
        api.add_tags_check(node=param[0], group='group', tags=hold_bit_14)

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

        api.add_tags_check(node=param[0], group='group', tags=hold_int16_m1)
        api.add_tags_check(node=param[0], group='group', tags=hold_int16_m2)
        api.add_tags_check(node=param[0], group='group', tags=hold_int16_m3)
        api.add_tags_check(node=param[0], group='group', tags=hold_int16_m4)
        api.add_tags_check(node=param[0], group='group', tags=hold_int16_m5)
        api.add_tags_check(node=param[0], group='group', tags=hold_int16_m6)
        api.add_tags_check(node=param[0], group='group', tags=hold_int16_m7)
        api.add_tags_check(node=param[0], group='group', tags=hold_int16_m8)
        api.add_tags_check(node=param[0], group='group', tags=hold_int16_m9)
        api.add_tags_check(node=param[0], group='group', tags=hold_int16_m10)
        api.add_tags_check(node=param[0], group='group', tags=hold_int16_m11)
        api.add_tags_check(node=param[0], group='group', tags=hold_int16_m12)
        api.add_tags_check(node=param[0], group='group', tags=hold_int16_m13)
        api.add_tags_check(node=param[0], group='group', tags=hold_int16_m14)
        api.add_tags_check(node=param[0], group='group', tags=hold_int16_m15)

        api.add_tags_check(node=param[0], group='group', tags=coil_bit_m1)
        api.add_tags_check(node=param[0], group='group', tags=coil_bit_m2)
        api.add_tags_check(node=param[0], group='group', tags=coil_bit_m3)
        api.add_tags_check(node=param[0], group='group', tags=coil_bit_m4)
        api.add_tags_check(node=param[0], group='group', tags=coil_bit_m5)
        api.add_tags_check(node=param[0], group='group', tags=coil_bit_m6)
        api.add_tags_check(node=param[0], group='group', tags=coil_bit_m7)
        api.add_tags_check(node=param[0], group='group', tags=coil_bit_m8)
        api.add_tags_check(node=param[0], group='group', tags=coil_bit_m9)
        api.add_tags_check(node=param[0], group='group', tags=coil_bit_m10)
        api.add_tags_check(node=param[0], group='group', tags=coil_bit_m11)
        api.add_tags_check(node=param[0], group='group', tags=coil_bit_m12)
        api.add_tags_check(node=param[0], group='group', tags=coil_bit_m13)
        api.add_tags_check(node=param[0], group='group', tags=coil_bit_m14)
        api.add_tags_check(node=param[0], group='group', tags=coil_bit_m15)

        api.add_tags_check(node=param[0], group='group', tags=hold_int16_B)
        api.add_tags_check(node=param[0], group='group', tags=hold_int16_L)
        api.add_tags_check(node=param[0], group='group', tags=hold_int32_LL)
        api.add_tags_check(node=param[0], group='group', tags=hold_int32_LB)
        api.add_tags_check(node=param[0], group='group', tags=hold_int32_BL)
        api.add_tags_check(node=param[0], group='group', tags=hold_int32_BB)

        api.add_tags_check(node=param[0], group='group', tags=hold_uint16_B)
        api.add_tags_check(node=param[0], group='group', tags=hold_uint16_L)
        api.add_tags_check(node=param[0], group='group', tags=hold_uint32_LL)
        api.add_tags_check(node=param[0], group='group', tags=hold_uint32_LB)
        api.add_tags_check(node=param[0], group='group', tags=hold_uint32_BL)
        api.add_tags_check(node=param[0], group='group', tags=hold_uint32_BB)

        api.add_tags_check(node=param[0], group='group', tags=hold_float_LL)
        api.add_tags_check(node=param[0], group='group', tags=hold_float_LB)
        api.add_tags_check(node=param[0], group='group', tags=hold_float_BL)
        api.add_tags_check(node=param[0], group='group', tags=hold_float_BB)

        api.add_tags_check(node=param[0], group='group', tags=hold_string_H)
        api.add_tags_check(node=param[0], group='group', tags=hold_string_L)

    @description(given="created modbus node", when="add and read static tags", then="add and read success")
    def test_add_read_static_tags(self, param):
        api.add_tags_check(node=param[0], group='group', tags=hold_int16_s)
        api.add_tags_check(node=param[0], group='group', tags=hold_uint16_s)
        api.add_tags_check(node=param[0], group='group', tags=hold_int32_s)
        api.add_tags_check(node=param[0], group='group', tags=hold_uint32_s)
        api.add_tags_check(node=param[0], group='group', tags=hold_int64_s)
        api.add_tags_check(node=param[0], group='group', tags=hold_uint64_s)
        api.add_tags_check(node=param[0], group='group', tags=hold_float_s)
        api.add_tags_check(node=param[0], group='group', tags=hold_double_s)
        api.add_tags_check(node=param[0], group='group', tags=hold_string_s)

        time.sleep(0.5)

        response = api.get_tags(node=param[0], group="group")
        assert 200 == response.status_code

        assert 1 == api.read_tag(node=param[0], group='group', tag=hold_int16_s[0]['name'])
        assert 1 == api.read_tag(node=param[0], group='group', tag=hold_uint16_s[0]['name'])
        assert 1 == api.read_tag(node=param[0], group='group', tag=hold_int32_s[0]['name'])
        assert 1 == api.read_tag(node=param[0], group='group', tag=hold_uint32_s[0]['name'])
        assert 1 == api.read_tag(node=param[0], group='group', tag=hold_int64_s[0]['name'])
        assert 1 == api.read_tag(node=param[0], group='group', tag=hold_uint64_s[0]['name'])
        assert 1.0 == api.read_tag(node=param[0], group='group', tag=hold_float_s[0]['name'])
        assert 1.0 == api.read_tag(node=param[0], group='group', tag=hold_double_s[0]['name'])
        assert 'a' == api.read_tag(node=param[0], group='group', tag=hold_string_s[0]['name'])

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
        assert 400 == response.status_code
        assert error.NEU_ERR_TAG_ATTRIBUTE_NOT_SUPPORT == response.json()[
            'error']

        response = api.add_tags(node=param[0], group='group', tags=wrong_tag2)
        assert 400 == response.status_code
        assert error.NEU_ERR_TAG_TYPE_NOT_SUPPORT == response.json()[
            'error']

        response = api.add_tags(node=param[0], group='group', tags=wrong_tag3)
        assert 400 == response.status_code
        assert error.NEU_ERR_TAG_ADDRESS_FORMAT_INVALID == response.json()[
            'error']

        response = api.add_tags(node=param[0], group='group', tags=wrong_tag4)
        assert 400 == response.status_code
        assert error.NEU_ERR_TAG_ADDRESS_FORMAT_INVALID == response.json()[
            'error']

        response = api.add_tags(node=param[0], group='group', tags=wrong_tag5)
        assert 400 == response.status_code
        assert error.NEU_ERR_TAG_ADDRESS_FORMAT_INVALID == response.json()[
            'error']
        
    @description(given="created modbus node and tags", when="write tags with invalid value", then="write failed")
    def test_write_tags_invalid_value(self, param):
        response = api.write_tag(node=param[0], group='group', tag=hold_int16[0]['name'], value=32768)
        assert 400 == response.status_code
        assert error.NEU_ERR_PLUGIN_TAG_VALUE_OUT_OF_RANGE == response.json()['error']

        response = api.write_tag(node=param[0], group='group', tag=hold_uint16[0]['name'], value=-1)
        assert 400 == response.status_code
        assert error.NEU_ERR_PLUGIN_TAG_VALUE_OUT_OF_RANGE == response.json()['error']

        response = api.write_tag(node=param[0], group='group', tag=hold_int32[0]['name'], value=2147483648)
        assert 400 == response.status_code
        assert error.NEU_ERR_PLUGIN_TAG_VALUE_OUT_OF_RANGE == response.json()['error']

        response = api.write_tag(node=param[0], group='group', tag=hold_uint32[0]['name'], value=-1)
        assert 400 == response.status_code
        assert error.NEU_ERR_PLUGIN_TAG_VALUE_OUT_OF_RANGE == response.json()['error']

        response = api.write_tag(node=param[0], group='group', tag=hold_string[0]['name'], value=1)
        assert 400 == response.status_code
        assert error.NEU_ERR_PLUGIN_TAG_TYPE_MISMATCH == response.json()['error']

        response = api.write_tag(node=param[0], group='group', tag=coil_bit_1[0]['name'], value=2)
        assert 400 == response.status_code
        assert error.NEU_ERR_PLUGIN_TAG_VALUE_OUT_OF_RANGE == response.json()['error']

        response = api.write_tag(node=param[0], group='group', tag=coil_bit_1[0]['name'], value=True)
        assert 400 == response.status_code
        assert error.NEU_ERR_PLUGIN_TAG_TYPE_MISMATCH == response.json()['error']

        api.add_tags_check(node=param[0], group='group', tags=hold_int16_decimal_w)

        response = api.write_tag(node=param[0], group='group', tag=hold_int16_decimal_w[0]['name'], value=1.11)
        assert 400 == response.status_code
        assert error.NEU_ERR_PLUGIN_TAG_TYPE_MISMATCH == response.json()['error']

        response = api.write_tag(node=param[0], group='group', tag=hold_float[0]['name'], value=1)
        assert 400 == response.status_code
        assert error.NEU_ERR_PLUGIN_TAG_TYPE_MISMATCH == response.json()['error']

        response = api.write_tags(node=param[0], group='group', tag_values=[
            {"tag": hold_int16[0]['name'], "value": 32768},
            {"tag": hold_uint16[0]['name'], "value": 2},
            {"tag": hold_int32[0]['name'], "value": 3},
            {"tag": hold_uint32[0]['name'], "value": 4}])
        assert 400 == response.status_code
        assert error.NEU_ERR_PLUGIN_TAG_VALUE_OUT_OF_RANGE == response.json()['error']

        response = api.write_tags(node=param[0], group='group', tag_values=[
            {"tag": hold_int16[0]['name'], "value": 1},
            {"tag": hold_uint16[0]['name'], "value": -1},
            {"tag": hold_int32[0]['name'], "value": 3},
            {"tag": hold_uint32[0]['name'], "value": 4}])
        assert 400 == response.status_code
        assert error.NEU_ERR_PLUGIN_TAG_VALUE_OUT_OF_RANGE == response.json()['error']

        response = api.write_tags(node=param[0], group='group', tag_values=[
            {"tag": hold_int16[0]['name'], "value": 1},
            {"tag": hold_uint16[0]['name'], "value": 2},
            {"tag": hold_int32[0]['name'], "value": 2147483648},
            {"tag": hold_uint32[0]['name'], "value": 4}])
        assert 400 == response.status_code
        assert error.NEU_ERR_PLUGIN_TAG_VALUE_OUT_OF_RANGE == response.json()['error']

        response = api.write_tags(node=param[0], group='group', tag_values=[
            {"tag": hold_int16[0]['name'], "value": 1},
            {"tag": hold_uint16[0]['name'], "value": 2},
            {"tag": hold_int32[0]['name'], "value": 3},
            {"tag": hold_uint32[0]['name'], "value": -1}])
        assert 400 == response.status_code
        assert error.NEU_ERR_PLUGIN_TAG_VALUE_OUT_OF_RANGE == response.json()['error']

        modbus_write_gtags = {"node": param[0], "groups": [{"group": "group", "tags" : [{"tag": "hold_int16", "value": 32768},
            {"tag": "hold_uint16", "value": 2}, {"tag": "hold_int32", "value": 3}, {"tag": "hold_uint32", "value": 4}]}]}
        response = api.write_gtags(json=modbus_write_gtags)
        assert 400 == response.status_code
        assert error.NEU_ERR_PLUGIN_TAG_VALUE_OUT_OF_RANGE == response.json()['error']

        modbus_write_gtags = {"node": param[0], "groups": [{"group": "group", "tags" : [{"tag": "hold_int16", "value": 1},
            {"tag": "hold_uint16", "value": -1}, {"tag": "hold_int32", "value": 3}, {"tag": "hold_uint32", "value": 4}]}]}
        response = api.write_gtags(json=modbus_write_gtags)
        assert 400 == response.status_code
        assert error.NEU_ERR_PLUGIN_TAG_VALUE_OUT_OF_RANGE == response.json()['error']

        modbus_write_gtags = {"node": param[0], "groups": [{"group": "group", "tags" : [{"tag": "hold_int16", "value": 1},
            {"tag": "hold_uint16", "value": 2}, {"tag": "hold_int32", "value": 2147483648}, {"tag": "hold_uint32", "value": 4}]}]}
        response = api.write_gtags(json=modbus_write_gtags)
        assert 400 == response.status_code
        assert error.NEU_ERR_PLUGIN_TAG_VALUE_OUT_OF_RANGE == response.json()['error']

        modbus_write_gtags = {"node": param[0], "groups": [{"group": "group", "tags" : [{"tag": "hold_int16", "value": 1},
            {"tag": "hold_uint16", "value": 2}, {"tag": "hold_int32", "value": 3}, {"tag": "hold_uint32", "value": -1}]}]}
        response = api.write_gtags(json=modbus_write_gtags)
        assert 400 == response.status_code
        assert error.NEU_ERR_PLUGIN_TAG_VALUE_OUT_OF_RANGE == response.json()['error']

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

        time.sleep(0.3)
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
            node=param[0], group='group', tag=hold_bit_0[0]['name'])
        assert 0 == api.read_tag(
            node=param[0], group='group', tag=hold_bit_1[0]['name'])
        assert 1 == api.read_tag(
            node=param[0], group='group', tag=hold_bit_2[0]['name'])
        assert 0 == api.read_tag(
            node=param[0], group='group', tag=hold_bit_3[0]['name'])
        assert 0 == api.read_tag(
            node=param[0], group='group', tag=hold_bit_4[0]['name'])
        assert 0 == api.read_tag(
            node=param[0], group='group', tag=hold_bit_5[0]['name'])
        assert 0 == api.read_tag(
            node=param[0], group='group', tag=hold_bit_6[0]['name'])
        assert 1 == api.read_tag(
            node=param[0], group='group', tag=hold_bit_7[0]['name'])
        assert 1 == api.read_tag(
            node=param[0], group='group', tag=hold_bit_8[0]['name'])
        assert 1 == api.read_tag(
            node=param[0], group='group', tag=hold_bit_9[0]['name'])
        assert 1 == api.read_tag(
            node=param[0], group='group', tag=hold_bit_10[0]['name'])
        assert 1 == api.read_tag(
            node=param[0], group='group', tag=hold_bit_11[0]['name'])
        assert 1 == api.read_tag(
            node=param[0], group='group', tag=hold_bit_12[0]['name'])
        assert 1 == api.read_tag(
            node=param[0], group='group', tag=hold_bit_13[0]['name'])
        assert 1 == api.read_tag(
            node=param[0], group='group', tag=hold_bit_14[0]['name'])
        assert 1 == api.read_tag(
            node=param[0], group='group', tag=hold_bit[0]['name'])

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

    @description(given="created modbus node and tags", when="read tags fuzz", then="should return correct result")
    def test_read_tags_fuzz(self, param):
        response = api.read_tags(
            node=param[0], group='group', query={"name": "input_bit"})
        assert 200 == response.status_code
        assert 5 == len(response.json()["tags"])

        response = api.read_tags(
            node=param[0], group='group', query={"name": "input_bit_1"})
        assert 200 == response.status_code
        assert 1 == len(response.json()["tags"])

        response = api.read_tags(
            node=param[0], group='group', query={"description": "bit"})
        assert 200 == response.status_code
        assert 5 == len(response.json()["tags"])

        response = api.read_tags(
            node=param[0], group='group', query={"description": "bit_1"})
        assert 200 == response.status_code
        assert 1 == len(response.json()["tags"])

        response = api.read_tags(
            node=param[0], group='group', query={"name": "input_bit", "description": "bit"})
        assert 200 == response.status_code
        assert 5 == len(response.json()["tags"])

        response = api.read_tags(
            node=param[0], group='group', query={"name": "input_bit_1", "description": "bit_1"})
        assert 200 == response.status_code
        assert 1 == len(response.json()["tags"])

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
        time.sleep(0.3)
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
        time.sleep(0.3)
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
        assert 400 == response.status_code
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

    @description(given="created modbus node", when="create group with invalid configuration", then="create failed")
    def test_config_group_with_invalid_configuration(self, param):
        response = api.add_group(node=param[0], group='group_config_test', interval = 0)
        assert 400 == response.status_code
        assert error.NEU_ERR_GROUP_PARAMETER_INVALID == response.json()[
            'error']
        
        response = api.add_group(node=param[0], group='group_config_test', interval = -1)
        assert 400 == response.status_code
        assert error.NEU_ERR_GROUP_PARAMETER_INVALID == response.json()[
            'error']

    @description(given="created modbus node", when="add decimal", then="add success")
    def test_add_decimal_tag(self, param):
        api.add_tags_check(
            node=param[0], group='group', tags=hold_int16_decimal)
        api.add_tags_check(
            node=param[0], group='group', tags=hold_int16_decimal_neg)
        api.add_tags_check(
            node=param[0], group='group', tags=hold_uint16_decimal)
        api.add_tags_check(
            node=param[0], group='group', tags=hold_int32_decimal)
        api.add_tags_check(
            node=param[0], group='group', tags=hold_uint32_decimal)
        api.add_tags_check(
            node=param[0], group='group', tags=hold_float_decimal)
        api.add_tags_check(
            node=param[0], group='group', tags=hold_float_decimal_neg)
        api.add_tags_check(
            node=param[0], group='group', tags=hold_double_decimal)

    @description(given="created modbus node/tag", when="read/write decimal tag", then="read/write success")
    def test_read_write_decimal_tag(self, param):
        api.write_tag_check(
            node=param[0], group='group', tag=hold_int16_decimal[0]['name'], value=11)
        api.write_tag_check(
            node=param[0], group='group', tag=hold_int16_decimal_neg[0]['name'], value=345)
        api.write_tag_check(
            node=param[0], group='group', tag=hold_uint16_decimal[0]['name'], value=23)
        api.write_tag_check(
            node=param[0], group='group', tag=hold_int32_decimal[0]['name'], value=123)
        api.write_tag_check(
            node=param[0], group='group', tag=hold_uint32_decimal[0]['name'], value=123)
        api.write_tag_check(
            node=param[0], group='group', tag=hold_float_decimal[0]['name'], value=121.314)
        api.write_tag_check(
            node=param[0], group='group', tag=hold_float_decimal_neg[0]['name'], value=345.6)
        api.write_tag_check(
            node=param[0], group='group', tag=hold_double_decimal[0]['name'], value=513.11)

        time.sleep(0.3)
        assert 11 == api.read_tag(
            node=param[0], group='group', tag=hold_int16_decimal[0]['name'])
        assert 345 == api.read_tag(
            node=param[0], group='group', tag=hold_int16_decimal_neg[0]['name'])
        assert 23 == api.read_tag(
            node=param[0], group='group', tag=hold_uint16_decimal[0]['name'])
        assert 123 == api.read_tag(
            node=param[0], group='group', tag=hold_int32_decimal[0]['name'])
        assert 123 == api.read_tag(
            node=param[0], group='group', tag=hold_uint32_decimal[0]['name'])
        compare_float(121.314, api.read_tag(
            node=param[0], group='group', tag=hold_float_decimal[0]['name']))
        compare_float(345.6, api.read_tag(
            node=param[0], group='group', tag=hold_float_decimal[0]['name']))
        assert 513.11 == api.read_tag(
            node=param[0], group='group', tag=hold_double_decimal[0]['name'])

    @description(given="created modbus node and tags", when="write/read tags, check utf8", then="write/read success")
    def test_write_read_tags_utf8(self, param):
        api.add_tags_check(node=param[0], group='group', tags=hold_uint16_utf8)
        api.add_tags_check(node=param[0], group='group', tags=hold_string_utf8)

        api.write_tag_check(
            node=param[0], group='group', tag=hold_uint16_utf8[0]['name'], value=0xc2a3)

        time.sleep(0.3)
        assert 0xc2a3 == api.read_tag(
            node=param[0], group='group', tag=hold_uint16_utf8[0]['name'])
        assert "£" == api.read_tag(
            node=param[0], group='group', tag=hold_string_utf8[0]['name'])

    @description(given="created modbus node/tag", when="read/write decimal/bias tag", then="read success")
    def test_read_decimal_bias_tag_i(self, param):
        tags = [
            hold_int16[0],
            hold_int16_L[0],
            hold_int16_B[0],
            hold_uint16[0],
            hold_uint16_L[0],
            hold_uint16_B[0],
            hold_int32[0],
            hold_int32_LL[0],
            hold_int32_LB[0],
            hold_int32_BB[0],
            hold_int32_BL[0],
            hold_uint32[0],
            hold_uint32_LL[0],
            hold_uint32_LB[0],
            hold_uint32_BB[0],
            hold_uint32_BL[0],
            hold_int64[0],
            hold_int64_L[0],
            hold_int64_B[0],
            hold_uint64[0],
            hold_uint64_L[0],
            hold_uint64_B[0],
        ]

        tags_bias = [
            {
                **tag,
                "bias": random.randint(1, 1000),
                "attribute": tag["attribute"] & ~config.NEU_TAG_ATTRIBUTE_WRITE,
            }
            for tag in tags
        ]

        tags_decimal_bias = [
            {
                **tag,
                "decimal": 0.1,
            }
            for tag in tags_bias
        ]

        tag_values = [
            {
                "tag": tag['name'],
                "value": random.randint(1, 10)
            }
            for tag in tags
        ]

        try:
            # 1. prepare tags
            grp = "group-bias"
            api.add_group_check(node=param[0], group=grp, interval=100)
            api.add_tags_check(node=param[0], group=grp, tags=tags)
            api.write_tags_check(node=param[0], group=grp, tag_values=tag_values)
            # 2. read value + bias
            api.update_tags_check(node=param[0], group=grp, tags=tags_bias)
            time.sleep(0.3)
            resp_values = api.read_tags(node=param[0], group=grp).json()['tags']
            assert len(tag_values) == len(resp_values)
            for tag, value, resp in zip(tags_bias, tag_values, resp_values):
                expected = value['value'] + tag['bias']
                assert compare_float(expected, resp['value'])
            # 3. read value * decimal + bias
            api.update_tags_check(
                node=param[0], group=grp, tags=tags_decimal_bias)
            time.sleep(0.3)
            resp_values = api.read_tags(node=param[0], group=grp).json()['tags']
            assert len(tag_values) == len(resp_values)
            for tag, value, resp in zip(tags_decimal_bias, tag_values, resp_values):
                expected = value['value'] * tag['decimal'] + tag['bias']
                assert compare_float(expected, resp['value'])
        finally:
            api.del_group(node=param[0], group=grp)

    @description(given="created modbus node/tag", when="read/write decimal/bias tag", then="read success")
    def test_read_decimal_bias_tag_f(self, param):
        tags = [
            hold_float[0],
            hold_float_LL[0],
            hold_float_LB[0],
            hold_float_BB[0],
            hold_float_BL[0],
            hold_double[0],
            hold_double_L[0],
            hold_double_B[0],
        ]

        tags_bias = [
            {
                **tag,
                "bias": random.randint(1, 1000),
                "attribute": tag["attribute"] & ~config.NEU_TAG_ATTRIBUTE_WRITE,
            }
            for tag in tags
        ]

        tags_decimal_bias = [
            {
                **tag,
                "decimal": random.randint(1, 10),
            }
            for tag in tags_bias
        ]

        tag_values = [
            {
                "tag": tag['name'],
                "value": random.uniform(1.0, 10.0)
            }
            for tag in tags
        ]

        try:
            # 1. prepare tags
            grp = "group-bias"
            api.add_group_check(node=param[0], group=grp, interval=100)
            api.add_tags_check(node=param[0], group=grp, tags=tags)
            api.write_tags_check(node=param[0], group=grp, tag_values=tag_values)
            # 2. read value + bias
            api.update_tags_check(node=param[0], group=grp, tags=tags_bias)
            time.sleep(0.3)
            resp_values = api.read_tags(node=param[0], group=grp).json()['tags']
            assert len(tag_values) == len(resp_values)
            for tag, value, resp in zip(tags_bias, tag_values, resp_values):
                expected = value['value'] + tag['bias']
                assert compare_float(expected, resp['value'])
            # 3. read value * decimal + bias
            api.update_tags_check(
                node=param[0], group=grp, tags=tags_decimal_bias)
            time.sleep(0.3)
            resp_values = api.read_tags(node=param[0], group=grp).json()['tags']
            assert len(tag_values) == len(resp_values)
            for tag, value, resp in zip(tags_decimal_bias, tag_values, resp_values):
                expected = value['value'] * tag['decimal'] + tag['bias']
                assert compare_float(expected, resp['value'])
        finally:
            api.del_group(node=param[0], group=grp)

    @description(given="created modbus node", when="add precision tag", then="add success")
    def test_add_precision_tag(self, param):
        api.add_tags_check(
            node=param[0], group='group', tags=hold_float_precision)
        api.add_tags_check(
            node=param[0], group='group', tags=hold_double_precision)
        time.sleep(0.3)

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

    @description(given="created partially continuous modbus node/tag", when="write multiple hold tags in one request", then="read/write success")
    def test_write_multiple_hold_tags_partially_continuous(self, param):
        api.write_tags_check(
            node=param[0], group='group', tag_values=[
            {"tag": hold_int16_m1[0]['name'], "value": 1},
            {"tag": hold_int16_m2[0]['name'], "value": 2},
            {"tag": hold_int16_m3[0]['name'], "value": 3},
            {"tag": hold_int16_m4[0]['name'], "value": 4},
            {"tag": hold_int16_m5[0]['name'], "value": 5},
            {"tag": hold_int16_m6[0]['name'], "value": 6},
            {"tag": hold_int16_m7[0]['name'], "value": 7},
            {"tag": hold_int16_m8[0]['name'], "value": 8},
            {"tag": hold_int16_m9[0]['name'], "value": 9},
            {"tag": hold_int16_m10[0]['name'], "value": 10}])
        time.sleep(0.3)
        assert 1 == api.read_tag(
            node=param[0], group='group', tag=hold_int16_m1[0]['name'])
        assert 2 == api.read_tag(
            node=param[0], group='group', tag=hold_int16_m2[0]['name'])
        assert 3 == api.read_tag(
            node=param[0], group='group', tag=hold_int16_m3[0]['name'])
        assert 4 == api.read_tag(
            node=param[0], group='group', tag=hold_int16_m4[0]['name'])
        assert 5 == api.read_tag(
            node=param[0], group='group', tag=hold_int16_m5[0]['name'])
        assert 6 == api.read_tag(
            node=param[0], group='group', tag=hold_int16_m6[0]['name'])
        assert 7 == api.read_tag(
            node=param[0], group='group', tag=hold_int16_m7[0]['name'])
        assert 8 == api.read_tag(
            node=param[0], group='group', tag=hold_int16_m8[0]['name'])
        assert 9 == api.read_tag(
            node=param[0], group='group', tag=hold_int16_m9[0]['name'])
        assert 10 == api.read_tag(
            node=param[0], group='group', tag=hold_int16_m10[0]['name'])

    @description(given="created discontinuous modbus node/tag", when="write multiple hold tags in one request", then="read/write success")
    def test_write_multiple_hold_tags_discontinuous(self, param):
        api.write_tags_check(
            node=param[0], group='group', tag_values=[
            {"tag": hold_int16_m6[0]['name'], "value": 11},
            {"tag": hold_int16_m7[0]['name'], "value": 12},
            {"tag": hold_int16_m8[0]['name'], "value": 13},
            {"tag": hold_int16_m9[0]['name'], "value": 14},
            {"tag": hold_int16_m10[0]['name'], "value": 15},
            {"tag": hold_int16_m11[0]['name'], "value": 16},
            {"tag": hold_int16_m12[0]['name'], "value": 17},
            {"tag": hold_int16_m13[0]['name'], "value": 18},
            {"tag": hold_int16_m14[0]['name'], "value": 19},
            {"tag": hold_int16_m15[0]['name'], "value": 20}])
        time.sleep(0.3)
        assert 11 == api.read_tag(
            node=param[0], group='group', tag=hold_int16_m6[0]['name'])
        assert 12 == api.read_tag(
            node=param[0], group='group', tag=hold_int16_m7[0]['name'])
        assert 13 == api.read_tag(
            node=param[0], group='group', tag=hold_int16_m8[0]['name'])
        assert 14 == api.read_tag(
            node=param[0], group='group', tag=hold_int16_m9[0]['name'])
        assert 15 == api.read_tag(
            node=param[0], group='group', tag=hold_int16_m10[0]['name'])
        assert 16 == api.read_tag(
            node=param[0], group='group', tag=hold_int16_m11[0]['name'])
        assert 17 == api.read_tag(
            node=param[0], group='group', tag=hold_int16_m12[0]['name'])
        assert 18 == api.read_tag(
            node=param[0], group='group', tag=hold_int16_m13[0]['name'])
        assert 19 == api.read_tag(
            node=param[0], group='group', tag=hold_int16_m14[0]['name'])
        assert 20 == api.read_tag(
            node=param[0], group='group', tag=hold_int16_m15[0]['name'])

    @description(given="created partially continuous modbus node/tag", when="write multiple coil tags in one request", then="read/write success")
    def test_write_multiple_coil_tags_partially_continuous(self, param):
        api.write_tags_check(
            node=param[0], group='group', tag_values=[
            {"tag": coil_bit_m1[0]['name'], "value": 0},
            {"tag": coil_bit_m2[0]['name'], "value": 1},
            {"tag": coil_bit_m3[0]['name'], "value": 0},
            {"tag": coil_bit_m4[0]['name'], "value": 1},
            {"tag": coil_bit_m5[0]['name'], "value": 0},
            {"tag": coil_bit_m6[0]['name'], "value": 1},
            {"tag": coil_bit_m7[0]['name'], "value": 0},
            {"tag": coil_bit_m8[0]['name'], "value": 1},
            {"tag": coil_bit_m9[0]['name'], "value": 0},
            {"tag": coil_bit_m10[0]['name'], "value": 1}])
        time.sleep(0.3)
        assert 0 == api.read_tag(
            node=param[0], group='group', tag=coil_bit_m1[0]['name'])
        assert 1 == api.read_tag(
            node=param[0], group='group', tag=coil_bit_m2[0]['name'])
        assert 0 == api.read_tag(
            node=param[0], group='group', tag=coil_bit_m3[0]['name'])
        assert 1 == api.read_tag(
            node=param[0], group='group', tag=coil_bit_m4[0]['name'])
        assert 0 == api.read_tag(
            node=param[0], group='group', tag=coil_bit_m5[0]['name'])
        assert 1 == api.read_tag(
            node=param[0], group='group', tag=coil_bit_m6[0]['name'])
        assert 0 == api.read_tag(
            node=param[0], group='group', tag=coil_bit_m7[0]['name'])
        assert 1 == api.read_tag(
            node=param[0], group='group', tag=coil_bit_m8[0]['name'])
        assert 0 == api.read_tag(
            node=param[0], group='group', tag=coil_bit_m9[0]['name'])
        assert 1 == api.read_tag(
            node=param[0], group='group', tag=coil_bit_m10[0]['name'])

    @description(given="created discontinuous modbus node/tag", when="write multiple coil tags in one request", then="read/write success")
    def test_write_multiple_coil_tags_discontinuous(self, param):
        api.write_tags_check(
            node=param[0], group='group', tag_values=[
            {"tag": coil_bit_m6[0]['name'], "value": 0},
            {"tag": coil_bit_m7[0]['name'], "value": 1},
            {"tag": coil_bit_m8[0]['name'], "value": 0},
            {"tag": coil_bit_m9[0]['name'], "value": 1},
            {"tag": coil_bit_m10[0]['name'], "value": 0},
            {"tag": coil_bit_m11[0]['name'], "value": 1},
            {"tag": coil_bit_m12[0]['name'], "value": 0},
            {"tag": coil_bit_m13[0]['name'], "value": 1},
            {"tag": coil_bit_m14[0]['name'], "value": 0},
            {"tag": coil_bit_m15[0]['name'], "value": 1}])
        time.sleep(0.3)
        assert 0 == api.read_tag(
            node=param[0], group='group', tag=coil_bit_m6[0]['name'])
        assert 1 == api.read_tag(
            node=param[0], group='group', tag=coil_bit_m7[0]['name'])
        assert 0 == api.read_tag(
            node=param[0], group='group', tag=coil_bit_m8[0]['name'])
        assert 1 == api.read_tag(
            node=param[0], group='group', tag=coil_bit_m9[0]['name'])
        assert 0 == api.read_tag(
            node=param[0], group='group', tag=coil_bit_m10[0]['name'])
        assert 1 == api.read_tag(
            node=param[0], group='group', tag=coil_bit_m11[0]['name'])
        assert 0 == api.read_tag(
            node=param[0], group='group', tag=coil_bit_m12[0]['name'])
        assert 1 == api.read_tag(
            node=param[0], group='group', tag=coil_bit_m13[0]['name'])
        assert 0 == api.read_tag(
            node=param[0], group='group', tag=coil_bit_m14[0]['name'])
        assert 1 == api.read_tag(
            node=param[0], group='group', tag=coil_bit_m15[0]['name'])

    @description(given="created modbus node/tag with different byte orders", when="write and read tag", then="read/write success")
    def test_read_write_tag_order(self, param):
        api.write_tag_check(
            node=param[0], group='group', tag=hold_int16_B[0]['name'], value=1234)
        api.write_tag_check(
            node=param[0], group='group', tag=hold_int16_L[0]['name'], value=1234)
        api.write_tag_check(
            node=param[0], group='group', tag=hold_int32_LL[0]['name'], value=12345678)
        api.write_tag_check(
            node=param[0], group='group', tag=hold_int32_LB[0]['name'], value=12345678)
        api.write_tag_check(
            node=param[0], group='group', tag=hold_int32_BL[0]['name'], value=12345678)
        api.write_tag_check(
            node=param[0], group='group', tag=hold_int32_BB[0]['name'], value=12345678)

        api.write_tag_check(
            node=param[0], group='group', tag=hold_uint16_B[0]['name'], value=1234)
        api.write_tag_check(
            node=param[0], group='group', tag=hold_uint16_L[0]['name'], value=1234)
        api.write_tag_check(
            node=param[0], group='group', tag=hold_uint32_LL[0]['name'], value=12345678)
        api.write_tag_check(
            node=param[0], group='group', tag=hold_uint32_LB[0]['name'], value=12345678)
        api.write_tag_check(
            node=param[0], group='group', tag=hold_uint32_BL[0]['name'], value=12345678)
        api.write_tag_check(
            node=param[0], group='group', tag=hold_uint32_BB[0]['name'], value=12345678)

        api.write_tag_check(
            node=param[0], group='group', tag=hold_float_LL[0]['name'], value=123.4)
        api.write_tag_check(
            node=param[0], group='group', tag=hold_float_LB[0]['name'], value=123.4)
        api.write_tag_check(
            node=param[0], group='group', tag=hold_float_BL[0]['name'], value=123.4)
        api.write_tag_check(
            node=param[0], group='group', tag=hold_float_BB[0]['name'], value=123.4)
        
        api.write_tag_check(
            node=param[0], group='group', tag=hold_string_H[0]['name'], value='abcd')

        time.sleep(0.3)
        assert 1234== api.read_tag(
            node=param[0], group='group', tag=hold_int16_B[0]['name'])
        assert 1234 == api.read_tag(
            node=param[0], group='group', tag=hold_int16_L[0]['name'])
        assert 12345678 == api.read_tag(
            node=param[0], group='group', tag=hold_int32_LL[0]['name'])
        assert 12345678 == api.read_tag(
            node=param[0], group='group', tag=hold_int32_LB[0]['name'])
        assert 12345678 == api.read_tag(
            node=param[0], group='group', tag=hold_int32_BL[0]['name'])
        assert 12345678 == api.read_tag(
            node=param[0], group='group', tag=hold_int32_BB[0]['name'])

        assert 1234== api.read_tag(
            node=param[0], group='group', tag=hold_uint16_B[0]['name'])
        assert 1234 == api.read_tag(
            node=param[0], group='group', tag=hold_uint16_L[0]['name'])
        assert 12345678 == api.read_tag(
            node=param[0], group='group', tag=hold_uint32_LL[0]['name'])
        assert 12345678 == api.read_tag(
            node=param[0], group='group', tag=hold_uint32_LB[0]['name'])
        assert 12345678 == api.read_tag(
            node=param[0], group='group', tag=hold_uint32_BL[0]['name'])
        assert 12345678 == api.read_tag(
            node=param[0], group='group', tag=hold_uint32_BB[0]['name'])

        assert compare_float(123.4, api.read_tag(
            node=param[0], group='group', tag=hold_float_LL[0]['name']))
        assert compare_float(123.4, api.read_tag(
            node=param[0], group='group', tag=hold_float_LB[0]['name']))
        assert compare_float(123.4, api.read_tag(
            node=param[0], group='group', tag=hold_float_BL[0]['name']))
        assert compare_float(123.4, api.read_tag(
            node=param[0], group='group', tag=hold_float_BB[0]['name']))
        
        assert "abcd" == api.read_tag(
            node=param[0], group='group', tag=hold_string_H[0]['name'])
        assert "badc" == api.read_tag(
            node=param[0], group='group', tag=hold_string_L[0]['name'])

    @description(given="created modbus node/tag", when="read consecutive tags with a length exceeding the protocol limit", then="read success")
    def test_read_tag_length_exceeds_250(self, param):
        api.add_tags_check(node=param[0], group='group', tags=hold_string_l1)
        api.add_tags_check(node=param[0], group='group', tags=hold_string_l2)

        api.write_tag_check(
            node=param[0], group='group', tag=hold_string_l1[0]['name'], value='a')
        api.write_tag_check(
            node=param[0], group='group', tag=hold_string_l2[0]['name'], value='b')
        time.sleep(0.5)
        assert 'a' == api.read_tag(
            node=param[0], group='group', tag=hold_string_l1[0]['name'])
        assert 'b' == api.read_tag(
            node=param[0], group='group', tag=hold_string_l2[0]['name'])
        
    @description(given="created modbus node ,groups and tags", when="write/read gtags", then="write/read success")
    def test_write_read_gtags(self, param):
        modbus_write_gtags = {"node": param[0], "groups": [{"group": "group", "tags" : [{"tag": "hold_uint16", "value": 1}, {"tag": "hold_string", "value": 'hello'},
                                    {"tag": "coil_bit_1", "value": 1}, {"tag": "hold_double_decimal", "value": 513.11}, {"tag": "hold_bytes", "value": [0x1, 0x2, 0x3, 0x4]}]},
                                                           {"group": "group1", "tags" : [{"tag": "hold_int16", "value": 1}]}]}
        response = api.write_gtags(json=modbus_write_gtags)
        assert 200 == response.status_code
        assert error.NEU_ERR_SUCCESS == response.json()['error']
        time.sleep(0.3)
        assert 1 == api.read_tag(
            node=param[0], group='group', tag=hold_uint16[0]['name'])
        assert 1 == api.read_tag(
            node=param[0], group='group1', tag=hold_int16[0]['name'])

    @description(given="created modbus device_error_test node/tag", when="read tag", then="read failed")
    def test_read_modbus_device_err(self, param):
        if param[0] == 'modbus-rtu-tty':
            pytest.skip("modbus rtu tty pass")
        api.add_tags_check(node=param[0], group='group', tags=hold_int16_device_err)
        time.sleep(4.0)
        assert error.NEU_ERR_PLUGIN_READ_FAILURE == api.read_tag_err(
            node=param[0], group='group', tag=hold_int16_device_err[0]['name'])

    @description(given="created modbus node", when="create modbus retry_test tag, write and read tag", then="read/write success after retrying")
    def test_read_tag_retry(self, param):
        if param[0] == 'modbus-rtu-tty':
            pytest.skip("modbus rtu tty pass")
        api.add_tags_check(node=param[0], group='group', tags=hold_int16_retry_1)
        api.add_tags_check(node=param[0], group='group', tags=hold_int16_retry_2)

        api.write_tag_check(
            node=param[0], group='group', tag=hold_int16_retry_1[0]['name'], value=111)
        api.write_tag_check(
            node=param[0], group='group', tag=hold_int16_retry_2[0]['name'], value=222)
        time.sleep(0.5)
        assert 111 == api.read_tag(
            node=param[0], group='group', tag=hold_int16_retry_1[0]['name'])
        assert 222 == api.read_tag(
            node=param[0], group='group', tag=hold_int16_retry_2[0]['name'])

    @description(given="close modbus simulator", when="create modbus node/tag, write and read tag", then="write/read failed")
    def test_write_read_modbus_disconnected(self, param):
        response = api.add_node(node=param[0]+"_3002", plugin=param[1])
        assert 200 == response.status_code
        if param[0] == 'modbus-tcp':
            api.modbus_tcp_node_setting(
                node=param[0]+"_3002", port=29998)
        elif param[0] == 'modbus-rtu':
            api.modbus_rtu_node_setting(
                node=param[0]+"_3002", port=29999)
        elif param[0] == 'modbus-rtu-tty':
            response = api.modbus_rtu_node_setting(
                node=param[0]+"_3002", device='tty', link=0)
        response = api.add_group(node=param[0]+"_3002", group='group')
        assert 200 == response.status_code

        api.add_tags_check(node=param[0]+"_3002", group='group', tags=hold_int16)
        time.sleep(0.3)
        assert error.NEU_ERR_PLUGIN_DISCONNECTED == api.read_tag_err(
            node=param[0]+"_3002", group='group', tag=hold_int16[0]['name'])

    @description(given="created modbus node", when="set modbus tcp_server mode", then="set success")
    def test_set_modbus_tcp_server_mode(self, param):
        if param[0] == 'modbus-tcp':
            response = api.modbus_tcp_node_setting(
                node=param[0]+"_3002", port=29997, connection_mode=1)
        elif param[0] == 'modbus-rtu':
            response = api.modbus_rtu_node_setting(
                node=param[0]+"_3002", port=29999, connection_mode=1)
        else:
            pytest.skip("modbus rtu tty pass")

        assert 200 == response.status_code
        assert error.NEU_ERR_SUCCESS == response.json()['error']