FROM ubuntu:18.04

RUN apt-get update -y \
    && apt-get install -y gcc gcc-arm-linux-gnueabihf gcc-aarch64-linux-gnu \
       g++ git curl make ninja-build libssl-dev

COPY install-libs.sh install-libs.sh

RUN echo "Building cmake (v3.18.5)" \
    && curl --silent --show-error -kfL -o cmake.tar.gz "https://github.com/Kitware/CMake/releases/download/v3.18.5/cmake-3.18.5.tar.gz" \
    && tar -zxf cmake.tar.gz \
    && rm -f cmake.tar.gz \
    && cd "cmake-3.18.5/" \
    && ./bootstrap --parallel=8 \
    && make -j8 \
    && make install \
    && cd .. \
    && cmake --version

RUN ./install-libs.sh -a 
RUN ./install-libs.sh -a -c -p aarch64-linux-gnu -s aarch64
RUN ./install-libs.sh -a -c -p arm-linux-gnueabihf -s armv4

RUN apt-get clean \
    && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
