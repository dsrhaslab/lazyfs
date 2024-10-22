#!/bin/bash

#Number of the server
server=$1 

#data_dir="/test/zookeeper-test/$server/data-m"
#data_root_dir="/test/zookeeper-test/$server/data-r"
#
#lfs_dir="/test/lazyfs/lazyfs"
#lfs_log="/test/lazyfs.log"
#lfs_config="/test/config.toml"
#
#zk_dir="/test/zookeeper"
#serverscript="/bin/zkServer.sh"
#clientscript="/bin/zkCli.sh"
#zk_out="/test/zookeeper-test/$server/zk_out.txt"
#config_file="/conf/zoo$server.cfg"

data_dir="/home/gsd/zookeeper/$server/data-m"
data_root_dir="/home/gsd/zookeeper/$server/data-r"

lfs_dir="/home/gsd/lazyfs-rep/lazyfs"
lfs_log="/home/gsd/zookeeper/lazyfs.log"
lfs_config="/home/gsd/zookeeper/lfs.toml"

zk_dir="/home/gsd/zookeeper"
serverscript="/bin/zkServer.sh"
clientscript="/bin/zkCli.sh"
config_file="/conf/zoo$server.cfg"

zk_out="/home/gsd/zookeeper/$server/zk_out.txt"
zk_out_cli="/home/gsd/zookeeper/$server/zk_output_cli.txt"