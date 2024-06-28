#!/bin/bash

#===============================================================================
#   DESCRIPTION: This script tests bug #1 of PostgreSQL v12.11
#       
#      - Error          corrupted statistics file
#      - Time           11 seconds
#
#        AUTHOR: Maria Ramos,
#      REVISION: 2 Apr 2024
#===============================================================================

DIR="$PWD"
. "$DIR/aux.sh"
. "$DIR/postgres-1-vars.sh" #File to configure with paths important for the tests

fault="lazyfs::clear-cache"
faults_fifo=$(grep 'fifo_path=' $lfs_config | sed 's/.*fifo_path="//; s/".*//')

#===============================================================================

#Clean data dirs and LazyFS log
create_and_clean_directory $data_dir
create_and_clean_directory $data_root_dir
truncate -s 0 $lfs_log
truncate -s 0 $postgres_out

#rsync -av /var/lib/postgresql $data_root_dir
#rsync -av /var/lib/postgresql $data_dir

#Record start time
start_time=$(date +%s)

#Mount LazyFS
cd $lfs_dir
./scripts/mount-lazyfs.sh -c "$lfs_config" -m "$data_dir" -r "$data_root_dir" > /dev/null 2>&1 

#Wait for LazyFS to start
echo -e "1.${YELLOW}Wait for LazyFS to start${RESET}."
wait_action "running LazyFS..." $lfs_log
echo -e "2.${GREEN}LazyFS started${RESET}."

#Start PostgreSQL
echo -e "3.${YELLOW}Start PostgreSQL${RESET}."
chown postgres $data_dir &&\
su - postgres -c "/usr/local/pgsql/bin/initdb -D $data_dir" > $postgres_out 2>&1
wait_action "Success. You can now start the database server" $postgres_out
su - postgres -c "/usr/local/pgsql/bin/pg_ctl -D /usr/local/pgsql/data -l $logfile start" > $postgres_out 2>&1
wait_action "server started" $postgres_out
echo -e "4.${GREEN}PostgreSQL started${RESET}."

#Execute queries
echo -e "5.${GREEN}Execute queries${RESET}."
su - postgres -c "create table t1(a int);" > /dev/null 2>&1  
su - postgres -c "insert into t1 values(1);" > /dev/null 2>&1  
su - postgres -c "insert into t1 values(2);" > /dev/null 2>&1  
su - postgres -c "insert into t1 values(3);" > /dev/null 2>&1  

#Stop PostgreSQL
echo -e "6.${YELLOW}Stopping PostgreSQL${RESET}."
su - postgres -c "/usr/local/pgsql/bin/pg_ctl -D $data_dir -l $logfile stop" > $postgres_out 2>&1
wait_action "server stopped" $postgres_out
echo -e "7.${green}PostgreSQL stopped${RESET}."
truncate -s 0 $postgres_out

#Inject fault
echo "$fault" > "$faults_fifo"
wait_action "cache" $lfs_log
echo -e "8.${RED}Injected fault${RESET}."

#Start PostgreSQL and check log
echo -e "9.${YELLOW}Start PostgreSQL${RESET}."
su - postgres -c "/usr/local/pgsql/bin/pg_ctl -D /usr/local/pgsql/data -l $logfile start" > $postgres_out 2>&1
wait_action "server started" $postgres_out
echo -e "10.${GREEN}PostgreSQL started${RESET}."

if grep -q "corrupted statistics file" $logfile; then
    echo -e "11.${GREEN}Error expected detected${RESET}:"
    grep "corrupted statistics file" $logfile
else
    echo -e "11.${RED}Error not detected${RESET}."
fi

#Stop PostgreSQL
su - postgres -c "/usr/local/pgsql/bin/pg_ctl -D $data_dir -l $logfile stop" > $postgres_out 2>&1

#Unmount LazyFS
scripts/umount-lazyfs.sh -m "$data_dir"  > /dev/null 2>&1 
echo -e "11.${GREEN}Unmounted Lazyfs${RESET}."

#Record end time
end_time=$(date +%s)
elapsed_time=$((end_time - start_time))

echo "Execution time: $elapsed_time seconds"