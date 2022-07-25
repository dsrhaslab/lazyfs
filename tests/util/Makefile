
# Compiles all examples
compile: write.c read.c fsync.c truncate.c
	gcc write.c -o write
	gcc read.c -o read
	gcc fsync.c -o fsync
	gcc truncate.c -o truncate

# Clears binaries
clean:
	rm -rf write read fsync truncate *.o
