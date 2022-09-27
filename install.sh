#!/bin/bash

set -e

install_dir=
arch=x86_64
compile=

while getopts ":d:a:c:x:" OPT; do
    case ${OPT} in
        a)
            arch=$OPTARG
            ;;
        d)
            install_dir=$OPTARG
            ;;
        c)
            compile=$OPTARG
            ;;
    esac
done

gcc_compile=${compile}-gcc
gxx_compile=${compile}-g++

#$1 rep
#$2 tag
#$3 compile option
function compile_source() {
    git clone https://github.com/$1 tmp
    cd tmp
    git checkout -b br $2
    mkdir build && cd build
    cmake .. -DCMAKE_C_COMPILER=${gcc_compile} \
        -DCMAKE_CXX_COMPILER=${gxx_compile} \
        -DCMAKE_STAGING_PREFIX=${install_dir} \
        -DCMAKE_PREFIX_PATH=${install_dir} \
        $3
    # github-hosted runners has 2 core
    make -j4 && sudo make install
    cd ../../
    rm -rf tmp
}

function build_openssl() {
    echo "Installing openssl (1.1.1)"
    if [ $arch == "x86_64" ]; then
        sudo apt-get install -y openssl libssl-dev
    else

        git clone -b OpenSSL_1_1_1 https://github.com/openssl/openssl.git
        cd openssl
        mkdir -p ${install_dir}/openssl/ssl
        ./Configure linux-${arch} no-asm shared \
            --prefix=${install_dir} \
            --openssldir=${install_dir}/openssl/ssl \
            --cross-compile-prefix=${compile}- 
        make clean
        make -j4
        make install_sw
        make clean
        cd ../
    fi
}

function build_zlog() {
    git clone -b 1.2.15 https://github.com/HardySimpson/zlog.git
    cd zlog
    echo ${gcc_compile}
    make CC=${gcc_compile}
    if [ $arch == "x86_64" ]; then
        sudo make install
        sudo make PREFIX=${install_dir} install
    else
        sudo make PREFIX=${install_dir} install
    fi
}

function build_sqlite3() {
    curl https://www.sqlite.org/2022/sqlite-autoconf-3390000.tar.gz \
      --output sqlite3.tar.gz
    mkdir -p sqlite3
    tar xzf sqlite3.tar.gz --strip-components=1 -C sqlite3
    cd sqlite3
    ./configure --prefix=${install_dir} \
                --disable-shared --disable-readline \
                --host ${arch} CC=${gcc_compile} \
      && make -j4 \
      && sudo make install
    cd ../
}

build_zlog
compile_source neugates/jansson.git HEAD "-DJANSSON_BUILD_DOCS=OFF -DJANSSON_EXAMPLES=OFF"
build_openssl 
compile_source neugates/nng.git HEAD "-DBUILD_SHARED_LIBS=OFF -DNNG_TESTS=OFF"
compile_source benmcollins/libjwt.git v1.13.1 "-DENABLE_PIC=ON -DBUILD_SHARED_LIBS=OFF"
compile_source neugates/MQTT-C.git HEAD "-DCMAKE_POSITION_INDEPENDENT_CODE=ON -DMQTT_C_OpenSSL_SUPPORT=ON -DMQTT_C_EXAMPLES=OFF"
build_sqlite3

if [ $arch == "x86_64" ]; then
    compile_source google/googletest.git release-1.11.0
fi
