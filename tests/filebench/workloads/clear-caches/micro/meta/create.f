# ------------------------------------------------------#
# workload: rread.f
# ------------------------------------------------------#
# These variables are changed dynamically

set $WORKLOAD_PATH="/tmp/lazyfs.fb.mnt"
set $WORKLOAD_TIME=200
set $NR_THREADS=1
set $LAZYFS_FIFO="/tmp/lfs.fb1.create.32768.fifo"

set $NR_FILES=10000
set $MEAN_DIR_WIDTH=1000
set $IO_SIZE=4k
set $NR_ITERATIONS=67108864

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
    thread name="thread-1", memsize=10m, instances=1
    {
        flowop createwriteclose name="createwriteclose-1", iters=$NR_FILES

        flowop finishoncount name="finishoncount-1", value=1
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