
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char* argv[]) {

	int fd = open("/tmp/lazyfs/file", O_WRONLY|O_TRUNC);

	printf("fd=%d\n", fd);	

    	return 0;
}
