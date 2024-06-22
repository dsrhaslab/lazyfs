#!/bin/bash

version=$1

#Dependencies
apt-get -y install libsnappy-dev libtool

if [ "$version" == "1.12" ] || [ "$version" == "1.15" ]; then
    wget https://github.com/google/leveldb/archive/refs/tags/v$version.tar.gz -O leveldb-$version.tar.gz
    tar -xzf leveldb-$version.tar.gz
    cd leveldb-$version
    make
    mv libleveldb.* /usr/local/lib 
    cd include
    cp -R leveldb /usr/local/include
    ldconfig
else 
    git clone --depth 1 --recurse-submodules --branch 1.23 https://github.com/google/leveldb.git
    cd leveldb
    mkdir -p build && cd build
    cmake -DCMAKE_BUILD_TYPE=Release .. && cmake --build .
    mv libleveldb.* /usr/local/lib 
    cd ../include
    cp -R leveldb /usr/local/include    
    ldconfig
fi