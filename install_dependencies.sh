#!/bin/bash

# Install openssl
echo "Installing openssl..."
sudo apt-get install libssl-dev openssl -y

# Install zlog
echo "Installing zlog..."
git clone -b 1.2.15 https://github.com/HardySimpson/zlog.git
cd zlog
make && sudo make install
cd ..

# Install jansson
echo "Installing jansson..."
git clone https://github.com/neugates/jansson.git
cd jansson && mkdir build && cd build
cmake -DJANSSON_BUILD_DOCS=OFF -DJANSSON_EXAMPLES=OFF .. && make && sudo make install
cd ../..

# Install mbedtls
echo "Installing mbedtls..."
git clone -b v2.16.12 https://github.com/Mbed-TLS/mbedtls.git
cd mbedtls && mkdir build && cd build
cmake -DUSE_SHARED_MBEDTLS_LIBRARY=OFF -DENABLE_TESTING=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON .. && make && sudo make install
cd ../..

# Install NanoSDK
echo "Installing NanoSDK..."
git clone https://github.com/neugates/NanoSDK.git
cd NanoSDK && mkdir build && cd build
cmake -DBUILD_SHARED_LIBS=OFF -DNNG_TESTS=OFF -DNNG_ENABLE_SQLITE=ON -DNNG_ENABLE_TLS=ON .. && make && sudo make install
cd ../..

# Install jwt
echo "Installing jwt..."
git clone -b v1.13.1 https://github.com/benmcollins/libjwt.git
cd libjwt && mkdir build && cd build
cmake -DENABLE_PIC=ON -DBUILD_SHARED_LIBS=OFF .. && make && sudo make install
cd ../..

# Install googletest
echo "Installing googletest..."
git clone -b release-1.11.0 https://github.com/google/googletest.git
cd googletest && mkdir build && cd build
cmake .. && make && sudo make install
cd ../..

# Install sqlite
echo "Installing sqlite..."
curl -o sqlite3.tar.gz https://www.sqlite.org/2022/sqlite-autoconf-3390000.tar.gz
mkdir sqlite3
tar xzf sqlite3.tar.gz --strip-components=1 -C sqlite3
cd sqlite3
./configure CFLAGS=-fPIC && make && sudo make install
cd ..

# Install protobuf
echo "Installing protobuf..."
wget --no-check-certificate --content-disposition https://github.com/protocolbuffers/protobuf/releases/download/v3.20.1/protobuf-cpp-3.20.1.tar.gz
tar -xzvf protobuf-cpp-3.20.1.tar.gz
cd protobuf-3.20.1
./configure --enable-shared=no CFLAGS=-fPIC CXXFLAGS=-fPIC
make && sudo make install
cd ..

# Install protobuf-c
echo "Installing protobuf-c..."
git clone -b v1.4.0 https://github.com/protobuf-c/protobuf-c.git
cd protobuf-c
./autogen.sh
./configure  --disable-protoc --enable-shared=no CFLAGS=-fPIC CXXFLAGS=-fPIC 
make && sudo make install
cd ..

# Install cyrus-sasl
echo "Installing cyrus-sasl..."
git clone -b cyrus-sasl-2.1.28 https://github.com/cyrusimap/cyrus-sasl.git
cd cyrus-sasl
autoreconf -fi
./configure --enable-shared --disable-static --disable-sample \
    --disable-checkapop --disable-cram --disable-digest --disable-otp \
    --disable-srp --disable-gssapi --disable-sql \
    --enable-plain --enable-scram --enable-login \
    --with-dblib=none --without-saslauthd --without-authdaemond
make && sudo make install
cd ..

# Install zstd
echo "Installing zstd..."
git clone -b v1.5.6 https://github.com/facebook/zstd.git
cd zstd/build/cmake && mkdir builddir && cd builddir
cmake -DZSTD_BUILD_PROGRAMS=OFF -DZSTD_BUILD_TESTS=OFF -DZSTD_BUILD_SHARED=ON -DZSTD_BUILD_STATIC=OFF -DCMAKE_BUILD_TYPE=Release ..
make && sudo make install
cd ../../../..

# Install librdkafka
echo "Installing librdkafka..."
git clone -b v2.6.1 https://github.com/confluentinc/librdkafka.git
cd librdkafka && mkdir build && cd build
cmake -DRDKAFKA_BUILD_STATIC=OFF -DRDKAFKA_BUILD_EXAMPLES=OFF -DRDKAFKA_BUILD_TESTS=OFF -DWITH_SSL=ON -DWITH_ZLIB=ON -DWITH_SASL=ON -DWITH_ZSTD=ON -DWITH_CURL=OFF -DCMAKE_BUILD_TYPE=Release ..
make && sudo make install
cd ../..

# Install libxml2
echo "Installing libxml2..."
git clone -b v2.9.14 https://github.com/GNOME/libxml2.git
cd libxml2
./autogen.sh
./configure --enable-shared=no CFLAGS=-fPIC CXXFLAGS=-fPIC
make && sudo make install
cd ..

echo "All dependencies installed successfully."
