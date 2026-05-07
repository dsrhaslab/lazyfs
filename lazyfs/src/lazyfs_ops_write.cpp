
/**
 * @file lazyfs_ops_write.cpp
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

off_t LazyFS::write_get_file_size_before (const string& inode, const struct stat& stats) {
    bool locked = this_ ()->FSCache->lockItemCheckExists (inode);

    if (!locked)
        return stats.st_size;

    off_t file_size = 0;
    Metadata* meta_now = this_ ()->FSCache->get_content_metadata (inode);
    if (meta_now != nullptr)
        file_size = meta_now->size;
    this_ ()->FSCache->unlockItem (inode);
    return file_size;
}

void LazyFS::write_handle_sparse_gap (const char* path, off_t file_size_before, off_t offset, struct fuse_file_info* fi) {
    if (fi == NULL || file_size_before >= offset)
        return;

    spdlog::info ("[lazyfs.ops]: Calling a sparse write.");
    off_t size_to_fill = offset - std::max (file_size_before, (off_t)0);
    if (size_to_fill > 0) {
        char* fill_zeros = (char*)calloc (size_to_fill, 1);
        lfs_write (path, fill_zeros, size_to_fill, file_size_before, NULL);
        free (fill_zeros);
    }
}

void LazyFS::write_update_metadata (const string& inode, off_t offset, size_t size, off_t file_size_before) {
    bool locked = this_ ()->FSCache->lockItemCheckExists (inode);
    if (!locked)
        return;

    off_t was_written_until_offset = offset + size;
    Metadata meta;
    meta.size = was_written_until_offset > file_size_before ? was_written_until_offset : file_size_before;

    vector<string> values_to_change;
    values_to_change.push_back ("size");

    struct timespec modify_time;
    clock_gettime (CLOCK_REALTIME, &modify_time);
    meta.mtim = modify_time;
    values_to_change.push_back ("mtime");

    if (meta.size > file_size_before) {
        meta.ctim = modify_time;
        values_to_change.push_back ("ctime");
    }

    this_ ()->FSCache->update_content_metadata (inode, meta, values_to_change);
    this_ ()->FSCache->unlockItem (inode);
}

int LazyFS::lfs_write (const char* path,
                       const char* buf,
                       size_t size,
                       off_t offset,
                       struct fuse_file_info* fi) {

    this_ ()->check_kill_before ();

    this_ ()->trigger_crash_fault ("write", "before", path, "");
    this_ ()->trigger_configured_clear_fault ("write", "before", path, "");

    std::shared_lock<std::shared_mutex> lock (cache_command_lock);

    if (this_ ()->FSConfig->log_all_operations) {
        spdlog::info ("[lazyfs.ops]: {}(path={},size={},off={})", __FUNCTION__, path, size, offset);
    }

    int fd;
    int res;

    (void)fi;
    if (fi == NULL) {
        fd = open (path, O_WRONLY);
    } else
        fd = fi->fh;

    if (fd == -1)
        return -errno;

    std::string OWNER (path);

    struct stat stats;

    lfs_getattr (path, &stats, fi);

    string inode = this_ ()->FSCache->get_original_inode (OWNER);

    int IO_BLOCK_SIZE = this_ ()->FSConfig->IO_BLOCK_SIZE;

    // -------------------------------------------------------------

    if (fi != NULL && fi->direct_io) {

        // std::printf ("\t[write] direct_io=1, flushing data...");

        if (fi == NULL)
            close (fd);

        return pwrite (fd, buf, size, offset);
    }

    // ----------------------------------------------------------------------------------

    off_t FILE_SIZE_BEFORE = this_ ()->write_get_file_size_before (inode, stats);

    // std::printf ("\twrite: file size before is %d bytes\n", (int)FILE_SIZE_BEFORE);

    // ----------------------------------------------------------------------------------

    this_ ()->write_handle_sparse_gap (path, FILE_SIZE_BEFORE, offset, fi);

    // ----------------------------------------------------------------------------------

    off_t blk_low              = offset / IO_BLOCK_SIZE;
    off_t blk_high             = (offset + size - 1) / IO_BLOCK_SIZE;
    off_t blk_readable_from    = 0;
    off_t blk_readable_to      = 0;
    off_t data_allocated       = 0;
    off_t data_buffer_iterator = 0;
    int fd_caching             = open (path, O_RDONLY);

    char block_caching_buffer[IO_BLOCK_SIZE];
    map<int, tuple<const char*, size_t, int, int>> put_mapping;

    bool cache_full =
        (this_ ()->FSCache->get_cache_usage () == 100) && !this_ ()->FSConfig->APPLY_LRU_EVICTION;

    // ----------------------------------------------------------------------------------

    // std::printf ("\t[write] from_block=%d to_block=%d\n", blk_low, blk_high);

    for (off_t CURR_BLK_IDX = blk_low; CURR_BLK_IDX <= blk_high; CURR_BLK_IDX++) {

        /*
            > Calculate readable block offsets:

            For each block, depending on the 'offset' and 'size' provided,
            the write is bounded from an index to another in each block.
            Each pair of offsets varies from [0 <-> IO_BLOCK_SIZE].
        */

        blk_readable_from = (CURR_BLK_IDX == blk_low) ? (offset % IO_BLOCK_SIZE) : 0;

        if (CURR_BLK_IDX == blk_high)
            blk_readable_to = ((offset + size - 1) % IO_BLOCK_SIZE);
        else if ((CURR_BLK_IDX == blk_low) && ((offset + size - 1) < IO_BLOCK_SIZE))
            blk_readable_to = offset + size - 1;
        else if (CURR_BLK_IDX < blk_high)
            blk_readable_to = IO_BLOCK_SIZE - 1;
        else if (CURR_BLK_IDX == blk_high)
            blk_readable_to = size - data_allocated - 1;

        // data_allocated += blk_readable_to - blk_readable_from + 1;

        bool is_block_cached = this_ ()->FSCache->is_block_cached (inode, CURR_BLK_IDX);

        if (is_block_cached == false) {

            bool needs_pread = FILE_SIZE_BEFORE > CURR_BLK_IDX * IO_BLOCK_SIZE;

            /*
                > Block not found in the caching lib:

                Try to cache it using pread first then inserting into the cache,
                if any of these fails, pwrite it to the underlying FS.
            */

            if (fd_caching >= 0) {

                // Always block sized reads for pread calls

                int pread_res = 0;

                if (cache_full)
                    needs_pread = false;

                if (needs_pread) {

                    pread_res = pread (fd_caching,
                                       block_caching_buffer,
                                       IO_BLOCK_SIZE,
                                       CURR_BLK_IDX * IO_BLOCK_SIZE);
                }

                if (pread_res < 0) {

                    // Read failed for this file,
                    // We assume that the file is not reachable

                    res = -1;
                    break;

                } else {

                    const char* cache_buf;
                    size_t cache_wr_sz;
                    off_t cache_from;
                    off_t cache_to;

                    if ((not needs_pread) || (pread_res == 0)) {

                        cache_buf   = buf + data_buffer_iterator;
                        cache_wr_sz = blk_readable_to - blk_readable_from + 1;
                        cache_from  = blk_readable_from;
                        cache_to    = blk_readable_to;

                    } else {

                        memcpy (block_caching_buffer + blk_readable_from,
                                buf + data_buffer_iterator,
                                blk_readable_to - blk_readable_from + 1);

                        if (blk_readable_from - pread_res > 0)
                            memset (block_caching_buffer + pread_res,
                                    0,
                                    blk_readable_from - pread_res);

                        cache_buf = block_caching_buffer;

                        cache_wr_sz =
                            pread_res > (blk_readable_to + 1) ? pread_res : blk_readable_to + 1;

                        cache_from = 0;
                        cache_to   = cache_wr_sz - 1;
                    }

                    // Increase the ammount of bytes already written from the argument
                    // 'size'
                    data_allocated += blk_readable_to - blk_readable_from + 1;

                    bool curr_block_put_exists = false;
                    std::map<int, bool> put_res;

                    if (!cache_full) {

                        put_res = this_ ()->FSCache->put_data_blocks (
                            inode,
                            {{CURR_BLK_IDX, {cache_buf, cache_wr_sz, cache_from, cache_to}}},
                            OP_WRITE);
                    }

                    curr_block_put_exists = put_res.find (CURR_BLK_IDX) != put_res.end ();

                    if (cache_full ||
                        (!curr_block_put_exists || (put_res.at (CURR_BLK_IDX) == false))) {

                        // Block allocation in cache failed

                        int pwrite_res = pwrite (fd,
                                                 cache_buf,
                                                 cache_wr_sz,
                                                 CURR_BLK_IDX * IO_BLOCK_SIZE + cache_from);

                        if (pwrite_res != cache_wr_sz) {
                            res = -1;
                            break;
                        }
                    }
                }

            } else {

                // Read failed for this file,
                // We assume that the file is not reachable

                res = -1;
                break;
            }

        } else {

            /*
                > Block seems to be cached:

                Try to update block data by replacing block contents for that offsets.
                If it fails, sync data...
            */

            // Increase the ammount of bytes already written from the argument 'size'
            data_allocated += blk_readable_to - blk_readable_from + 1;

            auto put_res =
                this_ ()->FSCache->put_data_blocks (inode,
                                                    {{CURR_BLK_IDX,
                                                      {buf + data_buffer_iterator,
                                                       blk_readable_to - blk_readable_from + 1,
                                                       blk_readable_from,
                                                       blk_readable_to}}},
                                                    OP_WRITE);
            bool curr_block_put_exists = put_res.find (CURR_BLK_IDX) != put_res.end ();

            if (!curr_block_put_exists || put_res.at (CURR_BLK_IDX) == false) {

                int pwrite_res = pwrite (fd,
                                         buf + data_buffer_iterator,
                                         blk_readable_to - blk_readable_from + 1,
                                         CURR_BLK_IDX * IO_BLOCK_SIZE + blk_readable_from);

                if (pwrite_res != (blk_readable_to - blk_readable_from + 1)) {
                    res = -1;
                    break;
                }
            }
        }

        data_buffer_iterator = data_allocated;
    }

    // ----------------------------------------------------------------------------------

    // close (fd_caching);

    // ----------------------------------------------------------------------------------

    this_ ()->write_update_metadata (inode, offset, size, FILE_SIZE_BEFORE);

    close (fd_caching);

    // ----------------------------------------------------------------------------------

    bool crash_added = this_ ()->persist_write (path, buf, size, offset) ||
                       this_ ()->split_write (
                           path,
                           buf,
                           size,
                           offset); // only one will be injected, becaus eby now we only allow one

    // std::printf ("\twrite: returning %d bytes\n", res);

    if (crash_added && this_ ()->kill_before.load ())
        spdlog::warn ("Syscall write returned. LazyFS will crash before the next system call.");
    else {
        this_ ()->trigger_crash_fault ("write", "after", path, "", false);
        this_ ()->trigger_configured_clear_fault ("write", "after", path, "", false);
    }

    // res should be = actual bytes written as pwrite could fail...
    res = size;

    if (res == -1)
        res = -errno;

    if (fi == NULL)
        close (fd);

    return res;
}

} // namespace lazyfs
