#!/bin/bash

# --------------------------------------------

declare -a workloads_to_run=("filemicro_createfiles.f")
workloads_tmp_dir="benchmark-direct-io-0"
# fs="lazyfs"
fs="passthrough"
mount_folder="/mnt/test-fs/$fs"
root_folder="/mnt/test-fs/$fs-root"
page_size=""

# --------------------------------------------

echo -e "process starting...\n"

mkdir -p $workloads_tmp_dir
cd $workloads_tmp_dir

for wload in "${workloads_to_run[@]}"
do
    # ---------------------------------------------------
    echo -e "[$fs:$page_size:workload:$wload] setting up results folder...\n"
    if [ ! -d $wload ]
    then
        echo -e "[$fs:$page_size:workload:$wload] results folder does not exist, creating...\n"
        cd .. > /dev/null
        ./results.sh -w $wload -o $workloads_tmp_dir
        cd - > /dev/null
    fi
    # ---------------------------------------------------
    for run_id in $(seq 1 3)
    do
        sudo rm -rf $mount_folder/* $root_folder/*
        echo -e "[$fs:$page_size:workload:$wload:#$run_id] preparing run $run_id...\n"
        # ---------------------------------------------------
        echo -e "[$fs:$page_size:workload:$wload:#$run_id] mounting filesystem...\n"
        if [ "$fs" = "passthrough" ]
        then
            cd /home/gsd/lazyfs-project/tests/util/fuse
            ./run-passthrough.sh
            cd - > /dev/null
        else
            cd /home/gsd/lazyfs-project/lazyfs
            ./scripts/mount-lazyfs.sh -c config/default.toml -m $mount_folder -r $root_folder
            sleep 5
            echo "LazyFS mounted..."
            echo "[$fs:$page_size:workload:$wload:#$run_id] running LazyFS..."
            cd -
        fi
        # ---------------------------------------------------
        echo -e "[$fs:$page_size:workload:$wload:#$run_id] creating workload folders...\n"
        cd .. > /dev/null
        ./setup-testbed.sh -d $mount_folder/fb-workload -f workloads/$wload 
        cd - > /dev/null
        # ---------------------------------------------------
        echo -e "\n[$fs:$page_size:workload:$wload:#$run_id] running...\n"
        if [ "$fs" = "passthrough" ]
        then
            echo -e "\n# $fs run number $run_id\n" >> $wload/$fs*
            sudo filebench -f ../workloads/$wload >> $wload/$fs*
        else
            echo -e "\n# $fs run number $run_id\n" >> $wload/$fs*$page_size**
            sudo filebench -f ../workloads/$wload >> $wload/$fs*$page_size**
        fi
        # ---------------------------------------------------
        echo -e "[$fs:$page_size:workload:$wload:#$run_id] umounting filesystem...\n"
        fusermount -uz $mount_folder
        echo -e "[$fs:$page_size:workload:$wload:#$run_id] waiting 15s for next round...\n"
        sleep 15
        # ---------------------------------------------------
    done
    echo -e "[$fs:$page_size:workload:$wload] sleeping 20s for next workload...\n"
    sleep 20
done

echo -e "process finished...\n"

# --------------------------------------------