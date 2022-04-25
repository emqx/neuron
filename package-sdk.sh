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

if [ ! -d $package_name ];then 
    mkdir -p $package_name
else 
    rm -rf ${package_name}/*
fi

if [ ! -d $package_name/include ];then 
    mkdir -p $package_name/include/neuron/
else 
    rm -rf ${package_name}/include/*
fi

if [ ! -d $package_name/lib ];then 
    mkdir -p $package_name/lib
else 
    rm -rf ${package_name}/lib/*
fi

if [ ! -d $package_name/ft ];then 
    mkdir -p $package_name/ft
else 
    rm -rf ${package_name}/ft/*
fi

case "$arch" in
    "arm-linux-gnueabihf") lib_path="/opt/externs/libs/arm-linux-gnueabihf/lib"
    ;;
    "aarch64-linux-gnu") lib_path="/opt/externs/libs/aarch64-linux-gnu/lib"
    ;;
esac

cp -r include/* ${package_name}/include/
cp build/libneuron-base.so ${package_name}/lib
cp -r ft/Keyword ${package_name}/ft/
cp ft/api_http.resource ${package_name}/ft/
cp ft/api_mqtt.resource ${package_name}/ft/
cp ft/common.resource ${package_name}/ft/
cp ft/error.resource ${package_name}/ft/
cp ft/requirements.txt ${package_name}/ft/

cp zlog/src/libzlog.so.1.2 ${package_name}/lib

tar czf ${package_name}-${arch}.tar.gz ${package_name}/
ls ${package_name}
rm -rf ${package_name}

echo "${package_name}-${arch}.tar.gz"
