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
else 
    rm -rf ${package_name}/*
fi

if [ ! -d $package_name/schema ];then 
    mkdir -p $package_name/schema
else 
    rm -rf ${package_name}/schema/*
fi

case "$arch" in
    "arm-linux-gnueabihf") lib_path="/opt/externs/libs/arm-linux-gnueabihf/lib"
    ;;
    "aarch64-linux-gnu") lib_path="/opt/externs/libs/aarch64-linux-gnu/lib"
    ;;
esac

cp default_plugins.json ${package_name}/persistence/plugins.json
cp build/lib*.so \
    build/neuron \
    build/neuron.yaml \
    ${package_name}/
cp build/schema/mqtt-plugin.json \
    build/schema/modbus-tcp-plugin.json \
    ${package_name}/schema/

cp private_test.key ${package_name}/
cp public_test.pem ${package_name}/

tar czf ${package_name}-${arch}.tar.gz ${package_name}/
ls ${package_name}
rm -rf ${package_name}

echo "${package_name}-${arch}.tar.gz"
