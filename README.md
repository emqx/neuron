# neuron-lite

NEURON IIoT System for Industry 4.0



## Build Requirements

- nng: https://github.com/nanomsg/nng

  ```shell
  $ git clone git@github.com:nanomsg/nng.git
  $ cd nng
  $ mkdir build
  $ cd build
  $ cmake -G Ninja ..
  $ ninja
  $ ninja test
  $ ninja install
  ```

- jansson: https://github.com/akheron/jansson

  ```
  $ git clone git@github.com:akheron/jansson.git
  $ cd jansson
  $ mkdir build
  $ cd build
  $ cmake -G Ninja -DJANSSON_BUILD_DOCS=OFF ..
  $ ninja
  $ ninja install
  ```

- jwt: https://github.com/benmcollins/libjwt

  ```
  $ git clone git@github.com:benmcollins/libjwt.git
  $ cd libjwt
  $ mkdir build
  $ cd build
  $ cmake -G Ninja  -DOPENSSL_ROOT_DIR=/usr/local/opt/openssl@1.1/lib -DOPENSSL_INCLUDE_DIR=/usr/local/opt/openssl@1.1/include     -DBUILD_EXAMPLES=OFF ..
  $ ninja
  $ ninja install
  ```



