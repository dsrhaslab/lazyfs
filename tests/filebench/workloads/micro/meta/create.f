# ------------------------------------------------------#
# workload: rread.f
# ------------------------------------------------------#
# These variables are changed dynamically

set $WORKLOAD_PATH="DEFAULT_VALUE"
set $WORKLOAD_TIME=300
set $NR_THREADS=1

set $NR_FILES=100000
set $MEAN_DIR_WIDTH=1000
set $IO_SIZE=4k

# ------------------------------------------------------#

define flowop name=createwriteclose
{
    flowop createfile name="createfile-1", filesetname="fileset-1", fd=1
    flowop write name="write-1", fd=1, iosize=$IO_SIZE
    flowop closefile name="closefile-1", fd=1
}

define fileset name="fileset-1", path=$WORKLOAD_PATH, entries=$NR_FILES, dirwidth=$MEAN_DIR_WIDTH,
               dirgamma=0, filesize=$IO_SIZE

define process name="process-1", instances=$NR_THREADS
{
    thread name="thread-1", memsize=$IO_SIZE, instances=1
    {
        flowop createwriteclose name="createwriteclose-1", iters=$NR_FILES

        flowop finishoncount name="finishoncount-1", value=1
    }
}

# ------------------------------------------------------#

create files

system "sync"
system "echo 3 > /proc/sys/vm/drop_caches"

echo "time sync"
system "date '+time sync %s.%N'"
echo "time sync"

#-----------------------------------------#
# REPLACE_LAZYFS_CACHE_CONFIGURATION_HERE #
#-----------------------------------------#

# ------------------------------------------------------#

run $WORKLOAD_TIME
