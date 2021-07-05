#!/bin/sh

lib_list=(nng jansson jwt paho)
list_str=""
for var in ${lib_list[*]};do
    list_str+=$var" "
done

function usage() {
    echo "Usage: $0 "
    echo "  -a \tBuild all dependencies"
    echo "  -i \tSpecify the  dependence (opts: $list_str)"
}

if [ $# -eq 0 ];then
usage
exit
fi

if [ `whoami` != "root" ];then
	echo "please use root access"
  exit 2
fi

# nng
function build_nng()
{
    echo "building nng (v1.4.0)"
    if [ ! -d nng ];then
        git clone -b v1.4.0 https://github.com/nanomsg/nng.git
    fi
    cd nng
    mkdir build
    cd build
    cmake -G Ninja ..
    ninja
    ninja install

    echo "leaving nng "
    cd ../../
}

# jansson
function build_jansson()
{
    echo "building jansson"
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

    echo "leaving jansson "
    cd ../../
}

# jwt
function build_jwt()
{
    echo "building libjwt"
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

    echo "leaving libjwt "
    cd ../../
}

# paho.mqtt.c
function build_paho()
{
    echo "building paho.mqtt.c (v1.3.9)"
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
    echo "leaving paho.mqtt.c "
    cd ../../
}

current=`pwd`
externs="/opt/externs"

if [ ! -d $externs ];then
    mkdir $externs
fi

echo "entering $externs"
cd $externs

system=`uname`
echo "System: $system"

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
        echo "build ${list_str}"
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


echo "leaving $extends"
cd $current

