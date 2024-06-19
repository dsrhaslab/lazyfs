#!/bin/bash

version="$1"

wget https://github.com/coreos/etcd/releases/download/v$version/etcd-v$version-linux-amd64.tar.gz -O etcd-v$version-linux-amd64.tar.gz
tar xzvf etcd-v$version-linux-amd64.tar.gz