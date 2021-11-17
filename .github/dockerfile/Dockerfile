FROM ubuntu:18.04

RUN apt-get update -y \
    && apt-get install -y gcc gcc-arm-linux-gnueabihf gcc-aarch64-linux-gnu \
       g++ git curl make ninja-build libssl-dev

COPY .github/dockerfile/install-libs.sh install-libs.sh

RUN ./install-libs.sh -a -s x86_64
RUN ./install-libs.sh -a -c -p aarch64-linux-gnu -s aarch64
RUN ./install-libs.sh -a -c -p arm-linux-gnueabihf -s armv4

RUN apt-get clean \
    && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
