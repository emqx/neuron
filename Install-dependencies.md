[openssl](https://github.com/openssl/openssl)

```shell
# Ubuntu
$ apt-get install libssl-dev openssl
```

[zlog](https://github.com/HardySimpson/zlog.git)
```shell
$ git clone -b 1.2.15 https://github.com/HardySimpson/zlog.git
$ cd zlog
$ make && sudo make install
```

[jansson](https://github.com/neugates/jansson.git)
```shell
$ git clone https://github.com/neugates/jansson.git
$ cd jansson && mkdir build && cd build
$ cmake -DJANSSON_BUILD_DOCS=OFF -DJANSSON_EXAMPLES=OFF ..&& make && sudo make install
```

[mbedtls](https://github.com/ARMmbed/mbedtls.git)
```shell
$ git clone -b v2.16.12 https://github.com/Mbed-TLS/mbedtls.git
$ cd mbedtls && mkdir build && cd build
$ cmake -DUSE_SHARED_MBEDTLS_LIBRARY=OFF -DENABLE_TESTING=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON .. && make && sudo make install
```

[NanoSDK](https://github.com/neugates/NanoSDK.git)
```shell
$ git clone https://github.com/neugates/NanoSDK.git
$ cd NanoSDK && mkdir build && cd build
$ cmake -DBUILD_SHARED_LIBS=OFF -DNNG_TESTS=OFF -DNNG_ENABLE_SQLITE=ON -DNNG_ENABLE_TLS=ON .. && make && sudo make install
```

[jwt](https://github.com/benmcollins/libjwt.git)
```shell
$ git clone -b v1.13.1 https://github.com/benmcollins/libjwt.git
$ cd libjwt && mkdir build && cd build
$ cmake -DENABLE_PIC=ON -DBUILD_SHARED_LIBS=OFF .. && make && sudo make install
```

[googletest](https://github.com/google/googletest.git)
```shell
$ git clone -b release-1.11.0 https://github.com/google/googletest.git 
$ cd googletest && mkdir build && cd build
$ cmake .. && make && sudo make install
```

[sqlite](https://github.com/sqlite/sqlite)
```shell
$ curl -o sqlite3.tar.gz https://www.sqlite.org/2022/sqlite-autoconf-3390000.tar.gz
$ mkdir sqlite3
$ tar xzf sqlite3.tar.gz --strip-components=1 -C sqlite3
$ cd sqlite3
$ ./configure CFLAGS=-fPIC && make && sudo make install
```

[protobuf](https://github.com/protocolbuffers/protobuf)
```shell
$ wget --no-check-certificate --content-disposition https://github.com/protocolbuffers/protobuf/releases/download/v3.20.1/protobuf-cpp-3.20.1.tar.gz
$ tar -xzvf protobuf-cpp-3.20.1.tar.gz
$ cd protobuf-3.20.1
$ ./configure --enable-shared=no CFLAGS=-fPIC CXXFLAGS=-fPIC
$ make && sudo make install
```

[protobuf-c](https://github.com/protobuf-c/protobuf-c.git)
```shell
$ git clone -b v1.4.0 https://github.com/protobuf-c/protobuf-c.git
$ cd protobuf-c
$ ./autogen.sh
$ ./configure  --disable-protoc --enable-shared=no CFLAGS=-fPIC CXXFLAGS=-fPIC 
$ make && sudo make install
```

[cyrus-sasl](https://github.com/cyrusimap/cyrus-sasl.git)
```shell
$ git clone -b cyrus-sasl-2.1.28 https://github.com/cyrusimap/cyrus-sasl.git
$ cd cyrus-sasl
$ autoreconf -fi
$ ./configure --enable-shared --disable-static --disable-sample \
    --disable-checkapop --disable-cram --disable-digest --disable-otp \
    --disable-srp --disable-gssapi --disable-sql \
    --enable-plain --enable-scram --enable-login \
    --with-dblib=none --without-saslauthd --without-authdaemond
$ make && sudo make install
```

[zstd](https://github.com/facebook/zstd.git)
```shell
$ git clone -b v1.5.6 https://github.com/facebook/zstd.git
$ cd zstd/build/cmake && mkdir builddir && cd builddir
$ cmake -DZSTD_BUILD_PROGRAMS=OFF -DZSTD_BUILD_TESTS=OFF -DZSTD_BUILD_SHARED=ON -DZSTD_BUILD_STATIC=OFF -DCMAKE_BUILD_TYPE=Release .. && make && sudo make install
```

[librdkafka](https://github.com/confluentinc/librdkafka.git)
```shell
$ git clone -b v2.6.1 https://github.com/confluentinc/librdkafka.git
$ cd librdkafka && mkdir build && cd build
$ cmake -DRDKAFKA_BUILD_STATIC=OFF -DRDKAFKA_BUILD_EXAMPLES=OFF -DRDKAFKA_BUILD_TESTS=OFF -DWITH_SSL=ON -DWITH_ZLIB=ON -DWITH_SASL=ON -DWITH_ZSTD=ON -DWITH_CURL=OFF -DCMAKE_BUILD_TYPE=Release .. && make && sudo make install
```

[libxml2](https://github.com/GNOME/libxml2.git)
```shell
$ git clone -b v2.9.14 https://github.com/GNOME/libxml2.git
$ cd libxml2
$ ./autogen.sh
$ ./configure --enable-shared=no CFLAGS=-fPIC CXXFLAGS=-fPIC
$ make && sudo make install
```

[arrow](https://github.com/apache/arrow)

To build Apache Arrow C++ for the datalayers plugin, please refer to the [official Arrow](https://github.com/apache/arrow) for the most up-to-date information on dependencies, configuration, and build steps.

You must enable the following features at a minimum for the datalayers plugin:

```cmake
-DARROW_FLIGHT=ON \
-DARROW_FLIGHT_SQL=ON
```

Required Dependencies:
When enabling DARROW_FLIGHT and DARROW_FLIGHT_SQL, you must ensure the following dependencies are installed on your system at least: \
gRPC \
Boost \
Thrift