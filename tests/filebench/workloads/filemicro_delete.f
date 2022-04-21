
set $WORKLOAD_PATH="/mnt/test-fs/lazyfs/fb-workload"

define fileset name="fileset-1", path=$WORKLOAD_PATH, entries=500000, dirwidth=1000,
               dirgamma=0, filesize=4k

define process name="process-1", instances=1
{
    thread name="thread-1", memsize=4k, instances=1
    {
        flowop deletefile name="deletefile-1", filesetname="fileset-1", iters=500000

        flowop finishoncount name="finishoncount-1", value=1
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

psrun -5 300