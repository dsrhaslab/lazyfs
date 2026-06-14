
/**
 * @file lazyfs_ops_read.cpp
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

Metadata LazyFS::read_get_file_meta (const char* path, const string& inode) {
    Metadata meta;
    bool cache_had_owner = this_ ()->FSCache->has_content_cached (inode);

    if (not cache_had_owner) {
        this_ ()->FSCache->put_data_blocks (inode, {}, OP_PASSTHROUGH);
        bool locked = this_ ()->FSCache->lockItemCheckExists (inode);

        struct stat stats;
        if (stat (path, &stats) == 0)
            meta.size = stats.st_size;

        struct timespec access_time;
        clock_gettime (CLOCK_REALTIME, &access_time);
        meta.atim = access_time;

        if (locked) {
            this_ ()->FSCache->update_content_metadata (inode, meta, {"size", "atime"});
            this_ ()->FSCache->unlockItem (inode);
        }
    } else {
        bool locked = this_ ()->FSCache->lockItemCheckExists (inode);
        if (locked) {
            Metadata* old_meta = this_ ()->FSCache->get_content_metadata (inode);
            if (old_meta != nullptr)
                meta.size = old_meta->size;
            this_ ()->FSCache->unlockItem (inode);
        }
    }

    return meta;
}

int LazyFS::lfs_read (const char* path,
                      char* buf,
                      size_t size,
                      off_t offset,
                      struct fuse_file_info* fi) {

    this_ ()->check_kill_before ();

    this_ ()->trigger_crash_fault ("read", "before", path, "");
    this_ ()->trigger_configured_clear_fault ("read", "before", path, "");

    std::shared_lock<std::shared_mutex> lock (cache_command_lock);

    if (this_ ()->FSConfig->log_all_operations) {
        spdlog::info ("[lazyfs.ops]: {}(path={},size={},off={})", __FUNCTION__, path, size, offset);
    }

    int fd;
    int res;

    if (fi == NULL)
        fd = open (path, O_RDONLY);
    else
        fd = fi->fh;

    if (fd == -1)
        return -errno;

    std::string OWNER (path);

    struct stat stats;
    lfs_getattr (path, &stats, fi);

    string inode = this_ ()->FSCache->get_original_inode (OWNER);

    int IO_BLOCK_SIZE = this_ ()->FSConfig->IO_BLOCK_SIZE;

    // ----------------------------------------------------------------------------------

    off_t blk_low        = offset / IO_BLOCK_SIZE;
    off_t blk_high       = (offset + size - 1) / IO_BLOCK_SIZE;
    int fd_caching       = fd;
    off_t BUF_ITERATOR   = 0;
    off_t BYTES_LEFT     = size;
    off_t data_allocated = 0;

    char read_buffer[IO_BLOCK_SIZE];

    // ----------------------------------------------------------------------------------

    Metadata meta = this_ ()->read_get_file_meta (path, inode);

    if (offset > (meta.size - 1))
        return 0;

    // ---------------------------------------------------------------------------------

    off_t last_pread_chunk_size   = 0;
    off_t last_pread_chunk_offset = offset;

    off_t blk_readable_from = 0;
    off_t blk_readable_to   = 0;

    for (off_t CURR_BLK_IDX = blk_low; CURR_BLK_IDX <= blk_high; CURR_BLK_IDX++) {

        if ((CURR_BLK_IDX * IO_BLOCK_SIZE) > meta.size)
            break;

        blk_readable_from = (CURR_BLK_IDX == blk_low) ? (offset % IO_BLOCK_SIZE) : 0;

        if (CURR_BLK_IDX == blk_high)
            blk_readable_to = ((offset + size - 1) % IO_BLOCK_SIZE);
        else if ((CURR_BLK_IDX == blk_low) && ((offset + size - 1) < IO_BLOCK_SIZE))
            blk_readable_to = offset + size - 1;
        else if (CURR_BLK_IDX < blk_high)
            blk_readable_to = IO_BLOCK_SIZE - 1;
        else if (CURR_BLK_IDX == blk_high)
            blk_readable_to = size - data_allocated - 1;

        if (this_ ()->FSCache->is_block_cached (inode, CURR_BLK_IDX)) {

            if (last_pread_chunk_size > 0) {

                int pread_res = pread (fd_caching,
                                       buf + BUF_ITERATOR,
                                       last_pread_chunk_size,
                                       last_pread_chunk_offset);

                BUF_ITERATOR += pread_res;
                BYTES_LEFT -= pread_res;

                last_pread_chunk_size   = 0;
                last_pread_chunk_offset = (CURR_BLK_IDX + 1) * IO_BLOCK_SIZE;

            } else if (CURR_BLK_IDX < blk_high) {

                last_pread_chunk_offset = (CURR_BLK_IDX + 1) * IO_BLOCK_SIZE;
            }

            if (BYTES_LEFT <= 0) {

                break;
            }

            /*
                > Block is cached, so buffer was filled with the requested data:
            */

            auto GET_BLOCKS_RES =
                this_ ()->FSCache->get_data_blocks (inode, {{CURR_BLK_IDX, read_buffer}});

            if ((GET_BLOCKS_RES.find (CURR_BLK_IDX) != GET_BLOCKS_RES.end ()) and
                GET_BLOCKS_RES.at (CURR_BLK_IDX).first) {

                pair<int, int> readable_offsets = GET_BLOCKS_RES.at (CURR_BLK_IDX).second;

                off_t max_readable_offset = readable_offsets.second;

                off_t read_to = std::min (max_readable_offset, blk_readable_to);

                memcpy (buf + BUF_ITERATOR,
                        read_buffer + blk_readable_from,
                        (read_to - blk_readable_from) + 1);

                BUF_ITERATOR += (read_to - blk_readable_from) + 1;
                BYTES_LEFT -= (read_to - blk_readable_from) + 1;

                if (read_to < (IO_BLOCK_SIZE - 1) && CURR_BLK_IDX < blk_high) {

                    memset (buf + BUF_ITERATOR + read_to, 0, IO_BLOCK_SIZE - read_to);
                }

                data_allocated += (read_to - blk_readable_from) + 1;

            } else {

                // todo: there could be a race condition between checking if the block is cached
                // todo: and retrieving its data, so either lock the operation or go to the else
                // todo: case

                // goto try_pread;
            }

        } else {

            // todo:
            // try_pread:

            /*
                > Block is not cached, cache it first: If it fails, call pread if
               needed.
            */

            bool needs_pread = meta.size > CURR_BLK_IDX * IO_BLOCK_SIZE;

            if (fd_caching > 0) {

                /*
                    > Calculate readable block offsets:

                    For each block, depending on the 'offset' and 'size' provided,
                    the write is bounded from an index to another in each block.
                    Each pair of offsets varies from [0 <-> IO_BLOCK_SIZE].
                */

                // ----------------------------------------------------------------------------------

                data_allocated += blk_readable_to - blk_readable_from + 1;

                // ----------------------------------------------------------------------------------

                if (needs_pread)
                    last_pread_chunk_size += blk_readable_to - blk_readable_from + 1;

            } else {

                // Read failed for this file,
                // We assume that the file is not reachable

                res = -1;
                break;
            }
        }
    }

    if (last_pread_chunk_size > 0) {
        int pread_res =
            pread (fd_caching, buf + BUF_ITERATOR, last_pread_chunk_size, last_pread_chunk_offset);

        BUF_ITERATOR += pread_res;
        BYTES_LEFT -= pread_res;
    }

    // ---------------------------------------------------------

    res = BUF_ITERATOR;

    if (res == -1)
        res = -errno;

    if (fi == NULL)
        close (fd);

    this_ ()->trigger_crash_fault ("read", "after", path, "", false);
    this_ ()->trigger_configured_clear_fault ("read", "after", path, "", false);

    return res;
}

} // namespace lazyfs
