#!/bin/bash
#-------------------------------------------#
# A script to run LazyFS tests
#-------------------------------------------#

TIME_MOUNT=3
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
ANY_TEST_FAILED=0

log_with_time() {
    now=$(date +"%T")
    echo -e "[$now] $1"
}

build_tests() {

    cd $CURRENT_DIR/../../libs/libpcache
    ./build.sh
    cd -

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
    mkdir -p $MOUNT_DIR $ROOT_DIR
    ./scripts/mount-lazyfs.sh -m $MOUNT_DIR -r $ROOT_DIR -c $CONFIG
    sleep $TIME_MOUNT

    RUN_FLAG=$(findmnt -T $MOUNT_DIR | grep lazyfs | wc -c)
    if [[ "$RUN_FLAG" != "0" ]]; then
        RUN_FLAG="TRUE"
    else
        RUN_FLAG="FALSE"
        log_with_time "\e[31mLazyFS running = \e[32m$RUN_FLAG\e[0m"
        exit
    fi

    log_with_time "\e[31mLazyFS running = \e[32m$RUN_FLAG\e[0m"
}

teardown_testbed() {

    cd $CURRENT_DIR/..

    log_with_time "umounting LazyFS in $MOUNT_DIR"
    ./scripts/umount-lazyfs.sh -m $MOUNT_DIR > /dev/null

    log_with_time "clearing folder contents"
    sudo rm -rf $MOUNT_DIR/* $ROOT_DIR/*

    RUN_FLAG=$(findmnt -T $MOUNT_DIR | grep lazyfs | wc -c)
    if [[ "$RUN_FLAG" != "0" ]]; then
        RUN_FLAG="TRUE"
    else
        RUN_FLAG="FALSE"
    fi
    log_with_time "\e[31mLazyFS running = \e[32m$RUN_FLAG\e[0m"
}

run_tests() {

    cd $CURRENT_DIR/../build
        log_with_time
    for test in test*; do
        log_with_time "running \e[33m'$test' ⏳\e[0m"
        log_with_time
        if ./$test $MOUNT_DIR; then
            log_with_time
            log_with_time "test \e[32m'$test' passed ✓\e[0m"
        else
            ANY_TEST_FAILED=1
            log_with_time
            log_with_time "test \e[31m'$test' failed ✗\e[0m"
        fi
        log_with_time
    done
}

#-------------------------------------------#

log_with_time "setting up LazyFS testbed"
log_with_time "using: mount=$MOUNT_DIR root=$ROOT_DIR"

build_tests
setup_testbed
run_tests
teardown_testbed

if [ $ANY_TEST_FAILED -eq 1 ]; then
    log_with_time "\e[31mSome tests failed!\e[0m"
    exit -1
else
    log_with_time "\e[32mAll tests passed!\e[0m"
    exit 0
fi

#-------------------------------------------#
