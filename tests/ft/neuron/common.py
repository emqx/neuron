import time
from functools import wraps
import random

import pytest

import neuron.process as process


class description(object):
    def __init__(self, given, when, then):
        self.given = given
        self.when = when
        self.then = then

    def __call__(self, func):
        @wraps(func)
        def wrapper(*args, **kwargs):
            ctx = kwargs.copy()
            given, when, then = eval('f"%s", f"%s", f"%s"' % (
                self.given, self.when, self.then), ctx)
            info = "Given " + given + ", when " + \
                when + ", then " + then + "."
            print('\033[1;33;40m')
            print("\n" + info)
            print('\033[0m', end=' Result: ')
            return func(*args, **kwargs)
        return wrapper


def random_port():
    return random.randint(30000, 65530)


def compare_float(v1, v2, delta=0.001):
    return abs(v1 - v2) < delta


@pytest.fixture(autouse=True, scope='class')
def class_setup_and_teardown():
    process.remove_persistence()
    p = process.NeuronProcess()
    p.start()
    yield p
    p.stop()


@pytest.fixture(autouse=True, scope='function')
def case_time():
    start = time.time()
    yield
    print(' Time cost: {:.3f}s'.format(time.time() - start))
