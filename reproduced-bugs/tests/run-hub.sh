#--------------------------------------------------------------------------
#
# DESCRIPTION: This script runs the tests for the reproduced bugs with LazyFS.
#              The images are pulled from Docker Hub and the tests are executed.
#
#--------------------------------------------------------------------------
# Pulling the images from Docker Hub

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

rep="mariajcr/lazyfs-reproduced-bugs"

for image in "${images[@]}"; do
    docker image pull "$rep:$image"
done


#--------------------------------------------
# etcd

## etcd 9 and 10


echo -e  "\n\n>>>>>  etcd bug #9 and #10 torn-op fault  <<<<<"

docker run --name etcd-9-10-torn-op --env script=/test/etcd-9-10-torn-op.sh --privileged "$rep:etcd-9-10" 

echo -e  "\n\n>>>>>  etcd bug #9 and #10 torn-seq fault  <<<<<"

docker run --name etcd-9-10-torn-seq --env script=/test/etcd-9-10-torn-seq.sh --privileged  "$rep:etcd-9-10" 

## etcd 16,17 and 18

### v3.4.25
echo -e  "\n\n>>>>>  etcd v3.4.25 bug #16  <<<<<"

docker run --name etcd-16-v3.4.25 --env script_arguments=16 --privileged  "$rep:etcd-16-17-18-v3.4.25" 

echo -e  "\n\n>>>>>  etcd v3.4.25 bug #17  <<<<<"

docker run --name etcd-17-v3.4.25 --env script_arguments=17 --privileged  "$rep:etcd-16-17-18-v3.4.25" 

echo -e  "\n\n>>>>>  etcd v3.4.25 bug #18  <<<<<"

docker run --name etcd-18-v3.4.25 --env script_arguments=18 --privileged  "$rep:etcd-16-17-18-v3.4.25" 

### v3.5.0

echo -e  "\n\n>>>>>  etcd v3.5 bug #16  <<<<<"

docker run --name etcd-16-v3.5 --env script_arguments=16 --privileged  "$rep:etcd-16-17-18-v3.5" 

echo -e  "\n\n>>>>>  etcd v3.5 bug #17  <<<<<"

docker run --name etcd-17-v3.5 --env script_arguments=17 --privileged  "$rep:etcd-16-17-18-v3.5" 

echo -e  "\n\n>>>>>  etcd v3.5 bug #18  <<<<<"

docker run --name etcd-18-v3.5 --env script_arguments=18 --privileged  "$rep:etcd-16-17-18-v3.5" 


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

docker run -it  --name postgres-vcc --env script_arguments="1 100 60 2" --privileged  "$rep:postgres-vcc"

