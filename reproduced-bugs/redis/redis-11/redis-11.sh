#!/bin/bash

#===============================================================================
#   DESCRIPTION: This script tests the bug #11 of Redis v7.0.4
#       
#      - Error           .
#      - Time            3 seconds
#
#        AUTHOR: Maria Ramos,
#       CREATED: 7 Apr 2024,
#      REVISION: 7 Apr 2024
#===============================================================================

DIR="$PWD"
. "$DIR/aux.sh"
. "$DIR/redis-11-vars.sh" #File to configure with paths important for the tests

fault="lazyfs::clear-cache" 

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
./scripts/mount-lazyfs.sh -c "$lfs_config" -m "$data_dir" -r "$data_root_dir" > /dev/null 2>&1 

#Wait for LazyFS to start
echo -e "1.${YELLOW}Wait for LazyFS to start${RESET}."
wait_action "running LazyFS..." $lfs_log
echo -e "2.${GREEN}LazyFS started${RESET}."

# Create ACL file
truncate -s 0  "$data_dir/$acl_file"

#Wait for Redis to start
echo -e "3.${YELLOW}Wait for Redis to start${RESET}."
"$redis_dir/redis-server" --dir $data_dir --aclfile "$data_dir/$acl_file" > $redis_out 2>&1 &
redis_pid=$!
wait_action "Ready to accept connections" $redis_out
echo -e "4.${GREEN}Redis started. Will execute workload${RESET}."

#Workload
echo "acl setuser joe on >test ~cached:* +get" | "$redis_dir/redis-cli" -h 127.0.0.1 -p 6379 > $redis_out_cli 
wait_action "OK" $redis_out_cli
truncate -s 0 $redis_out_cli
echo "acl save" | "$redis_dir/redis-cli" -h 127.0.0.1 -p 6379 > $redis_out_cli 
wait_action "OK" $redis_out_cli
echo -e "5.${GREEN}Redis ACL created and updated${RESET}."
echo ">> (Redis) Current users:" 
echo "acl users" | "$redis_dir/redis-cli" -h 127.0.0.1 -p 6379

#Shutdown Redis
echo "shutdown" | "$redis_dir/redis-cli" -h 127.0.0.1 -p 6379 > $redis_out_cli &&\

#Inject fault and until it is injected
echo $fault > $faults_fifo
wait_action "clear cache request submitted..." $lfs_log
echo -e "6.${GREEN}Fault of clear-cache injected${RESET}."

#Restart Redis
echo -e "7.${YELLOW}Wait for Redis to start${RESET}."
"$redis_dir/redis-server" --dir $data_dir --aclfile $acl_file > $redis_out 2>&1 &
redis_pid=$!
wait_action "Ready to accept connections" $redis_out
echo -e "8.${GREEN}Redis started${RESET}."

echo "acl users" | "$redis_dir/redis-cli" -h 127.0.0.1 -p 6379 > $redis_out_cli &&\

#Check result
if ! grep -q "joe" "$redis_out_cli"; then
    echo -e "${RED}User joe not found${RESET}."
fi

pkill -TERM -P $redis_pid /dev/null

#Unmount LazyFS
scripts/umount-lazyfs.sh -m "$data_dir"  > /dev/null 2>&1 
echo -e "9.${GREEN}Unmounted LazyFS${RESET}."

#Record the end time and print elapsed time
end_time=$(date +%s)
elapsed_time=$((end_time - start_time))

echo ">> Execution time: $elapsed_time seconds"