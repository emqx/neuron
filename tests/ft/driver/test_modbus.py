import neuron.api as api
import neuron.error as error
import neuron.config as config
from neuron.common import *


@pytest.fixture(params=[('modbus-tcp', config.PLUGIN_MODBUS_TCP),
                        ('modbus-rtu', config.PLUGIN_MODBUS_RTU)], scope='class')
def param(request):
    return request.param


@pytest.fixture(autouse=True, scope='class')
def simulator_setup_teardown(param):
    port = random_port()
    if param[1] == config.PLUGIN_MODBUS_TCP:
        p = process.start_simulator(
            ['./modbus_simulator', 'tcp', f'{port}', 'ip_v4'])
    else:
        p = process.start_simulator(
            ['./modbus_simulator', 'rtu', f'{port}', 'ip_v4'])

    response = api.add_node(node=param[0], plugin=param[1])
    assert 200 == response.status_code

    if param[1] == config.PLUGIN_MODBUS_TCP:
        response = api.modbus_tcp_node_setting(
            node=param[0], port=port)
    else:
        response = api.modbus_rtu_node_setting(
            node=param[0], port=port)

    yield
    process.stop_simulator(p)


class TestModbus:

    def test_read(self):
        print("x")
