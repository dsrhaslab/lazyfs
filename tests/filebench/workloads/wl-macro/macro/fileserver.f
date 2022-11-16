# ------------------------------------------------------#
# workload: fileserver.f
# ------------------------------------------------------#
# These variables are changed dynamically

set $WORKLOAD_PATH="/tmp/passt.fb.mnt"
set $WORKLOAD_TIME=900
set $NR_THREADS=50
set $LAZYFS_FIFO="/dev/null"

set $NR_FILES=10000
set $MEAN_DIR_WIDTH=20
set $IO_SIZE=1m
set $meanappendsize=16k
set $FILE_SIZE=cvar(type=cvar-gamma,parameters=mean:131072;gamma:1.5)

# ------------------------------------------------------#

define fileset name=bigfileset,path=$WORKLOAD_PATH,size=$FILE_SIZE,entries=$NR_FILES,dirwidth=$MEAN_DIR_WIDTH,prealloc=80

define process name=filereader,instances=1
{
  thread name=filereaderthread,memsize=10m,instances=$NR_THREADS
  {
    flowop createfile name=createfile1,filesetname=bigfileset,fd=1
    flowop writewholefile name=wrtfile1,srcfd=1,fd=1,iosize=$IO_SIZE
    flowop closefile name=closefile1,fd=1
    flowop openfile name=openfile1,filesetname=bigfileset,fd=1
    flowop appendfilerand name=appendfilerand1,iosize=$meanappendsize,fd=1
    flowop closefile name=closefile2,fd=1
    flowop openfile name=openfile2,filesetname=bigfileset,fd=1
    flowop readwholefile name=readfile1,fd=1,iosize=$IO_SIZE
    flowop closefile name=closefile3,fd=1
    flowop deletefile name=deletefile1,filesetname=bigfileset
    flowop statfile name=statfile1,filesetname=bigfileset
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
