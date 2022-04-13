
#include <cmath>
#include <dirent.h>
#include <iostream>
#include <lazyfs/lazyfs.hpp>
#include <map>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <tuple>
#include <unistd.h>

// include in one .cpp file
#include <cache/cache.hpp>
#include <cache/config/config.hpp>
#include <cache/constants/constants.hpp>
#include <cache/engine/backends/custom/custom_cache.hpp>
#include <lazyfs/fusepp/Fuse-impl.h>

using namespace std;
using namespace cache;
using namespace cache::engine::backends::custom;

namespace lazyfs {

LazyFS::LazyFS () {}

LazyFS::LazyFS (Cache* cache, cache::config::Config* config) {

    this->FSConfig = config;
    this->FSCache  = cache;
}

LazyFS::~LazyFS () {}

void LazyFS::fault_clear_cache () {
    cout << "\t\t\t[cache]: clearing cached contents...\n";
    FSCache->clear_all_cache ();
    cout << "\t\t\t[cache]: cache is now empty...\n";
}

void LazyFS::display_cache_usage () {

    std::printf ("\t\t\t[cache] current usage: %0.2lf %% \n", FSCache->get_cache_usage ());
}

void* LazyFS::lfs_init (struct fuse_conn_info* conn, struct fuse_config* cfg) {

    (void)conn;

    // cfg->direct_io = 1;

    return this_ ();
}

void LazyFS::lfs_destroy (void*) {}

int LazyFS::lfs_getattr (const char* path, struct stat* stbuf, struct fuse_file_info* fi) {

    // std::printf ("getattr::(path=%s, ...)\n", path);

    (void)fi;
    int res;

    string content_owner (path);

    res = lstat (path, stbuf);

    if (res == -1)
        return -errno;

    if (S_ISDIR (stbuf->st_mode))
        return 0;

    if (this_ ()->FSCache->has_content_cached (content_owner)) {

        /*
            Content is cached, must return cached metadata
            to override existing values:
            - size, atime, ctime, mtime
        */

        this_ ()->FSCache->lockItem (content_owner);
        Metadata* meta  = this_ ()->FSCache->get_content_metadata (content_owner);
        off_t get_size  = (off_t)meta->size;
        long remainder  = get_size / this_ ()->FSConfig->IO_BLOCK_SIZE;
        blkcnt_t blocks = ((remainder + 1) * (long)this_ ()->FSConfig->IO_BLOCK_SIZE) /
                          (long)this_ ()->FSConfig->DISK_SECTOR_SIZE;

        stbuf->st_size         = get_size;
        stbuf->st_blocks       = blocks;
        stbuf->st_atim.tv_nsec = meta->atim.tv_nsec;
        stbuf->st_atim.tv_sec  = meta->atim.tv_sec;
        stbuf->st_ctim.tv_nsec = meta->ctim.tv_nsec;
        stbuf->st_ctim.tv_sec  = meta->ctim.tv_sec;
        stbuf->st_mtim.tv_nsec = meta->mtim.tv_nsec;
        stbuf->st_mtim.tv_sec  = meta->mtim.tv_sec;

        this_ ()->FSCache->unlockItem (content_owner);

    } else {

        /*
            Content is not cached, cache this values:
            - size, atime, ctime, mtime
        */

        this_ ()->FSCache->put_data_blocks (content_owner, {}, OP_PASSTHROUGH);
        this_ ()->FSCache->lockItem (content_owner);

        Metadata meta;
        meta.size = stbuf->st_size;
        meta.atim = stbuf->st_atim;
        meta.ctim = stbuf->st_ctim;
        meta.mtim = stbuf->st_mtim;

        this_ ()->FSCache->update_content_metadata (content_owner,
                                                    meta,
                                                    {"size", "atime", "ctime", "mtime"});

        this_ ()->FSCache->unlockItem (content_owner);
    }

    return 0;
}

int LazyFS::lfs_readdir (const char* path,
                         void* buf,
                         fuse_fill_dir_t filler,
                         off_t offset,
                         struct fuse_file_info* fi,
                         enum fuse_readdir_flags flags) {

    // std::printf ("readdir::(path=%s, offset=%d, ...)\n", path, (int)offset);

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

int LazyFS::lfs_open (const char* path, struct fuse_file_info* fi) {

    int res;

    res = open (path, fi->flags);

    // --------------------------------------------------------------------------

    int access_mode = fi->flags & O_ACCMODE;

    string owner (path);
    struct timespec time_register;
    clock_gettime (CLOCK_REALTIME, &time_register);
    Metadata meta;
    vector<string> update_meta_values;

    if (!this_ ()->FSCache->has_content_cached (owner)) {
        // just to cache the metadata
        this_ ()->FSCache->put_data_blocks (owner, {}, OP_PASSTHROUGH);
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

    this_ ()->FSCache->update_content_metadata (owner, meta, update_meta_values);

    // --------------------------------------------------------------------------

    if (res == -1)
        return -errno;

    fi->fh = res;

    return 0;
}

int LazyFS::lfs_create (const char* path, mode_t mode, struct fuse_file_info* fi) {

    // std::printf ("create::(path=%s, ...)\n", path);

    int res;

    res = open (path, fi->flags, mode);

    int access_mode = fi->flags & O_ACCMODE;

    string owner (path);
    struct timespec time_register;
    clock_gettime (CLOCK_REALTIME, &time_register);
    Metadata meta;
    vector<string> update_meta_values;

    if (!this_ ()->FSCache->has_content_cached (owner)) {
        // just to cache the metadata
        this_ ()->FSCache->put_data_blocks (owner, {}, OP_PASSTHROUGH);
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

    this_ ()->FSCache->update_content_metadata (owner, meta, update_meta_values);

    if (res == -1)
        return -errno;

    fi->fh = res;
    return 0;
}

int LazyFS::lfs_write (const char* path,
                       const char* buf,
                       size_t size,
                       off_t offset,
                       struct fuse_file_info* fi) {

    // std::printf ("write::(path=%s, size=%d, offset=%d)\n", path, (int)size, (int)offset);

    int fd;
    int res;

    (void)fi;
    if (fi == NULL)
        fd = open (path, O_WRONLY);
    else
        fd = fi->fh;

    if (fd == -1)
        return -errno;

    int IO_BLOCK_SIZE = this_ ()->FSConfig->IO_BLOCK_SIZE;

    // -------------------------------------------------------------

    std::string OWNER (path);

    if (fi->direct_io) {

        // std::printf ("\t[write] direct_io=1, flushing data...");

        res = pwrite (fd, buf, size, offset);

    } else {

        // ----------------------------------------------------------------------------------

        bool cache_had_owner = this_ ()->FSCache->has_content_cached (OWNER);
        int FILE_SIZE_BEFORE = 0;

        if (not cache_had_owner) {

            struct stat stats;

            if (stat (path, &stats) == 0)
                FILE_SIZE_BEFORE = stats.st_size;

        } else {

            FILE_SIZE_BEFORE = this_ ()->FSCache->get_content_metadata (OWNER)->size;
        }

        // std::printf ("\twrite: file size before is %d bytes\n", (int)FILE_SIZE_BEFORE);

        // ----------------------------------------------------------------------------------

        int blk_low              = offset / IO_BLOCK_SIZE;
        int blk_high             = (offset + size - 1) / IO_BLOCK_SIZE;
        int blk_readable_from    = 0;
        int blk_readable_to      = 0;
        int data_allocated       = 0;
        int data_buffer_iterator = 0;
        int fd_caching           = open (path, O_RDONLY);

        char block_caching_buffer[IO_BLOCK_SIZE];
        map<int, tuple<const char*, size_t, int, int>> put_mapping;

        // ----------------------------------------------------------------------------------

        // std::printf ("\t[write] from_block=%d to_block=%d\n", blk_low, blk_high);

        for (int CURR_BLK_IDX = blk_low; CURR_BLK_IDX <= blk_high; CURR_BLK_IDX++) {

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

            bool is_block_cached = this_ ()->FSCache->is_block_cached (OWNER, CURR_BLK_IDX);

            if (not is_block_cached) {

                bool needs_pread = FILE_SIZE_BEFORE > CURR_BLK_IDX * IO_BLOCK_SIZE;

                /*
                    > Block not found in the caching lib:

                    Try to cache it using pread first then inserting into the cache,
                    if any of these fails, pwrite it to the underlying FS.
                */

                if (fd_caching >= 0) {

                    // Always block sized reads for pread calls

                    int pread_res = 0;

                    if (needs_pread)
                        pread_res = pread (fd_caching,
                                           block_caching_buffer,
                                           IO_BLOCK_SIZE,
                                           CURR_BLK_IDX * IO_BLOCK_SIZE);

                    if (pread_res < 0) {

                        // Read failed for this file,
                        // We assume that the file is not reachable

                        res = -1;
                        break;

                    } else {

                        const char* cache_buf;
                        int cache_wr_sz;
                        int cache_from;
                        int cache_to;

                        if (not needs_pread || pread_res == 0) {

                            cache_buf   = buf + data_buffer_iterator;
                            cache_wr_sz = blk_readable_to - blk_readable_from + 1;
                            cache_from  = blk_readable_from;
                            cache_to    = blk_readable_to;

                        } else {

                            memcpy (block_caching_buffer + blk_readable_from,
                                    buf + data_buffer_iterator,
                                    blk_readable_to - blk_readable_from + 1);

                            int st_index = !pread_res ? 0 : (pread_res - 1);

                            if (blk_readable_from - st_index > 0)
                                memset (block_caching_buffer + pread_res,
                                        '?',
                                        blk_readable_from - pread_res);

                            cache_buf   = block_caching_buffer;
                            cache_wr_sz = std::max (pread_res, blk_readable_to + 1);
                            cache_from  = 0;
                            cache_to    = cache_wr_sz - 1;
                        }

                        // Increase the ammount of bytes already written from the argument
                        // 'size'
                        data_allocated += blk_readable_to - blk_readable_from + 1;

                        auto put_res = this_ ()->FSCache->put_data_blocks (
                            OWNER,
                            {{CURR_BLK_IDX, {cache_buf, cache_wr_sz, cache_from, cache_to}}},
                            OP_WRITE);

                        bool curr_block_put_exists = put_res.find (CURR_BLK_IDX) != put_res.end ();

                        if (!curr_block_put_exists || put_res.at (CURR_BLK_IDX) == false) {

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
                    this_ ()->FSCache->put_data_blocks (OWNER,
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

        close (fd_caching);

        // ----------------------------------------------------------------------------------

        this_ ()->FSCache->lockItem (OWNER);

        int was_written_until_offset = offset + size;

        Metadata meta;
        meta.size = was_written_until_offset > FILE_SIZE_BEFORE ? was_written_until_offset
                                                                : FILE_SIZE_BEFORE;

        vector<string> values_to_change;
        values_to_change.push_back ("size");

        struct timespec modify_time;
        clock_gettime (CLOCK_REALTIME, &modify_time);
        meta.mtim = modify_time;
        values_to_change.push_back ("mtime");

        if (meta.size > FILE_SIZE_BEFORE) {
            struct timespec change_time;
            clock_gettime (CLOCK_REALTIME, &change_time);
            meta.ctim = change_time;
            values_to_change.push_back ("ctime");
        }

        this_ ()->FSCache->update_content_metadata (OWNER, meta, values_to_change);

        // std::printf ("\twrite: file size now is %d bytes\n", (int)meta.size);

        this_ ()->FSCache->unlockItem (OWNER);
    }

    // ----------------------------------------------------------------------------------

    // res should be = actual bytes written as pwrite could fail...
    res = size;

    // std::printf ("\twrite: returning %d bytes\n", res);

    if (res == -1)
        res = -errno;

    if (fi == NULL)
        close (fd);

    // this_ ()->FSCache->print_cache ();
    // this_ ()->FSCache->print_engine ();

    return res;
}

int LazyFS::lfs_read (const char* path,
                      char* buf,
                      size_t size,
                      off_t offset,
                      struct fuse_file_info* fi) {

    // std::printf ("read::(path=%s,size=%d,offset=%d, ...)\n", path, (int)size, (int)offset);

    int fd;
    int res;

    if (fi == NULL)
        fd = open (path, O_RDONLY);
    else
        fd = fi->fh;

    if (fd == -1)
        return -errno;

    int IO_BLOCK_SIZE = this_ ()->FSConfig->IO_BLOCK_SIZE;

    // ----------------------------------------------------------------------------------

    int blk_low        = offset / IO_BLOCK_SIZE;
    int blk_high       = (offset + size - 1) / IO_BLOCK_SIZE;
    int fd_caching     = open (path, O_RDONLY);
    int BUF_ITERATOR   = 0;
    int BYTES_LEFT     = size;
    int data_allocated = 0;

    char read_buffer[IO_BLOCK_SIZE];
    std::string OWNER (path);

    // ----------------------------------------------------------------------------------

    bool cache_had_owner = this_ ()->FSCache->has_content_cached (OWNER);

    Metadata meta;
    if (not cache_had_owner) {

        struct stat stats;

        if (stat (path, &stats) == 0)
            meta.size = stats.st_size;

        struct timespec access_time;
        clock_gettime (CLOCK_REALTIME, &access_time);
        meta.atim = access_time;

        this_ ()->FSCache->lockItem (OWNER);

        this_ ()->FSCache->update_content_metadata (OWNER, meta, {"size", "atime"});

        this_ ()->FSCache->unlockItem (OWNER);

    } else {

        meta.size = (int)(this_ ()->FSCache->get_content_metadata (OWNER)->size);
    }

    if (offset > (meta.size - 1))
        return 0;

    // ----------------------------------------------------------------------------------

    int last_pread_chunk_size   = 0;
    int last_pread_chunk_offset = offset;

    for (int CURR_BLK_IDX = blk_low; CURR_BLK_IDX <= blk_high; CURR_BLK_IDX++) {

        int blk_readable_from = (CURR_BLK_IDX == blk_low) ? (offset % IO_BLOCK_SIZE) : 0;
        int blk_readable_to   = 0;

        if (CURR_BLK_IDX == blk_high)
            blk_readable_to = ((offset + size - 1) % IO_BLOCK_SIZE);
        else if ((CURR_BLK_IDX == blk_low) && ((offset + size - 1) < IO_BLOCK_SIZE))
            blk_readable_to = offset + size - 1;
        else if (CURR_BLK_IDX < blk_high)
            blk_readable_to = IO_BLOCK_SIZE - 1;
        else if (CURR_BLK_IDX == blk_high)
            blk_readable_to = size - data_allocated - 1;

        if (this_ ()->FSCache->is_block_cached (OWNER, CURR_BLK_IDX)) {

            if (last_pread_chunk_size > 0) {

                int pread_res = pread (fd_caching,
                                       buf + BUF_ITERATOR,
                                       last_pread_chunk_size,
                                       last_pread_chunk_offset);

                BUF_ITERATOR += pread_res;
                BYTES_LEFT -= pread_res;

                last_pread_chunk_size   = 0;
                last_pread_chunk_offset = CURR_BLK_IDX * IO_BLOCK_SIZE;
            }

            if (BYTES_LEFT <= 0)
                break;

            /*
                > Block is cached, so buffer was filled with the requested data:
            */

            auto GET_BLOCKS_RES =
                this_ ()->FSCache->get_data_blocks (OWNER, {{CURR_BLK_IDX, read_buffer}});

            if ((GET_BLOCKS_RES.find (CURR_BLK_IDX) != GET_BLOCKS_RES.end ()) and
                GET_BLOCKS_RES.at (CURR_BLK_IDX).first) {

                pair<int, int> readable_offsets = GET_BLOCKS_RES.at (CURR_BLK_IDX).second;

                int max_readable_offset = readable_offsets.second;
                int read_to             = std::min (max_readable_offset, blk_readable_to);

                memcpy (buf + BUF_ITERATOR,
                        read_buffer + blk_readable_from,
                        (read_to - blk_readable_from) + 1);

                BUF_ITERATOR += (read_to - blk_readable_from) + 1;
                BYTES_LEFT -= (read_to - blk_readable_from) + 1;

                data_allocated += (read_to - blk_readable_from) + 1;
            }

        } else {

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
                    last_pread_chunk_size += blk_readable_to + 1;

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

    close (fd_caching);

    if (res == -1)
        res = -errno;

    if (fi == NULL)
        close (fd);

    // this_ ()->FSCache->print_cache ();
    // this_ ()->FSCache->print_engine ();

    return res;
}

int LazyFS::lfs_fsync (const char* path, int isdatasync, struct fuse_file_info* fi) {

    // std::printf ("fsync::(path=%s, isdatasync=%d, ...)\n", path, isdatasync);

    string owner (path);

    bool is_owner_cached = this_ ()->FSCache->has_content_cached (owner);

    int res;

    if (isdatasync)
        res = is_owner_cached ? this_ ()->FSCache->sync_owner (owner, true) : fdatasync (fi->fh);
    else
        res = is_owner_cached ? this_ ()->FSCache->sync_owner (owner, false) : fsync (fi->fh);

    return res;
}

int LazyFS::lfs_rename (const char* from, const char* to, unsigned int flags) {

    // std::printf ("rename::(from=%s,to=%s, ...)\n", from, to);

    int res;

    if (flags)
        return -EINVAL;

    string last_owner (from);
    string new_owner (to);

    res = this_ ()->FSCache->rename_item (last_owner, new_owner) ? 0 : -1;

    res = rename (from, to);

    this_ ()->FSCache->sync_owner (new_owner, true);

    // std::printf ("\trename:: rename returned %d\n", res);

    if (res == -1)
        return -errno;

    return 0;
}

int LazyFS::lfs_truncate (const char* path, off_t truncate_size, struct fuse_file_info* fi) {

    //  std::printf ("truncate::(path=%s, size=%zu, ...)\n", path, truncate_size);

    int res;

    string owner (path);

    Metadata* previous_metadata = this_ ()->FSCache->get_content_metadata (owner);
    bool has_content_cached     = previous_metadata != nullptr;
    bool behave_as_lfs_write    = false;

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

    if (truncate_size > previous_metadata->size) {

        behave_as_lfs_write = true;
    }

    if (behave_as_lfs_write) {

        /*
            In this case, cache file data to reach the size to truncate with zeroes.
            1: Content does not exist? Fill 0 -> size with zeroes
            2: Content exists? Fill size_before -> size with zeros
        */

        size_t IO_BLOCK_SIZE       = this_ ()->FSConfig->IO_BLOCK_SIZE;
        int file_size              = previous_metadata->size;
        int add_bytes_from_offset  = file_size;
        int add_bytes_total        = truncate_size - add_bytes_from_offset;
        int blk_low                = add_bytes_from_offset / IO_BLOCK_SIZE;
        int blk_high               = (add_bytes_from_offset + add_bytes_total - 1) / IO_BLOCK_SIZE;
        int blk_readable_from      = 0;
        int blk_readable_to        = 0;
        int data_allocated         = 0;
        bool caching_blocks_failed = false;

        char buf[IO_BLOCK_SIZE];
        memset (buf, '0', IO_BLOCK_SIZE);

        for (int CURR_BLK_IDX = blk_low; CURR_BLK_IDX <= blk_high; CURR_BLK_IDX++) {

            blk_readable_from =
                (CURR_BLK_IDX == blk_low) ? (add_bytes_from_offset % IO_BLOCK_SIZE) : 0;

            if (CURR_BLK_IDX == blk_high)
                blk_readable_to = ((add_bytes_from_offset + add_bytes_total - 1) % IO_BLOCK_SIZE);
            else if ((CURR_BLK_IDX == blk_low) &&
                     ((add_bytes_from_offset + add_bytes_total - 1) < IO_BLOCK_SIZE))
                blk_readable_to = add_bytes_from_offset + add_bytes_total - 1;
            else if (CURR_BLK_IDX < blk_high)
                blk_readable_to = IO_BLOCK_SIZE - 1;
            else if (CURR_BLK_IDX == blk_high)
                blk_readable_to = add_bytes_total - data_allocated - 1;

            // Increase the ammount of bytes already written from the argument 'size'
            data_allocated += blk_readable_to - blk_readable_from + 1;

            auto put_res = this_ ()->FSCache->put_data_blocks (
                owner,
                {{CURR_BLK_IDX,
                  {buf,
                   (size_t) (blk_readable_to - blk_readable_from + 1),
                   blk_readable_from,
                   blk_readable_to}}},
                OP_WRITE);
            bool curr_block_put_exists = put_res.find (CURR_BLK_IDX) != put_res.end ();

            if (!curr_block_put_exists || put_res.at (CURR_BLK_IDX) == false) {

                caching_blocks_failed = true;
                break;
            }
        }

        if (caching_blocks_failed) {

            int res;

            if (fi != NULL)
                res = ftruncate (fi->fh, truncate_size);
            else
                res = truncate (path, truncate_size);
        }

    } else if (truncate_size < previous_metadata->size) {

        this_ ()->FSCache->truncate_item (owner, truncate_size);
    }

    // todo: update file size

    Metadata new_meta;
    new_meta.size = truncate_size;

    this_ ()->FSCache->lockItem (owner);
    this_ ()->FSCache->update_content_metadata (owner, new_meta, {"size"});
    this_ ()->FSCache->unlockItem (owner);

    // todo: should file times update after truncate, even if truncate_size == current file size?

    // this_ ()->FSCache->print_cache ();
    // this_ ()->FSCache->print_engine ();

    if (res == -1)
        return -errno;

    return 0;
}

int LazyFS::lfs_symlink (const char* from, const char* to) {
    int res;

    res = symlink (from, to);
    if (res == -1)
        return -errno;

    return 0;
}

int LazyFS::lfs_access (const char* path, int mask) {

    // std::printf ("access::(path=%s, ...)\n", path);

    int res;

    res = access (path, mask);
    if (res == -1)
        return -errno;

    return 0;
}

int LazyFS::lfs_mkdir (const char* path, mode_t mode) {

    // std::printf ("mkdir::(path=%s, ...)\n", path);

    int res;

    res = mkdir (path, mode);
    if (res == -1)
        return -errno;

    return 0;
}

int LazyFS::lfs_link (const char* from, const char* to) {

    int res;

    res = link (from, to);
    if (res == -1)
        return -errno;

    return 0;
}

int LazyFS::lfs_readlink (const char* path, char* buf, size_t size) {
    int res;

    res = readlink (path, buf, size - 1);
    if (res == -1)
        return -errno;

    buf[res] = '\0';
    return 0;
}

int LazyFS::lfs_release (const char* path, struct fuse_file_info* fi) {
    (void)path;
    close (fi->fh);
    return 0;
}

int LazyFS::lfs_rmdir (const char* path) {

    // std::printf ("rmdir::(path=%s, ...)\n", path);

    int res;

    res = rmdir (path);
    if (res == -1)
        return -errno;

    return 0;
}

int LazyFS::lfs_chmod (const char* path, mode_t mode, struct fuse_file_info* fi) {

    (void)fi;
    int res;

    res = chmod (path, mode);
    if (res == -1)
        return -errno;

    return 0;
}

int LazyFS::lfs_chown (const char* path, uid_t uid, gid_t gid, fuse_file_info* fi) {

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

#ifdef HAVE_UTIMENSAT
int LazyFS::lfs_utimens (const char* path, const struct timespec ts[2], struct fuse_file_info* fi) {
    (void)fi;
    int res;

    /* don't use utime/utimes since they follow symlinks */
    res = utimensat (0, path, ts, AT_SYMLINK_NOFOLLOW);
    if (res == -1)
        return -errno;

    return 0;
}
#endif

off_t LazyFS::lfs_lseek (const char* path, off_t off, int whence, struct fuse_file_info* fi) {

    // std::printf ("lseek::(path=%s, off=%d, ...)\n", path, (int)off);

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

    // std::printf ("unlink::(path=%s, ...)\n", path);

    int res;

    string owner (path);
    this_ ()->FSCache->remove_cached_item (owner);

    res = unlink (path);

    if (res == -1)
        return -errno;

    return 0;
}

} // namespace lazyfs
