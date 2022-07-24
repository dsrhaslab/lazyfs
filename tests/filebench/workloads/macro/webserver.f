# ------------------------------------------------------#
# workload: webserver.f
# ------------------------------------------------------#
# These variables are changed dynamically

set $WORKLOAD_PATH="DEFAULT_VALUE"
set $WORKLOAD_TIME=300
set $NR_THREADS=1

set $NR_FILES=1000
set $MEAN_DIR_WIDTH=20
set $IO_SIZE=4k
set $FILE_SIZE=cvar(type=cvar-gamma,parameters=mean:16384;gamma:1.5)

# ------------------------------------------------------#

define fileset name=bigfileset,path=$WORKLOAD_PATH,size=$FILE_SIZE,entries=$NR_FILES,dirwidth=$MEAN_DIR_WIDTH,prealloc=100,readonly
define fileset name=logfiles,path=$WORKLOAD_PATH,size=$FILE_SIZE,entries=1,dirwidth=$MEAN_DIR_WIDTH,prealloc

define process name=filereader,instances=1
{
  thread name=filereaderthread,memsize=10m,instances=$nthreads
  {
    flowop openfile name=openfile1,filesetname=bigfileset,fd=1
    flowop readwholefile name=readfile1,fd=1,iosize=$IO_SIZE
    flowop closefile name=closefile1,fd=1
    flowop openfile name=openfile2,filesetname=bigfileset,fd=1
    flowop readwholefile name=readfile2,fd=1,iosize=$IO_SIZE
    flowop closefile name=closefile2,fd=1
    flowop openfile name=openfile3,filesetname=bigfileset,fd=1
    flowop readwholefile name=readfile3,fd=1,iosize=$IO_SIZE
    flowop closefile name=closefile3,fd=1
    flowop openfile name=openfile4,filesetname=bigfileset,fd=1
    flowop readwholefile name=readfile4,fd=1,iosize=$IO_SIZE
    flowop closefile name=closefile4,fd=1
    flowop openfile name=openfile5,filesetname=bigfileset,fd=1
    flowop readwholefile name=readfile5,fd=1,iosize=$IO_SIZE
    flowop closefile name=closefile5,fd=1
    flowop openfile name=openfile6,filesetname=bigfileset,fd=1
    flowop readwholefile name=readfile6,fd=1,iosize=$IO_SIZE
    flowop closefile name=closefile6,fd=1
    flowop openfile name=openfile7,filesetname=bigfileset,fd=1
    flowop readwholefile name=readfile7,fd=1,iosize=$IO_SIZE
    flowop closefile name=closefile7,fd=1
    flowop openfile name=openfile8,filesetname=bigfileset,fd=1
    flowop readwholefile name=readfile8,fd=1,iosize=$IO_SIZE
    flowop closefile name=closefile8,fd=1
    flowop openfile name=openfile9,filesetname=bigfileset,fd=1
    flowop readwholefile name=readfile9,fd=1,iosize=$IO_SIZE
    flowop closefile name=closefile9,fd=1
    flowop openfile name=openfile10,filesetname=bigfileset,fd=1
    flowop readwholefile name=readfile10,fd=1,iosize=$IO_SIZE
    flowop closefile name=closefile10,fd=1
    flowop appendfilerand name=appendlog,filesetname=logfiles,iosize=$IO_SIZE,fd=2
  }
}

# ------------------------------------------------------#

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