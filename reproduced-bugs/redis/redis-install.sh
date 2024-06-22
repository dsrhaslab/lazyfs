#!/bin/bash

version=$1

wget https://github.com/redis/redis/archive/refs/tags/$version.tar.gz -O "redis.tar.gz"
tar -xzf "redis.tar.gz"
mv "redis-$version" "redis"
cd redis
make