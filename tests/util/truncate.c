
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char* argv[]) {

    if (argc < 3) {
        printf("run ./truncate <size> <path>\n");
        return -1;
    }

    int size = atoi(argv[1]);

    char* path = argv[2];

    int fd = open(path, O_WRONLY);
	
	if (fd < 0) {
		printf("could not open path %s\n", path);
		return -1;
	}

    printf("truncating path <%s> to <%d> bytes...\n", path, size);

    int r = ftruncate(fd, (off_t) size);

    printf("\tftruncate done: returned %d\n", r);

    close(fd);

    return 0;
}