#!/bin/bash

#===============================================================================
#   DESCRIPTION: This script tests bug #21 of ZooKeeper v3.4.8
#       
#      - Error           (v3.4.8) --
#                        (v3.7.1) --
#      - Time            16 seconds
#
#        AUTHOR: Maria Ramos,
#      REVISION: 1 Oct 2024
#===============================================================================

DIR="$PWD"
. "$DIR/../../aux.sh"

#========================Generate data for each server==========================

servers=(1 2 3)
server_fault=3
connect_server=2

for i in "${servers[@]}"
do 
    . "$DIR/zookeeper-21-vars.sh" $i #File to configure with paths important for the tests

    #Clean data dirs
    create_and_clean_directory $data_dir

    #Generate configuration file
    . "$DIR/zookeeper-generate-config.sh" $i

    if (("$i" != $server_fault)); then
        echo $i > "$data_dir/myid"
    else 
        create_and_clean_directory $data_root_dir
        echo $i > "$data_root_dir/myid"
    fi
done

. "$DIR/zookeeper-21-vars.sh" $server_fault

#Clean logs
truncate -s 0 $lfs_log
truncate -s 0 $zk_out

fault="lazyfs::crash::timing=after::op=fsync::from_rgx=version-2/log.[0-9]0000000[0-9]"
faults_fifo=$(grep 'fifo_path=' $lfs_config | sed 's/.*fifo_path="//; s/".*//')

sdir1="/home/gsd/zookeeper/1/saved_data"
sdir2="/home/gsd/zookeeper/2/saved_data"
sdir3="/home/gsd/zookeeper/3/saved_data"

create_and_clean_directory $sdir1
create_and_clean_directory $sdir2
create_and_clean_directory $sdir3

dir1="/home/gsd/zookeeper/1/data-m/version-2"
dir2="/home/gsd/zookeeper/2/data-m/version-2"
dir3="/home/gsd/zookeeper/3/data-m/version-2"

#===============================================================================

#Record start time
start_time=$(date +%s)

#Mount LazyFS
cd $lfs_dir
./mount-lazyfs.sh -c "$lfs_config" -m "$data_dir" -r "$data_root_dir" -s -f & > /dev/null 2>&1 

#Wait for LazyFS to start
echo -e "1.${YELLOW}Wait for LazyFS to start${RESET}."
wait_action "running LazyFS..." $lfs_log
echo -e "2.${GREEN}LazyFS started${RESET}."

#Start other ZoKeeper servers
for i in "${servers[@]}"
do 
    if (("$i" != $server_fault)); then
    . "$DIR/zookeeper-21-vars.sh" $i 

    "$zk_dir$serverscript" start "$zk_dir$config_file" > /dev/null 2>&1 
    echo -e "${GREEN}ZooKeeper server.$i started${RESET}."
    fi
done

. "$DIR/zookeeper-21-vars.sh" $server_fault

#Start ZooKeeper server 
"$zk_dir$serverscript" start $zk_dir$config_file > $zk_out 2>&1
zk_pid=$(lsof -t -i:$clientPort)
wait_action "STARTED" $zk_out
echo -e "3.${GREEN}ZooKeeper started${RESET}."

#Connect client to server
"$zk_dir$clientscript" -server 127.0.0.1:218${connect_server} > /dev/null 2>&1 &
zk_client_pid=$!
echo -e "4.${GREEN}Client connected to server ${connect_server}${RESET}."

#Create znode /a
echo -e "5.${YELLOW}Waiting for creation of znode /a on server ${connect_server}${RESET}..."
"$zk_dir$clientscript" -server 127.0.0.1:218${connect_server} <<EOF >> "$zk_out_cli" 2>&1
create /a aaa
EOF
echo -e "6.${GREEN}Znode /a created${RESET}."

#cd $zk_dir
#java -cp zookeeper-3.4.8.jar:lib/log4j-1.2.16.jar:lib/slf4j-log4j12-1.6.1.jar:lib/slf4j-api-1.6.1.jar org.apache.zookeeper.server.SnapshotFormatter /home/gsd/zookeeper/3/data-m/version-2/snapshot.100000000
#java -cp zookeeper-3.4.8.jar:lib/log4j-1.2.16.jar:lib/slf4j-log4j12-1.6.1.jar:lib/slf4j-api-1.6.1.jar org.apache.zookeeper.server.SnapshotFormatter /home/gsd/zookeeper/3/data-m/version-2/snapshot.0
#cd $lfs_dir

#Inject fault
echo $fault > $faults_fifo
#Wait for fault to be injected
echo -e "7.${YELLOW}Waiting for fault to be received${RESET}..."
wait_action "received: VALID crash fault" $lfs_log 
echo -e "8.${GREEN}Fault injected${RESET}."

#Create znode /b
echo -e "9.${YELLOW}Waiting for creation of znode /b on server ${connect_server}${RESET}..."
"$zk_dir$clientscript" -server 127.0.0.1:218${connect_server} <<EOF >> "$zk_out_cli" 2>&1
create /b bbb
EOF
echo -e "10.${GREEN}Znode /b created${RESET}."

#Wait for LazyFS to crash
echo -e "11.${YELLOW}Waiting for LazyFS to crash${RESET}..."
wait_action "Killing LazyFS" $lfs_log
echo -e "12.${RED}LazyFS crashed${RESET}."

#Kill ZooKeeper process
kill -9 $zk_pid
echo -e "13.${RED}Killed ZooKeeper${RESET}."

#Copy LazyFS log
cp $lfs_log $sdir3
mv $sdir3/lazyfs.log $sdir3/lazyfs.log.1

#==================================After 1st crash======================================

#Create znode /c
echo -e "14.${YELLOW}Waiting for creation of znode /c on server ${connect_server}${RESET}..."
"$zk_dir$clientscript" -server 127.0.0.1:218${connect_server} <<EOF >> "$zk_out_cli" 2>&1
create /c ccc
EOF
echo -e "15.${GREEN}Znode /c created${RESET}."

pkill -TERM -P $zk_client_pid

#Restart LazyFS
fusermount -uz "$data_dir"
truncate -s 0 "$lfs_log"
./mount-lazyfs.sh -c "$lfs_config" -m "$data_dir" -r "$data_root_dir" -s > /dev/null 2>&1 

#Wait for LazyFS to start
echo -e "16.${YELLOW}Waiting for LazyFS to restart${RESET}..."
wait_action "running LazyFS..." $lfs_log
echo -e "17.${GREEN}LazyFS restarted${RESET}."

#Restart ZooKeeper 
echo -e "18.${YELLOW}Waiting for ZooKeeper to restart${RESET}..."
truncate -s 0 $zk_out
#strace -f -e trace=network -s 9000 -o /home/gsd/zookeeper/strace.log "$zk_dir$serverscript" start $zk_dir$config_file  #> $zk_out 2>&1
ZOO_LOG_DIR=/home/gsd/zookeeper/3/zk-log.out "$zk_dir$serverscript" start $zk_dir$config_file  > $zk_out 2>&1
zk_pid=$(lsof -t -i:$clientPort)
wait_action "STARTED" $zk_out
echo -e "19.${GREEN}ZooKeeper restarted${RESET}."

#cd $zk_dir
#java -cp zookeeper-3.4.8.jar:lib/log4j-1.2.16.jar:lib/slf4j-log4j12-1.6.1.jar:lib/slf4j-api-1.6.1.jar org.apache.zookeeper.server.SnapshotFormatter /home/gsd/zookeeper/3/data-m/version-2/snapshot.100000007
#cd $lfs_dir

#Connect client to faulty server
"$zk_dir$clientscript" -server 127.0.0.1:218${server_fault} <<EOF
get /a
get /b
get /c
EOF

#Inject fault
echo $fault > $faults_fifo
#Wait for fault to be injected
echo -e "\n20.${YELLOW}Waiting for fault to be received${RESET}..."
wait_action "received: VALID crash fault" $lfs_log 
echo -e "21.${GREEN}Fault injected${RESET}."

#Create znode /d
echo -e "22.${YELLOW}Waiting for creation of znode /d on server ${server_fault}${RESET}..."
"$zk_dir$clientscript" -server 127.0.0.1:218${server_fault} <<EOF >> "$zk_out_cli" 2>&1
create /d ddd
EOF
echo -e "23.${GREEN}Znode /d created${RESET}."

#Wait for LazyFS to crash
echo -e "24.${YELLOW}Waiting for LazyFS to crash${RESET}..."
wait_action "Killing LazyFS" $lfs_log
echo -e "25.${RED}LazyFS crashed${RESET}."

#Kill ZooKeeper process
kill -9 $zk_pid
echo -e "26.${RED}Killed ZooKeeper${RESET}."
pkill -TERM -P $zk_client_pid

#Copy LazyFS log
cp $lfs_log $sdir3
mv $sdir3/lazyfs.log $sdir3/lazyfs.log.2

#==================================After 2nd crash======================================
#Restart LazyFS
fusermount -uz "$data_dir"
truncate -s 0 "$lfs_log"
./mount-lazyfs.sh -c "$lfs_config" -m "$data_dir" -r "$data_root_dir" -s -f & #> /dev/null 2>&1 

#Wait for LazyFS to start
echo -e "27.${YELLOW}Waiting for LazyFS to restart${RESET}..."
wait_action "running LazyFS..." $lfs_log
echo -e "28.${GREEN}LazyFS restarted${RESET}."

#Restart ZooKeeper 
echo -e "29.${YELLOW}Waiting for ZooKeeper to restart${RESET}..."
truncate -s 0 $zk_out
"$zk_dir$serverscript" start $zk_dir$config_file > $zk_out 2>&1
zk_pid=$(lsof -t -i:$clientPort)
wait_action "STARTED" $zk_out
echo -e "30.${GREEN}ZooKeeper restarted${RESET}."

#Connect client to faulty server
"$zk_dir$clientscript" -server 127.0.0.1:218${server_fault} <<EOF
get /a
get /b
get /c
get /d
EOF

#Connect client to a correct server
"$zk_dir$clientscript" -server 127.0.0.1:218${connect_server} <<EOF
get /a
get /b
get /c
get /d
EOF

#cd $zk_dir
#java -cp zookeeper-3.4.8.jar:lib/log4j-1.2.16.jar:lib/slf4j-log4j12-1.6.1.jar:lib/slf4j-api-1.6.1.jar org.apache.zookeeper.server.SnapshotFormatter /home/gsd/zookeeper/3/data-m/version-2/snapshot.10000000a
#cd $lfs_dir

#Kill ZooKeeper processes
for i in "${servers[@]}"; do
    pid=$(lsof -t -i:218${i})
    kill -9 $pid > /dev/null 2>&1 
done

sleep 2

#Unmount LazyFS
fusermount3 -u "$data_dir" > /dev/null 2>&1 
echo -e "\n11.${GREEN}Unmounted LazyFS${RESET}."

#Copy LazyFS log
cp $lfs_log $sdir3
mv $sdir3/lazyfs.log $sdir3/lazyfs.log.3

#Record the end time and print elapsed time
end_time=$(date +%s)
elapsed_time=$((end_time - start_time))

echo ">> Execution time: $elapsed_time seconds" 