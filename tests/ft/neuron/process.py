import os
import re
import time
import subprocess


def remove_persistence(dir='build/'):
    os.system("rm -rf " + dir + "/persistence/sqlite.db")
    os.system("rm -rf " + dir + "/persistence/*")
    os.system("mkdir -p " + dir + "/persistence")


class NeuronProcess:
    def __init__(self, dir='build/'):
        self.dir = dir

    def start(self):
        self.p = start_neuron(self.dir)

    def restart(self):
        self.p = restart_neuron(self.p, self.dir)

    def stop(self):
        stop_neuron(self.p)
        self.p = None

    def start_disable_auth(self):
        self.p = start_neuron_disable_auth(self.dir)

    def start_debug(self):
        self.p = start_neuron_debug(self.dir)

def start_neuron_disable_auth(dir='build/'):
    command = ['./neuron', '--disable_auth']
    process = subprocess.Popen(
        command, stderr=subprocess.PIPE, cwd=dir)
    time.sleep(1)
    assert process.poll() is None
    return process

def start_neuron_debug(dir='build/'):
    command = ['./neuron', '--log_level', 'DEBUG']
    process = subprocess.Popen(
        command, stderr=subprocess.PIPE, cwd=dir)
    time.sleep(1)
    assert process.poll() is None
    return process

def start_neuron(dir='build/'):
    process = subprocess.Popen(
        ['./neuron'], stderr=subprocess.PIPE, cwd=dir)
    time.sleep(1)
    assert process.poll() is None
    return process


def restart_neuron(process, dir='build/'):
    stop_neuron(process)
    return start_neuron(dir)


def stop_neuron(process):
    result = process.terminate()
    process.wait()
    stderr = process.stderr.read().decode()
    stderr = re.sub(
        "(libfaketime|Please check specification).*\n?", "", stderr)
    assert stderr == '', "Neuron process has errors: " + stderr


def start_simulator(args, dir='build/simulator'):
    process = subprocess.Popen(
        args, stderr=subprocess.PIPE, cwd=dir)
    time.sleep(1)
    assert process.poll() is None
    return process


def stop_simulator(process):
    assert process.poll() is None
    process.terminate()
    process.wait()
    stderr = process.stderr.read().decode()
    if stderr != '':
        print("Simulator process has errors: " + stderr)
