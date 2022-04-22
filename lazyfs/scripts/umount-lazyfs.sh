#!/bin/bash

HELP_MSG="Umount LazyFS with:
\t-m | --mount.dir   : FUSE mount dir."

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
    -h|--help)
      CMD_HELP="help"
      shift
      echo -e "$HELP_MSG"
      exit
      ;;
  esac
done

set -- "${POSITIONAL_ARGS[@]}"

if [ ! -d "$MOUNT_DIR" ]; then
    echo "Error: Mount directory '${MOUNT_DIR}' does not exist."    
    exit
fi

echo "Umounting FUSE on $MOUNT_DIR..."
fusermount -uz $MOUNT_DIR