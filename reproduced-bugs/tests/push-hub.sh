#!/bin/bash

#--------------------------------------------------------------------------
#
# DESCRIPTION: This script pushes images built with the script build.sh to Docker Hub.
#
#--------------------------------------------------------------------------


images=(
    "postgres-1"
    "postgres-vcc"
    "lnd-12"
    "redis-11"
    "redis-vcc"
    "zookeeper-2-v3.4.8"
    "zookeeper-2-v3.7.1"
    "zookeeper-7-v3.7.1"
    "zookeeper-7-v3.4.8"
    "zookeeper-8-v3.7.1"
    "zookeeper-8-v3.4.8"
    "pebblesdb-13"
    "pebblesdb-14"
    "pebblesdb-15"
    "leveldb-3"
    "leveldb-4-5-v1.23"
    "leveldb-4-5-v1.15"
    "leveldb-4-5-v1.12"
    "leveldb-6"
    "etcd-9-10"
    "etcd-16-17-18-v3.5"
    "etcd-16-17-18-v3.4.25"
    "etcd-19"
)

for image in "${images[@]}"; do
    docker tag "$image" "mariajcr/lazyfs-reproduced-bugs:$image"
    docker push "mariajcr/lazyfs-reproduced-bugs:$image"
done
