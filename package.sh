#!/bin/bash

package_name=

if [ $# -ge 1 ];then
    package_name=$1
else 
    package_name=neuron
fi

if [ ! -d $package_name ];then 
    mkdir -p $package_name
fi

cp build/lib*.so \
    build/neuron \
    build/neuron.yaml \
    build/plugin_param_schema.json \
    $package_name/

tar czf ${package_name}.tar.gz ${package_name}/
rm -rf ${package_name}

echo "${package_name}.tar.gz"
