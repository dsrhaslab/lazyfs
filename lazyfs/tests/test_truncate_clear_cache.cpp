
#include <chrono>
#include <errno.h>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <mntent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

#define IO_BLOCK_SIZE 4096
#define FAULTS_PIPE_PATH "/home/gsd/faults-example.fifo"

using namespace std;

int custom_argc;
char** custom_argv;

int clear_cache_command () {
    int pipefd = open (FAULTS_PIPE_PATH, O_WRONLY);
    if (pipefd < 0)
        return -1;
    int r = write (pipefd, "lazyfs::clear-cache\n", 20);
    close (pipefd);
    std::this_thread::sleep_for (0.3s);
    return r == 20 ? 0 : -1;
}

namespace lazyfs::tests {

TEST (LazyFSTests, SetupTest) {
    ASSERT_EQ (custom_argc, 2);
    int r = chdir (custom_argv[1]);
    ASSERT_TRUE (r >= 0);
}

TEST (TruncateTests, SimpleTruncate) {

    const char* path = "SimpleTruncate";

    int fd = open (path, O_CREAT | O_RDWR, 0640);

    ASSERT_TRUE (fd >= 0);

    ASSERT_TRUE (truncate (path, IO_BLOCK_SIZE) >= 0);
    ASSERT_EQ (lseek (fd, 0, SEEK_END), IO_BLOCK_SIZE);

    char buf_read[IO_BLOCK_SIZE];
    ASSERT_EQ (pread (fd, buf_read, IO_BLOCK_SIZE, 0), IO_BLOCK_SIZE);
    char buf_expected[IO_BLOCK_SIZE];
    memset (buf_expected, 0, IO_BLOCK_SIZE);
    ASSERT_TRUE (!memcmp (buf_read, buf_expected, IO_BLOCK_SIZE));

    ASSERT_EQ (clear_cache_command (), 0);

    ASSERT_EQ (pread (fd, buf_read, IO_BLOCK_SIZE, 0), 0);
    ASSERT_EQ (lseek (fd, 0, SEEK_END), 0);

    ASSERT_TRUE (close (fd) >= 0);

    ASSERT_TRUE (unlink (path) >= 0);
}

TEST (TruncateTests, TruncateRewrite) {

    const char* path = "TruncateRewrite";

    int fd = open (path, O_CREAT | O_RDWR, 0640);

    ASSERT_TRUE (fd >= 0);

    ASSERT_TRUE (truncate (path, IO_BLOCK_SIZE) >= 0);
    char buf[IO_BLOCK_SIZE];
    memset (buf, 1, IO_BLOCK_SIZE);
    ASSERT_EQ (pwrite (fd, buf, IO_BLOCK_SIZE, 0), IO_BLOCK_SIZE);

    char buf_read[IO_BLOCK_SIZE + 1];
    ASSERT_EQ (pread (fd, buf_read, IO_BLOCK_SIZE, 0), IO_BLOCK_SIZE);
    char buf_expected[IO_BLOCK_SIZE + 1];
    memset (buf_expected, 1, IO_BLOCK_SIZE);
    ASSERT_TRUE (!memcmp (buf_read, buf_expected, IO_BLOCK_SIZE));

    ASSERT_TRUE (truncate (path, IO_BLOCK_SIZE + 1) >= 0);
    memset (buf_expected + IO_BLOCK_SIZE, 0, 1);
    ASSERT_EQ (pread (fd, buf_read, IO_BLOCK_SIZE + 1, 0), IO_BLOCK_SIZE + 1);
    ASSERT_TRUE (!memcmp (buf_read, buf_expected, IO_BLOCK_SIZE + 1));

    ASSERT_TRUE (close (fd) >= 0);

    ASSERT_TRUE (unlink (path) >= 0);
}

TEST (TruncateTests, TruncateRewrite_SyncOn) {

    const char* path = "TruncateRewrite";

    int fd = open (path, O_CREAT | O_RDWR, 0640);

    ASSERT_TRUE (fd >= 0);

    ASSERT_TRUE (truncate (path, IO_BLOCK_SIZE) >= 0);

    ASSERT_TRUE (fsync (fd) >= 0);
    ASSERT_EQ (clear_cache_command (), 0);
    ASSERT_EQ (lseek (fd, 0, SEEK_END), IO_BLOCK_SIZE);

    ASSERT_TRUE (truncate (path, IO_BLOCK_SIZE - 10) >= 0);
    ASSERT_TRUE (fsync (fd) >= 0);
    ASSERT_EQ (clear_cache_command (), 0);
    ASSERT_EQ (lseek (fd, 0, SEEK_END), IO_BLOCK_SIZE - 10);

    char buf[IO_BLOCK_SIZE * 3];
    memset (buf, 3, IO_BLOCK_SIZE * 3);
    ASSERT_EQ (pwrite (fd, buf, IO_BLOCK_SIZE * 3, 0), IO_BLOCK_SIZE * 3);
    ASSERT_TRUE (fsync (fd) >= 0);
    ASSERT_EQ (clear_cache_command (), 0);
    ASSERT_EQ (lseek (fd, 0, SEEK_END), IO_BLOCK_SIZE * 3);

    char buf_read[IO_BLOCK_SIZE * 3];
    ASSERT_EQ (pread (fd, buf_read, IO_BLOCK_SIZE * 3, 0), IO_BLOCK_SIZE * 3);
    char buf_expected[IO_BLOCK_SIZE * 3];
    memset (buf_expected, 3, IO_BLOCK_SIZE * 3);
    ASSERT_TRUE (!memcmp (buf_read, buf_expected, IO_BLOCK_SIZE * 3));

    ASSERT_TRUE (close (fd) >= 0);

    ASSERT_TRUE (unlink (path) >= 0);
}

} // namespace lazyfs::tests

int main (int argc, char** argv) {

    ::testing::InitGoogleTest (&argc, argv);

    custom_argc = argc;
    custom_argv = argv;

    return RUN_ALL_TESTS ();
}