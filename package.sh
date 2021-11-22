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
    mkdir -p $package_name
else 
    rm -rf ${package_name}/*
fi

case "$arch" in
    "arm-linux-gnueabihf") lib_path="/opt/externs/libs/arm-linux-gnueabihf/lib"
    ;;
    "aarch64-linux-gnu") lib_path="/opt/externs/libs/aarch64-linux-gnu/lib"
    ;;
esac

cp ${lib_path}/libyaml.so ${package_name}/

cp build/lib*.so \
    build/neuron \
    build/neuron.yaml \
    build/plugin_param_schema.json \
    ${package_name}/

tar czf ${package_name}.tar.gz ${package_name}/
ls ${package_name}
rm -rf ${package_name}

echo "${package_name}.tar.gz"
