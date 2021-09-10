#!/bin/bash

system=`uname`
echo "System: $system"

if [ $system == 'Linux' ];then
    if [ `whoami` != "root" ];then
        echo "Please use root access"
    exit 2
    fi
fi

lib_list=(nng jansson jwt paho uuid openssl gtest open62541 yaml)

list_str=""
for var in ${lib_list[*]};do
    list_str+=$var" "
done

function usage() {
    echo "Usage: $0 "
    echo "  -a  Build all dependencies"
    echo "  -i  Specify installation options (opts: $list_str)"
}

if [ $# -eq 0 ];then
usage
exit
fi

# nng
function build_nng()
{
    echo "Building nng (v1.5.2)"
    if [ ! -d nng ];then
        git clone -b v1.5.2 https://github.com/nanomsg/nng.git
    fi
    cd nng
    mkdir build
    cd build
    cmake -G Ninja ..
    ninja
    ninja install

    echo "Leaving nng "
    cd ../../
}

# jansson
function build_jansson()
{
    echo "Building jansson"
    if [ ! -d jansson ];then
        git clone https://github.com/akheron/jansson.git
    fi
    cd jansson
    rm -rf build
    mkdir build
    cd build
    cmake -G Ninja -DJANSSON_BUILD_DOCS=OFF ..
    ninja
    ninja install

    echo "Leaving jansson "
    cd ../../
}

# jwt
function build_jwt()
{
    echo "Building libjwt"
    if [ ! -d libjwt ];then
        git clone https://github.com/benmcollins/libjwt.git
    fi
    cd libjwt
    rm -rf build
    mkdir build
    cd build
    cmake -G Ninja -DBUILD_EXAMPLES=OFF $ssl_lib_flag  ..
    ninja
    ninja install

    echo "Leaving libjwt "
    cd ../../
}

# paho.mqtt.c
function build_paho()
{
    echo "Building paho.mqtt.c (v1.3.9)"
    if [ ! -d paho.mqtt.c ];then
        git clone -b v1.3.9 https://github.com/eclipse/paho.mqtt.c.git
    fi
    cd paho.mqtt.c
    rm -rf build
    mkdir build
    cd build
    cmake -G Ninja -DPAHO_BUILD_SAMPLES=FALSE  -DPAHO_WITH_SSL=TRUE -DPAHO_BUILD_SHARED=FALSE  -DPAHO_BUILD_STATIC=TRUE $ssl_lib_flag -DPAHO_HIGH_PERFORMANCE=TRUE -DCMAKE_BUILD_TYPE=Release  ..
    ninja
    ninja install
    echo "Leaving paho.mqtt.c "
    cd ../../
}

function build_yaml()
{   
    git clone https://github.com/yaml/libyaml.git
    cd libyaml
    rm -rf build
    mkdir build && cd build && cmake .. && make
    make install

    cd ../../
}

function install_tool() 
{
    ubuntu=$1
    centos=$2
    darwin=$3
    
    case "$system" in
    "Linux") release=`awk -F= '/^NAME/{print $2}' /etc/os-release`
        case "$release" in
        "\"Ubuntu\"") 
        apt-get -y install $ubuntu
        ;;
        "\"CentOS Linux\"")
        yum install -y $centos
        ;;
        esac
    ;;

    "Darwin") 
    brew install $darwin
    ;;

    *)
    echo "Unsupported System!"
    echo "Please install library manually!"
    ;;
    esac
}

function build_uuid()
{
    echo "Installing uuid"
    install_tool uuid-dev libuuid-devel ossp-uuid
}

function build_openssl()
{  
    echo "Installing openssl"
    install_tool libssl-dev openssl-devel openssl
}

function build_gtest()
{
    echo "Install gtest"
    install_tool libgtest-dev gtest-devel googletest
}

function build_open62541() {
    echo "Install open62541 (v1.2.2)"

    if [ ! -d open62541 ];then
        git clone -b v1.2.2 https://github.com/open62541/open62541.git
    fi
    cd open62541 
    rm -rf build
    mkdir build
    cd build
    cmake -G Ninja -DBUILD_SHARED_LIBS=OFF -DUA_ENABLE_AMALGAMATION=ON -DUA_ENABLE_ENCRYPTION=ON -DUA_ENABLE_ENCRYPTION_OPENSSL=ON ..
    ninja
    ninja install

    echo "Leaving open62541"
    cd ../../
}

current=`pwd`
externs="/opt/externs"

if [ ! -d $externs ];then
    mkdir $externs
fi

echo "Entering $externs"
cd $externs

ssl_lib_flag=""
case "$system" in
    "Linux") ssl_lib_flag=""
    ;;
    "Darwin") ssl_lib_flag="-DOPENSSL_ROOT_DIR=/usr/local/opt/openssl"
    ;;
    *)
    ;;
esac

while getopts "a-:i:h" OPT; do
    case ${OPT} in
    a) 
        echo "Build ${list_str}"
        for var in ${lib_list[*]};do
            build_$var
        done
        ;;
    i) 
        pkgname=$OPTARG
        build_$pkgname
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

echo "Leaving $externs"
cd $current

