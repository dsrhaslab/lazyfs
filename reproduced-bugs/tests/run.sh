# etcd

## etcd 9 and 10

echo -e  ">>>>>  etcd bug #9 and #10 torn-op fault  <<<<<"

docker run --name etcd-9-10-torn-op --env script=/test/etcd-9-10-torn-op.sh --privileged etcd-9-10-image 

echo -e  "\n\n>>>>>  etcd bug #9 and #10 torn-seq fault  <<<<<"

docker run --name etcd-9-10-torn-seq --env script=/test/etcd-9-10-torn-seq.sh --privileged etcd-9-10-image 

## etcd 16,17 and 18

echo -e  "\n\n>>>>>  etcd bug #16  <<<<<"

docker run --name etcd-16 --env script_arguments=16 --privileged etcd-16-17-18-image 

echo -e  "\n\n>>>>>  etcd bug #17  <<<<<"

docker run --name etcd-17 --env script_arguments=17 --privileged etcd-16-17-18-image 

echo -e  "\n\n>>>>>  etcd bug #18  <<<<<"
docker run --name etcd-18 --env script_arguments=18 --privileged etcd-16-17-18-image 


## etcd 19

echo -e  "\n\n>>>>>  etcd bug #19  <<<<<"
docker run --name etcd-19 --privileged etcd-19-image 

#--------------------------------------------

# LevelDB

## LevelDB 3

echo -e  "\n\n>>>>>  LevelDB bug #3  <<<<<"

docker run --name leveldb-3 --privileged leveldb-3-image 


## LevelDB 4 and 5

### v1.12
echo -e  "\n\n>>>>>  LevelDB v1.12 bug #4 and #5  <<<<<"

docker run  --name leveldb-4-5-v1.12 --env script_arguments="1.12" --privileged leveldb-4-5-v1.12-image

## v1.15
echo -e  "\n\n>>>>>  LevelDB v1.15 bug #4 and #5  <<<<<"

docker run --name leveldb-4-5-v1.15 --env script_arguments="1.15" --privileged leveldb-4-5-v1.15-image 

## v1.23
echo -e  "\n\n>>>>>  LevelDB v1.23 bug #4 and #5  <<<<<"

docker run --name leveldb-4-5-v1.23 --env script_arguments="1.23" --privileged leveldb-4-5-v1.23-image 


## LevelDB 6
echo -e  "\n\n>>>>>  LevelDB v1.15 bug #6  <<<<<"

docker run --name leveldb-6-v1.15 --privileged leveldb-6-v1.15-image 

#--------------------------------------------

# PebblesDB

## PebblesDB 13
echo -e  "\n\n>>>>>  PebblesDB bug #13  <<<<<"

docker run --name pebblesdb-13 --privileged pebblesdb-13-image 

## PebblesDB 14
echo -e  "\n\n>>>>>  PebblesDB bug #14  <<<<<"

docker run --name pebblesdb-14 --privileged pebblesdb-14-image 

## PebblesDB 15
echo -e  "\n\n>>>>>  PebblesDB bug #15  <<<<<"

docker run --name pebblesdb-15 --privileged pebblesdb-15-image 

#--------------------------------------------

# ZooKeeper

## ZooKeeper 2

### v3.4.8
echo -e  "\n\n>>>>>  ZooKeeper v3.4.8 bug #2  <<<<<"

docker run  --name zookeeper-2-v3.4.8 --privileged zookeeper-2-v3.4.8-image 

### v3.7.1
echo -e  "\n\n>>>>>  ZooKeeper v3.7.1 bug #2  <<<<<"

docker run --name zookeeper-2-v3.7.1 --privileged zookeeper-2-v3.7.1-image

## ZooKeeper 8

### v3.4.8
echo -e  "\n\n>>>>>  ZooKeeper v3.4.8 bug #8  <<<<<"

docker run  --name zookeeper-8-v3.4.8 --privileged zookeeper-8-v3.4.8-image 

### v3.7.1
echo -e  "\n\n>>>>>  ZooKeeper v3.7.1 bug #8  <<<<<"

docker run  --name zookeeper-8-v3.7.1 --privileged zookeeper-8-v3.7.1-image

## ZooKeeper 7

### v3.4.8
echo -e  "\n\n>>>>>  ZooKeeper v3.4.8 bug #7  <<<<<"

docker run  --name zookeeper-7-v3.4.8 --privileged zookeeper-7-v3.4.8-image 

### v3.7.1
echo -e  "\n\n>>>>>  ZooKeeper v3.7.1 bug #7  <<<<<"

docker run  --name zookeeper-7-v3.7.1 --privileged zookeeper-7-v3.7.1-image


#--------------------------------------------

# Redis

## Redis 11
echo -e "\n\n>>>>>  Redis bug #11  <<<<<"

docker run  --name redis-11 --privileged redis-11-image

## Redis VCC

echo -e "\n\n>>>>>  Redis tests for checking crash consistency mechanisms  <<<<<"

docker run  --name redis-vcc-1 --env script_arguments=1 --privileged redis-vcc-image

echo -e "\n"

docker run  --name redis-vcc-2 --env script_arguments=2 --privileged redis-vcc-image

#--------------------------------------------

# Lightning Network Daemon

## Lightning Network Daemon 12
echo -e  "\n\n>>>>>  Lightning Network Daemon bug #12  <<<<<"

docker run -i  --name lnd-12 --privileged lnd-12-image

#--------------------------------------------

# PostgreSQL

## PostgreSQL 1
echo -e  "\n\n>>>>>  PostgreSQL bug #1  <<<<<"

docker run -it  --name postgres-1 --privileged postgres-1-image

## PostgreSQL VCC
echo -e  "\n\n>>>>>  PostgreSQL test for checking crash consistency mechanisms  <<<<<"

docker run -it  --name postgres-vcc --env script_arguments="1 100 60 2 5" --privileged postgres-vcc-image