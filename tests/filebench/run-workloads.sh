#!/bin/bash

# --------------------------------------------------#
# Test variables

# LazyFS
LAZYFS_MOUNT_DIR=/tmp/lazyfs.mnt
LAZYFS_ROOT_DIR=/tmp/lazyfs.root
LAZYFS_FOLDER=/home/gsd/lazyfs-jepsen/lazyfs

# FUSE passthrough.c example
PASST_MOUNT_DIR=/tmp/passt.mnt
PASST_ROOT_DIR=/tmp/passt.root
PASST_BIN=/home/gsd/libfuse/example/passthrough.c

TEST_VAR_LAZYFS_PAGE_SIZE=(4096 32768) # block/page size specified in bytes

# --------------------------------------------------#
# Other less important variables

# Bash colors
RED_COLOR='\e[31m'
GREEN_COLOR='\e[32m'
NO_COLOR='\e[0m'

# --------------------------------------------------#
# Utilities

log_time () {
	
    # $1 = context
	# $2 = message
	
    echo -e $(date "+%Y-%m-%d %H:%M:%S")": #$1 - $2"
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
	(set -x; sleep 10s)
    check_filesystem
    return $?
}

umount_lazyfs () {
	
	log_time $1 "Umounting LazyFS (mnt=$LAZYFS_MOUNT_DIR)..."

	cd $LAZYFS_FOLDER > /dev/null
	{
		./scripts/umount-lazyfs.sh -m $LAZYFS_MOUNT_DIR
	} &> /dev/null
	(set -x; sleep 5s)
	check_filesystem
	cd - > /dev/null

    rm -rf /tmp/lfs.fb*config.toml
    rm -rf /tmp/lfs.fb*fifo
}

check_filesystem () {

	fs_res=$(findmnt -T $LAZYFS_MOUNT_DIR -o FSTYPE -n)
	log_time "filesystem" "${GREEN_COLOR}$fs_res${NO_COLOR}"
    if [ $fs_res != "fuse.lazyfs" ]; then
        return 1
    fi
    return 0
}

mount_lazyfs "test" 4096
sleep 2
umount_lazyfs "test"

# --------------------------------------------------#
# Test loop