import subprocess


class NeuronLib(object):
    process = 0

    def __init__(self):
        pass

    def Start_Neuron(self):
        self.process = subprocess.Popen(['./neuron'], cwd='build/')

    def Stop_Neuron(self):
        self.process.kill()
