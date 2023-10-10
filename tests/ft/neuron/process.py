import os
import time
import pytest
import subprocess


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
