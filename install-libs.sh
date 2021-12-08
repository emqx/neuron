#!/bin/bash

set -e

system=$(uname)
echo "System: $system"

if [ $system == 'Linux' ]; then
    if [ $(whoami) != "root" ]; then
        echo "Please use root access"
        exit 2
    fi
fi

lib_list=(zlib openssl nng jansson jwt MQTT-C gtest open62541 yaml)

list_str=""
for var in ${lib_list[*]}; do
    list_str+=$var" "
done

arch=x86_64
is_build_all=FALSE
pkgname=
is_cross=FALSE
compiler_opt=
install_opt=
cross_compiler_prefix=
ssl_lib_flag=
git_url_prefix="https://github.com/"

function usage() {
    echo "Usage: $0 "
    echo "  -a  Build all dependencies"
    echo "  -n  Specify dependence name (opts: $list_str)"
    echo "  -c  Cross compiler mode (default: FALSE)"
    echo "  -p  Cross compiler prefix (opts: x86_64-linux-gnu, arm-linux-gnueabihf, aarch64-linux-gnu, ...)"
    echo "  -s  System architecture (only for openssl, opts: x86_64, armv4, aarch64, ...) "
    echo "  -r  Use github repository SSH url (default: HTTPS url)"
}

if [ $# -eq 0 ]; then
    usage
    exit
fi

# cmake
function build_cmake() {
    if [ $is_cross != "TRUE" ]; then
      echo "Building cmake (v3.18.5)"
      curl --silent --show-error -kfL -o cmake.tar.gz "https://github.com/Kitware/CMake/releases/download/v3.18.5/cmake-3.18.5.tar.gz"
      tar -zxf cmake.tar.gz
      rm -f cmake.tar.gz
      cd "cmake-3.18.5/"

      ./bootstrap --parallel=8
      make -j8
      make install
      cd ..
      cmake --version
    fi
}

# nng
function build_nng() {
    echo "Building nng (neuron/v1.5.2)"
    if [ ! -d nng ]; then
        git clone -b neuron/v1.5.2 ${git_url_prefix}neugates/nng.git
    fi
    cd nng
    rm -rf build
    mkdir build
    cd build
    cmake -G Ninja -DBUILD_SHARED_LIBS=OFF -DNNG_TESTS=OFF ${compiler_opt} ${install_opt} ..
    ninja
    ninja install

    echo "Leaving nng "
    cd ../../
}

# jansson
function build_jansson() {
    echo "Building jansson"
    if [ ! -d jansson ]; then
        git clone ${git_url_prefix}akheron/jansson.git
    fi
    cd jansson
    rm -rf build
    mkdir build
    cd build
    cmake -G Ninja -DJANSSON_BUILD_DOCS=OFF \
        -DJANSSON_EXAMPLES=OFF \
        ${compiler_opt} ${install_opt} ..
    ninja
    ninja install

    echo "Leaving jansson "
    cd ../../
}

# jwt
function build_jwt() {
    echo "Building libjwt"
    if [ ! -d libjwt ]; then
        git clone ${git_url_prefix}benmcollins/libjwt.git
    fi
    cd libjwt
    rm -rf build
    mkdir build
    cd build
    cmake -G Ninja -DBUILD_EXAMPLES=OFF \
        -DBUILD_TESTS=OFF \
        -DBUILD_SHARED_LIBS=OFF \
        -DENABLE_PIC=ON \
        -DENABLE_DEBUG_INFO_IN_RELEASE=OFF \
        -Djansson_DIR=${install_dir}/lib/cmake/jansson \
        -DJANSSON_INCLUDE_DIRS=${install_dir}/include \
        ${ssl_lib_flag} ${compiler_opt} ${install_opt} ..
    ninja
    ninja install

    echo "Leaving libjwt "
    cd ../../
}

# MQTT-C
function build_MQTT-C() {
    echo "Building MQTT-C (neugates/main)"
    if [ ! -d MQTT-C ]; then
        git clone ${git_url_prefix}neugates/MQTT-C.git
    fi
    cd MQTT-C
    rm -rf build
    mkdir build
    cd build
    cmake -G Ninja -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
        -DMQTT_C_OpenSSL_SUPPORT=ON \
        -DMQTT_C_EXAMPLES=OFF \
        ${ssl_lib_flag} ${compiler_opt} ${install_opt} ..
    ninja
    ninja install
    echo "Leaving MQTT-C "
    cd ../../
}

function build_yaml() {
    echo "Building yaml"
    if [ ! -d libyaml ]; then
        git clone ${git_url_prefix}yaml/libyaml.git
    fi
    cd libyaml
    rm -rf build
    mkdir build
    cd build
    cmake -G Ninja -DBUILD_SHARED_LIBS=ON ${compiler_opt} ${install_opt} ..
    ninja
    ninja install

    echo "Leaving yaml"
    cd ../../
}

function install_tool() {
    ubuntu=$1
    centos=$2
    darwin=$3

    case "$system" in
    "Linux")
        release=$(awk -F= '/^NAME/{print $2}' /etc/os-release)
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

function build_zlib() {
    echo "Installing zlib"
    if [ ! -d zlib ]; then
        git clone ${git_url_prefix}madler/zlib.git
    fi

    cd zlib
    rm -rf build
    mkdir build
    cd build
    cmake -G Ninja ${compiler_opt} ${install_opt} ..
    ninja
    ninja install
    echo "Leaving zlib "
    cd ../../
}

function build_openssl() {
    echo "Installing openssl (1.1.1)"
    if [ $is_cross == "TRUE" ]; then
        if [ ! -n "$cross_compiler_prefix" ]; then
            echo "Error: cross_compiler_prefix must be set"
            usage
            return
        fi

        if [ ! -d openssl ]; then
            git clone -b OpenSSL_1_1_1-stable ${git_url_prefix}openssl/openssl.git
        fi

        cd openssl
        mkdir -p ${install_dir}/openssl/ssl
        ./Configure ${system,}-${arch} no-asm shared \
            --prefix=${install_dir}/openssl \
            --openssldir=${install_dir}/openssl/ssl \
            --cross-compile-prefix=${cross_compiler_prefix}- \
            --with-zlib-include=${install_dir}/zlib/include \
            --with-zlib-lib=${install_dir}/zlib/lib
        make clean
        make
        make install
        make clean
        cd ../

    else
        install_tool libssl-dev openssl-devel openssl
    fi
}

function build_gtest() {
    echo "Install gtest"
    if [ $is_cross != "TRUE" ]; then
        install_tool libgtest-dev gtest-devel googletest
    fi
}

function build_open62541() {
    echo "Install open62541 (v1.2.2)"

    if [ ! -d open62541 ]; then
        git clone -b v1.2.2 ${git_url_prefix}open62541/open62541.git
    fi
    cd open62541
    rm -rf build
    mkdir build
    cd build
    cmake -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=OFF \
        -DUA_ENABLE_AMALGAMATION=ON \
        -DUA_ENABLE_ENCRYPTION=ON \
        -DUA_ENABLE_ENCRYPTION_OPENSSL=ON \
        ${ssl_lib_flag} ${compiler_opt} ${install_opt} ..
    ninja
    ninja install

    echo "Leaving open62541"
    cd ../../
}

current=$(pwd)
externs="/opt/externs"

if [ ! -d $externs ]; then
    mkdir $externs
fi
cd $externs

while getopts "h:c-:r-:a-:n:p:s:" OPT; do
    case ${OPT} in
    c)
        is_cross=TRUE
        ;;
    s)
        arch=$OPTARG
        ;;
    p)
        cross_compiler_prefix=$OPTARG
        ;;
    r)
        git_url_prefix="git@github.com:"
        ;;
    a)
        is_build_all=TRUE
        ;;
    n)
        pkgname=$OPTARG
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

if [ $is_cross == "TRUE" ]; then
    install_dir=${externs}/libs/${cross_compiler_prefix}
    compiler_opt="-DCMAKE_C_COMPILER=${cross_compiler_prefix}-gcc \
        -DCMAKE_STAGING_PREFIX=${install_dir} -DCMAKE_PREFIX_PATH=${install_dir}"
    install_opt="-DCMAKE_INSTALL_PREFIX=${install_dir}"
    ssl_lib_flag="-DOPENSSL_ROOT_DIR=${install_dir}/openssl \
        -DOPENSSL_INCLUDE_DIR=${install_dir}/openssl/include \
        -DOPENSSL_LIBRARIES='${install_dir}/openssl/lib/libssl.so ${install_dir}/openssl/lib/libcrypto.so'"

    echo "Entering $externs"
else
    case "$system" in
    "Linux")
        ssl_lib_flag=""
        ;;
    "Darwin")
        ssl_lib_flag="-DOPENSSL_ROOT_DIR=/usr/local/opt/openssl"
        ;;
    *) ;;

    esac
fi

if [ x$install_dir != "x" ]; then
    echo "mkdir install dir"
    mkdir -p ${install_dir}
fi

if [ $is_build_all == "TRUE" ]; then
    echo "Build ${list_str}"
    for var in ${lib_list[*]}; do
        install_opt=${install_opt}/$var
        build_$var
    done
else
    install_opt=${install_opt}/$pkgname
    build_$pkgname
fi

echo "Leaving $externs"
cd $current
