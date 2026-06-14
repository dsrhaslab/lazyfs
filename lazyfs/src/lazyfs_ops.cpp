
/**
 * @file lazyfs_ops.cpp
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

/************ LazyFS FUSE operations ***************/

void* LazyFS::lfs_init (struct fuse_conn_info* conn, struct fuse_config* cfg) {

    (void)conn;

    cfg->entry_timeout    = 0;
    cfg->attr_timeout     = 0;
    cfg->negative_timeout = 0;
    cfg->use_ino          = 1;
    // cfg->direct_io        = 1;

    new (this_ ()->faults_handler_thread) std::thread (fht_worker, this_ (), this_ ()->FSConfig);

    // Checking snapshot folders to get the next snapshot index
    if (this_ ()->FSConfig->SNAPSHOT_SAVE != "") {
        int maxIndex = -1;
        regex snapshot_save_regex(R"(snapshot(\d+))");

        for (const auto& entry : filesystem::directory_iterator(this_ ()->FSConfig->SNAPSHOT_SAVE)) {
            if (entry.is_directory()) {
                string folderName = entry.path().filename().string();
                smatch match;
                if (regex_match(folderName, match, snapshot_save_regex)) {
                    int index = stoi(match[1]);
                    maxIndex = max(maxIndex, index);
                }
            }
        }

        if (maxIndex == -1)
            this_ ()->snapshot_counter.store(0);
        else
            this_ ()->snapshot_counter.store(maxIndex + 1);

        //Taking snapshots when starting LazyFS
        if (filesystem::directory_iterator(this_ ()->root_dir) != filesystem::end(filesystem::directory_iterator{})) {
            this_ ()->command_snapshot_files (regex(this_ ()->FSConfig->SNAPSHOT_FILES), this_ ()->FSConfig->SNAPSHOT_SAVE);
        }
    }

    return this_ ();
}

void LazyFS::lfs_destroy (void*) {
    if (this_ ()->snapshot_counter.load () >= 0)
        this_ ()->command_snapshot_files (regex(this_ ()->FSConfig->SNAPSHOT_FILES), this_ ()->FSConfig->SNAPSHOT_SAVE);
    spdlog::info ("[lazyfs]: stopping LazyFS...");
}

int LazyFS::lfs_getattr (const char* path, struct stat* stbuf, struct fuse_file_info* fi) {

    std::shared_lock<std::shared_mutex> lock (cache_command_lock);

    if (this_ ()->FSConfig->log_all_operations) {
        spdlog::info ("[lazyfs.ops]: {}(path={})", __FUNCTION__, path);
    }

    (void)fi;
    int res;

    string content_owner (path);

    res = lstat (path, stbuf);

    if (res == -1)
        return -errno;

    if (not S_ISREG (stbuf->st_mode))
        return 0;

    string inode = this_ ()->FSCache->get_original_inode (content_owner);

    if (inode.empty ()) {
        inode = to_string (stbuf->st_ino);
        this_ ()->FSCache->insert_inode_mapping (content_owner, inode, false);
    }

    bool locked = this_ ()->FSCache->lockItemCheckExists (inode);

    if (!locked) {

        this_ ()->FSCache->put_data_blocks (inode, {}, OP_PASSTHROUGH);

        bool locked_now = this_ ()->FSCache->lockItemCheckExists (inode);

        if (locked_now) {

            Metadata meta;

            meta.size   = stbuf->st_size;
            meta.atim   = stbuf->st_atim;
            meta.ctim   = stbuf->st_ctim;
            meta.mtim   = stbuf->st_mtim;
            meta.nlinks = stbuf->st_nlink;

            this_ ()->FSCache->update_content_metadata (
                inode,
                meta,
                {"size", "atime", "ctime", "mtime", "nlinks"});

            this_ ()->FSCache->unlockItem (inode);
        }

    } else if (locked) {

        /*
        Content is cached, must return cached metadata
        to override existing values:
        - size, atime, ctime, mtime
    */

        Metadata* meta = this_ ()->FSCache->get_content_metadata (inode);

        if (meta != nullptr) {

            off_t get_size            = meta->size;
            blkcnt_t blocks_ioblksize = 0;

            if (get_size > 0)
                blocks_ioblksize = ((get_size - 1) / this_ ()->FSConfig->IO_BLOCK_SIZE) + 1;

            blkcnt_t blocks = (blocks_ioblksize * this_ ()->FSConfig->IO_BLOCK_SIZE) /
                              this_ ()->FSConfig->DISK_SECTOR_SIZE;

            stbuf->st_size         = get_size;
            stbuf->st_blocks       = blocks;
            stbuf->st_atim.tv_nsec = meta->atim.tv_nsec;
            stbuf->st_atim.tv_sec  = meta->atim.tv_sec;
            stbuf->st_ctim.tv_nsec = meta->ctim.tv_nsec;
            stbuf->st_ctim.tv_sec  = meta->ctim.tv_sec;
            stbuf->st_mtim.tv_nsec = meta->mtim.tv_nsec;
            stbuf->st_mtim.tv_sec  = meta->mtim.tv_sec;
            stbuf->st_nlink        = meta->nlinks;
        }

        this_ ()->FSCache->unlockItem (inode);
    }

    return 0;
}

int LazyFS::lfs_open (const char* path, struct fuse_file_info* fi) {

    this_ ()->check_kill_before ();

    this_ ()->trigger_crash_fault ("open", "before", path, "");
    this_ ()->trigger_configured_clear_fault ("open", "before", path, "");

    std::shared_lock<std::shared_mutex> lock (cache_command_lock);

    int res;

    int access_tmp = fi->flags;
    if (fi->flags & O_TRUNC)
        access_tmp = O_WRONLY;

    res = open (path, access_tmp);

    if (res == -1)
        return -errno;

    fi->fh = res;

    struct stat st;
    lfs_getattr (path, &st, fi);

    // --------------------------------------------------------------------------

    int access_mode = fi->flags & O_ACCMODE;

    string access_mode_str = "OTHER";

    if (fi->flags & O_TRUNC)
        access_mode_str = "O_TRUNC";
    else if (access_mode == O_WRONLY)
        access_mode_str = "O_WRONLY";
    else if (access_mode == O_RDONLY)
        access_mode_str = "O_RDONLY";
    else if (access_mode == O_RDWR)
        access_mode_str = "O_RDWR";

    if (this_ ()->FSConfig->log_all_operations) {
        spdlog::info ("[lazyfs.ops]: {}(path={},mode={})", __FUNCTION__, path, access_mode_str);
    }

    if (fi->flags & O_TRUNC)
        lfs_truncate (path, 0, fi);

    this_ ()->trigger_crash_fault ("open", "after", path, "", false);
    this_ ()->trigger_configured_clear_fault ("open", "after", path, "", false);

    return 0;
}

int LazyFS::lfs_create (const char* path, mode_t mode, struct fuse_file_info* fi) {

    this_ ()->check_kill_before ();

    this_ ()->trigger_crash_fault ("create", "before", path, "");
    this_ ()->trigger_configured_clear_fault ("create", "before", path, "");

    std::shared_lock<std::shared_mutex> lock (cache_command_lock);

    int res;

    res = open (path, fi->flags, mode);

    if (res == -1)
        return -errno;

    fi->fh = res;

    struct stat stbuf;
    lfs_getattr (path, &stbuf, fi);

    string owner (path);
    string inode = to_string (stbuf.st_ino);

    int access_mode = fi->flags & O_ACCMODE;

    string access_mode_str = "OTHER";

    if (fi->flags & O_TRUNC)
        access_mode_str = "O_TRUNC";
    else if (access_mode == O_WRONLY)
        access_mode_str = "O_WRONLY";
    else if (access_mode == O_RDONLY)
        access_mode_str = "O_RDONLY";
    else if (access_mode == O_RDWR)
        access_mode_str = "O_RDWR";

    if (this_ ()->FSConfig->log_all_operations) {
        spdlog::info ("[lazyfs.ops]: {}(path={},mode={})", __FUNCTION__, path, access_mode_str);
    }

    struct timespec time_register;
    clock_gettime (CLOCK_REALTIME, &time_register);
    Metadata meta;
    vector<string> update_meta_values;

    if (!this_ ()->FSCache->has_content_cached (inode)) {
        // just to cache the metadata
        this_ ()->FSCache->put_data_blocks (inode, {}, OP_PASSTHROUGH);
        update_meta_values.push_back ("atime");
        meta.atim = time_register;
    }

    if ((access_mode == O_RDONLY) || (access_mode == O_RDWR) || (access_mode == O_CREAT)) {

        meta.atim = time_register;
        update_meta_values.push_back ("atime");
    }

    if ((access_mode == O_WRONLY) || (access_mode == O_RDWR)) {

        meta.mtim = time_register;
        update_meta_values.push_back ("mtime");
    }

    bool locked = this_ ()->FSCache->lockItemCheckExists (inode);
    if (locked) {
        this_ ()->FSCache->update_content_metadata (inode, meta, update_meta_values);
        this_ ()->FSCache->unlockItem (inode);
    }

    this_ ()->trigger_crash_fault ("create", "before", path, "", false);
    this_ ()->trigger_configured_clear_fault ("create", "before", path, "", false);
    return 0;
}

int LazyFS::lfs_fsync (const char* path, int isdatasync, struct fuse_file_info* fi) {

    this_ ()->check_kill_before ();

    this_ ()->trigger_crash_fault ("fsync", "before", path, "");
    this_ ()->trigger_configured_clear_fault ("fsync", "before", path, "");

    std::shared_lock<std::shared_mutex> lock (cache_command_lock);

    struct stat stats;
    lfs_getattr (path, &stats, fi);

    if (this_ ()->FSConfig->log_all_operations) {
        spdlog::info ("[lazyfs.ops]: {}(path={},isdatasync={})", __FUNCTION__, path, isdatasync);
    }

    this_ ()->restart_counter (path, "write");      // Reset counter of op write for this path
    this_ ()->check_and_delete_pendingwrite (path); // If there's a pending write, remove it

    string owner (path);
    string inode = this_ ()->FSCache->get_original_inode (owner);

    bool is_owner_cached = this_ ()->FSCache->has_content_cached (inode);

    int res = this_ ()->FSCache->sync_owner (inode, isdatasync, (char*)path);

    if (!is_owner_cached) {
        res = isdatasync ? fdatasync (fi->fh) : fsync (fi->fh);
    }

    this_ ()->trigger_crash_fault ("fsync", "after", path, "", false);
    this_ ()->trigger_configured_clear_fault ("fsync", "after", path, "", false);

    return res ? 0 : 1;
}

int LazyFS::lfs_truncate (const char* path, off_t truncate_size, struct fuse_file_info* fi) {
    cout << ">> FTRUNCATE" << endl;

    this_ ()->check_kill_before ();

    this_ ()->trigger_crash_fault ("truncate", "before", path, "");
    this_ ()->trigger_configured_clear_fault ("truncate", "before", path, "");

    std::shared_lock<std::shared_mutex> lock (cache_command_lock);

    struct stat stats;
    lfs_getattr (path, &stats, fi);

    if (this_ ()->FSConfig->log_all_operations) {
        spdlog::info ("[lazyfs.ops]: {}(path={},size={})", __FUNCTION__, path, truncate_size);
    }

    int res;

    string owner (path);
    string inode = this_ ()->FSCache->get_original_inode (owner);

    bool locked_initial = this_ ()->FSCache->lockItemCheckExists (inode);

    Metadata* previous_metadata;

    if (locked_initial) {

        previous_metadata = this_ ()->FSCache->get_content_metadata (inode);

        this_ ()->FSCache->unlockItem (inode);

    } else {

        if (fi != NULL)
            res = ftruncate (fi->fh, 0);
        else
            res = truncate (path, 0);

        lfs_getattr (path, &stats, fi);
        string newino = to_string (stats.st_ino);
        if (this_ ()->FSCache->lockItemCheckExists (newino)) {
            previous_metadata = this_ ()->FSCache->get_content_metadata (newino);
            this_ ()->FSCache->unlockItem (newino);
            inode = newino;
        } else
            return -1;
    }

    bool has_content_cached  = previous_metadata != nullptr;
    bool behave_as_lfs_write = false;

    if (not has_content_cached) {

        /*
            If getattr is enabled for every call on read/write/open...the content
            should be already cached when calling it.
        */

        behave_as_lfs_write = true;
    }

    if (has_content_cached && truncate_size == previous_metadata->size) {

        behave_as_lfs_write = false;

        // Since truncate size is the same as the file size, there's nothing to change/truncate.
    }

    if (has_content_cached && truncate_size > previous_metadata->size) {

        behave_as_lfs_write = true;
        cout << "***************************** BEHAVE AS " << endl;
    }

    if (behave_as_lfs_write) {

        /*
            In this case, cache file data to reach the size to truncate with zeroes.
            1: Content does not exist? Fill 0 -> size with zeroes
            2: Content exists? Fill size_before -> size with zeros
        */

        size_t IO_BLOCK_SIZE        = this_ ()->FSConfig->IO_BLOCK_SIZE;
        off_t file_size             = has_content_cached ? previous_metadata->size : 0;
        off_t add_bytes_from_offset = file_size;
        off_t add_bytes_total       = truncate_size - add_bytes_from_offset;

        char* buf = (char*)malloc (add_bytes_total);
        memset (buf, 0, add_bytes_total);

        int r = lfs_write (path, buf, add_bytes_total, add_bytes_from_offset, fi);

        if (r != add_bytes_total) {

            int res;

            if (fi != NULL)
                res = ftruncate (fi->fh, truncate_size);
            else
                res = truncate (path, truncate_size);
        }

        free (buf);

    } else if (truncate_size < previous_metadata->size) {

        this_ ()->FSCache->truncate_item (owner, truncate_size);
    }

    // todo: update file size

    if (previous_metadata != nullptr) {

        Metadata new_meta;
        new_meta.size = truncate_size;

        vector<string> values_to_change;
        values_to_change.push_back ("size");

        struct timespec modify_time;
        clock_gettime (CLOCK_REALTIME, &modify_time);
        new_meta.mtim = modify_time;
        values_to_change.push_back ("mtime");

        if (new_meta.size > previous_metadata->size) {
            new_meta.ctim = modify_time;
            values_to_change.push_back ("ctime");
        }

        bool locked = this_ ()->FSCache->lockItemCheckExists (inode);

        if (locked) {

            this_ ()->FSCache->update_content_metadata (inode, new_meta, values_to_change);
            this_ ()->FSCache->unlockItem (inode);
        }
    }

    if (res == -1)
        return -errno;

    this_ ()->trigger_crash_fault ("truncate", "after", path, "", false);
    this_ ()->trigger_configured_clear_fault ("truncate", "after", path, "", false);

    return 0;
}

int LazyFS::lfs_symlink (const char* from, const char* to) {

    this_ ()->check_kill_before ();

    this_ ()->trigger_crash_fault ("symlink", "before", from, to);
    this_ ()->trigger_configured_clear_fault ("symlink", "before", from, to);

    std::shared_lock<std::shared_mutex> lock (cache_command_lock);

    if (this_ ()->FSConfig->log_all_operations) {
        spdlog::info ("[lazyfs.ops]: {}(from={},to={})", __FUNCTION__, from, to);
    }

    int res;

    res = symlink (from, to);
    if (res == -1)
        return -errno;

    this_ ()->trigger_crash_fault ("symlink", "after", from, to, false);
    this_ ()->trigger_configured_clear_fault ("symlink", "after", from, to, false);
    return 0;
}

int LazyFS::lfs_link (const char* from, const char* to) {

    this_ ()->check_kill_before ();

    this_ ()->trigger_crash_fault ("link", "before", from, to);
    this_ ()->trigger_configured_clear_fault ("link", "before", from, to);

    std::shared_lock<std::shared_mutex> lock (cache_command_lock);

    if (this_ ()->FSConfig->log_all_operations) {
        spdlog::info ("[lazyfs.ops]: {}(from={},to={})", __FUNCTION__, from, to);
    }

    int res;

    res = link (from, to);

    if (res == -1)
        return -errno;

    string inode = this_ ()->FSCache->get_original_inode (from);
    if (!inode.empty ())
        this_ ()->FSCache->insert_inode_mapping (to, inode, true);

    this_ ()->trigger_crash_fault ("link", "after", from, to, false);
    this_ ()->trigger_configured_clear_fault ("link", "after", from, to, false);

    return 0;
}

int LazyFS::lfs_readlink (const char* path, char* buf, size_t size) {

    this_ ()->check_kill_before ();

    std::shared_lock<std::shared_mutex> lock (cache_command_lock);

    if (this_ ()->FSConfig->log_all_operations) {
        spdlog::info ("[lazyfs.ops]: {}(path={},size={})", __FUNCTION__, path, size);
    }

    int res;

    res = readlink (path, buf, size - 1);
    if (res == -1)
        return -errno;

    buf[res] = '\0';
    return 0;
}

int LazyFS::lfs_release (const char* path, struct fuse_file_info* fi) {

    this_ ()->check_kill_before ();

    std::shared_lock<std::shared_mutex> lock (cache_command_lock);

    if (this_ ()->FSConfig->log_all_operations) {
        spdlog::info ("[lazyfs.ops]: {}(path={})", __FUNCTION__, path);
    }

    (void)path;

    close (fi->fh);

    return 0;
}

off_t LazyFS::lfs_lseek (const char* path, off_t off, int whence, struct fuse_file_info* fi) {

    std::shared_lock<std::shared_mutex> lock (cache_command_lock);

    if (this_ ()->FSConfig->log_all_operations) {
        spdlog::info ("[lazyfs.ops]: {}(path={}, off={})", __FUNCTION__, path, off);
    }

    int fd;
    off_t res;

    if (fi == NULL)
        fd = open (path, O_RDONLY);
    else
        fd = fi->fh;

    if (fd == -1)
        return -errno;

    res = lseek (fd, off, whence);
    if (res == -1)
        res = -errno;

    if (fi == NULL)
        close (fd);

    return res;
}

int LazyFS::lfs_unlink (const char* path) {

    this_ ()->check_kill_before ();

    this_ ()->trigger_crash_fault ("unlink", "before", path, "");
    this_ ()->trigger_configured_clear_fault ("unlink", "before", path, "");

    std::shared_lock<std::shared_mutex> lock (cache_command_lock);

    if (this_ ()->FSConfig->log_all_operations) {
        spdlog::info ("[lazyfs.ops]: {}(path={})", __FUNCTION__, path);
    }

    int res;

    string inode = this_ ()->FSCache->get_original_inode (path);
    if (!inode.empty ())
        this_ ()->FSCache->remove_cached_item (inode, path, false);

    res = unlink (path);

    if (res == 0 && inode.empty ()) {
        this_ ()->FSCache->remove_cached_item (inode, path, false);
    }

    if (res == -1)
        return -errno;

    this_ ()->trigger_crash_fault ("unlink", "after", path, "", false);
    this_ ()->trigger_configured_clear_fault ("unlink", "before", path, "", false);

    return 0;
}

} // namespace lazyfs
