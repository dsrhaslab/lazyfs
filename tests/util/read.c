
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char* argv[]) {

	if (argc < 4) {
		printf("run ./read <size> <offset> <path>\n");
		return -1;
	}

	int size=atoi(argv[1]);
	int offset=atoi(argv[2]);

	int fd = open(argv[3], O_RDONLY);
	
	if (fd < 0) {
		printf("could not open path %s\n", argv[3]);
		return -1;
	}
	
	printf("reading from <%s> %d bytes at offset %d\n", argv[3], size, offset);
	
	char *buf=malloc(size);

	int r = pread(fd, buf, size, offset);
    printf("\t> read %d bytes...\n", r);

    for (int it = 0; it < r; it++) {
        printf("%c", buf[it]);
    }
    printf("\n");

    close(fd);

    return 0;
}
