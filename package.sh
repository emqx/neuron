#!/bin/bash

set -euo

lib_path=/usr/local/lib
arch=x86_64-linux-gnu
package_name=neuron

function usage() {
    echo "Usage: $0 "
    echo "-n package name [default: ${package_name}]"
    echo "-p target platform compiler prefix (opts: x86_64-linux-gnu, arm-linux-gnueabihf, aarch64-linux-gnu, ...)[default: x86_64-linux-gnu]"
}

while getopts "p:n:h-:" OPT; do
    case ${OPT} in
    p)
        arch=$OPTARG
        ;;
    n)
        package_name=$OPTARG
        ;;
    h)
        usage
        exit 0
        ;;
    \?)
        usage
        exit 1
        ;;
    esac
done

if [ ! -d $package_name ];then 
    mkdir -p $package_name/persistence
    mkdir -p $package_name/config
    mkdir -p $package_name/plugins/schema
    mkdir -p $package_name/logs
else 
    rm -rf ${package_name}/*
fi

cp zlog/src/libzlog.so.1.2 ${package_name}

cp .gitkeep ${package_name}/logs/
cp default_plugins.json ${package_name}/persistence/plugins.json
cp zlog.conf ${package_name}/config/

cp build/libneuron-base.so \
    build/neuron \
    ${package_name}

cp build/config/neuron.yaml \
    neuron.key \
    neuron.pem \
    ${package_name}/config/

cp build/plugins/schema/ekuiper.json \
    build/plugins/schema/mqtt.json \
    build/plugins/schema/modbus-tcp.json \
    ${package_name}/plugins/schema/

cp build/plugins/libplugin-ekuiper.so \
    build/plugins/libplugin-modbus-tcp.so \
    build/plugins/libplugin-mqtt.so \
    ${package_name}/plugins/

tar czf ${package_name}-${arch}.tar.gz ${package_name}/
ls ${package_name}
rm -rf ${package_name}

echo "${package_name}-${arch}.tar.gz"
