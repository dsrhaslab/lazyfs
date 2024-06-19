#!/bin/bash

#===============================================================================
#   DESCRIPTION: This script tests the crach consistency mechanisms of Redis v7.2.3
#       
#                        fault 1: Truncating the AOF
#      - Error           fault 2: Bad file format
#                        fault 1: 3 seconds 
#      - Time            fault 2: 3 seconds 
#
#        AUTHOR: Maria Ramos,
#       CREATED: 4 Apr 2024,
#      REVISION: 30 Apr 2024
#===============================================================================

fault_nr=$1

DIR="$PWD"
. "$DIR/aux.sh"
. "$DIR/redis-vcc-vars.sh" #File to configure with paths important for the tests

fault="\n[[injection]]\ntype=\"torn-seq\"\nop=\"write\"\nfile=\"$data_root_dir/appendonlydir/appendonly.aof.1.incr.aof\"\noccurrence=1\npersist=[$fault_nr]" 

faults_fifo=$(grep 'fifo_path=' $lfs_config | sed 's/.*fifo_path="//; s/".*//')

#===============================================================================

#Clean data dirs and LazyFS log
create_and_clean_directory $data_dir
create_and_clean_directory $data_root_dir
truncate -s 0 "$lfs_log"

#Record start time
start_time=$(date +%s)

#Mount LazyFS
cd $lfs_dir
echo -e "$fault" >> $lfs_config
./scripts/mount-lazyfs.sh -c "$lfs_config" -m "$data_dir" -r "$data_root_dir" > /dev/null 2>&1 

#Wait for LazyFS to start
echo -e "1.${YELLOW}Wait for LazyFS to start${RESET}."
wait_action "running LazyFS..." $lfs_log
echo -e "2.${GREEN}LazyFS started${RESET}."

#Wait for Redis to start
echo -e "3.${YELLOW}Wait for Redis to start${RESET}."
"$redis_dir/redis-server" $redis_conf > $redis_out 2>&1 &
wait_action "Ready to accept connections tcp" $redis_out
echo -e "4.${GREEN}Redis started. Will execute workload${RESET}."

#Workload
. "$DIR/redis-vcc-wk.sh" > /dev/null 2>&1 

#Wait for LazyFS to crash
echo -e "5.${YELLOW}Wait for LazyFS to crash${RESET}."
wait_action "Killing LazyFS" $lfs_log
echo -e "6.${RED}LazyFS crashed${RESET}."

#==================================After crash======================================

#Start LazyFS again
fusermount -uz "$data_dir"
sed -i '/^\[\[injection\]\]$/,/^\s*persist=\[([0-9],?)+\]$/d' $lfs_config
truncate -s 0 "$lfs_log"
./scripts/mount-lazyfs.sh -c "$lfs_config" -m "$data_dir" -r "$data_root_dir" > /dev/null 2>&1 

#Wait for LazyFS to start
echo -e "7.${YELLOW}Wait for LazyFS to restart${RESET}."
wait_action "running LazyFS..." $lfs_log
echo -e "8.${GREEN}LazyFS restarted${RESET}."

#Start Redis
echo -e "9.${YELLOW}Starting Redis${RESET}."
truncate -s 0 $redis_out
"$redis_dir/redis-server" $redis_conf > $redis_out 2>&1 &
redis_pid=$!

#Check error
if [ "$fault_nr" -eq 2 ]; then
    wait_action "Bad file format" $redis_out
    echo -e "10.${GREEN}Error expected detected${RESET}:"
    sed -n 14p $redis_out

    #Repair
    echo -e "11.${YELLOW}Repairing Redis data${RESET}"
    echo "yes" | "$redis_dir/redis-check-aof" --fix "$data_dir/appendonlydir/appendonly.aof.1.incr.aof"  > $redis_out &

    #Wair Redis repairing
    wait_action "Successfully truncated AOF" $redis_out
    echo -e "12.${GREEN}Successfully repaired Redis${RESET}."

    #"$redis_dir/redis-server" $redis_conf > $redis_out 2>&1 &
    #redis_pid=$! 

    #Wait for Redis to start
    #wait_action "Ready to accept connections tcp" $redis_out
    #echo -e "11.${GREEN}Redis started${RESET}."
    #echo "GET data" | "$redisdir/redis-cli" -h 127.0.0.1 -p 6379
else 
    wait_action "Truncating the AOF" $redis_out
    echo -e "\n>> Redis log:"
    sed -n 15,20p $redis_out
    echo -e "\n10.${GREEN}Redis repaired itself${RESET}."
    wait_action "Ready to accept connections tcp" $redis_out
    echo -e "11.${GREEN}Redis started${RESET}."
fi 

pkill -TERM -P $redis_pid > /dev/null 2>&1

#Unmount LazyFS
scripts/umount-lazyfs.sh -m "$data_dir"  > /dev/null 2>&1 
echo -e "${GREEN}Unmounted LazyFS${RESET}"

#Record the end time and print elapsed time
end_time=$(date +%s)
elapsed_time=$((end_time - start_time))

echo ">> Execution time: $elapsed_time seconds"

