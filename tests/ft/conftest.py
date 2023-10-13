import pytest


def pytest_addoption(parser):
    parser.addoption(
        "--env", default="github", choices=["github", "test"], help="enviroment parameter"
    )


@pytest.fixture(autouse=True, scope="session")
def get_envx(request):
    return request.config.getoption("--env")
