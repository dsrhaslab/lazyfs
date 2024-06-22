#!/bin/bash

#===============================================================================
#   DESCRIPTION: This script tests bug #19 of etcd v2.3.0.
#       
#      - Error           wal: snapshot not found
#      - Time            2 seconds 
#
#        AUTHOR: Maria Ramos,
#      REVISION: 21 Mar 2024
#===============================================================================

DIR="$PWD"
. "$DIR/aux.sh"
. "$DIR/etcd-19-vars.sh" #File to configure with paths important for the tests

fault="lazyfs::crash::timing=after::op=write::from_rgx=member/wal/[0-9]+-[0-9]+.wal"
faults_fifo=$(grep 'fifo_path=' $lfs_config | sed 's/.*fifo_path="//; s/".*//')

#===============================================================================

#Clean data dirs and LazyFS log
create_and_clean_directory $data_dir
create_and_clean_directory $data_root_dir
truncate -s 0 "$lfs_log"
truncate -s 0 "$etcd_out"
truncate -s 0 "$etcd_cli_out"

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

#Start etcd
$etcd --data-dir "$data_dir" > "$etcd_out" 2>&1 & 
etcd_pid=$!
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

grep "snapshot not found" "$etcd_out"
check_error "wal error" "$etcd_out"

#Unmount LazyFS
scripts/umount-lazyfs.sh -m "$data_dir"  > /dev/null 2>&1 
echo -e "11.${GREEN}Unmounted LazyFS${RESET}."

#Record the end time and print elapsed time
end_time=$(date +%s)
elapsed_time=$((end_time - start_time))

echo ">> Execution time: $elapsed_time seconds"
