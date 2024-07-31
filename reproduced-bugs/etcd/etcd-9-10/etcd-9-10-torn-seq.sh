#!/bin/bash

#===============================================================================
#   DESCRIPTION: This script tests bugs #9#10 of etcd v2.3.0
#                Injection of a torn-seq fault.
#       
#      - Error            walpb: crc mismatch
#      - Time             17 seconds
#
#        AUTHOR: Maria Ramos,
#      REVISION: 19 Mar 2024
#===============================================================================

DIR="$PWD"

. "$DIR/aux.sh"
. "$DIR/etcd-9-10-vars.sh" #File to configure with paths important for the tests

fault="\n[[injection]]\ntype=\"torn-seq\"\nfile=\"$data_root_dir/member/wal/0000000000000000-0000000000000000.wal\"\noccurrence=1\nop=\"write\"\npersist=[2]"

#===============================================================================

#Clean data dirs and LazyFS log
create_and_clean_directory $data_dir
create_and_clean_directory $data_root_dir
truncate -s 0 "$lfs_log"
truncate -s 0 "$etcd_out"

#Record start time
start_time=$(date +%s)

#Mount LazyFS
cd $lfs_dir
echo -e "$fault" >> "$lfs_config"
./scripts/mount-lazyfs.sh -c "$lfs_config" -m "$data_dir" -r "$data_root_dir" > /dev/null 2>&1 

#Wait for LazyFS to start
echo -e "1.${YELLOW}Wait for LazyFS to start${RESET}."
wait_action "running LazyFS..." $lfs_log
echo -e "2.${GREEN}LazyFS started${RESET}."

#Wait for etcd to start
$etcd --data-dir "$data_dir" > "$etcd_out" 2>&1 & 
etcd_pid=$!
echo -e "3.${YELLOW}Wait for etcd to start${RESET}."
wait_action "set the initial cluster" $etcd_out 
echo -e "4.${GREEN}etcd started${RESET}."

#Wait for LazyFS to crash
echo -e "5.${YELLOW}Wait for LazyFS to crash${RESET}."
wait_action "Killing LazyFS" $lfs_log
echo -e "6.${RED}LazyFS crashed${RESET}."

#Wait for etcd to crash
wait $etcd_pid
echo -e "7.${RED}etcd crashed${RESET}."

#==================================After crash======================================

#Start LazyFS again
fusermount -uz "$data_dir"
sed -i '/^\[\[injection\]\]$/,/^\s*persist=\[2\]$/d' "$lfs_config"
truncate -s 0 "$lfs_log"
./scripts/mount-lazyfs.sh -c "$lfs_config" -m "$data_dir" -r "$data_root_dir" > /dev/null 2>&1 

#Wait for LazyFS to start
echo -e "8.${YELLOW}Wait for LazyFS to restart${RESET}."
wait_action "running LazyFS..." $lfs_log
echo -e "9.${GREEN}LazyFS restarted${RESET}."

#Restart etcd and wait 
$etcd --data-dir "$data_dir" > "$etcd_out" 2>&1 &
etcd_pid=$!
wait $etcd_pid 
echo -e "10.${RED}etcd crashed${RESET}."

if grep -q "crc mismatch" "$etcd_out"; then
    echo -e "11.${GREEN}Error expected detected${RESET}:"
    grep "crc mismatch" $etcd_out
else 
    echo -e "11.${RED}Error expected not detected${RESET}!"
fi

#Unmount LazyFS
scripts/umount-lazyfs.sh -m "$data_dir"  > /dev/null 2>&1 
echo -e "12.${GREEN}Unmounted Lazyfs${RESET}."

#Record the end time and print elapsed time
end_time=$(date +%s)
elapsed_time=$((end_time - start_time))

echo ">> Execution time: $elapsed_time seconds"
