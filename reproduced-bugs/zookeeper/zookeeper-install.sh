#!/bin/bash

version=$1

apt-get update #without this, the next command will fail
apt-get install -y openjdk-8-jdk
apt-get install -y lsof
apt-get install -y python3.8
apt-get install -y pip
pip install kazoo

link=""

if [ "$version" == "3.4.8" ]; then
    link=https://archive.apache.org/dist/zookeeper/zookeeper-$version/zookeeper-$version.tar.gz

    wget $link -O "zookeeper-$version.tar.gz"
    tar -xzf "zookeeper-$version.tar.gz"
    mv "zookeeper-$version" zookeeper

elif [ "$version" == "3.7.1" ]; then
    link=https://archive.apache.org/dist/zookeeper/zookeeper-$version/apache-zookeeper-$version-bin.tar.gz

    wget $link -O "zookeeper-$version.tar.gz"
    tar -xzf "zookeeper-$version.tar.gz"
    mv "apache-zookeeper-$version-bin" zookeeper
else
    echo "Invalid version"
    exit 1
fi

