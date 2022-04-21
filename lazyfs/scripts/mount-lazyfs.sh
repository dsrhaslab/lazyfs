#!/bin/bash

# ------------------------------------------------
# Variables that can be changed

# MOUNT_DIR=/tmp/lazyfs
# ROOT_DIR=/tmp/lazyfs-root
MOUNT_DIR=/mnt/test-fs/lazyfs
ROOT_DIR=/mnt/test-fs/lazyfs-root

# ------------------------------------------------

HELP_MSG="Run LazyFS with:\n\t-c | --config-path : The path to the toml configuration file.
\t-f | --foreground  : Run FUSE in foreground."

if [ $# -eq 0 ]; then
    echo -e "$HELP_MSG"
    exit 1
fi

POSITIONAL_ARGS=()
while [[ $# -gt 0 ]]; do
  case $1 in
    -c|--config-path)
      CMD_CONFIG="$2"
      shift # past argument
      shift # past value
      ;;
    -f|--foreground)
      CMD_FG="foreground"
      shift # past argument
      ;;
    -h|--help)
      CMD_HELP="help"
      shift # past argument
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

echo "::Reading config from ${CMD_CONFIG}..."    
echo -e "::FUSE: Root=$ROOT_DIR Mount=$MOUNT_DIR"
mkdir -p $MOUNT_DIR
mkdir -p $ROOT_DIR
echo -e "::Running lazyfs (stop with ctrl+c or umount-lazyfs.sh)...\n"
if [ -z "$CMD_FG" ]; then
   echo -e "(running in no foreground mode)\n"
   echo -e "Note: run in foreground to see the <stdio> logs.\n"
   ./build/lazyfs $MOUNT_DIR --config-path config/default.toml -o allow_other -o modules=subdir -o subdir=$ROOT_DIR
else
   echo -e "(running in foreground mode)\n"
   ./build/lazyfs $MOUNT_DIR --config-path config/default.toml -o allow_other -o modules=subdir -o subdir=$ROOT_DIR -f
fi
