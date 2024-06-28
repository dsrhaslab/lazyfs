#!/bin/bash

#===============================================================================
#  Installs LND                           
#===============================================================================

#Install Go
apt-get update
apt-get install -y golang-1.18-go
export GOPATH=/usr/lib/go-1.18
export PATH=$PATH:$GOPATH/bin

#Install LND
git clone https://github.com/lightningnetwork/lnd
cd lnd
git checkout v0.15.1-beta
make install

#Install expect
apt-get install -y expect