# Description: This script runs the tests for the reproduced bugs with LazyFS.

#--------------------------------------------
# Pulling the images from Docker Hub

docker image pull mariajcr/lazyfs-reproduced-bugs:etcd-9-10
docker image pull mariajcr/lazyfs-reproduced-bugs:etcd-16-17-18
docker image pull mariajcr/lazyfs-reproduced-bugs:etcd-19
docker image pull mariajcr/lazyfs-reproduced-bugs:zookeeper-2-v3.7.1
docker image pull mariajcr/lazyfs-reproduced-bugs:zookeeper-2-v3.4.8
docker image pull mariajcr/lazyfs-reproduced-bugs:zookeeper-7-v3.7.1
docker image pull mariajcr/lazyfs-reproduced-bugs:zookeeper-7-v3.4.8
docker image pull mariajcr/lazyfs-reproduced-bugs:zookeeper-8-v3.7.1
docker image pull mariajcr/lazyfs-reproduced-bugs:zookeeper-8-v3.4.8
docker image pull mariajcr/lazyfs-reproduced-bugs:pebblesdb-13
docker image pull mariajcr/lazyfs-reproduced-bugs:pebblesdb-14
docker image pull mariajcr/lazyfs-reproduced-bugs:pebblesdb-15
docker image pull mariajcr/lazyfs-reproduced-bugs:leveldb-3
docker image pull mariajcr/lazyfs-reproduced-bugs:leveldb-4-5-v1.23
docker image pull mariajcr/lazyfs-reproduced-bugs:leveldb-4-5-v1.15
docker image pull mariajcr/lazyfs-reproduced-bugs:leveldb-4-5-v1.12
docker image pull mariajcr/lazyfs-reproduced-bugs:leveldb-6-v1.15
docker image pull mariajcr/lazyfs-reproduced-bugs:postgres-1
docker image pull mariajcr/lazyfs-reproduced-bugs:postgres-vcc
docker image pull mariajcr/lazyfs-reproduced-bugs:redis-11
docker image pull mariajcr/lazyfs-reproduced-bugs:redis-vcc
docker image pull mariajcr/lazyfs-reproduced-bugs:lnd-12

rep="mariajcr/lazyfs-reproduced-bugs"

#--------------------------------------------
# etcd

## etcd 9 and 10


echo -e  "\n\n>>>>>  etcd bug #9 and #10 torn-op fault  <<<<<"

docker run --name etcd-9-10-torn-op --env script=/test/etcd-9-10-torn-op.sh --privileged "$rep:etcd-9-10" 

echo -e  "\n\n>>>>>  etcd bug #9 and #10 torn-seq fault  <<<<<"

docker run --name etcd-9-10-torn-seq --env script=/test/etcd-9-10-torn-seq.sh --privileged  "$rep:etcd-9-10" 

## etcd 16,17 and 18

echo -e  "\n\n>>>>>  etcd bug #16  <<<<<"

docker run --name etcd-16 --env script_arguments=16 --privileged  "$rep:etcd-16-17-18" 

echo -e  "\n\n>>>>>  etcd bug #17  <<<<<"

docker run --name etcd-17 --env script_arguments=17 --privileged  "$rep:etcd-16-17-18" 

echo -e  "\n\n>>>>>  etcd bug #18  <<<<<"
docker run --name etcd-18 --env script_arguments=18 --privileged  "$rep:etcd-16-17-18" 


## etcd 19

echo -e  "\n\n>>>>>  etcd bug #19  <<<<<"
docker run --name etcd-19 --privileged  "$rep:etcd-19" 

#--------------------------------------------

# LevelDB

## LevelDB 3

echo -e  "\n\n>>>>>  LevelDB bug #3  <<<<<"

docker run --name leveldb-3 --privileged  "$rep:leveldb-3" 


## LevelDB 4 and 5

### v1.12
echo -e  "\n\n>>>>>  LevelDB v1.12 bug #4 and #5  <<<<<"

docker run  --name leveldb-4-5-v1.12 --env script_arguments="1.12" --privileged  "$rep:leveldb-4-5-v1.12"

## v1.15
echo -e  "\n\n>>>>>  LevelDB v1.15 bug #4 and #5  <<<<<"

docker run --name leveldb-4-5-v1.15 --env script_arguments="1.15" --privileged  "$rep:leveldb-4-5-v1.15" 

## v1.23
echo -e  "\n\n>>>>>  LevelDB v1.23 bug #4 and #5  <<<<<"

docker run --name leveldb-4-5-v1.23 --env script_arguments="1.23" --privileged  "$rep:leveldb-4-5-v1.23" 


## LevelDB 6
echo -e  "\n\n>>>>>  LevelDB v1.15 bug #6  <<<<<"

docker run --name leveldb-6-v1.15 --privileged  "$rep:leveldb-6-v1.15" 

#--------------------------------------------

# PebblesDB

## PebblesDB 13
echo -e  "\n\n>>>>>  PebblesDB bug #13  <<<<<"

docker run --name pebblesdb-13 --privileged  "$rep:pebblesdb-13" 

## PebblesDB 14
echo -e  "\n\n>>>>>  PebblesDB bug #14  <<<<<"

docker run --name pebblesdb-14 --privileged  "$rep:pebblesdb-14" 

## PebblesDB 15
echo -e  "\n\n>>>>>  PebblesDB bug #15  <<<<<"

docker run --name pebblesdb-15 --privileged  "$rep:pebblesdb-15" 

#--------------------------------------------

# ZooKeeper

## ZooKeeper 2

### v3.4.8
echo -e  "\n\n>>>>>  ZooKeeper v3.4.8 bug #2  <<<<<"

docker run  --name zookeeper-2-v3.4.8 --privileged  "$rep:zookeeper-2-v3.4.8" 

### v3.7.1
echo -e  "\n\n>>>>>  ZooKeeper v3.7.1 bug #2  <<<<<"

docker run --name zookeeper-2-v3.7.1 --privileged  "$rep:zookeeper-2-v3.7.1"

## ZooKeeper 8

### v3.4.8
echo -e  "\n\n>>>>>  ZooKeeper v3.4.8 bug #8  <<<<<"

docker run  --name zookeeper-8-v3.4.8 --privileged  "$rep:zookeeper-8-v3.4.8" 

### v3.7.1
echo -e  "\n\n>>>>>  ZooKeeper v3.7.1 bug #8  <<<<<"

docker run  --name zookeeper-8-v3.7.1 --privileged  "$rep:zookeeper-8-v3.7.1"

## ZooKeeper 7

### v3.4.8
echo -e  "\n\n>>>>>  ZooKeeper v3.4.8 bug #7  <<<<<"

docker run  --name zookeeper-7-v3.4.8 --privileged  "$rep:zookeeper-7-v3.4.8" 

### v3.7.1
echo -e  "\n\n>>>>>  ZooKeeper v3.7.1 bug #7  <<<<<"

docker run  --name zookeeper-7-v3.7.1 --privileged  "$rep:zookeeper-7-v3.7.1"


#--------------------------------------------

# Redis

## Redis 11
echo -e "\n\n>>>>>  Redis bug #11  <<<<<"

docker run  --name redis-11 --privileged  "$rep:redis-11"

## Redis VCC

echo -e "\n\n>>>>>  Redis tests for checking crash consistency mechanisms  <<<<<"

docker run  --name redis-vcc-1 --env script_arguments=1 --privileged  "$rep:redis-vcc"

echo -e "\n"

docker run  --name redis-vcc-2 --env script_arguments=2 --privileged  "$rep:redis-vcc"

#--------------------------------------------

# Lightning Network Daemon

## Lightning Network Daemon 12
echo -e  "\n\n>>>>>  Lightning Network Daemon bug #12  <<<<<"

docker run -i  --name lnd-12 --privileged  "$rep:lnd-12"

#--------------------------------------------

# PostgreSQL

## PostgreSQL 1
echo -e  "\n\n>>>>>  PostgreSQL bug #1  <<<<<"

docker run -it  --name postgres-1 --privileged  "$rep:postgres-1"

## PostgreSQL VCC
echo -e  "\n\n>>>>>  PostgreSQL test for checking crash consistency mechanisms  <<<<<"

docker run -it  --name postgres-vcc --env script_arguments="1 100 60 2 5" --privileged  "$rep:postgres-vcc"

