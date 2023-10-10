import neuron.api as api
from neuron.common import *


@pytest.fixture(autouse=True, scope='class')
def simulator_setup_teardown():
    port = random_port()
    p = process.start_simulator(
        ['./modbus_simulator', 'tcp', f'{port}', 'ip_v4'])

    yield
    process.stop_simulator(p)


class TestModbusTCP:

    @description(given="name and password", when="login", then="login success with token")
    def test_login_success(self):
        response = api.login()
        assert 200 == response.status_code
