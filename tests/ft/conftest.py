import pytest

import inspect

import neuron.api as api
import neuron.config as config
import neuron.process as process


def pytest_addoption(parser):
    parser.addoption(
        "--env", default="github", choices=["github", "test"], help="enviroment parameter"
    )


@pytest.fixture(autouse=True, scope="session")
def get_envx(request):
    return request.config.getoption("--env")


def _refresh_default_jwt(token):
    old_jwt = config.default_jwt
    new_jwt = f"Bearer {token}"

    config.default_jwt = new_jwt

    for _, func in inspect.getmembers(api, inspect.isfunction):
        defaults = func.__defaults__
        if defaults:
            func.__defaults__ = tuple(
                new_jwt if value == old_jwt else value for value in defaults
            )

        kwdefaults = func.__kwdefaults__
        if kwdefaults:
            func.__kwdefaults__ = {
                key: new_jwt if value == old_jwt else value
                for key, value in kwdefaults.items()
            }


@pytest.fixture(autouse=True, scope="session")
def init_default_jwt():
    process.remove_persistence()
    neuron = process.NeuronProcess()
    neuron.start()

    response = api.login()
    assert 200 == response.status_code
    _refresh_default_jwt(response.json()["token"])

    neuron.stop()
