
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char* argv[]) {


	if (argc < 5) {
		printf("run ./write <size> <offset> <char> <path>\n");
		return -1;
	}

	int size=atoi(argv[1]);
	int offset=atoi(argv[2]);
	char content = argv[3][0];

	if (size == 0) {
		printf("cannot write with size 0!\n");	
		return -1;
	}

	if (strlen(argv[3]) > 1) {
		printf("<char> must not be a string!\n");
		return -1;
	}
		

	int fd = open(argv[4], O_CREAT|O_WRONLY, 0666);
	
	if (fd < 0) {
		printf("could not open path %s\n", argv[4]);
		return -1;
	}
	
	printf("path <%s> content <%c>\n", argv[4], content);
	printf("\t> writing <%d bytes> from <offset %d to %d>...\n", size, offset, offset+size-1);
	
	char buf[size];

	memset(buf, content, size);
	int r = pwrite(fd, buf, size, offset);
    printf("\t> wrote %d bytes...\n", r);

	close(fd);

    return 0;
}
