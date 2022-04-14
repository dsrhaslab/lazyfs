#!/bin/bash
PASSTBIN=/home/gsd/testbed/libfuse/example/passthrough
MOUNT_DIR=/tmp/passthrough
ROOT_DIR=/tmp/passthrough-root
rm -rf $MOUNT_DIR $ROOT_DIR
mkdir -p $MOUNT_DIR $ROOT_DIR
$PASSTBIN $MOUNT_DIR -o allow_other -o modules=subdir -o subdir=$ROOT_DIR -f
