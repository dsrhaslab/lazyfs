#include <gtest/gtest.h>
#include <sys/statvfs.h>
#include <unistd.h>

using namespace std;

int custom_argc;
char** custom_argv;

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

} // namespace lazyfs::tests

int main (int argc, char** argv) {

    ::testing::InitGoogleTest (&argc, argv);

    custom_argc = argc;
    custom_argv = argv;

    return RUN_ALL_TESTS ();
}
