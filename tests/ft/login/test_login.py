import os
import sys

import neuron.api as api
from neuron.common import *
from neuron.error import *


class TestLogin:

    @description(given="name and password", when="login", then="login success with token")
    def test_login_success(self):
        response = api.login()
        assert 200 == response.status_code

    @description(given="invalid user", when="login", then="login failed")
    def test_login_invalid_user(self):
        response = api.login('invalid_user')
        assert 401 == response.status_code
        assert NEU_ERR_INVALID_USER_OR_PASSWORD == response.json()['error']

    @description(given="invalid password", when="login", then="login failed")
    def test_login_invalid_password(self):
        response = api.login(password='invalid_password')
        assert 401 == response.status_code
        assert NEU_ERR_INVALID_USER_OR_PASSWORD == response.json()['error']

    @description(given="user, old and new password", when="change password", then="change success")
    def test_change_password(self):
        response = api.change_password(new_password='invalid_password')
        assert 200 == response.status_code
        assert 0 == response.json()['error']

        response = api.login(password='invalid_password')
        assert 200 == response.status_code
