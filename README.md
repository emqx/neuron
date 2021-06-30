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

  ```shell
  $ git clone git@github.com:akheron/jansson.git
  $ cd jansson
  $ mkdir build
  $ cd build
  $ cmake -G Ninja -DJANSSON_BUILD_DOCS=OFF ..
  $ ninja
  $ ninja install
  ```

- jwt: https://github.com/benmcollins/libjwt

  ```shell
  $ git clone git@github.com:benmcollins/libjwt.git
  $ cd libjwt
  $ mkdir build
  $ cd build
  $ cmake -G Ninja -DBUILD_EXAMPLES=OFF -DOPENSSL_ROOT_DIR={YOUR_OPENSSL_ROOT_DIR} -DOPENSSL_INCLUDE_DIR={YOUR_OPENSSL_INCLUDE_DIR} ..
  $ ninja
  $ ninja install
  ```



