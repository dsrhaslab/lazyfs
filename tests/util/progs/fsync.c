
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char* argv[]) {

    if (argc < 3) {
        printf("run ./fsync <mode=all,data> <path>\n");
        return -1;
    }

    char *mode = argv[1];
    if (strcmp(mode, "all") && strcmp(mode,"data")) {
        printf("set mode to \"all\" or \"data\"\n");
        return -1;
    }

    char* path = argv[2];

    int fd = open(path, O_RDONLY);
	
	if (fd < 0) {
		printf("could not open path %s\n", path);
		return -1;
	}

    printf("fsyncing path <%s>...\n", path);

    int r;
    if (!strcmp(mode, "all")) {
        r = fsync(fd);
        printf("> fsync done, returned %d\n", r);
    }
    else {

        r = fdatasync(fd);
        printf("> fdatasync done, returned %d\n", r);
    }

    close(fd);

    return 0;
}