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

[yaml](https://github.com/yaml/libyaml.git)
```shell
$ git clone -b 0.2.5 https://github.com/yaml/libyaml.git
$ cd libyaml && mkdir build && cd build 
$ cmake DBUILD_SHARED_LIBS=OFF .. && make
$ sudo make install
```

[jansson](https://github.com/akheron/jansson)
```shell
$ git clone -b v2.14 https://github.com/akheron/jansson.git
$ cd jansson && mkdir build && cd build
$ cmake -DJANSSON_BUILD_DOCS=OFF -DJANSSON_EXAMPLES=OFF ..&& make && sudo make install
```

[nng](https://github.com/neugates/nng.git)
```shell
$ git clone https://github.com/neugates/nng.git
$ cd nng && mkdir build && cd build
$ cmake -DBUILD_SHARED_LIBS=OFF -DNNG_TESTS=OFF .. && make && sudo make install
```

[jwt](https://github.com/benmcollins/libjwt.git)
```shell
$ git clone -b v1.13.1 https://github.com/benmcollins/libjwt.git
$ cd libjwt && mkdir build && cd build
$ cmake -DENABLE_PIC=ON -DBUILD_SHARED_LIBS=OFF .. && make && sudo make install
```

[MQTT-C](https://github.com/neugates/MQTT-C.git)
```shell
$ git clone https://github.com/neugates/MQTT-C.git 
$ cd MQTT-C && mkdir build && cd build
$ cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DMQTT_C_OpenSSL_SUPPORT=ON -DMQTT_C_EXAMPLES=OFF .. && make && sudo make install
```

[googletest](https://github.com/google/googletest.git)
```shell
$ git clone -b release-1.11.0 https://github.com/google/googletest.git 
$ cd googletest && mkdir build && cd build
$ cmake .. && make && sudo make install
```