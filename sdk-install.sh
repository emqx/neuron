#!/bin/bash

set -e

neuron_bin="/usr/local/bin/neuron"
neuron_include="/usr/local/include/neuron"
neuron_lib="/usr/local/lib/neuron"

cp neuron.conf /etc/ld.so.conf.d/

mkdir -p /usr/local/lib/cmake/neuron/
cp neuron-config.cmake /usr/local/lib/cmake/neuron/

mkdir -p ${neuron_include}
cp -r include/neuron/* ${neuron_include}/

mkdir -p ${neuron_lib}
cp lib/* ${neuron_lib}/

mkdir -p ${neuron_bin}
mkdir -p ${neuron_bin}/config
mkdir -p ${neuron_bin}/persistence
mkdir -p ${neuron_bin}/plugins

cp ./neuron ${neuron_bin}/
cp config/sdk-zlog.conf ${neuron_bin}/config/zlog.conf
cp config/dev.conf ${neuron_bin}/config/
cp config/default_plugins.json ${neuron_bin}/config/
cp config/*.sql ${neuron_bin}/config/

cp -r plugins/* ${neuron_bin}/plugins/

cp -r dist ${neuron_bin}/

ldconfig

echo "Install neuron header to ${neuron_include}, include_directories ${neuron_include} in CMakeLists.txt."
echo "Install neuron library to ${neuron_lib}, link_directories ${neuron_lib} in CMakeLists.txt."
echo "Install neuron to ${neuron_bin}, the custom plugin is placed in the ${neuron_bin}/plugins directory and the ${neuron_bin}/persistence/plugins.json is modified."
