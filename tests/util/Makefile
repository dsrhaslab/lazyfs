
# Compiles all examples
compile: write.c read.c fsync.c truncate.c preallocate.c
	gcc write.c -o write
	gcc read.c -o read
	gcc fsync.c -o fsync
	gcc truncate.c -o truncate
	gcc preallocate.c -o preallocate

# Clears binaries
clean:
	rm -rf write read fsync truncate preallocate *.o
