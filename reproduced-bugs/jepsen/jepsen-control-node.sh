#===============================================================================
#  Script to be executed inside the control node   
#  -----------------------------------------------    
#  Installs etcd and runs Jepsen tests                      
#===============================================================================


#Install etcd tests in control node
git clone https://github.com/jepsen-io/etcd.git
cd etcd
git checkout 181656bb551bbc10cdc3d959866637574cdc9e17 

ssh-keyscan -t rsa n1 >> ~/.ssh/known_hosts
ssh-keyscan -t rsa n2 >> ~/.ssh/known_hosts
ssh-keyscan -t rsa n3 >> ~/.ssh/known_hosts
ssh-keyscan -t rsa n4 >> ~/.ssh/known_hosts
ssh-keyscan -t rsa n5 >> ~/.ssh/known_hosts

#Run etcd tests
lein run test-all -w append --rate 1000 --concurrency 4n --lazyfs --nemesis kill --time-limit 300 --test-count 50 --nemesis-interval 15
