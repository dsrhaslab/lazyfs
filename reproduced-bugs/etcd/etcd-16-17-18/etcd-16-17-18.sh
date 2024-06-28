#!/bin/bash

#===============================================================================
#   DESCRIPTION: This script tests bugs #16#17#18 of etcd v3.4.25
#   ARGUMENTS: This script receives the number of the fault as argument.
#
#      - Error            #16: bus error
#                         #17: invalid database
#                         #18: file size too small
#      - Time             #16: 2 seconds
#                         #17: 2 seconds
#                         #18: 2 seconds
#
#        AUTHOR: Maria Ramos,
#      REVISION: 21 Mar 2024
#===============================================================================

DIR="$PWD"

. "$DIR/aux.sh"
. "$DIR/etcd-16-17-18-vars.sh" #File to configure with paths important for the tests

fault="\n[[injection]]\ntype=\"torn-op\"\nfile=\"$data_root_dir/member/snap/db\"\noccurrence=1\n"
fault17="parts=2\npersist=[2]"
fault16="parts=2\npersist=[1]"
fault18="parts=4\npersist=[1]"

fault_selected="fault$1"
fault+=${!fault_selected}

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

#Start etcd
$etcd --data-dir "$data_dir" > "$etcd_out" 2>&1 &
etcd_pid=$! 
echo -e "3.${GREEN}etcd started${RESET}."

#Wait for LazyFS to crash
echo -e "4.${YELLOW}Wait for LazyFS to crash${RESET}."
wait_action "Killing LazyFS" $lfs_log
echo -e "5.${RED}LazyFS crashed${RESET}."

#Wait for etcd to crash
wait $etcd_pid
echo -e "6.${RED}etcd crashed${RESET}."

#==================================After crash======================================

#Start LazyFS again
fusermount -uz "$data_dir"
sed -i '/^\[\[injection\]\]$/,/^\s*persist=\[[0-9]\]$/d' "$lfs_config"
truncate -s 0 "$lfs_log"
./scripts/mount-lazyfs.sh -c "$lfs_config" -m "$data_dir" -r "$data_root_dir" > /dev/null 2>&1 

#Wait for LazyFS to start
echo -e "7.${YELLOW}Wait for LazyFS to restart${RESET}."
wait_action "running LazyFS..." $lfs_log
echo -e "8.${GREEN}LazyFS restarted${RESET}."

#Restart etcd and wait 
$etcd --data-dir "$data_dir" > "$etcd_out" 2>&1 &
etcd_pid=$!
wait $etcd_pid 
echo -e "9.${RED}etcd crashed${RESET}."

if [ "$1" == "16" ]; then
    if grep -q "bus error" "$etcd_out"; then
        echo -e "10.${GREEN}Expected error detected${RESET}:"
        grep "bus error" "$etcd_out"
    else 
        echo -e "10.${RED}Expected error not detected${RESET}!"
    fi
elif [ "$1" == "17" ]; then
    if grep -q "invalid database" "$etcd_out"; then
        echo -e "10.${GREEN}Expected error detected${RESET}:"
        grep "invalid database" "$etcd_out"
    else 
        echo -e "10.${RED}Expected error not detected${RESET}!"
    fi
elif [ "$1" == "18" ]; then
    if grep -q "file size too small" "$etcd_out"; then
        echo -e "10.${GREEN}Expected error detected${RESET}:"
        grep "file size too small" "$etcd_out"
    else 
        echo -e "10.${RED}Expected error not detected${RESET}!"
    fi
fi

#Unmount LazyFS
scripts/umount-lazyfs.sh -m "$data_dir"  > /dev/null 2>&1 
echo -e "11.${GREEN}Unmounted LazyFS${RESET}."

#Record the end time and print elapsed time
end_time=$(date +%s)
elapsed_time=$((end_time - start_time))

echo ">> Execution time: $elapsed_time seconds"