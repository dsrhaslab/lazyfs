#!/bin/bash

#---------------------------------------------------------
# Just a simple script to run pre-compiled binary of the
# FUSE passthrough filesystem. 
#
# Edit this variables:

PASSTBIN=/home/gsd/testbed/libfuse/example/passthrough
MOUNT_DIR=/mnt/test-fs/passthrough
ROOT_DIR=/mnt/test-fs/passthrough-root

#---------------------------------------------------------

rm -rf $MOUNT_DIR $ROOT_DIR
mkdir -p $MOUNT_DIR $ROOT_DIR
$PASSTBIN $MOUNT_DIR -o allow_other -o modules=subdir -o subdir=$ROOT_DIR -f
