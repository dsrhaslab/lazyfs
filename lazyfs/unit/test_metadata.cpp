#include <dirent.h>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <setjmp.h>
#include <signal.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

using namespace std;

int custom_argc;
char** custom_argv;

static sigjmp_buf sigbus_jump;

static void on_sigbus (int) { siglongjmp (sigbus_jump, 1); }

namespace lazyfs::tests {

TEST (LazyFSTests, SetupTest) {
    ASSERT_EQ (custom_argc, 2);
    int r = chdir (custom_argv[1]);
    ASSERT_TRUE (r >= 0);
}

// Programs that check for free space before writing must see the backing
// filesystem's space, not zero.
TEST (MetadataTests, StatfsReportsBackingSpace) {
    struct statvfs st;
    ASSERT_EQ (statvfs (".", &st), 0);
    EXPECT_GT (st.f_blocks, 0u);
    EXPECT_GT (st.f_bavail, 0u);
}

// Listing a directory must not change what the kernel believes about the files in it. With
// incomplete attributes (size 0) in the listing, the kernel shrank the file to 0 bytes: reads
// through an existing mapping got SIGBUS, and dirty pages were dropped.
TEST (MetadataTests, ListingKeepsFileSizes) {
    const size_t size = 1 << 20;
    ASSERT_EQ (mkdir ("ListingKeepsFileSizes", 0750), 0);

    int fd = open ("ListingKeepsFileSizes/file", O_CREAT | O_RDWR, 0640);
    ASSERT_TRUE (fd >= 0);
    ASSERT_EQ (ftruncate (fd, size), 0);
    char* map = (char*)mmap (NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    ASSERT_NE (map, MAP_FAILED);
    memset (map, 7, 64);

    DIR* dir = opendir ("ListingKeepsFileSizes");
    ASSERT_NE (dir, nullptr);
    while (readdir (dir) != NULL) {}
    closedir (dir);

    struct sigaction sa = {}, old_sa;
    sa.sa_handler       = on_sigbus;
    sigaction (SIGBUS, &sa, &old_sa);
    volatile char seen = 0;
    bool sigbus        = sigsetjmp (sigbus_jump, 1) != 0;
    if (!sigbus)
        seen = map[57];
    sigaction (SIGBUS, &old_sa, NULL);

    EXPECT_FALSE (sigbus) << "SIGBUS reading through the mapping after listing the directory";
    EXPECT_EQ (seen, 7) << "write through the mapping was lost after listing the directory";

    struct stat st;
    ASSERT_EQ (fstat (fd, &st), 0);
    EXPECT_EQ ((size_t)st.st_size, size);

    munmap (map, size);
    close (fd);
    unlink ("ListingKeepsFileSizes/file");
    rmdir ("ListingKeepsFileSizes");
}

} // namespace lazyfs::tests

int main (int argc, char** argv) {

    ::testing::InitGoogleTest (&argc, argv);

    custom_argc = argc;
    custom_argv = argv;

    return RUN_ALL_TESTS ();
}
