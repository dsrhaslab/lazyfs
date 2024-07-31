#!/bin/bash

#==================================================================================================
#   DESCRIPTION: This script tests the crach consistency mechanisms of PostgreSQL v15.2 
#   ARGUMENTS: This script requires the following arguments:
#           - persistw: write to persist
#           - transactions: number of transactions for pgbench
#           - scale: scale factor for pgbench
#           - seed: random seed for pgbench
#       
#      - Error          heap tuple .. from table .. lacks matching index tuple within index ..
#      - Time           84 seconds
#
#        AUTHOR: Maria Ramos,
#      REVISION: 1 Apr 2024
#==================================================================================================

#Arguments
persistw=$1
transactions=$2
scale=$3
seed=$4

DIR="$PWD"
. "$DIR/aux.sh"
. "$DIR/postgres-vcc-vars.sh" #File to configure with paths important for the tests

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
echo -e "3.${YELLOW}Starting PostgreSQL${RESET}."
chown postgres $data_dir &&\
su - postgres -c "$postgres_bin/initdb -D $data_dir" > $postgres_out 2>&1
wait_action "Success. You can now start the database server" $postgres_out
sed -i 's/#full_page_writes = on/full_page_writes = off/g' "$postgres_conf"
su - postgres -c "$postgres_bin/pg_ctl -D $data_dir -l $logfile start" > $postgres_out 2>&1
wait_action "server started" $postgres_out
#su - postgres -c "full_page_writes=off"
echo -e "4.${GREEN}PostgreSQL started${RESET}."

#Import data 
echo -e "5.${YELLOW}Importing data${RESET}."
echo -e "> pgbench: "

su - postgres -c "$postgres_bin/pgbench -q -i -s $scale"

su - postgres -c "$postgres_bin/pgbench -c 5 -T $transactions --random-seed=$seed" #> /dev/null 2>&1 -c 5

su - postgres -c "$postgres_bin/psql -c 'SELECT current_database();'"

#su - postgres -c "$postgres_bin/pgbench -c $clients -t $transactions --random-seed=$seed -n" #> /dev/null 2>&1 
#su - postgres -c "$postgres_bin/pgbench -c 1 -t 20000 --random-seed=2 -n" #> /dev/null 2>&1 

output=$(su - postgres -c "$postgres_bin/psql -c \"SELECT pg_relation_filepath('pgbench_accounts');\"")
db_file=$(echo "$output" | awk '/pg_relation_filepath/ {getline; getline; print $1}')

echo "lazyfs::display-cache-usage" > $faults_fifo

#Terminate LazyFS and PostgreSQL (necessary to inject the following fault)
echo -e "6.${YELLOW}Stopping PostgreSQL${RESET}."
su - postgres -c "$postgres_bin/pg_ctl -D $data_dir -l $logfile stop" > $postgres_out 2>&1
wait_action "server stopped" $postgres_out
echo -e "7.${GREEN}PostgreSQL stopped${RESET}."
truncate -s 0 $

echo -e "8.${YELLOW}Stopping LazyFS${RESET}."
./scripts/umount-lazyfs.sh -m "$data_dir" > /dev/null 2>&1 
wait_action "stopping LazyFS" $lfs_log
echo -e "9.${GREEN}LazyFS stopped${RESET}."

#Inject fault
fault="\n[[injection]]\ntype=\"torn-op\"\nfile=\"$data_root_dir/$db_file\"\noccurrence=$persistw\nparts=2\npersist=[2]"
echo -e "$fault" >> "$lfs_config"

#Restart LazyFS 
truncate -s 0 $lfs_log
echo -e "10.${YELLOW}Wait for LazyFS to start${RESET}."
./scripts/mount-lazyfs.sh -c "$lfs_config" -m "$data_dir" -r "$data_root_dir" > /dev/null 2>&1 
wait_action "running LazyFS..." $lfs_log
echo -e "11.${GREEN}LazyFS started${RESET}."

# Restart PostgreSQL and force checkpoint
echo -e "12.${YELLOW}Restarting PostgreSQL${RESET}."
su - postgres -c "$postgres_bin/pg_ctl -D $data_dir -l $logfile start" > $postgres_out 2>&1
wait_action "server started" $postgres_out
echo -e "13.${GREEN}PostgreSQL restarted${RESET}."

#Force checkpoint
#su - postgres -c "$postgres_bin/psql -c 'CHECKPOINT;'"
su - postgres -c "$postgres_bin/psql -c 'SELECT * FROM pgbench_accounts;'" > /dev/null 2>&1

#Wait for LazyFS to crash
echo -e "14.${YELLOW}Wait for LazyFS to crash${RESET}."
wait_action "Killing LazyFS" $lfs_log
echo -e "15.${RED}LazyFS crashed${RESET}."

#Stop PostgreSQL
postgres_pid=$(ps aux | grep "/usr/local/pgsql/bin/postgres -D" | awk '{print $2}' | head -n 1)
kill -9 $postgres_pid > /dev/null 2>&1

#Restart LazyFS
fusermount -uz "$data_dir"
truncate -s 0 "$lfs_log"
truncate -s 0 "$postgres_out"
sed -i '/^\[\[injection\]\]$/,/^\s*persist=\[([0-9],?)+\]$/d' $lfs_config
./scripts/mount-lazyfs.sh -c "$lfs_config" -m "$data_dir" -r "$data_root_dir" > /dev/null 2>&1 

#Wait for LazyFS to start
echo -e "16.${YELLOW}Wait for LazyFS to restart${RESET}."
wait_action "running LazyFS..." $lfs_log
echo -e "17.${GREEN}LazyFS restarted${RESET}."

#Restart PostgreSQL
echo -e "18.${YELLOW}Restarting PostgreSQL${RESET}."
su - postgres -c "$postgres_bin/pg_ctl -D $data_dir -l $logfile start" > $postgres_out 2>&1
wait_action "server started" $postgres_out
echo -e "19.${GREEN}PostgreSQL restarted${RESET}."

#amcheck
echo -e "20.${YELLOW}Using amcheck${RESET}."
su - postgres -c "$postgres_bin/psql -c \"CREATE EXTENSION amcheck; SELECT bt_index_parent_check('pgbench_accounts_pkey', TRUE,TRUE);\"" > $postgres_out 2>&1 

#Check error
if grep -q "lacks matching index tuple" $postgres_out; then
    echo -e "21.${GREEN}Error expected detected${RESET}:"
    grep "lacks matching index tuple" $postgres_out
else
    echo -e "21.${RED}Error not detected${RESET}."
fi

#Terminate PostgreSQL
echo -e "22.${YELLOW}Stopping PostgreSQL${RESET}."
su - postgres -c "$postgres_bin/pg_ctl -D $data_dir -l $logfile stop" > $postgres_out 2>&1
wait_action "server stopped" $postgres_out
echo -e "23.${GREEN}PostgreSQL stopped${RESET}."

#Unmount LazyFS
scripts/umount-lazyfs.sh -m "$data_dir"  > /dev/null 2>&1 
echo -e "24.${GREEN}Unmounted Lazyfs${RESET}."

#Record end time
end_time=$(date +%s)
elapsed_time=$((end_time - start_time))

echo "Execution time: $elapsed_time seconds"