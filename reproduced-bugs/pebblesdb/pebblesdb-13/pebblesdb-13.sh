#!/bin/bash

#===============================================================================
#   DESCRIPTION: This script tests bug #13 of PebblesDB
#       
#      - Error           Corruption: no meta-nextfile entry in descriptor
#      - Time            2 seconds
#
#        AUTHOR: Maria Ramos,
#      REVISION: 26 Mar 2024
#===============================================================================

DIR="$PWD"
. "$DIR/aux.sh"
. "$DIR/pebblesdb-13-vars.sh" #File to configure with paths important for the tests

fault="lazyfs::crash::timing=after::op=write::from_rgx=MANIFEST-000002"
faults_fifo=$(grep 'fifo_path=' $lfs_config | sed 's/.*fifo_path="//; s/".*//')

#===============================================================================

#Clean data dirs and LazyFS log
create_and_clean_directory $data_dir
create_and_clean_directory $data_root_dir
truncate -s 0 $lfs_log
truncate -s 0 $pebblesdb_out

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

#Start PebblesDB
g++ "$DIR/pebblesdb-13-write.cpp" -o "$DIR/db_test" -lpebblesdb -lsnappy -lpthread 
"$DIR/db_test" $data_dir > $pebblesdb_out 2>&1 

#Wait for LazyFS to crash
echo -e "4.${YELLOW}Wait for LazyFS to crash${RESET}."
wait_action "Killing LazyFS" $lfs_log
echo -e "5.${RED}LazyFS crashed${RESET}."

#==================================After crash======================================

#Restart LazyFS
fusermount -uz "$data_dir"
truncate -s 0 "$lfs_log"
./scripts/mount-lazyfs.sh -c "$lfs_config" -m "$data_dir" -r "$data_root_dir" > /dev/null 2>&1 

#Wait for LazyFS to start
echo -e "6.${YELLOW}Wait for LazyFS to restart${RESET}."
wait_action "running LazyFS..." $lfs_log
echo -e "7.${GREEN}LazyFS restarted${RESET}."

#Restart PebblesDB
g++ "$DIR/pebblesdb-13-read.cpp" -o "$DIR/db_read" -lpebblesdb -lsnappy -lpthread 
"$DIR/db_read" $data_dir > $pebblesdb_out 2>&1 

if grep -q "Corruption" "$pebblesdb_out"; then
    echo -e "8.${GREEN}Error expected detected${RESET}:"
    grep "Corruption" $pebblesdb_out
else
    echo -e "8.${RED}Error not detected${RESET}."
fi

#Unmount LazyFS
scripts/umount-lazyfs.sh -m "$data_dir"  > /dev/null 2>&1 
echo -e "9.${GREEN}Unmounted LazyFS${RESET}."

#Record the end time and print elapsed time
end_time=$(date +%s)
elapsed_time=$((end_time - start_time))

echo ">> Execution time: $elapsed_time seconds"

#Remove executables
rm "$DIR/db_test"
rm "$DIR/db_read"
