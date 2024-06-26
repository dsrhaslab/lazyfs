#!/bin/bash

#===============================================================================
#  Installs PostgreSQL         
#                      
#  Parameters:
#    - version: The version of PostgreSQL to install
#===============================================================================

version=$1

wget https://ftp.postgresql.org/pub/source/v$version/postgresql-$version.tar.gz -O "postgresql.tar.gz"
tar -xvf "postgresql.tar.gz"
mv "postgresql-$version" "postgresql"
cd postgresql
./configure --without-readline --without-zlib --without-icu
make world-bin
su
make install-world-bin
adduser postgres
mkdir -p /usr/local/pgsql/data
#su - postgres
#/usr/local/pgsql/bin/initdb -D /usr/local/pgsql/data
#/usr/local/pgsql/bin/pg_ctl -D /usr/local/pgsql/data -l logfile start
#/usr/local/pgsql/bin/createdb test
#/usr/local/pgsql/bin/psql test

