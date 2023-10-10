import os
import time
import pytest
import subprocess
import random


def remove_persistence(dir='build/'):
    os.system("rm -rf " + dir + "/persistence/sqlite.db")
    os.system("mkdir -p " + dir + "/persistence")


def start_neuron(dir='build/'):
    process = subprocess.Popen(
        ['./neuron'], stderr=subprocess.PIPE, cwd=dir)
    time.sleep(1)
    assert process.poll() is None
    return process


def stop_neuron(process):
    result = process.terminate()
    stderr = process.stderr.read().decode()
    assert stderr == '', "Neuron process has errors: " + stderr


def start_simulator(args, dir='build/simulator'):
    process = subprocess.Popen(
        args, stderr=subprocess.PIPE, cwd=dir)
    time.sleep(1)
    assert process.poll() is None
    return process


def stop_simulator(process):
    process.terminate()


def random_port():
    return random.randint(30000, 65535)
