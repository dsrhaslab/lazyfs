
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

namespace lazyfs::tests {

TEST (LazyFSTests, SetupTest) {
    ASSERT_EQ (custom_argc, 2);
    int r = chdir (custom_argv[1]);
    ASSERT_TRUE (r >= 0);
}

TEST (WriteTests, WriteSeq1) {

    int fd = open ("WriteSeq1", O_CREAT | O_WRONLY, 0640);

    ASSERT_TRUE (fd >= 0);

    char buf[IO_BLOCK_SIZE];
    memset (buf, 0, IO_BLOCK_SIZE);

    // Write block 1 check size
    ASSERT_EQ (pwrite (fd, buf, IO_BLOCK_SIZE, 0), IO_BLOCK_SIZE);
    ASSERT_EQ (lseek (fd, 0, SEEK_END), IO_BLOCK_SIZE);

    // Append block 2 check size
    ASSERT_EQ (pwrite (fd, buf, IO_BLOCK_SIZE, IO_BLOCK_SIZE), IO_BLOCK_SIZE);
    ASSERT_EQ (lseek (fd, 0, SEEK_END), 2 * IO_BLOCK_SIZE);

    ASSERT_TRUE (close (fd) >= 0);
}

TEST (WriteTests, WriteSeq1CheckIntegrity) {

    int fd = open ("WriteSeq1", O_RDONLY, 0640);

    ASSERT_TRUE (fd >= 0);

    char buf[IO_BLOCK_SIZE];
    char buf_expected[IO_BLOCK_SIZE];
    memset (buf_expected, 0, IO_BLOCK_SIZE);

    ASSERT_EQ (pread (fd, buf, IO_BLOCK_SIZE, 0), IO_BLOCK_SIZE);
    ASSERT_TRUE (!memcmp (buf, buf_expected, IO_BLOCK_SIZE));
    ASSERT_EQ (pread (fd, buf, IO_BLOCK_SIZE, IO_BLOCK_SIZE), IO_BLOCK_SIZE);
    ASSERT_TRUE (!memcmp (buf, buf_expected, IO_BLOCK_SIZE));

    ASSERT_TRUE (close (fd) >= 0);

    ASSERT_TRUE (unlink ("WriteSeq1") >= 0);
}

TEST (WriteTests, WriteBlockEdges1) {

    int fd = open ("WriteSeq1WriteBlockEdges1", O_CREAT | O_WRONLY, 0640);

    ASSERT_TRUE (fd >= 0);

    char buf[IO_BLOCK_SIZE];
    memset (buf, 0, IO_BLOCK_SIZE);

    // Write block 1 check size
    ASSERT_EQ (pwrite (fd, buf, IO_BLOCK_SIZE, 0), IO_BLOCK_SIZE);
    ASSERT_EQ (lseek (fd, 0, SEEK_END), IO_BLOCK_SIZE);

    char edge_buf[2];
    memset (edge_buf, 'E', 2);

    // Append block 2 check size
    ASSERT_EQ (pwrite (fd, edge_buf, 2, IO_BLOCK_SIZE - 1), 2);
    ASSERT_EQ (lseek (fd, 0, SEEK_END), IO_BLOCK_SIZE + 1);

    // Write block 1 check size
    ASSERT_EQ (pwrite (fd, buf, IO_BLOCK_SIZE, 2 * IO_BLOCK_SIZE), IO_BLOCK_SIZE);
    ASSERT_EQ (lseek (fd, 0, SEEK_END), 3 * IO_BLOCK_SIZE);

    ASSERT_TRUE (close (fd) >= 0);
}

TEST (WriteTests, WriteBlockEdges1CheckIntegrity) {

    int fd = open ("WriteSeq1WriteBlockEdges1", O_RDONLY, 0640);

    ASSERT_TRUE (fd >= 0);

    char buf[IO_BLOCK_SIZE];
    char buf_expected[IO_BLOCK_SIZE];
    memset (buf_expected, 0, IO_BLOCK_SIZE);

    ASSERT_EQ (pread (fd, buf, IO_BLOCK_SIZE, 0), IO_BLOCK_SIZE);
    ASSERT_TRUE (!memcmp (buf, buf_expected, IO_BLOCK_SIZE - 1));

    ASSERT_EQ (buf[IO_BLOCK_SIZE - 1], 'E');
    ASSERT_EQ (pread (fd, buf, IO_BLOCK_SIZE, IO_BLOCK_SIZE), IO_BLOCK_SIZE);
    ASSERT_EQ (buf[0], 'E');
    ASSERT_EQ (buf[1], 0);

    ASSERT_TRUE (close (fd) >= 0);

    ASSERT_TRUE (unlink ("WriteSeq1WriteBlockEdges1") >= 0);
}

TEST (WriteTests, WriteSparseBlocks1) {

    int fd = open ("WriteSparseBlocks1", O_CREAT | O_WRONLY, 0640);

    ASSERT_TRUE (fd >= 0);

    char buf[IO_BLOCK_SIZE];
    memset (buf, 'A', 10);

    // Write 10 bytes at the end of block 0
    ASSERT_EQ (pwrite (fd, buf, 10, IO_BLOCK_SIZE - 10), 10);
    ASSERT_EQ (lseek (fd, 0, SEEK_END), IO_BLOCK_SIZE);

    // Write 20 bytes at block 1 to 2 (-10 -> +10)
    memset (buf, 'B', 20);
    ASSERT_EQ (pwrite (fd, buf, 20, 2 * IO_BLOCK_SIZE - 10), 20);
    ASSERT_EQ (lseek (fd, 0, SEEK_END), 2 * IO_BLOCK_SIZE + 10);

    // Rewrite 20 bytes from block

    ASSERT_TRUE (close (fd) >= 0);
}

TEST (WriteTests, WriteSparseBlocks1CheckIntegrity) {

    int fd = open ("WriteSparseBlocks1", O_RDONLY, 0640);

    ASSERT_TRUE (fd >= 0);

    char buf[IO_BLOCK_SIZE * 2 + 10];
    char buf_expected[IO_BLOCK_SIZE * 2 + 10];
    memset (buf_expected, 0, IO_BLOCK_SIZE - 10);
    memset (buf_expected + IO_BLOCK_SIZE - 10, 'A', 10);
    memset (buf_expected + IO_BLOCK_SIZE, 0, IO_BLOCK_SIZE - 10);
    memset (buf_expected + IO_BLOCK_SIZE * 2 - 10, 'B', 20);

    ASSERT_EQ (pread (fd, buf, IO_BLOCK_SIZE * 2 + 10, 0), IO_BLOCK_SIZE * 2 + 10);
    ASSERT_TRUE (!memcmp (buf, buf_expected, IO_BLOCK_SIZE * 2 + 10));

    ASSERT_TRUE (close (fd) >= 0);

    ASSERT_TRUE (unlink ("WriteSparseBlocks1") >= 0);
}

} // namespace lazyfs::tests

int main (int argc, char** argv) {

    ::testing::InitGoogleTest (&argc, argv);

    custom_argc = argc;
    custom_argv = argv;

    return RUN_ALL_TESTS ();
}
