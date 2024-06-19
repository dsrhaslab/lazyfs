#!/bin/bash

#Dependencies
apt-get install -y libsnappy-dev libtool 

#Installation
git clone https://github.com/utsaslab/pebblesdb.git
cd pebblesdb/src
autoreconf -i
./configure
make
make install
ldconfig