########################################
#  To be executed in the control node  #
########################################

#Enter Jepsen container of the control node
docker/bin/console #or docker exec -it jepsen-control bash

#Install etcd tests in control node
git clone https://github.com/jepsen-io/etcd.git
cd etcd
git checkout 181656bb551bbc10cdc3d959866637574cdc9e17 

#Run etcd tests
lein run test-all -w append --rate 1000 --concurrency 4n --lazyfs --nemesis kill --time-limit 300 --test-count 50 --nemesis-interval 15

#Sometimes the test fails because the control node cannot connect to the nodes running etcd. To fix this, ssh to each node (ssh n1, ssh n...)