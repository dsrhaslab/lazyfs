#===============================================================================
#  Jepsen configurations and cluster setup       
#  ------------------------------------------------------                    
#  This script requires Docker to be installed previously
#===============================================================================

#Jepsen dependencies
apt-get install leiningen

#Download Jepsen repository
git clone https://github.com/jepsen-io/jepsen.git
cd jepsen
git checkout 9e40a61d89de1343e06b9e8d1f77fe2c0be2e6ec

#Change debian:buster to debian:bullseye
sed -i 's/debian:buster/debian:bullseye/g' docker/bin/up
sed -i 's/debian:buster/debian:bullseye/g' docker/control/Dockerfile
sed -i 's/debian-base-standard:buster/debian-base-standard:bullseye/g' docker/node/Dockerfile

#Change LazyFS version
sed -i 's/(def commit\s*".*"\s*")[^"]*"/\1"0.2.0"/' jepsen/src/jepsen/lazyfs.clj
lein install

#Create a cluster with 5 nodes
docker/bin/up -n 5