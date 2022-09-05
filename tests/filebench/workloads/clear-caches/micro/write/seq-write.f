# ------------------------------------------------------#
# workload: seq-write.f
# ------------------------------------------------------#
# These variables are changed dynamically

set $WORKLOAD_PATH="/tmp/lazyfs.fb.mnt"
set $WORKLOAD_TIME=200
set $NR_THREADS=1
set $LAZYFS_FIFO="/tmp/lfs.fb1.seq-write.32768.fifo"

set $NR_FILES=1
set $MEAN_DIR_WIDTH=1
set $IO_SIZE=4k
set $FILE_SIZE=1g
set $NR_ITERATIONS=20000000

# ------------------------------------------------------#

define fileset name="fileset-1", path=$WORKLOAD_PATH, entries=$NR_FILES, dirwidth=$MEAN_DIR_WIDTH, dirgamma=0,
               filesize=$FILE_SIZE

define process name="process-1", instances=1
{
    thread name="thread-1", memsize=10m, instances=$NR_THREADS
    {
        flowop createfile name="create-1", filesetname="fileset-1", fd=1, indexed=1
        flowop write name="write-1", fd=1, iosize=$IO_SIZE, iters=$NR_ITERATIONS
        flowop closefile name="close-1", fd=1

        flowop finishoncount name="finish-1", value=1
    }
}

# ------------------------------------------------------#

create files

system "echo LazyFS: performing a cache checkpoint..."
system "sudo -u gsd sh -c 'echo lazyfs::cache-checkpoint > $LAZYFS_FIFO'"
sleep 20

system "echo LazyFS: clearing the cache..."
system "sudo -u gsd sh -c 'echo lazyfs::clear-cache > $LAZYFS_FIFO'"
sleep 20

system "echo OS: syncing workload folder..."
system "sync $WORKLOAD_PATH"
system "echo OS: clearing caches..."
system "echo 3 > /proc/sys/vm/drop_caches"

system "echo time: sync..."
system "date '+time sync %s.%N'"
system "echo time: sync..."

# ------------------------------------------------------#

system "echo workload: running..."
run $WORKLOAD_TIME