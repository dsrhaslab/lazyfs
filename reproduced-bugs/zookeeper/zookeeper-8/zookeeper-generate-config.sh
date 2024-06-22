#!/bin/bash

#===============================================================================
#   DESCRIPTION: This script produces a valid configuration file for ZooKeeper.
#                Receives the number of a server as argument.
#
#        AUTHOR: Maria Ramos,
#      REVISION: 1 Apr 2024
#===============================================================================

DIR="$PWD"
. "$DIR/zookeeper-8-vars.sh" $server

tickTime="2000"  
clientPort="218$server"  
maxClientCnxns="60"  
initLimit="50"  
syncLimit="50"  
server1="127.0.0.1:2887:3887"  
server2="127.0.0.1:2888:3888"  
server3="127.0.0.1:2889:3889"

truncate -s 0 $zk_dir$config_file
echo "tickTime=$tickTime" >> $zk_dir$config_file
echo "dataDir=$data_dir" >> $zk_dir$config_file
echo "clientPort=$clientPort" >> $zk_dir$config_file
echo "maxClientCnxns=$maxClientCnxns" >> $zk_dir$config_file
echo "initLimit=$initLimit" >> $zk_dir$config_file
echo "syncLimit=$syncLimit" >> $zk_dir$config_file
echo "server.1=$server1" >> $zk_dir$config_file
echo "server.2=$server2" >> $zk_dir$config_file
echo "server.3=$server3" >> $zk_dir$config_file