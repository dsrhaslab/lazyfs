
/**
 * @file lazyfs_ops_dir.cpp
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

int LazyFS::lfs_readdir (const char* path,
                         void* buf,
                         fuse_fill_dir_t filler,
                         off_t offset,
                         struct fuse_file_info* fi,
                         enum fuse_readdir_flags flags) {

    std::shared_lock<std::shared_mutex> lock (cache_command_lock);

    if (this_ ()->FSConfig->log_all_operations) {
        spdlog::info ("[lazyfs.ops]: {}(path={},off={})", __FUNCTION__, path, offset);
    }

    DIR* dp;
    struct dirent* de;

    (void)offset;
    (void)fi;
    (void)flags;

    dp = opendir (path);
    if (dp == NULL)
        return -errno;

    while ((de = readdir (dp)) != NULL) {
        struct stat st;
        memset (&st, 0, sizeof (st));
        st.st_ino  = de->d_ino;
        st.st_mode = de->d_type << 12;
        if (filler (buf, de->d_name, &st, 0, FUSE_FILL_DIR_PLUS))
            break;
    }

    closedir (dp);

    return 0;
}

int LazyFS::lfs_is_dir_empty (const char* dirname) {

    this_ ()->check_kill_before ();

    struct dirent* from_DIRENT;
    DIR* from_DIR = opendir (dirname);

    if (!from_DIR)
        return -1;

    int nr_files = 0;

    while ((from_DIRENT = readdir (from_DIR)) != NULL) {

        if (++nr_files > 2)
            break;
    }

    closedir (from_DIR);

    return (nr_files <= 2) ? 1 : 0;
}

void LazyFS::lfs_get_dir_filenames (const char* dirname, std::vector<string>* result) {

    struct dirent* from_DIRENT;
    DIR* from_DIR = opendir (dirname);

    if (!from_DIR)
        return;

    while ((from_DIRENT = readdir (from_DIR)) != NULL) {

        // skip "." and ".." folders
        if (strcmp (from_DIRENT->d_name, ".") != 0 && strcmp (from_DIRENT->d_name, "..") != 0) {

            string base_path = string (string (dirname) + "/" + string (from_DIRENT->d_name));

            if (from_DIRENT->d_type == DT_REG)
                result->push_back (base_path);

            lfs_get_dir_filenames (base_path.c_str (), result);
        }
    }

    closedir (from_DIR);
}

int LazyFS::lfs_recursive_rename (const char* from, const char* to, unsigned int flags) {

    std::vector<string> from_dir_filepaths;
    lfs_get_dir_filenames (from, &from_dir_filepaths);

    // Check if destination folder exists

    struct stat to_stat;
    if (!stat (to, &to_stat) && S_ISDIR (to_stat.st_mode)) {

        // destination folder exists and is a directory
        // prepend with new prepend and merge

        int is_empty = lfs_is_dir_empty (to);

        if (is_empty == 1) {

            goto replace_prepend;

        } else if (is_empty == 0) {

            errno = ENOTEMPTY;

            return -1;
        }

    } else {

    replace_prepend:

        // destination folder doesn't exist
        // all 'from' paths should be replaced with the new prepend

        for (auto const& it : from_dir_filepaths) {

            this_ ()->FSCache->rename_item (it, string (to + it.substr (string (from).size ())))
                ? 0
                : -1;
        }
    }

    return 0;
}

int LazyFS::lfs_rename (const char* from, const char* to, unsigned int flags) {

    this_ ()->check_kill_before ();

    this_ ()->trigger_crash_fault ("rename", "before", from, to);
    this_ ()->trigger_configured_clear_fault ("rename", "before", from, to);

    std::shared_lock<std::shared_mutex> lock (cache_command_lock);

    if (this_ ()->FSConfig->log_all_operations) {
        spdlog::info ("[lazyfs.ops]: {}(from={},to={})", __FUNCTION__, from, to);
    }

    int res;

    if (flags)
        return -EINVAL;

    string last_owner (from);
    string inode = this_ ()->FSCache->get_original_inode (last_owner);
    string new_owner (to);

    if (inode.empty ()) {

        // from: is a dir, because getattr does not cache dirs

        lfs_recursive_rename (from, to, flags);
        res = rename (from, to);

    } else {

        res = this_ ()->FSCache->rename_item (last_owner, new_owner) ? 0 : -1;
        res = rename (from, to);
    }

    if (res == -1)
        return -errno;

    this_ ()->trigger_crash_fault ("rename", "after", from, to, false);
    this_ ()->trigger_configured_clear_fault ("rename", "after", from, to, false);

    return 0;
}

int LazyFS::lfs_mkdir (const char* path, mode_t mode) {

    this_ ()->check_kill_before ();

    std::shared_lock<std::shared_mutex> lock (cache_command_lock);

    if (this_ ()->FSConfig->log_all_operations) {
        spdlog::info ("[lazyfs.ops]: {}(path={})", __FUNCTION__, path);
    }

    int res;

    res = mkdir (path, mode);
    if (res == -1)
        return -errno;

    return 0;
}

int LazyFS::lfs_rmdir (const char* path) {

    this_ ()->check_kill_before ();

    std::shared_lock<std::shared_mutex> lock (cache_command_lock);

    if (this_ ()->FSConfig->log_all_operations) {
        spdlog::info ("[lazyfs.ops]: {}(path={})", __FUNCTION__, path);
    }

    int res;

    res = rmdir (path);
    if (res == -1)
        return -errno;

    return 0;
}

} // namespace lazyfs
