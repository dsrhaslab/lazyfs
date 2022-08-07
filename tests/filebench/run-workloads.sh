#!/bin/bash

#--------------------------------------------------#
# Main required variables

TEST_VAR_WORKLOADS_FOLDER=workloads/clear-caches
TEST_VAR_OUTPUT_RESULTS_FOLDER=outputs

TEST_VAR_TOTAL_RUNTIME_EACH_WORKLOAD=30 # seconds
TEST_VAR_REPEAT_WORKLOADS=2 # number of times to re-run workloads
TEST_VAR_FOR_FILESYSTEMS=("fuse.lazyfs-4096" "fuse.passthrough" "fuse.lazyfs-32768")
# TEST_VAR_FOR_FILESYSTEMS=("fuse.lazyfs-4096") # options: 'fuse.passthrough' and 'fuse.lazyfs-pagesize'
# TEST_VAR_FOR_FILESYSTEMS=("fuse.lazyfs-4096" "fuse.passthrough" "fuse.lazyfs-32768")

#--------------------------------------------------#
# Test variables

# LazyFS
LAZYFS_MOUNT_DIR=/tmp/lazyfs.fb.mnt
LAZYFS_ROOT_DIR=/tmp/lazyfs.fb.root
LAZYFS_FOLDER=/home/gsd/lfs.repos/lfs.jepsen/lazyfs

# FUSE passthrough.c example
PASST_MOUNT_DIR=/tmp/passt.fb.mnt
PASST_ROOT_DIR=/tmp/passt.fb.root
PASST_BIN=/home/gsd/other/libfuse/example/passthrough

TEST_VAR_SLEEP_AFTER_RUN=15 # seconds
TEST_VAR_SLEEP_AFTER_CHANGE_FS=15 # seconds
TEST_VAR_SLEEP_AFTER_WORKLOAD_TYPE=15 # seconds

#--------------------------------------------------#
# Other less important variables

# Bash colors
RED_COLOR='\e[31m'
GREEN_COLOR='\e[32m'
NO_COLOR='\e[0m'

#--------------------------------------------------#
# Utilities

log_time () {
	
    # $1 = context
	# $2 = message
	
    echo -e $(date "+%Y-%m-%d %H:%M:%S")": #${GREEN_COLOR}$1${NO_COLOR} - $2"
}

horizontal_line () {

	printf '%*s\n' "${COLUMNS:-$(tput cols)}" '' | tr ' ' -
}

spawn_dstat_process () {

    # $1 = log id
    # $2 = dstat process id
    # $3 = results direcotry
    # $4 = workload name
    # $5 = filesystem

    touch $3/run-$4-$5-fb.dstat.csv

    log_time $1 "spawning dstat process..."

    screen -S $2 -d -m dstat -tcdmg --noheaders --output $3/run-$4-$5-fb.dstat.csv
}

join_dstat_process () {

    # $1 = log id
    # $2 = dstat process id

    log_time $1 "joining dstat process..."

    {
        screen -S dstat1 -X stuff '^C'
        screen -wipe
    } &> /dev/null
}

mount_lazyfs () {
	
    # $1 = context
	# $2 = lazyfs page size in bytes
    
    log_time $1 "Mounting LazyFS (mnt=$LAZYFS_MOUNT_DIR,root=$LAZYFS_ROOT_DIR)..."

	mkdir -p $LAZYFS_MOUNT_DIR $LAZYFS_ROOT_DIR
	sudo rm -rf $LAZYFS_MOUNT_DIR/* $LAZYFS_ROOT_DIR/*

    OUTPUT_LAZYFS_CONFIG_FILE="/tmp/lfs.fb.$1.$2.config.toml"
    OUTPUT_LAZYFS_FIFO="/tmp/lfs.fb.$1.$2.fifo"
    CONFIG_NUMBER_PAGES=0

    # Specifying a total of 5gb cache
    # Everything fits in cache

    if test $2 -eq 4096; then
        CONFIG_NUMBER_PAGES=1310719
    elif test $2 -eq 32768; then
        CONFIG_NUMBER_PAGES=163839
    fi

cat > $OUTPUT_LAZYFS_CONFIG_FILE <<- EOM
[faults]
fifo_path="$OUTPUT_LAZYFS_FIFO"
[cache.manual]
io_block_size=$2
page_size=$2
no_pages=$CONFIG_NUMBER_PAGES
EOM

	cd $LAZYFS_FOLDER > /dev/null
	{
		./scripts/mount-lazyfs.sh -c $OUTPUT_LAZYFS_CONFIG_FILE -m $LAZYFS_MOUNT_DIR -r $LAZYFS_ROOT_DIR
	} &> /dev/null
	cd - > /dev/null
	(set -x; sleep 15s)

    check_filesystem $LAZYFS_MOUNT_DIR
    
    return $?
}

umount_lazyfs () {
	
	log_time $1 "Umounting LazyFS (mnt=$LAZYFS_MOUNT_DIR)..."

	cd $LAZYFS_FOLDER > /dev/null
	{
		./scripts/umount-lazyfs.sh -m $LAZYFS_MOUNT_DIR
	} &> /dev/null
	(set -x; sleep 5s)
	check_filesystem $LAZYFS_MOUNT_DIR
	cd - > /dev/null

    sudo rm -rf /tmp/lfs.fb*config.toml
    sudo rm -rf /tmp/lfs.fb*fifo
    sudo rm -r $LAZYFS_MOUNT_DIR $LAZYFS_ROOT_DIR
}

mount_passthrough () {

    # $1 = context

    log_time $1 "Mounting FUSE Passthrough (mnt=$PASST_MOUNT_DIR,root=$PASST_ROOT_DIR)..."

	mkdir -p $PASST_MOUNT_DIR $PASST_ROOT_DIR
	sudo rm -rf $PASST_MOUNT_DIR/* $PASST_ROOT_DIR/*

    $PASST_BIN $PASST_MOUNT_DIR -o allow_other -o modules=subdir -o subdir=$PASST_ROOT_DIR

	(set -x; sleep 15s)

    check_filesystem $PASST_MOUNT_DIR

    return $?
}

umount_passthrough () {
	
	log_time $1 "Umounting FUSE Passthrough (mnt=$PASST_MOUNT_DIR)..."

	{
        fusermount -uz $PASST_MOUNT_DIR
	} &> /dev/null
	(set -x; sleep 5s)
	check_filesystem $PASST_MOUNT_DIR

    sudo rm -r $PASST_MOUNT_DIR $PASST_ROOT_DIR
}

check_filesystem () {

    # $1 = mount point

	fs_res=$(findmnt -T $1 -o FSTYPE -n)
	log_time "filesystem" "${GREEN_COLOR}$fs_res${NO_COLOR}"
    if [ "$fs_res" = "fuse.lazyfs" ]; then
        return 1
    elif [ "$fs_res" = "fuse.passthrough" ]; then
        return 2
    fi
    return 0
}

#--------------------------------------------------#
# Test loop

LOOP_START_TIME=$(date -u +"%s")

log_time "setup" "starting workloads..."
horizontal_line

# create outputs folder
REAL_OUTPUT_PATH="$TEST_VAR_OUTPUT_RESULTS_FOLDER-run-$(date +"%Y_%m_%d_%I_%M_%p")"
mkdir -p $REAL_OUTPUT_PATH

#--------------------------------------------------#

TEST_OK_COUNT=0
TEST_TOTAL_WORKLOADS_NR=$(find $TEST_VAR_WORKLOADS_FOLDER -maxdepth 4 -type f -printf "%p\n" | wc -l)
TEST_EXPECT_OK=$((TEST_VAR_REPEAT_WORKLOADS*${#TEST_VAR_FOR_FILESYSTEMS[*]}*TEST_TOTAL_WORKLOADS_NR))

for workload_fullpath_file in $(find $TEST_VAR_WORKLOADS_FOLDER -maxdepth 4 -type f -printf "%p\n"); do

    workload_file=$(basename $workload_fullpath_file)
    workload_name=$(echo $workload_file | cut -f 1 -d '.')
    workload_folder=$(echo $workload_fullpath_file | sed -e "s/$workload_file//")
    
    # update general workload specifications
    sed -i "/set \$WORKLOAD_TIME.*/c\set \$WORKLOAD_TIME=$TEST_VAR_TOTAL_RUNTIME_EACH_WORKLOAD" $workload_fullpath_file

    for filesystem in ${TEST_VAR_FOR_FILESYSTEMS[@]}; do

        # configure filebench workload with FS specifications

        log_time "FS:$filesystem,WL:$workload_file" "starting test..."

        current_iteration=0

        output_results_file="$REAL_OUTPUT_PATH/run-$workload_name-$filesystem-fb.output"
        touch $output_results_file

        for ((i=0; i<TEST_VAR_REPEAT_WORKLOADS; i++)); do

            log_time "FS:$filesystem,WL:$workload_file" "starting RUN #$current_iteration"

            echo -e "\nrun: #$current_iteration,wl_name:$workload_name,wl_path:$workload_fullpath_file,fs:$filesystem\n\n" >> $output_results_file

            if [ "$filesystem" = "fuse.passthrough" ]; then

                # update filesystem specific workload specifications
                sed -i "/set \$WORKLOAD_PATH.*/c\set \$WORKLOAD_PATH=\"$PASST_MOUNT_DIR\"" $workload_fullpath_file
                sed -i "/set \$LAZYFS_FIFO.*/c\set \$LAZYFS_FIFO=\"/dev/null\"" $workload_fullpath_file

                # mount, run, umount fuse.passthrough

                mount_passthrough "FS:$filesystem,WL:$workload_file"                
                
                if [ "$?" -eq "2" ]; then
                
                    spawn_dstat_process "FS:$filesystem,WL:$workload_file" dstat1 $REAL_OUTPUT_PATH $workload_name $filesystem

                    log_time "FS:$filesystem,WL:$workload_file" "running filebench..."
                    sudo filebench -f $workload_fullpath_file &>> $output_results_file
                    log_time "FS:$filesystem,WL:$workload_file" "filebench test finished..."

                    join_dstat_process "FS:$filesystem,WL:$workload_file" dstat1

                    umount_passthrough "FS:$filesystem,WL:$workload_file"

                    TEST_OK_COUNT=$((TEST_OK_COUNT+1))

                else

                    log_time "FS:$filesystem,WL:$workload_file" "${RED_COLOR}mounting failed!${NO_COLOR}"

                fi
                
            elif [[ $filesystem = fuse.lazyfs* ]]; then

                page_size="$(cut -d'-' -f2 <<<"$filesystem")"

                sed -i "/set \$WORKLOAD_PATH.*/c\set \$WORKLOAD_PATH=\"$LAZYFS_MOUNT_DIR\"" $workload_fullpath_file
                OUTPUT_LAZYFS_FIFO="/tmp/lfs.fb$current_iteration.$workload_name.$page_size.fifo"
                sed -i "/set \$LAZYFS_FIFO.*/c\set \$LAZYFS_FIFO=\"$OUTPUT_LAZYFS_FIFO\"" $workload_fullpath_file
                
                # mount, run, umount fuse.lazyfs

                mount_lazyfs $workload_name $page_size                

                if [ "$?" -eq "1" ]; then

                    spawn_dstat_process "FS:$filesystem,WL:$workload_file" dstat1 $REAL_OUTPUT_PATH $workload_name $filesystem

                    log_time "FS:$filesystem,WL:$workload_file" "running filebench..."
                    sudo filebench -f $workload_fullpath_file &>> $output_results_file
                    log_time "FS:$filesystem,WL:$workload_file" "filebench test finished..."

                    join_dstat_process "FS:$filesystem,WL:$workload_file" dstat1

                    umount_lazyfs $workload_name

                    TEST_OK_COUNT=$((TEST_OK_COUNT+1))

                else
                
                    log_time "FS:$filesystem,WL:$workload_file" "${RED_COLOR}mounting failed!${NO_COLOR}"

                fi
            fi

            current_iteration=$((current_iteration+1))

        	(set -x; sleep $TEST_VAR_SLEEP_AFTER_RUN)

        done

        log_time "FS:$filesystem,WL:$workload_file" "test finished..."

        horizontal_line

    	(set -x; sleep $TEST_VAR_SLEEP_AFTER_CHANGE_FS)

    done

	(set -x; sleep $TEST_VAR_SLEEP_AFTER_WORKLOAD_TYPE)

done

# remove filebench log files
sudo rm -rf /tmp/filebench*

#--------------------------------------------------#

LOOP_END_TIME=$(date -u +"%s")
LOOP_TOTAL_TIME=$(date -u -d "0 $LOOP_END_TIME sec - $LOOP_START_TIME sec" +"%Hh:%Mm:%Ss")

#--------------------------------------------------#

log_time "test_total_time" "$LOOP_TOTAL_TIME (time elapsed)"
log_time "test_ok_count" "$TEST_OK_COUNT/$TEST_EXPECT_OK (number of tests tests ok / expected ok)"

log_time "teardown" "workloads ended..."
