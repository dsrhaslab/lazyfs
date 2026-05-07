
/**
 * @file lazyfs_ops_attr.cpp
 * @author João Azevedo joao.azevedo@inesctec.pt
 *
 * @copyright Copyright (c) 2020-2022 INESC TEC.
 *
 */

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <dirent.h>
#include <iostream>
#include <map>
#include <regex>
#include <signal.h>
#include <spdlog/spdlog.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <tuple>
#include <unistd.h>
#include <vector>
#include <filesystem>

// LazyFS specific imports
#include <cache/cache.hpp>
#include <cache/config/config.hpp>
#include <cache/constants/constants.hpp>
#include <cache/engine/backends/custom/custom_cache.hpp>
#include <lazyfs/fusepp/Fuse-impl.h>
#include <lazyfs/lazyfs.hpp>
#include <faults_handler.hpp>

using namespace std;
using namespace cache;
using namespace cache::engine::backends::custom;

extern std::shared_mutex cache_command_lock;

namespace lazyfs {

int LazyFS::lfs_access (const char* path, int mask) {

    this_ ()->check_kill_before ();

    this_ ()->trigger_crash_fault ("access", "before", path, "");
    this_ ()->trigger_configured_clear_fault ("access", "before", path, "");

    std::shared_lock<std::shared_mutex> lock (cache_command_lock);

    if (this_ ()->FSConfig->log_all_operations) {
        spdlog::info ("[lazyfs.ops]: {}(path={})", __FUNCTION__, path);
    }

    int res;

    res = access (path, mask);
    if (res == -1)
        return -errno;

    this_ ()->trigger_crash_fault ("access", "after", path, "", false);
    this_ ()->trigger_configured_clear_fault ("access", "after", path, "", false);

    return 0;
}

int LazyFS::lfs_chmod (const char* path, mode_t mode, struct fuse_file_info* fi) {

    this_ ()->check_kill_before ();

    std::shared_lock<std::shared_mutex> lock (cache_command_lock);

    if (this_ ()->FSConfig->log_all_operations) {
        spdlog::info ("[lazyfs.ops]: {}(path={})", __FUNCTION__, path);
    }

    (void)fi;
    int res;

    res = chmod (path, mode);
    if (res == -1)
        return -errno;

    return 0;
}

int LazyFS::lfs_chown (const char* path, uid_t uid, gid_t gid, fuse_file_info* fi) {

    this_ ()->check_kill_before ();

    std::shared_lock<std::shared_mutex> lock (cache_command_lock);

    if (this_ ()->FSConfig->log_all_operations) {
        spdlog::info ("[lazyfs.ops]: {}(path={})", __FUNCTION__, path);
    }

    (void)fi;
    int res;

    res = lchown (path, uid, gid);
    if (res == -1)
        return -errno;

    return 0;
}

#ifdef HAVE_SETXATTR

int LazyFS::lfs_setxattr (const char* path,
                          const char* name,
                          const char* value,
                          size_t size,
                          int flags) {

    int res = lsetxattr (path, name, value, size, flags);

    if (res == -1)
        return -errno;

    return 0;
}

int LazyFS::lfs_getxattr (const char* path, const char* name, char* value, size_t size) {

    // //std::printf ("?\n");

    int res = lgetxattr (path, name, value, size);

    if (res == -1)
        return -errno;

    return res;
}

int LazyFS::lfs_listxattr (const char* path, char* list, size_t size) {

    int res = llistxattr (path, list, size);

    if (res == -1)
        return -errno;

    return res;
}

int LazyFS::lfs_removexattr (const char* path, const char* name) {

    int res = lremovexattr (path, name);

    if (res == -1)
        return -errno;

    return 0;
}
#endif /* HAVE_SETXATTR */

// #ifdef HAVE_UTIMENSAT
int LazyFS::lfs_utimens (const char* path, const struct timespec ts[2], struct fuse_file_info* fi) {

    std::shared_lock<std::shared_mutex> lock (cache_command_lock);

    if (this_ ()->FSConfig->log_all_operations) {
        spdlog::info ("[lazyfs.ops]: {}(path={})", __FUNCTION__, path);
    }

    (void)fi;
    int res;

    /* don't use utime/utimes since they follow symlinks */
    res = utimensat (0, path, ts, AT_SYMLINK_NOFOLLOW);
    if (res == -1)
        return -errno;

    return 0;
}
// #endif

} // namespace lazyfs
