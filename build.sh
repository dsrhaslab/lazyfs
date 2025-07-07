#!/bin/sh
mkdir -p build
cd build
cmake -DLAZYFS_BUILD_TESTS=OFF -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Debug ..
cmake --build .