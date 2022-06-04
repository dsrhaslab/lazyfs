
#include <errno.h>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <mntent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define IO_BLOCK_SIZE 4096

using namespace std;

int custom_argc;
char** custom_argv;

void clear_cache_command () {
    system ("echo lazyfs::clear-cache > ~/faults-example.fifo");
    sleep (0.5);
}

namespace lazyfs::tests {

TEST (LazyFSTests, SetupTest) {
    ASSERT_EQ (custom_argc, 2);
    int r = chdir (custom_argv[1]);
    ASSERT_TRUE (r >= 0);
}

TEST (ConsitencyAfterCacheClearTests, ReadBlocks_SyncOff) {

    int fd = open ("WriteSparseBlocks1", O_CREAT | O_RDWR, 0640);

    ASSERT_TRUE (fd >= 0);

    char buf[IO_BLOCK_SIZE];
    memset (buf, 'A', IO_BLOCK_SIZE);
    ASSERT_EQ (pwrite (fd, buf, IO_BLOCK_SIZE, 0), IO_BLOCK_SIZE);

    clear_cache_command ();

    ASSERT_EQ (pread (fd, buf, IO_BLOCK_SIZE, 0), 0);

    ASSERT_TRUE (close (fd) >= 0);
}

TEST (ConsitencyAfterCacheClearTests, ReadBlocks_SyncOn) {}

} // namespace lazyfs::tests

int main (int argc, char** argv) {

    ::testing::InitGoogleTest (&argc, argv);

    custom_argc = argc;
    custom_argv = argv;

    return RUN_ALL_TESTS ();
}
