# ------------------------------------------------------#
# workload: varmail.f
# ------------------------------------------------------#
# These variables are changed dynamically

set $WORKLOAD_PATH="/tmp/passt.fb.mnt"
set $WORKLOAD_TIME=900
set $NR_THREADS=16
set $LAZYFS_FIFO="/dev/null"

set $NR_FILES=1000
set $MEAN_DIR_WIDTH=1000000
set $IO_SIZE=1m
set $meanappendsize=16k
set $FILE_SIZE=cvar(type=cvar-gamma,parameters=mean:16384;gamma:1.5)

# ------------------------------------------------------#

define fileset name=bigfileset,path=$WORKLOAD_PATH,size=$FILE_SIZE,entries=$NR_FILES,dirwidth=$MEAN_DIR_WIDTH,prealloc=80

define process name=filereader,instances=1
{
  thread name=filereaderthread,memsize=10m,instances=$NR_THREADS
  {
    flowop deletefile name=deletefile1,filesetname=bigfileset
    flowop createfile name=createfile2,filesetname=bigfileset,fd=1
    flowop appendfilerand name=appendfilerand2,iosize=$meanappendsize,fd=1
    flowop fsync name=fsyncfile2,fd=1
    flowop closefile name=closefile2,fd=1
    flowop openfile name=openfile3,filesetname=bigfileset,fd=1
    flowop readwholefile name=readfile3,fd=1,iosize=$IO_SIZE
    flowop appendfilerand name=appendfilerand3,iosize=$meanappendsize,fd=1
    flowop fsync name=fsyncfile3,fd=1
    flowop closefile name=closefile3,fd=1
    flowop openfile name=openfile4,filesetname=bigfileset,fd=1
    flowop readwholefile name=readfile4,fd=1,iosize=$IO_SIZE
    flowop closefile name=closefile4,fd=1
  }
}

# ------------------------------------------------------#

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
