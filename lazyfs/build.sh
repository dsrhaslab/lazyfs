#!/bin/sh
mkdir -p build
cd build
cmake -DLAZYFS_BUILD_TESTS=OFF ..
cmake --build .
