# ------------------------------------------------------#
# workload: rand-write.f
# ------------------------------------------------------#
# These variables are changed dynamically

set $WORKLOAD_PATH="DEFAULT_VALUE"
set $WORKLOAD_TIME=300
set $NR_THREADS=1

set $NR_FILES=1
set $MEAN_DIR_WIDTH=1
set $IO_SIZE=4k
set $FILE_SIZE=1g

# ------------------------------------------------------#

set mode quit firstdone

define fileset name="fileset-1", path=$WORKLOAD_PATH, entries=$NR_FILES, dirwidth=$MEAN_DIR_WIDTH, dirgamma=0,
               filesize=$FILE_SIZE, prealloc

define process name="process-1", instances=1
{
    thread name="thread-1", memsize=4k, instances=$NR_THREADS
    {
        flowop openfile name="open-1", filesetname="fileset-1", fd=1, indexed=1
        flowop write name="write-1", fd=1, iosize=$IO_SIZE, iters=67108864, random
        flowop closefile name="close-1", fd=1

        flowop finishoncount name="finish-1", value=1
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