#!/bin/bash

#===============================================================================
#   DESCRIPTION: This script tests the bug #12 of Lightning Network Daemon (lnd) v0.15.1-beta
#       
#      - Error          could not load global options: unable to decode macaroon: empty macaroon data
#      - Time           12 seconds 
#
#        AUTHOR: Maria Ramos,
#       CREATED: 7 Apr 2024,
#      REVISION: 7 Apr 2024
#===============================================================================
export GOPATH=/usr/lib/go-1.18
export PATH=$PATH:$GOPATH/bin

#===============================================================================
DIR="$PWD"
chmod +x "$DIR/lnd-12-wallet.expect"
. "$DIR/aux.sh"
. "$DIR/lnd-12-vars.sh" #File to configure with paths important for the tests

fault="lazyfs::clear-cache" 

faults_fifo=$(grep 'fifo_path=' $lfs_config | sed 's/.*fifo_path="//; s/".*//')

#===============================================================================

#Clean data dirs and LazyFS log
create_and_clean_directory $data_dir
create_and_clean_directory $data_root_dir
truncate -s 0 "$lfs_log"
truncate -s 0  "$lnd_out"

#Record start time
start_time=$(date +%s)

#Mount LazyFS
cd $lfs_dir
./scripts/mount-lazyfs.sh -c "$lfs_config" -m "$data_dir" -r "$data_root_dir" > /dev/null 2>&1 

#Wait for LazyFS to start
echo -e "1.${YELLOW}Wait for LazyFS to start${RESET}."
wait_action "running LazyFS..." $lfs_log
echo -e "2.${GREEN}LazyFS started${RESET}."

#Wait for LND to start
cd $lnd_dir
lnd --datadir $data_dir --bitcoin.active --bitcoin.testnet --debuglevel=debug --bitcoin.node=neutrino --neutrino.connect=faucet.lightning.community > $lnd_out 2>&1 &
lnd_pid=$!
echo -e "3.${YELLOW}Wait for LND to start${RESET}."
wait_action "Broadcaster now active" $lnd_out
echo -e "4.${GREEN}LND started${RESET}."

#Create wallet
echo -e "5.${YELLOW}Creating wallet${RESET}."
/usr/bin/expect -f "../lnd-12-wallet.expect" > $lnd_out_cli 
wait_action "!!!YOU MUST WRITE DOWN THIS SEED TO BE ABLE TO RESTORE THE WALLET!!!" $lnd_out_cli 
echo -e "6.${GREEN}Wallet created${RESET}."

#Stop LND 
echo -e "7.${YELLOW}Wait for LND to stop${RESET}."
kill -SIGINT $lnd_pid 
wait_action "Shutdown complete" $lnd_out
echo -e "> LND server: "
tail -11 $lnd_out
echo -e "8.${GREEN}LND stopped${RESET}."
truncate -s 0  "$lnd_out"

#Inject fault and until it is injected
echo $fault > $faults_fifo
wait_action "clear cache request submitted..." $lfs_log
echo -e "9.${GREEN}Fault of clear-cache injected${RESET}."

#Restart LND
lnd --datadir $data_dir --bitcoin.active --bitcoin.testnet --debuglevel=debug --bitcoin.node=neutrino --neutrino.connect=faucet.lightning.community > $lnd_out 2>&1 &
lnd_pid=$!
echo -e "10.${YELLOW}Wait for LND to restart${RESET}."
wait_action "Broadcaster now active" $lnd_out
echo -e "11.${GREEN}LND restarted${RESET}."

#Check wallet
lncli --network testnet --macaroonpath $data_dir/chain/bitcoin/testnet/admin.macaroon walletbalance > $lnd_out_cli 2>&1
wait_action "empty macaroon data" $lnd_out_cli
echo -e "> LND client: "
cat $lnd_out_cli
echo -e "${GREEN}Error expected detected${RESET}!"

#Stop LND
kill -SIGINT $lnd_pid > /dev/null 2>&1

#Unmount LazyFS
scripts/umount-lazyfs.sh -m "$data_dir"  > /dev/null 2>&1 
echo -e "12.${GREEN}Unmounted LazyFS${RESET}."

#Record the end time and print elapsed time
end_time=$(date +%s)
elapsed_time=$((end_time - start_time))

echo ">> Execution time: $elapsed_time seconds"