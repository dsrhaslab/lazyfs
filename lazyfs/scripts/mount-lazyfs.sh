#!/bin/bash

#------------------------------------------------------------------
# A script to run the LazyFS with custom arguments:
# Please run ./mount-lazyfs.sh --help to show help.
#------------------------------------------------------------------

HELP_MSG="Mount LazyFS with:
\t-c | --config-path : The path to the LazyFS toml config file.
\t-f | --foreground  : Run FUSE in foreground.
\t-m | --mount.dir   : FUSE mount dir.
\t-r | --root.dir    : FUSE root dir.
\t-s | --single-thread : Run FUSE in single-thread mode.
"

if [ $# -eq 0 ]; then
    echo -e "$HELP_MSG"
    exit 1
fi

SINGLE_THREAD=""

POSITIONAL_ARGS=()
while [[ $# -gt 0 ]]; do
  case $1 in
    -c|--config-path)
      CMD_CONFIG="$2"
      shift
      shift
      ;;
    -f|--foreground)
      CMD_FG="foreground"
      shift
      ;;
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
    -s|--single-thread)
      SINGLE_THREAD="-s"
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

if [ ! -f "$CMD_CONFIG" ]; then
    echo "Error: Configuration file '${CMD_CONFIG}' does not exist."    
    exit
fi

if [ ! -d "$MOUNT_DIR" ]; then
    echo "Error: Mount directory '${MOUNT_DIR}' does not exist."    
    exit
fi

if [ ! -d "$ROOT_DIR" ]; then
    echo "Error: Root directory '${ROOT_DIR}' does not exist."    
    exit
fi

echo "Creating ROOT_DIR=$ROOT_DIR and MOUNT_DIR=$MOUNT_DIR"    
mkdir -p $MOUNT_DIR
mkdir -p $ROOT_DIR
echo "Using config '${CMD_CONFIG}'"    
echo -e "Running LazyFS (stop with ctrl+c or umount-lazyfs.sh)...\n"
if [ -z "$CMD_FG" ]; then
   echo -e "(running in no foreground mode)\n"
   echo -e "Note: run in foreground to see the <stdio> logs.\n"
   ./build/lazyfs $MOUNT_DIR --config-path $CMD_CONFIG -o allow_other -o modules=subdir -o subdir=$ROOT_DIR $SINGLE_THREAD &
else
   echo -e "(running in foreground mode)\n"
   ./build/lazyfs $MOUNT_DIR --config-path $CMD_CONFIG -o allow_other -o modules=subdir -o subdir=$ROOT_DIR $SINGLE_THREAD -f
fi
