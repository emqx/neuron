#!/bin/bash

#set -euo

lib_path=/usr/local/lib
arch=x86_64-linux-gnu
package_name=neuron-sdk

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

case "$arch" in
    "arm-linux-gnueabihf") lib_path="/opt/externs/libs/arm-linux-gnueabihf/lib"
    ;;
    "aarch64-linux-gnu") lib_path="/opt/externs/libs/aarch64-linux-gnu/lib"
    ;;
esac

rm -rf ${package_name}/*

mkdir -p $package_name/include/neuron/
mkdir -p $package_name/lib
mkdir -p $package_name/config
mkdir -p $package_name/plugins/schema

cp neuron.conf ${package_name}/
cp -r include/* ${package_name}/include/

cp build/neuron ${package_name}
cp build/libneuron-base.so ${package_name}/lib
cp zlog/src/libzlog.so.1.2 ${package_name}/lib

cp zlog.conf ${package_name}/config/
cp neuron.key ${package_name}/config/
cp neuron.pem ${package_name}/config/

cp build/plugins/schema/ekuiper.json \
    build/plugins/schema/mqtt.json \
    ${package_name}/plugins/schema/

cp build/plugins/libplugin-ekuiper.so \
    build/plugins/libplugin-mqtt.so \
    ${package_name}/plugins/

tar czf ${package_name}-${arch}.tar.gz ${package_name}/
ls ${package_name}
rm -rf ${package_name}

echo "${package_name}-${arch}.tar.gz"
