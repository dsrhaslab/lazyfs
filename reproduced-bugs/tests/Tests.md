# etcd

## etcd 9 and 10
docker build -t etcd-9-10-image --build-arg application=etcd --build-arg test_id=etcd-9-10 --build-arg app_version=2.3.0 -f Dockerfile ..

docker run --name etcd-9-10-torn-op --env script=/test/etcd-9-10-torn-op.sh --privileged etcd-9-10-image 

docker run --name etcd-9-10-torn-seq --env script=/test/etcd-9-10-torn-seq.sh --privileged etcd-9-10-image 

## etcd 16,17 and 18
docker build -t etcd-16-17-18-image --build-arg application=etcd --build-arg test_id=etcd-16-17-18 --build-arg app_version=3.4.25 -f Dockerfile ..

docker run --name etcd-16 --env script_arguments=16 --privileged etcd-16-17-18-image 
docker run --name etcd-17 --env script_arguments=17 --privileged etcd-16-17-18-image 
docker run --name etcd-18 --env script_arguments=18 --privileged etcd-16-17-18-image 

## etcd 19
docker build -t etcd-19-image --build-arg application=etcd --build-arg test_id=etcd-19 --build-arg app_version=2.3.0 -f Dockerfile ..

docker run --name etcd-19 --privileged etcd-19-image 

---

# LevelDB

## LevelDB 3
docker build -t leveldb-3-image --build-arg application=leveldb --build-arg test_id=leveldb-3 --build-arg app_version=1.15 -f Dockerfile ..

docker run --name leveldb-3 --privileged leveldb-3-image 

## LevelDB 4 and 5

### v1.12
docker build -t leveldb-4-5-v1.12-image --build-arg application=leveldb --build-arg test_id=leveldb-4-5 --build-arg app_version=1.12 -f Dockerfile ..

docker run --rm --name leveldb-4-5-v1.12 --privileged leveldb-4-5-v1.12-image

## v1.15
docker build -t leveldb-4-5-v1.15-image --build-arg application=leveldb --build-arg test_id=leveldb-4-5 --build-arg app_version=1.15 -f Dockerfile ..

docker run --name leveldb-4-5-v1.15 --privileged leveldb-4-5-v1.15-image 

## v1.23
docker build -t leveldb-4-5-v1.23-image --build-arg application=leveldb --build-arg test_id=leveldb-4-5 --build-arg app_version=1.23 -f Dockerfile ..

docker run --name leveldb-4-5-v1.23 --privileged leveldb-4-5-v1.23-image 

## LevelDB 6

docker build -t leveldb-6-v1.15-image --build-arg application=leveldb --build-arg test_id=leveldb-6 --build-arg app_version=1.15 -f Dockerfile ..

docker run --name leveldb-6-v1.15 --privileged leveldb-6-v1.15-image 

---

# PebblesDB

## PebblesDB 13
docker build -t pebblesdb-13-image --build-arg application=pebblesdb --build-arg test_id=pebblesdb-13 -f Dockerfile ..

docker run --name pebblesdb-13 --privileged pebblesdb-13-image 

## PebblesDB 14
docker build -t pebblesdb-14-image --build-arg application=pebblesdb --build-arg test_id=pebblesdb-14 -f Dockerfile ..

docker run --name pebblesdb-14 --privileged pebblesdb-14-image 

## PebblesDB 15
docker build -t pebblesdb-15-image --build-arg application=pebblesdb --build-arg test_id=pebblesdb-15 -f Dockerfile ..

docker run --name pebblesdb-15 --privileged pebblesdb-15-image 

---

# ZooKeeper

## ZooKeeper 2

### v3.4.8

docker build -t zookeeper-2-v3.4.8-image --build-arg application=zookeeper --build-arg test_id=zookeeper-2 --build-arg app_version=3.4.8 -f Dockerfile ..

docker run --rm --name zookeeper-2-v3.4.8 --privileged zookeeper-2-v3.4.8-image 

### v3.7.1
docker build -t zookeeper-2-v3.7.1-image --build-arg application=zookeeper --build-arg test_id=zookeeper-2 --build-arg app_version=3.7.1 -f Dockerfile ..

docker run --name zookeeper-2-v3.7.1 --privileged zookeeper-2-v3.7.1-image

## ZooKeeper 8

### v3.4.8

docker build -t zookeeper-8-v3.4.8-image --build-arg application=zookeeper --build-arg test_id=zookeeper-8 --build-arg app_version=3.4.8 -f Dockerfile ..

docker run --rm --name zookeeper-8-v3.4.8 --privileged zookeeper-8-v3.4.8-image 

### v3.7.1
docker build -t zookeeper-8-v3.7.1-image --build-arg application=zookeeper --build-arg test_id=zookeeper-8 --build-arg app_version=3.7.1 -f Dockerfile ..

docker run --rm --name zookeeper-8-v3.7.1 --privileged zookeeper-8-v3.7.1-image

## ZooKeeper 7

### v3.4.8

docker build -t zookeeper-7-v3.4.8-image --build-arg application=zookeeper --build-arg test_id=zookeeper-7 --build-arg app_version=3.4.8 -f Dockerfile ..

docker run --rm --name zookeeper-7-v3.4.8 --privileged zookeeper-7-v3.4.8-image 

### v3.7.1
docker build -t zookeeper-7-v3.7.1-image --build-arg application=zookeeper --build-arg test_id=zookeeper-7 --build-arg app_version=3.7.1 -f Dockerfile ..

docker run --rm --name zookeeper-7-v3.7.1 --privileged zookeeper-7-v3.7.1-image


---

# Redis

## Redis 11

docker build -t redis-11-image --build-arg application=redis --build-arg test_id=redis-11 --build-arg app_version=7.0.4 -f Dockerfile ..

docker run --rm --name redis-11 --privileged redis-11-image

## Redis VCC

docker build -t redis-vcc-image --build-arg application=redis --build-arg test_id=redis-vcc --build-arg app_version=7.2.3 -f Dockerfile ..

docker run --rm --name redis-vcc-1 --env script_arguments=1 --privileged redis-vcc-image
docker run --rm --name redis-vcc-2 --env script_arguments=2 --privileged redis-vcc-image

--

# Lightning Network Daemon

## Lightning Network Daemon 12

docker build -t lnd-12-image --build-arg application=lnd --build-arg test_id=lnd-12 -f Dockerfile ..

docker run -i --rm --name lnd-12 --privileged lnd-12-image

--

# PostgreSQL

## PostgreSQL 1
docker build -t postgres-1-image --build-arg application=postgres --build-arg test_id=postgres-1 --build-arg app_version=12.11 -f Dockerfile ..

docker run -it --rm --name postgres-1 --privileged postgres-1-image

## PostgreSQL VCC
docker build -t postgres-vcc-image --build-arg application=postgres --build-arg test_id=postgres-vcc --build-arg app_version=15.2 -f Dockerfile ..

docker run -it --rm --name postgres-vcc --privileged postgres-vcc-image
