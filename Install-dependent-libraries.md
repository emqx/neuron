# Script to install dependent libraries

[openssl](https://github.com/openssl/openssl)

```shell
# Ubuntu
$ apt-get install libssl-dev

# CentOS
$ yum install openssl-devel

# MacOS
$ brew install openssl
```

[mbedtls](https://github.com/ARMmbed/mbedtls)
```shell
git clone -b v3.1.0 https://github.com/ARMmbed/mbedtls.git
cd mbedtls
mkdir build
cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DUSE_SHARED_MBEDTLS_LIBRARY=OFF \
-DENABLE_TESTING=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON ..
ninja
ninja install
```

[nanomq/nng](https://github.com/nanomq/nng)
```shell
git clone -b nng-mqtt https://github.com/nanomq/nng.git nanomq-nng
cd nanomq-nng
mkdir build
cd build
cmake -G Ninja -DBUILD_SHARED_LIBS=OFF -DNNG_TESTS=OFF \
-DCMAKE_POSITION_INDEPENDENT_CODE=ON -DNNG_ENABLE_TLS=ON ..
ninja
ninja install
```

[nng](https://github.com/nanomsg/nng/tree/v1.5.1)

```shell
git clone -b v1.5.1 git@github.com:nanomsg/nng.git
cd nng
mkdir build
cd build
cmake -G Ninja -DBUILD_SHARED_LIBS=1 ..
ninja
ninja test
ninja install
```

[jansson](https://github.com/akheron/jansson)

```shell
git clone git@github.com:akheron/jansson.git
cd jansson
mkdir build
cd build
cmake -G Ninja -DJANSSON_BUILD_DOCS=OFF ..
ninja
ninja install
```

[jwt](https://github.com/benmcollins/libjwt)

```shell
git clone git@github.com:benmcollins/libjwt.git
cd libjwt
mkdir build
cd build
cmake -G Ninja -DBUILD_EXAMPLES=OFF -DOPENSSL_ROOT_DIR={YOUR_OPENSSL_ROOT_DIR} ..
ninja
ninja install
```

[MQTT-C](https://github.com/LiamBindle/MQTT-C.git)

```shell
git clone -b 1.1.5 https://github.com/LiamBindle/MQTT-C.git 
cd MQTT-C
mkdir build
cd build
cmake -G Ninja -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DMQTT_C_OpenSSL_SUPPORT=ON ..
ninja
ninja install
```

[yaml](https://github.com/yaml/libyaml.git)

```shell
git clone https://github.com/yaml/libyaml.git
cd libyaml
mkdir build && cd build && cmake DBUILD_SHARED_LIBS=1 .. && make
make install
```

[open62541](https://open62541.org/)

```shell
git clone -b v1.2.2 https://github.com/open62541/open62541.git
cd open62541
mkdir build
cd build
cmake -G Ninja -DBUILD_SHARED_LIBS=OFF -DUA_ENABLE_AMALGAMATION=ON -DUA_ENABLE_ENCRYPTION=ON -DUA_ENABLE_ENCRYPTION_OPENSSL=ON ..
ninja && ninja install
```
