#!/bin/bash
#-------------------------------------------#
# A script to run LazyFS tests
#-------------------------------------------#

TIME_MOUNT=2
CONFIG=config/default.toml

#-------------------------------------------#
# Read positional args

HELP_MSG="Run LazyFS tests with:
\t-m | --mount.dir   : FUSE mount dir.
\t-r | --root.dir   : FUSE root dir."

if [ $# -eq 0 ]; then
    echo -e "$HELP_MSG"
    exit 1
fi

POSITIONAL_ARGS=()
while [[ $# -gt 0 ]]; do
  case $1 in
    -m|--mount.dir)
      MOUNT_DIR="$2"
      shift
      shift
      ;;
    -r|--root.dir)
      ROOT_DIR="$2"
      shift
      shift
      ;;
    -h|--help)
      CMD_HELP="help"
      shift
      echo -e "$HELP_MSG"
      exit
      ;;
  esac
done

set -- "${POSITIONAL_ARGS[@]}"

CURRENT_DIR=$PWD

log_with_time() {
    now=$(date +"%T")
    echo "[$now] $1"
}

build_tests() {

    cd $CURRENT_DIR/..

    log_with_time "building tests"
    ./build.sh

    cd -
}

setup_testbed() {

    cd $CURRENT_DIR/..

    log_with_time "clearing folder contents"
    sudo rm -rf $MOUNT_DIR/* $ROOT_DIR/*

    log_with_time "mounting LazyFS in $MOUNT_DIR"
    ./scripts/mount-lazyfs.sh -m $MOUNT_DIR -r $ROOT_DIR -c $CONFIG > /dev/null
    sleep $TIME_MOUNT
}

teardown_testbed() {

    cd $CURRENT_DIR/..

    log_with_time "umounting LazyFS in $MOUNT_DIR"
    ./scripts/umount-lazyfs.sh -m $MOUNT_DIR > /dev/null

    log_with_time "clearing folder contents"
    sudo rm -rf $MOUNT_DIR/* $ROOT_DIR/*
}

run_tests() {

    cd $CURRENT_DIR/../build

    for test in test*; do
        log_with_time "running $test..."
        ./$test $MOUNT_DIR
    done
}

#-------------------------------------------#

log_with_time "setting up LazyFS testbed"
log_with_time "using: mount=$MOUNT_DIR root=$ROOT_DIR"

build_tests
setup_testbed
run_tests
teardown_testbed

#-------------------------------------------#
