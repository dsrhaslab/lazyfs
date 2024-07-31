#!/bin/bash

#Number of the server
server=$1 

data_dir="/test/zookeeper-test/$server/data-m"
data_root_dir="/test/zookeeper-test/$server/data-r"

lfs_dir="/test/lazyfs/lazyfs"
lfs_log="/test/lazyfs.log"
lfs_config="/test/config.toml"

#zk_dir="/opt/zookeeper"
zk_dir="/test/zookeeper"
serverscript="/bin/zkServer.sh"
clientscript="/bin/zkCli.sh"
zk_out="/test/zookeeper-test/$server/zk_out.txt"
config_file="/conf/zoo$server.cfg"

workload="zk-cli-w.py"
