# neuron

NEURON IIoT System for Industry 4.0



## Build Requirements

- [openssl](https://github.com/openssl/openssl)

  ```shell
  # Ubuntu
  $ apt-get install libssl-dev
  
  # CentOS
  $ yum install openssl-devel
  
  # MacOS
  $ brew install openssl
  ```

  

- [nng](https://github.com/nanomsg/nng/tree/v1.5.1)

  ```shell
  $ git clone -b v1.5.1 git@github.com:nanomsg/nng.git
  $ cd nng
  $ mkdir build
  $ cd build
  $ cmake -G Ninja ..
  $ ninja
  $ ninja test
  $ ninja install
  ```

- [jansson](https://github.com/akheron/jansson)

  ```shell
  $ git clone git@github.com:akheron/jansson.git
  $ cd jansson
  $ mkdir build
  $ cd build
  $ cmake -G Ninja -DJANSSON_BUILD_DOCS=OFF ..
  $ ninja
  $ ninja install
  ```

- [jwt](https://github.com/benmcollins/libjwt)

  ```shell
  $ git clone git@github.com:benmcollins/libjwt.git
  $ cd libjwt
  $ mkdir build
  $ cd build
  $ cmake -G Ninja -DBUILD_EXAMPLES=OFF -DOPENSSL_ROOT_DIR={YOUR_OPENSSL_ROOT_DIR} ..
  $ ninja
  $ ninja install
  ```

- [paho-mqtt.c](https://github.com/eclipse/paho.mqtt.c/tree/v1.3.9)

  ```shell
  $ git clone -b v1.3.9 git@github.com:eclipse/paho.mqtt.c.git
  $ cd paho.mqtt.c
  $ mkdir build
  $ cd build
  $ cmake -G Ninja -DPAHO_BUILD_SAMPLES=FALSE  -DPAHO_WITH_SSL=TRUE -DPAHO_BUILD_SHARED=FALSE  -DPAHO_BUILD_STATIC=TRUE -DOPENSSL_ROOT_DIR={YOUR_OPENSSL_ROOT_DIR} -DPAHO_HIGH_PERFORMANCE=TRUE -DCMAKE_BUILD_TYPE=Release  ..
  $ ninja
  $ ninja install
  ```
  
- uuid
  ```shell
  # Ubuntu
  $ apt-get install uuid-dev
  
  # CentOS
  $ yum install libuuid-devel
   
  # MacOS
  $ brew install ossp-uuid
  ```

- [yaml](https://github.com/yaml/libyaml.git)
  ```shell
  $ git clone https://github.com/yaml/libyaml.git
  $ cd libyaml
  $ mkdir build && cd build && cmake .. && make
  $ make install
  ```

- [neuron-dashboard](https://github.com/neugates/neuron-dashboard-src/releases)
  ```shell
  # Download lastest neuron-dashboard release from https://github.com/neugates/neuron-dashboard-src/releases
  # Unzip and move to "build"

  $ unzip neuron-dashboard.zip -d build
  ```
