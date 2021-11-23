FROM ubuntu:18.04

RUN apt-get update -y \
    && apt-get install -y curl wget vim unzip make ninja-build \
       g++ gcc gcc-arm-linux-gnueabihf gcc-aarch64-linux-gnu \
       libssl-dev libghc-zlib-dev libcurl4-gnutls-dev libexpat1-dev gettext \
       pkg-config \
       python3 python3-pip

COPY install-libs.sh install-libs.sh

RUN echo "Building git (v2.18.0)" \
    && curl --silent --show-error -kfL -o git.zip https://github.com/git/git/archive/v2.18.0.zip \
    && unzip git.zip \
    && cd git-* \
    && make prefix=/usr/local all \
    && make prefix=/usr/local install

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

RUN echo "Building googletest (v1.11.0)" \
    && apt-get -y install libgtest-dev \
    && cd /usr/src/gtest \
    && rm -rf build \
    && mkdir build \
    && cd build \
    && make  \
    && cp *.a /usr/local/lib \
    && cd .. 

RUN ./install-libs.sh -a
RUN ./install-libs.sh -a -c -p aarch64-linux-gnu -s aarch64
RUN ./install-libs.sh -a -c -p arm-linux-gnueabihf -s armv4

RUN apt-get clean \
    && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

RUN ldconfig
