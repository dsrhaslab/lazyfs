# etcd

## etcd 9 and 10
docker build -t etcd-9-10 --build-arg application=etcd --build-arg test_id=etcd-9-10 --build-arg app_version=2.3.0 -f ../dockerfiles/Dockerfile ..


## etcd 16,17 and 18

### v3.4.25
docker build -t etcd-16-17-18-v3.4.25 --build-arg application=etcd --build-arg test_id=etcd-16-17-18 --build-arg app_version=3.4.25 -f ../dockerfiles/Dockerfile ..

### v3.5.0
docker build -t etcd-16-17-18-v3.5 --build-arg application=etcd --build-arg test_id=etcd-16-17-18 --build-arg app_version=3.5.0 -f ../../dockerfiles/Dockerfiles/../dockerfiles/Dockerfile ..

## etcd 19
docker build -t etcd-19 --build-arg application=etcd --build-arg test_id=etcd-19 --build-arg app_version=2.3.0 -f ../../dockerfiles/Dockerfiles/../dockerfiles/Dockerfile ..

#--------------------------------------------
# LevelDB

## LevelDB 3
docker build -t leveldb-3 --build-arg application=leveldb --build-arg test_id=leveldb-3 --build-arg app_version=1.15 -f ../../dockerfiles/Dockerfiles/../dockerfiles/Dockerfile ..


## LevelDB 4 and 5

### v1.12
docker build -t leveldb-4-5-v1.12 --build-arg application=leveldb --build-arg test_id=leveldb-4-5 --build-arg app_version=1.12 -f ../../dockerfiles/Dockerfiles/../dockerfiles/Dockerfile ..


### v1.15
docker build -t leveldb-4-5-v1.15 --build-arg application=leveldb --build-arg test_id=leveldb-4-5 --build-arg app_version=1.15 -f ../dockerfiles/Dockerfile ..


### v1.23
docker build -t leveldb-4-5-v1.23 --build-arg application=leveldb --build-arg test_id=leveldb-4-5 --build-arg app_version=1.23 -f ../dockerfiles/Dockerfile ..


## LevelDB 6

docker build -t leveldb-6 --build-arg application=leveldb --build-arg test_id=leveldb-6 --build-arg app_version=1.15 -f ../dockerfiles/Dockerfile ..


#--------------------------------------------

# PebblesDB

## PebblesDB 13
docker build -t pebblesdb-13 --build-arg application=pebblesdb --build-arg test_id=pebblesdb-13 -f ../dockerfiles/Dockerfile ..


## PebblesDB 14
docker build -t pebblesdb-14 --build-arg application=pebblesdb --build-arg test_id=pebblesdb-14 -f ../dockerfiles/Dockerfile ..


## PebblesDB 15
docker build -t pebblesdb-15 --build-arg application=pebblesdb --build-arg test_id=pebblesdb-15 -f ../dockerfiles/Dockerfile ..


#--------------------------------------------

# ZooKeeper

## ZooKeeper 2

### v3.4.8

docker build -t zookeeper-2-v3.4.8 --build-arg application=zookeeper --build-arg test_id=zookeeper-2 --build-arg app_version=3.4.8 -f ../dockerfiles/Dockerfile ..


### v3.7.1
docker build -t zookeeper-2-v3.7.1 --build-arg application=zookeeper --build-arg test_id=zookeeper-2 --build-arg app_version=3.7.1 -f ../dockerfiles/Dockerfile ..


## ZooKeeper 8

### v3.4.8

docker build -t zookeeper-8-v3.4.8 --build-arg application=zookeeper --build-arg test_id=zookeeper-8 --build-arg app_version=3.4.8 -f ../dockerfiles/Dockerfile ..


### v3.7.1
docker build -t zookeeper-8-v3.7.1 --build-arg application=zookeeper --build-arg test_id=zookeeper-8 --build-arg app_version=3.7.1 -f ../dockerfiles/Dockerfile ..


## ZooKeeper 7 

### v3.4.8

docker build -t zookeeper-7-v3.4.8 --build-arg application=zookeeper --build-arg test_id=zookeeper-7 --build-arg app_version=3.4.8 -f ../dockerfiles/Dockerfile ..


### v3.7.1
docker build -t zookeeper-7-v3.7.1 --build-arg application=zookeeper --build-arg test_id=zookeeper-7 --build-arg app_version=3.7.1 -f ../dockerfiles/Dockerfile ..


#--------------------------------------------

# Redis

## Redis 11

docker build -t redis-11 --build-arg application=redis --build-arg test_id=redis-11 --build-arg app_version=7.0.4 -f ../dockerfiles/Dockerfile ..


## Redis VCC

docker build -t redis-vcc --build-arg application=redis --build-arg test_id=redis-vcc --build-arg app_version=7.2.3 -f ../dockerfiles/Dockerfile ..


#--------------------------------------------

# Lightning Network Daemon

## Lightning Network Daemon 12

docker build -t lnd-12 --build-arg application=lnd --build-arg test_id=lnd-12 -f ../dockerfiles/Dockerfile ..


#--------------------------------------------

# PostgreSQL

## PostgreSQL 1
docker build -t postgres-1 --build-arg application=postgres --build-arg test_id=postgres-1 --build-arg app_version=12.11 -f ../dockerfiles/Dockerfile ..


## PostgreSQL VCC
docker build -t postgres-vcc --build-arg application=postgres --build-arg test_id=postgres-vcc --build-arg app_version=15.2 -f ../dockerfiles/Dockerfile ..