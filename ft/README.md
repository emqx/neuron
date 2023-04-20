# Functional test

Functional testing uses the robotframework automated testing framework, providing templates for each test case of the northbound APP and southbound DRIVER types.

Testing is divided into the following categories.
|Test case|Description|
|-----|----------|
|Core interface test|Mainly including the addition, deletion, modification, query, subscription, and other operations of the HTTP API interface for core data(PLUGIN, NODE, GROUP, TAG)|
|Southbound DRIVER test||
|Northbound APP test||
|Plugin specific test case|Such as OPC UA specific device discovery, can different devices be connected normally|

To run all functional testers

Enter the neuron/build directory.
Install Mosquito software.
```shell
$ sudo apt-get install -y mosquitto
```

Start the mosquitto server.
```shell
$ mosquitto -v &
```

Install requirements.
```shell
$ python3 -m pip install -U pip
$ python3 -m pip install -r ft/requirements.txt
```

Run functional tests and generate test reports.
```shell
$ python3 -m robot --maxerroelines=600 -P ft/ -d ft/reports ft
```
