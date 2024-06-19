#!/bin/bash

#===============================================================================
#   DESCRIPTION: This script tests bug #15 of PebblesDB
#       
#      - Error           Corruption: checksum mismatch
#      - Time            2 seconds
#
#        AUTHOR: Maria Ramos,
#       CREATED: 27 Mar 2024,
#      REVISION: 27 Mar 2024
#===============================================================================

DIR="$PWD"
. "$DIR/aux.sh"
. "$DIR/pebblesdb-15-vars.sh" #File to configure with paths important for the tests

fault="\n[[injection]]\ntype=\"torn-op\"\nfile=\"$data_root_dir/000003.log\"\noccurrence=4\nparts=3\npersist=[1,3]"

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
echo -e "$fault" >> "$lfs_config"
./scripts/mount-lazyfs.sh -c "$lfs_config" -m "$data_dir" -r "$data_root_dir" > /dev/null 2>&1 

#Wait for LazyFS to start
echo -e "1.${YELLOW}Wait for LazyFS to start${RESET}."
wait_action "running LazyFS..." $lfs_log
echo -e "2.${GREEN}LazyFS started${RESET}."

#Start pebblesdb
g++ "$DIR/pebblesdb-15-write.cpp" -o "$DIR/db_test" -lpebblesdb -lsnappy -lpthread 
"$DIR/db_test" $data_dir $pebblesdb_pairs > $pebblesdb_out 2>&1 

#Wait for LazyFS to crash
echo -e "3.${YELLOW}Wait for LazyFS to crash${RESET}."
wait_action "Killing LazyFS" $lfs_log
echo -e "4.${RED}LazyFS crashed${RESET}."

#==================================After crash======================================

#Restart LazyFS
fusermount -uz "$data_dir"
truncate -s 0 "$lfs_log"
sed -i '/^\[\[injection\]\]$/,/^\s*persist=\[([0-9],?)+\]$/d' "$lfs_config"
./scripts/mount-lazyfs.sh -c "$lfs_config" -m "$data_dir" -r "$data_root_dir" > /dev/null 2>&1 

#Wait for LazyFS to start
echo -e "5.${YELLOW}Wait for LazyFS to restart${RESET}."
wait_action "running LazyFS..." $lfs_log
echo -e "6.${GREEN}LazyFS restarted${RESET}."

#Restart PebblesDB
g++ "$DIR/pebblesdb-15-read.cpp" -o "$DIR/db_read" -lpebblesdb -lsnappy -lpthread 
"$DIR/db_read" $data_dir $pebblesdb_pairs > $pebblesdb_out 2>&1 

if grep -q "Corruption: checksum mismatch" "$pebblesdb_out"; then
    #Repair DB
    echo -e "7.${YELLOW}Repairing PebblesDB data${RESET}."
    g++ "$DIR/pebblesdb-15-repair.cpp" -o "$DIR/db_repair" -lpebblesdb -lsnappy -lpthread 
    "$DIR/db_repair" $data_dir > $pebblesdb_out 2>&1 

    #Read DB
    "$DIR/db_read" $data_dir $pebblesdb_pairs >> $pebblesdb_out 2>&1 

    # We expect to k3 to be corrupted
    check_error "k3 does not correspond to expectation" $pebblesdb_out

else 
    echo -e "7.${YELLOW}No need to repair pebblesdb data${RESET}."
fi 

#Unmount LazyFS
scripts/umount-lazyfs.sh -m "$data_dir"  > /dev/null 2>&1 
echo -e "8.${GREEN}Unmounted LazyFS${RESET}."

#Record the end time and print elapsed time
end_time=$(date +%s)
elapsed_time=$((end_time - start_time))

echo ">> Execution time: $elapsed_time seconds"

#Remove executables
rm "$DIR/db_test"
rm "$DIR/db_read"
rm "$DIR/db_repair"
