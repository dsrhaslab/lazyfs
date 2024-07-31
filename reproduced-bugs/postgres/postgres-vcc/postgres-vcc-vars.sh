#!/bin/bash

data_dir="/usr/local/pgsql/data"
data_root_dir="/test/test_postgres/postgres-r"

lfs_dir="/test/lazyfs/lazyfs"
lfs_log="/test/lazyfs.log"
lfs_config="/test/config.toml"

postgres_bin="/usr/local/pgsql/bin"
postgres_conf="$data_dir/postgresql.conf"
postgres_conf="$data_root_dir/postgresql.conf"
postgres_status="/test/test_postgres/postgres_status.txt"
postgres_out="/test/test_postgres/out.txt"
logfile="$data_dir/logfile"