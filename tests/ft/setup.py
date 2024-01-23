from setuptools import setup, find_packages

packages = find_packages(include=["neuron*"])
print("Found packages:", packages)

setup(
    name="neuronft",
    version="0.0.2",
    description="Neuron function test utilities",
    author="Neuron Team",
    author_email="neuron@emqx.io",
    packages=packages,
    package_data={"neuron.data": ["*.key", "*.pem"]},
)
