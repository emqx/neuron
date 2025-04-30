!/bin/bash

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
git clone -b neuron https://github.com/neugates/NanoSDK.git
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

# Install libxml2
echo "Installing libxml2..."
git clone -b v2.9.14 https://github.com/GNOME/libxml2.git
cd libxml2
./autogen.sh
./configure --enable-shared=no CFLAGS=-fPIC CXXFLAGS=-fPIC
make && sudo make install
cd ..

# Install ninja
echo "Installing ninja..."
sudo apt-get install ninja-build -y

# Install gRPC
echo "Installing gRPC..."
sudo apt-get install -y libgrpc++-dev protobuf-compiler-grpc

# Install Apache Arrow
echo "Installing Apache Arrow..."
wget https://github.com/apache/arrow/releases/download/apache-arrow-19.0.1/apache-arrow-19.0.1.tar.gz
tar -xzvf apache-arrow-19.0.1.tar.gz --overwrite
cd apache-arrow-19.0.1/cpp
rm -rf build 2>/dev/null || true
mkdir build && cd build
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DARROW_BUILD_SHARED=ON \
  -DARROW_COMPUTE=ON \
  -DARROW_CSV=ON \
  -DARROW_JSON=ON \
  -DARROW_PARQUET=ON \
  -DARROW_DATASET=ON \
  -DARROW_FLIGHT=ON \
  -DARROW_FLIGHT_SQL=ON \
  -DARROW_WITH_GRPC=ON \
  -DARROW_PROTOBUF_USE_SHARED=OFF \
  -DProtobuf_ROOT=/usr/local \
  -DgRPC_ROOT=/usr/local \
  -GNinja
ninja
sudo ninja install
cd ../../..

echo "All dependencies installed successfully."
