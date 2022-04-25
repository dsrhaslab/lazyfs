
# Compiles all examples
compile: write.c read.c fsync.c truncate.c .c
	gcc write.c -o write
	gcc read.c -o read
	gcc fsync.c -o fsync
	gcc truncate.c -o truncate

# Clears binaries
clean:
	rm -rf a.out write read fsync truncate *.o

# Clears folders for LazyFS tests
clear_lazyfs:
	rm -rf /tmp/lazyfs/* /tmp/lazyfs-root/* /tmp/passthrough/* /tmp/passthrough-root/*
	rm -rf /tmp/lazyfs/.psql_history
	rm -rf /home/gsd/*.fifo
	rm -rf /mnt/test-fs/lazyfs/*
	rm -rf /mnt/test-fs/lazyfs-root/*
	rm -rf /mnt/test-fs/passthrough/*
	rm -rf /mnt/test-fs/passthrough-root/*

