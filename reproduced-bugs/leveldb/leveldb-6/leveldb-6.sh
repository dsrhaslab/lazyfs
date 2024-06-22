#!/bin/bash

#===============================================================================
#   DESCRIPTION: This script tests bug #6 of LevelDB. 
#       
#      - Error           Corruption: checksum mismatch (before repair)
#      - Time            2 seconds
#
#        AUTHOR: Maria Ramos,
#      REVISION: 27 Mar 2024
#===============================================================================

DIR="$PWD"
. "$DIR/aux.sh"
. "$DIR/leveldb-6-vars.sh" #File to configure with paths important for the tests

fault="\n[[injection]]\ntype=\"torn-seq\"\nop=\"write\"\nfile=\"$data_root_dir/000003.log\"\noccurrence=1\npersist=[1,5]"

#===============================================================================

#Clean data dirs and LazyFS log
create_and_clean_directory $data_dir
create_and_clean_directory $data_root_dir
truncate -s 0 $lfs_log
truncate -s 0 $leveldb_out

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

#Start LevelDB
g++ "$DIR/leveldb-6-write.cpp" -o "$DIR/db_test" -lleveldb -lsnappy -lpthread 
"$DIR/db_test" $data_dir $leveldb_pairs > $leveldb_out 2>&1 

#Wait for LazyFS to crash
echo -e "3.${YELLOW}Wait for LazyFS to crash${RESET}."
wait_action "Killing LazyFS" $lfs_log
echo -e "4.${RED}LazyFS crashed${RESET}."

#==================================After crash======================================

#Restart LazyFS
fusermount -uz "$data_dir"
truncate -s 0 "$lfs_log"
sed -i '/^\[\[injection\]\]$/,/^\s*persist=\[1,5\]$/d' "$lfs_config"
./scripts/mount-lazyfs.sh -c "$lfs_config" -m "$data_dir" -r "$data_root_dir" > /dev/null 2>&1 

#Wait for LazyFS to start
echo -e "5.${YELLOW}Wait for LazyFS to restart${RESET}."
wait_action "running LazyFS..." $lfs_log
echo -e "6.${GREEN}LazyFS restarted${RESET}."

#Restart LevelDB
g++ "$DIR/leveldb-6-read.cpp" -o "$DIR/db_read" -lleveldb -lsnappy -lpthread 
"$DIR/db_read" $data_dir $leveldb_pairs > $leveldb_out 2>&1 

i=7
if grep "Corruption: checksum mismatch" "$leveldb_out"; then
    #Repair DB
    echo -e "$i.${YELLOW}Repairing LevelDB data${RESET}."
    g++ "$DIR/leveldb-6-repair.cpp" -o "$DIR/db_repair" -lleveldb -lsnappy -lpthread 
    "$DIR/db_repair" $data_dir > $leveldb_out 2>&1 

    i=$((i+1))

    #Read DB
    "$DIR/db_read" $data_dir $leveldb_pairs >> $leveldb_out 2>&1 

    # We expect to k3 to be corrupted
    if grep -q "k3 does not correspond to expectation" $leveldb_out; then 
        echo -e "$i.${GREEN}Corruption expected detected${RESET}:"
        echo -e "${RED}k3 does not correspond to expectation${RESET}!"
    else 
        echo -e "$i.${RED}Corruption expected not detected${RESET}!"
    fi

else 
    echo -e "$i.${YELLOW}No need to repair LevelDB data${RESET}."
fi 

i=$((i+1))

#Unmount LazyFS
scripts/umount-lazyfs.sh -m "$data_dir"  > /dev/null 2>&1 
echo -e "$i.${GREEN}Unmounted LazyFS${RESET}."

#Record the end time and print elapsed time
end_time=$(date +%s)
elapsed_time=$((end_time - start_time))

echo ">> Execution time: $elapsed_time seconds"

#Remove executables
rm "$DIR/db_test"
rm "$DIR/db_read"
rm "$DIR/db_repair"
