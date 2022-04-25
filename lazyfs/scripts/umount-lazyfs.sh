#!/bin/bash

#------------------------------------------------------------------
# A script to umount the LazyFS with custom arguments:
# Please run ./umount-lazyfs.sh --help to show help.
#------------------------------------------------------------------

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

echo "Umounting FUSE on $MOUNT_DIR..."
fusermount3 -u ${MOUNT_DIR}