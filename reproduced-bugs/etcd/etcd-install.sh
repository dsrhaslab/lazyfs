#!/bin/bash

#===============================================================================
#  Installs etcd         
#                      
#  Parameters:
#    - version: The version of etcd to install
#===============================================================================

version="$1"

wget https://github.com/coreos/etcd/releases/download/v$version/etcd-v$version-linux-amd64.tar.gz -O etcd-v$version-linux-amd64.tar.gz
tar xzvf etcd-v$version-linux-amd64.tar.gz
mv etcd-v$version-linux-amd64 etcd-linux-amd64