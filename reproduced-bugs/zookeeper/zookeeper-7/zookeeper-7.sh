#!/bin/bash

#===============================================================================
#   DESCRIPTION: This script tests bug #7 of ZooKeeper v3.4.8 and v3.7.1
#       
#      - Error           (v3.4.8) Unable to load database on disk
#                        (v3.7.1) No snapshot found, but there are log entries. Something is broken!
#      - Time            6 seconds
#
#        AUTHOR: Maria Ramos,
#      REVISION: 31 Mar 2024
#===============================================================================

DIR="$PWD"
. "$DIR/aux.sh"

#========================Generate data for each server==========================

servers=(1 2 3)
server_fault=3

for i in "${servers[@]}"
do 
    . "$DIR/zookeeper-7-vars.sh" $i #File to configure with paths important for the tests

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

. "$DIR/zookeeper-7-vars.sh" $server_fault

#Clean logs
truncate -s 0 $lfs_log
truncate -s 0 $zk_out

fault="lazyfs::crash::timing=before::op=fsync::from_rgx=version-2/log.[0-9]0000000[0-9]"
faults_fifo=$(grep 'fifo_path=' $lfs_config | sed 's/.*fifo_path="//; s/".*//')

#===============================================================================

#Record start time
start_time=$(date +%s)

#Mount LazyFS
cd $lfs_dir
./scripts/mount-lazyfs.sh -c "$lfs_config" -m "$data_dir" -r "$data_root_dir" > /dev/null 2>&1 

#Wait for LazyFS to start
echo -e "1.${YELLOW}Wait for LazyFS to start${RESET}."
wait_action "running LazyFS..." $lfs_log
echo -e "2.${GREEN}LazyFS started${RESET}."

#Inject fault and wait until it is injected
echo $fault > $faults_fifo
wait_action "received: VALID crash fault" $lfs_log
echo -e "3.${GREEN}Fault injected${RESET}."

#Start ZooKeeper server 
"$zk_dir$serverscript" start $zk_dir$config_file > $zk_out 2>&1
zk_pid=$(lsof -t -i:$clientPort)
wait_action "STARTED" $zk_out
echo -e "4.${GREEN}ZooKeeper started${RESET}."

#Start other ZoKeeper servers
for i in "${servers[@]}"
do 
    if (("$i" != $server_fault)); then
    . "$DIR/zookeeper-7-vars.sh" $i 

    "$zk_dir$serverscript" start "$zk_dir$config_file" > /dev/null 2>&1 
    echo -e "${GREEN}ZooKeeper server.$i started${RESET}."
    fi
done

. "$DIR/zookeeper-7-vars.sh" $server_fault

#Connect client to server 3
"$zk_dir$clientscript" -server 127.0.0.1:218${server_fault} > /dev/null 2>&1 &
zk_client_pid=$!

#Wait for LazyFS to crash
echo -e "5.${YELLOW}Wait for LazyFS to crash${RESET}."
wait_action "Killing LazyFS" $lfs_log
echo -e "6.${RED}LazyFS crashed${RESET}."

#Kill ZooKeeper process
kill -9 $zk_pid
echo -e "7.${RED}Killed ZooKeeper${RESET}."
pkill -TERM -P $zk_client_pid

#==================================After crash======================================

#Restart LazyFS
fusermount -uz "$data_dir"
truncate -s 0 "$lfs_log"
./scripts/mount-lazyfs.sh -c "$lfs_config" -m "$data_dir" -r "$data_root_dir" > /dev/null 2>&1 

#Wait for LazyFS to start
echo -e "8.${YELLOW}Wait for LazyFS to restart${RESET}."
wait_action "running LazyFS..." $lfs_log
echo -e "9.${GREEN}LazyFS restarted${RESET}."

#Restart ZooKeeper 
"$zk_dir$serverscript" start-foreground $zk_dir$config_file > $zk_out 2>&1 & 
pid=$!
wait $pid

#Check for errors
if grep -E -q "Unable to load database on disk|No snapshot found" $zk_out; then
    echo -e "10.${GREEN}Error expected found${RESET}:"
    grep -E "Unable to load database on disk|No snapshot found" $zk_out
else
    echo -e "10.${RED}Error expected not found${RESET}!"
fi

#Kill ZooKeeper processes
for i in "${servers[@]}"; do
    if (("$i" != $server_fault)); then
        pid=$(lsof -t -i:218${i})
        kill -9 $pid > /dev/null 2>&1 
    fi
done

#Unmount LazyFS
scripts/umount-lazyfs.sh -m "$data_dir"  > /dev/null 2>&1 
echo -e "11.${GREEN}Unmounted LazyFS${RESET}."

#Record the end time and print elapsed time
end_time=$(date +%s)
elapsed_time=$((end_time - start_time))

echo ">> Execution time: $elapsed_time seconds" 