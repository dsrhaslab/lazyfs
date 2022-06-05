
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
using namespace std::chrono_literals;

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

TEST (ConsitencyAfterCacheClearTests, SimpleReadBlocks_SyncOff) {

    int fd = open ("SimpleReadBlocks_SyncOff", O_CREAT | O_RDWR, 0640);

    ASSERT_TRUE (fd >= 0);

    char buf[IO_BLOCK_SIZE];
    memset (buf, 'A', IO_BLOCK_SIZE);
    ASSERT_EQ (pwrite (fd, buf, IO_BLOCK_SIZE, 0), IO_BLOCK_SIZE);

    ASSERT_EQ (clear_cache_command (), 0);

    ASSERT_EQ (pread (fd, buf, IO_BLOCK_SIZE, 0), 0);

    ASSERT_EQ (pwrite (fd, buf, IO_BLOCK_SIZE, 0), IO_BLOCK_SIZE);
    ASSERT_EQ (pread (fd, buf, IO_BLOCK_SIZE, 0), IO_BLOCK_SIZE);

    // ASSERT_EQ (clear_cache_command (), 0);
    // ASSERT_EQ (pread (fd, buf, IO_BLOCK_SIZE, 0), IO_BLOCK_SIZE);

    ASSERT_TRUE (close (fd) >= 0);

    ASSERT_TRUE (unlink ("SimpleReadBlocks_SyncOff") >= 0);
}

TEST (ConsitencyAfterCacheClearTests, SimpleReadBlocks_SyncOn) {

    int fd = open ("SimpleReadBlocks_SyncOn", O_CREAT | O_RDWR, 0640);

    ASSERT_TRUE (fd >= 0);

    char buf[IO_BLOCK_SIZE];
    memset (buf, 'A', IO_BLOCK_SIZE);
    ASSERT_EQ (pwrite (fd, buf, IO_BLOCK_SIZE, 0), IO_BLOCK_SIZE);

    ASSERT_TRUE (fsync (fd) >= 0);

    ASSERT_EQ (clear_cache_command (), 0);

    ASSERT_EQ (pread (fd, buf, IO_BLOCK_SIZE, 0), IO_BLOCK_SIZE);

    ASSERT_EQ (pwrite (fd, buf, IO_BLOCK_SIZE, 0), IO_BLOCK_SIZE);
    ASSERT_EQ (pread (fd, buf, IO_BLOCK_SIZE, 0), IO_BLOCK_SIZE);

    ASSERT_EQ (clear_cache_command (), 0);

    ASSERT_EQ (pread (fd, buf, IO_BLOCK_SIZE, 0), IO_BLOCK_SIZE);

    ASSERT_TRUE (close (fd) >= 0);

    ASSERT_TRUE (unlink ("SimpleReadBlocks_SyncOn") >= 0);
}

TEST (ConsitencyAfterCacheClearTests, SequentialRW_SyncOn) {

    int fd = open ("SequentialRW_SyncOn", O_CREAT | O_RDWR, 0640);

    ASSERT_TRUE (fd >= 0);

    char buf[IO_BLOCK_SIZE];
    memset (buf, 'A', IO_BLOCK_SIZE);
    ASSERT_EQ (pwrite (fd, buf, IO_BLOCK_SIZE, 0), IO_BLOCK_SIZE);
    ASSERT_EQ (lseek (fd, 0, SEEK_END), IO_BLOCK_SIZE);

    ASSERT_TRUE (fsync (fd) >= 0);
    ASSERT_EQ (clear_cache_command (), 0);

    memset (buf, 'B', IO_BLOCK_SIZE);
    ASSERT_EQ (pwrite (fd, buf, IO_BLOCK_SIZE, IO_BLOCK_SIZE), IO_BLOCK_SIZE);
    ASSERT_EQ (lseek (fd, 0, SEEK_END), 2 * IO_BLOCK_SIZE);

    ASSERT_TRUE (fsync (fd) >= 0);
    ASSERT_EQ (clear_cache_command (), 0);

    char buf_read[2 * IO_BLOCK_SIZE];
    char buf_expected[2 * IO_BLOCK_SIZE];
    memset (buf_expected, 'A', IO_BLOCK_SIZE);
    memset (buf_expected + IO_BLOCK_SIZE, 'B', IO_BLOCK_SIZE);

    ASSERT_EQ (pread (fd, buf_read, 3 * IO_BLOCK_SIZE, 0), 2 * IO_BLOCK_SIZE);
    ASSERT_TRUE (!memcmp (buf_read, buf_expected, IO_BLOCK_SIZE * 2));

    ASSERT_TRUE (close (fd) >= 0);

    ASSERT_TRUE (unlink ("SequentialRW_SyncOn") >= 0);
}

TEST (ConsitencyAfterCacheClearTests, SparseSyncsConsistencyCheck) {

    int fd = open ("SparseSyncsConsistencyCheck", O_CREAT | O_RDWR, 0640);

    ASSERT_TRUE (fd >= 0);

    char buf[IO_BLOCK_SIZE];
    memset (buf, 'A', IO_BLOCK_SIZE);
    ASSERT_EQ (pwrite (fd, buf, IO_BLOCK_SIZE, 0), IO_BLOCK_SIZE);
    ASSERT_EQ (lseek (fd, 0, SEEK_END), IO_BLOCK_SIZE);

    ASSERT_TRUE (fsync (fd) >= 0);
    ASSERT_EQ (clear_cache_command (), 0);

    memset (buf, 'B', IO_BLOCK_SIZE);
    ASSERT_EQ (pwrite (fd, buf, IO_BLOCK_SIZE, 0), IO_BLOCK_SIZE);
    ASSERT_EQ (lseek (fd, 0, SEEK_END), IO_BLOCK_SIZE);

    ASSERT_EQ (clear_cache_command (), 0);

    char buf_read[IO_BLOCK_SIZE];
    char buf_expected[IO_BLOCK_SIZE];
    memset (buf_expected, 'A', IO_BLOCK_SIZE);

    ASSERT_EQ (pread (fd, buf_read, 2 * IO_BLOCK_SIZE, 0), IO_BLOCK_SIZE);
    ASSERT_TRUE (!memcmp (buf_read, buf_expected, IO_BLOCK_SIZE));

    ASSERT_EQ (pwrite (fd, buf, IO_BLOCK_SIZE, 2 * IO_BLOCK_SIZE), IO_BLOCK_SIZE);
    ASSERT_EQ (lseek (fd, 0, SEEK_END), 3 * IO_BLOCK_SIZE);

    ASSERT_EQ (clear_cache_command (), 0);

    // ASSERT_EQ (lseek (fd, 0, SEEK_END), IO_BLOCK_SIZE);

    ASSERT_TRUE (close (fd) >= 0);

    ASSERT_TRUE (unlink ("SparseSyncsConsistencyCheck") >= 0);
}

} // namespace lazyfs::tests

int main (int argc, char** argv) {

    ::testing::InitGoogleTest (&argc, argv);

    custom_argc = argc;
    custom_argv = argv;

    return RUN_ALL_TESTS ();
}
