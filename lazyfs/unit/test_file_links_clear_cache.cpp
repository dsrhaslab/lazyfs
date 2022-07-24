
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

TEST (FileLinkTests, HardLinks) {

    string test_file_name = "1_HardLinks";

    int fd = open (test_file_name.c_str (), O_CREAT, 0640);
    ASSERT_TRUE (fd >= 0);
    ASSERT_TRUE (close (fd) >= 0);

    string link1_test_file = string (test_file_name).replace (0, 1, "2");

    ASSERT_TRUE (link (test_file_name.c_str (), link1_test_file.c_str ()) >= 0);

    ASSERT_TRUE (unlink (test_file_name.c_str ()) >= 0);
    ASSERT_TRUE (unlink (link1_test_file.c_str ()) >= 0);
}

TEST (FileLinkTests, HardLinksRead) {

    string test_file_name = "1_HardLinks";

    int fd = open (test_file_name.c_str (), O_CREAT, 0640);
    ASSERT_TRUE (fd >= 0);
    ASSERT_TRUE (close (fd) >= 0);

    string link1_test_file = string (test_file_name).replace (0, 1, "2");

    ASSERT_TRUE (link (test_file_name.c_str (), link1_test_file.c_str ()) >= 0);

    int fd_main = open (test_file_name.c_str (), O_RDWR, 0640);
    int fd_link = open (test_file_name.c_str (), O_RDWR, 0640);
    char buf[IO_BLOCK_SIZE];
    memset (buf, 'X', IO_BLOCK_SIZE);
    ASSERT_EQ (pwrite (fd_main, buf, IO_BLOCK_SIZE, 0), IO_BLOCK_SIZE);
    memset (buf, 'Y', IO_BLOCK_SIZE);
    ASSERT_EQ (pwrite (fd_link, buf, IO_BLOCK_SIZE, IO_BLOCK_SIZE), IO_BLOCK_SIZE);

    char expected[IO_BLOCK_SIZE * 2];
    memset (expected, 'X', IO_BLOCK_SIZE);
    memset (expected + IO_BLOCK_SIZE, 'Y', IO_BLOCK_SIZE);
    char read_buf[IO_BLOCK_SIZE * 2];
    ASSERT_EQ (pread (fd_link, read_buf, IO_BLOCK_SIZE * 2, 0), IO_BLOCK_SIZE * 2);

    ASSERT_TRUE (!memcmp (expected, read_buf, IO_BLOCK_SIZE * 2));

    ASSERT_TRUE (unlink (test_file_name.c_str ()) >= 0);
    ASSERT_TRUE (unlink (link1_test_file.c_str ()) >= 0);
}

TEST (FileLinkTests, HardLinksSyncClear) {

    string test_file_name = "1_HardLinks";

    int fd = open (test_file_name.c_str (), O_CREAT, 0640);
    ASSERT_TRUE (fd >= 0);
    ASSERT_TRUE (close (fd) >= 0);

    string link1_test_file = string (test_file_name).replace (0, 1, "2");

    ASSERT_TRUE (link (test_file_name.c_str (), link1_test_file.c_str ()) >= 0);

    int fd_main = open (test_file_name.c_str (), O_RDWR, 0640);
    int fd_link = open (test_file_name.c_str (), O_RDWR, 0640);
    char buf[IO_BLOCK_SIZE];
    memset (buf, 'X', IO_BLOCK_SIZE);
    ASSERT_EQ (pwrite (fd_main, buf, IO_BLOCK_SIZE, 0), IO_BLOCK_SIZE);
    memset (buf, 'Y', IO_BLOCK_SIZE);
    ASSERT_EQ (pwrite (fd_link, buf, IO_BLOCK_SIZE, IO_BLOCK_SIZE), IO_BLOCK_SIZE);

    ASSERT_EQ (clear_cache_command (), 0);

    memset (buf, 'A', IO_BLOCK_SIZE);
    ASSERT_EQ (pwrite (fd_main, buf, IO_BLOCK_SIZE, 0), IO_BLOCK_SIZE);
    memset (buf, 'B', IO_BLOCK_SIZE);
    ASSERT_EQ (pwrite (fd_link, buf, IO_BLOCK_SIZE, IO_BLOCK_SIZE), IO_BLOCK_SIZE);

    ASSERT_EQ (fsync (fd_main), 0);
    ASSERT_EQ (clear_cache_command (), 0);

    char expected[IO_BLOCK_SIZE * 2];
    memset (expected, 'A', IO_BLOCK_SIZE);
    memset (expected + IO_BLOCK_SIZE, 'B', IO_BLOCK_SIZE);
    char read_buf[IO_BLOCK_SIZE * 2];
    ASSERT_EQ (pread (fd_link, read_buf, IO_BLOCK_SIZE * 2, 0), IO_BLOCK_SIZE * 2);

    ASSERT_TRUE (!memcmp (expected, read_buf, IO_BLOCK_SIZE * 2));

    memset (buf, 'Z', IO_BLOCK_SIZE);
    ASSERT_EQ (pwrite (fd_link, buf, IO_BLOCK_SIZE, IO_BLOCK_SIZE), IO_BLOCK_SIZE);
    memset (expected + IO_BLOCK_SIZE, 'Z', IO_BLOCK_SIZE);

    ASSERT_EQ (fsync (fd_link), 0);
    ASSERT_EQ (clear_cache_command (), 0);

    ASSERT_EQ (pread (fd_link, read_buf, IO_BLOCK_SIZE * 2, 0), IO_BLOCK_SIZE * 2);

    ASSERT_TRUE (!memcmp (expected, read_buf, IO_BLOCK_SIZE * 2));

    ASSERT_TRUE (unlink (test_file_name.c_str ()) >= 0);
    ASSERT_TRUE (unlink (link1_test_file.c_str ()) >= 0);
}

} // namespace lazyfs::tests

int main (int argc, char** argv) {

    ::testing::InitGoogleTest (&argc, argv);

    custom_argc = argc;
    custom_argv = argv;

    return RUN_ALL_TESTS ();
}
