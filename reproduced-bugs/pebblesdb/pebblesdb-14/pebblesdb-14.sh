#!/bin/bash

#===============================================================================
#   DESCRIPTION: This script tests bug #14 of PebblesDB 
#       
#      - Error           .
#      - Time            2 seconds
#
#        AUTHOR: Maria Ramos,
#      REVISION: 22 Mar 2024
#===============================================================================

DIR="$PWD"
. "$DIR/aux.sh"
. "$DIR/pebblesdb-14-vars.sh" #File to configure with paths important for the tests

fault="lazyfs::clear-cache"
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

#Execute workload
g++ "$DIR/pebblesdb-14-write.cpp" -o "$DIR/db_write" -lpebblesdb -lsnappy -lpthread 
"$DIR/db_write" $data_dir 98000 1 

#Wait until workload finishes
echo -e "3.${YELLOW}Wait for workload to finish${RESET}."
wait_action "lfs_release(path=$data_root_dir/000004.log)" $lfs_log
echo -e "4.${GREEN}Workload finished${RESET}."

#Inject fault and until it is injected
echo $fault > $faults_fifo
wait_action "clear cache request submitted..." $lfs_log
echo -e "5.${GREEN}Fault of clear-cache injected${RESET}."

#Check database 
g++ "$DIR/pebblesdb-14-read.cpp" -o "$DIR/db_read" -lpebblesdb -lsnappy -lpthread 
"$DIR/db_read" $data_dir 98001 > $pebblesdb_out 2>&1 

#Check result
if grep -q "There are 28190 pairs in the database from 98001 inserted" $pebblesdb_out; then 
    echo -e "6.${GREEN}Corruption expected detected${RESET}:"
        result=$(cat $pebblesdb_out)
    echo -e "${RED}$result${RESET}."
fi


#Unmount LazyFS
scripts/umount-lazyfs.sh -m "$data_dir"  > /dev/null 2>&1 
echo -e "7.${GREEN}Unmounted Lazyfs${RESET}."

#Record the end time and print elapsed time
end_time=$(date +%s)
elapsed_time=$((end_time - start_time))

echo ">> Execution time: $elapsed_time seconds"

#Remove executables
rm "$DIR/db_read"
rm "$DIR/db_write"
