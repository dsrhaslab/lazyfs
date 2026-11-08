#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define CHECK(expression) do {                                                \
    if (!(expression)) {                                                     \
        fprintf(stderr, "%s:%d: %s: %s\n", __FILE__, __LINE__, #expression,    \
                strerror(errno));                                            \
        exit(EXIT_FAILURE);                                                  \
    }                                                                        \
} while (0)

static void check_backing(const char *path, off_t size, blkcnt_t blocks) {
    struct stat info;
    CHECK(stat(path, &info) == 0);
    CHECK(info.st_size == size);
    CHECK(info.st_blocks >= blocks);
}

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "usage: preallocate prepare|verify MOUNTED_FILE BACKING_FILE\n");
        return EXIT_FAILURE;
    }
    char data[4096];
    struct stat info;
    if (strcmp(argv[1], "prepare") == 0) {
        int fd = open(argv[2], O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC, 0600);
        CHECK(fd >= 0);
        CHECK(fallocate(fd, FALLOC_FL_KEEP_SIZE, 0, 8192) == 0);
        CHECK(fstat(fd, &info) == 0 && info.st_size == 0);
        check_backing(argv[3], 0, 8192 / 512);
        memset(data, '0', sizeof(data));
        CHECK(pwrite(fd, data, sizeof(data), 0) == sizeof(data));
        CHECK(fsync(fd) == 0);
        memset(data, 'X', sizeof(data));
        CHECK(pwrite(fd, data, sizeof(data), 0) == sizeof(data));
        CHECK(fallocate(fd, FALLOC_FL_KEEP_SIZE, 0, 16384) == 0);
        CHECK(fstat(fd, &info) == 0 && info.st_size == sizeof(data));
        check_backing(argv[3], sizeof(data), 16384 / 512);
        errno = 0;
        CHECK(fallocate(fd, FALLOC_FL_KEEP_SIZE, -1, 8192) == -1 && errno == EINVAL);
        errno = 0;
        CHECK(fallocate(fd, FALLOC_FL_KEEP_SIZE | FALLOC_FL_PUNCH_HOLE, 0, 4096) == -1
              && errno == EOPNOTSUPP);
        CHECK(close(fd) == 0);
        puts("Prepared: physical allocation preserved logical EOF and dirty data.");
    } else if (strcmp(argv[1], "verify") == 0) {
        int fd = open(argv[2], O_RDONLY | O_CLOEXEC);
        CHECK(fd >= 0);
        CHECK(fstat(fd, &info) == 0 && info.st_size == sizeof(data));
        CHECK(pread(fd, data, sizeof(data), 0) == sizeof(data));
        for (size_t i = 0; i < sizeof(data); ++i)
            CHECK(data[i] == '0');
        CHECK(close(fd) == 0);
        check_backing(argv[3], sizeof(data), 16384 / 512);
        puts("Verified: allocation did not persist the unsynced overwrite.");
    } else {
        fprintf(stderr, "unknown mode: %s\n", argv[1]);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
