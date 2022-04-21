
set $WORKLOAD_PATH="/mnt/test-fs/lazyfs/fb-workload"

set mode quit firstdone

define fileset name="fileset-1", path=$WORKLOAD_PATH, entries=1, dirwidth=1, dirgamma=0,
               filesize=1g, prealloc

define process name="process-1", instances=1
{
    thread name="thread-1", memsize=4k, instances=1
    {
        flowop openfile name="open-1", filesetname="fileset-1", fd=1, indexed=1
        flowop read name="read-1", fd=1, iosize=4k, iters=67108864, random
        flowop closefile name="close-1", fd=1

        flowop finishoncount name="finish-1", value=1
    }
}

# explicitly preallocate files
create files

# drop file system caches
system "sync ."
system "echo 3 > /proc/sys/vm/drop_caches"

echo "time sync"
system "date '+time sync %s.%N'"
echo "time sync"

run 300
