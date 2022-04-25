#!/bin/bash

#------------------------------------------------
# This script replaces the WORKLOAD_PATH in each
# Filebench test with the one specified, also it
# creates the fileset destination folder.
#------------------------------------------------

HELP_CMD="Please specify the following arguments:\n\t-f: Filebench workload file.\n\t-d: Fileset workload directory."

while getopts h:d:f: flag
do
    case "${flag}" in
        d) workload_dir=${OPTARG};;
        f) filebench_workload_file=${OPTARG};;
        h) echo -e $HELP_CMD; exit;;
    esac
done

if [ -z "$filebench_workload_file" ] || [ -z "$workload_dir" ];
then
    echo -e $HELP_CMD;
    exit;
fi

echo "Filebench workload file: $filebench_workload_file"
echo "Fileset workload directory: $workload_dir"

echo "replacing workload path in the workload file..."
sed -i "/set \$WORKLOAD_PATH.*/c\set \$WORKLOAD_PATH=\"${workload_dir}\"" $filebench_workload_file

echo "creating workloads dir..."
rm -rf $workload_dir
mkdir -p $workload_dir
echo "now run 'filebench -f $filebench_workload_file'"