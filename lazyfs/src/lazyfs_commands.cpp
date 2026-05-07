
/**
 * @file lazyfs_commands.cpp
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

off_t LazyFS::get_file_size (string path) {

    off_t res;
    struct stat stbuf;

    res = lstat (path.c_str(), &stbuf);

    if (res == -1)
        return -errno;

    if (not S_ISREG (stbuf.st_mode))
        return 0;

    string inode = this->FSCache->get_original_inode (path);

    if (inode.empty ()) {
        inode = to_string (stbuf.st_ino);
    }

    bool locked = this->FSCache->lockItemCheckExists (inode);

    if (!locked) {

        res = stbuf.st_size;

    } else if (locked) {

        /*
        Content is cached, must return cached metadata
        */

        Metadata* meta = this->FSCache->get_content_metadata (inode);

        if (meta != nullptr) {
            res  = meta->size;
        }

        this->FSCache->unlockItem (inode);
    }

    return res;
}

int LazyFS::read_file (const char * path, char* buf, size_t size, off_t offset) {
    int fd;
    int res;

    fd = open (path, O_RDONLY);

    if (fd == -1)
        return -errno;

    std::string OWNER (path);

    string inode = this->FSCache->get_original_inode (OWNER);

    if (inode.empty ()) {
        // File is not cached, will read it directly from the file system
        return pread (fd, buf, size, offset);
    }

    int IO_BLOCK_SIZE = this->FSConfig->IO_BLOCK_SIZE;

    // ----------------------------------------------------------------------------------

    off_t blk_low        = offset / IO_BLOCK_SIZE;
    off_t blk_high       = (offset + size - 1) / IO_BLOCK_SIZE;
    int fd_caching       = fd;
    off_t BUF_ITERATOR   = 0;
    off_t BYTES_LEFT     = size;
    off_t data_allocated = 0;

    char read_buffer[IO_BLOCK_SIZE];

    // ----------------------------------------------------------------------------------

    bool cache_had_owner = this->FSCache->has_content_cached (inode);

    Metadata meta;

    if (not cache_had_owner) {

        // File is not cached, will read it directly from the file system
        return pread (fd, buf, size, offset);

    } else {

        bool locked = this->FSCache->lockItemCheckExists (inode);

        if (locked) {

            Metadata* old_meta = this->FSCache->get_content_metadata (inode);

            if (old_meta != nullptr)
                meta.size = old_meta->size;

            this->FSCache->unlockItem (inode);
        }

        // std::printf ("\tread: file has %d bytes\n", (int)meta.size);
    }

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

        if (this->FSCache->is_block_cached (inode, CURR_BLK_IDX)) {

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
                this->FSCache->get_data_blocks (inode, {{CURR_BLK_IDX, read_buffer}});

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

    close (fd);

    return res;
}

int LazyFS::copy_file (string file, string destination) {
    struct stat file_stat;
    int size;

    spdlog::info("[lazyfs.cmds]: Snapshotting file {} to {}", file, destination);

    off_t file_size = get_file_size(file);

    if (file_size < 0) {
        spdlog::error("[lazyfs.cmds]: Error snapshoting file {}: {}", file, (errno));
        return -errno;
    } else if (file_size == 0) {
        spdlog::warn("[lazyfs.cmds]: Did not snapshot file {} because size is 0", file);
        return 0;
    }

    char * buf = new char[file_size];
    int res_read;

    if ((res_read = read_file(file.c_str(), buf, file_size, 0)) < 0) {
        spdlog::error("[lazyfs.cmds]: Error snapshotting file {}: {}", file, (errno));
        delete [] buf;
        return -errno;
    } else if (res_read != file_size) {
        spdlog::error("[lazyfs.cmds]: Error snapshotting file {}: bytes read different from file size", file);
        delete [] buf;
        return 0;
    }

    int fd = open(destination.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if (fd < 0) {
        spdlog::error("[lazyfs.cmds]: Error snapshotting file {}: {}", file, (errno));
        delete [] buf;
        return -errno;
    }

    if (write(fd, buf, file_size) != file_size) {
        spdlog::error("[lazyfs.cmds]: Error snapshotting file {}: {}", file, (errno));
        delete [] buf;
        return -errno;
    }

    delete [] buf;

    return 0;
}

void LazyFS::command_snapshot_files(regex files_rgx, string save_dir, bool lock_needed) {
    if (regex_match("", files_rgx) || save_dir == "") return;
    bool found_path = false;
    string destination = save_dir + "/snapshot" + to_string(this->snapshot_counter.load());

    vector<tuple<string,string>> files_to_copy;

    if (lock_needed) std::unique_lock<std::shared_mutex> lock (cache_command_lock);

    if (!filesystem::exists(this->root_dir)) {
        spdlog::error("[lazyfs.cmds]: Root directory {} does not exist.", this->mount_dir);
    } else {
        for (const auto& entry : filesystem::recursive_directory_iterator(this->root_dir)) {
            if (filesystem::is_regular_file(entry.path())) {

                if (regex_match(entry.path().string(), files_rgx)) {
                    if (!found_path) {
                        filesystem::create_directory(destination);
                        this->snapshot_counter.fetch_add(1);
                    }
                    found_path = true;
                    string src = entry.path().string();
                    string dest = destination + "/" + entry.path().filename().string();
                    tuple <std::string, std::string> src_dest = make_tuple(src, dest);

                    files_to_copy.push_back(src_dest);
                }
            }
        }
    }

    for (auto const& it : files_to_copy) {
        copy_file(get<0>(it), get<1>(it));
    }
}

void LazyFS::command_unsynced_data_report (vector<string> paths_to_exclude) {

    spdlog::warn ("[lazyfs.cmds]: report request submitted...");

    auto const& res = FSCache->report_unsynced_data ();

    spdlog::warn ("[lazyfs.cmds]: report generated.");

    if (res.empty ()) {

        spdlog::info ("[lazyfs.cmds]: report: all data blocks are synced!");

    } else {

        spdlog::set_pattern ("[%^lazyfs.report%$] %v");
        spdlog::info ("----------------------------------------------------");

        size_t total_bytes_unsynced = 0;

        for (auto const& it : res) {

            string ino        = string (get<0> (it).c_str ());
            auto files_mapped = FSCache->find_files_mapped_to_inode (ino);

            bool report = true;
            if (paths_to_exclude.size () > 0) {
                report = find_first_of (files_mapped.begin (),
                                        files_mapped.end (),
                                        paths_to_exclude.begin (),
                                        paths_to_exclude.end ()) != files_mapped.end ();
            }

            if (report) {

                if (std::get<2> (it).size () > 0 && files_mapped.size () > 0) {

                    spdlog::info ("Inode {} is not fully synced!", ino);

                    spdlog::info ("[inode {}]: files mapped to this inode:", ino);

                    for (auto it = files_mapped.begin (); it != files_mapped.end (); ++it)
                        spdlog::info ("\t=> file: '{}'", *it);

                    int last_block_index     = -1;
                    int last_block_off_start = -1;
                    int first_block_id       = -1;

                    for (auto block_it = std::get<2> (it).begin ();
                         block_it != std::get<2> (it).end ();
                         block_it++) {

                        int block_id = get<0> (*block_it);
                        auto offs    = get<1> (*block_it);

                        if (last_block_index < 0) {
                            last_block_index     = block_id;
                            last_block_off_start = offs.first;
                            first_block_id       = block_id;
                        }

                        if (block_id != (get<0> (*std::next (block_it)) - 1)) {

                            spdlog::info ("[inode {}]: (block {}) to (block "
                                          "{}) [byte index {} to index {}]",
                                          ino,
                                          last_block_index,
                                          block_id,
                                          last_block_off_start,
                                          (offs.second + block_id * FSConfig->IO_BLOCK_SIZE) -
                                              (first_block_id * FSConfig->IO_BLOCK_SIZE));

                            last_block_off_start = offs.first;
                            last_block_index     = block_id;
                        }

                        total_bytes_unsynced +=
                            get<1> (*block_it).second - get<1> (*block_it).first + 1;
                    }
                }
            }

            spdlog::info ("----------------------------------------------------");
        }
        spdlog::info ("Total number of bytes un-fsynced: {} bytes.", total_bytes_unsynced);

        if (paths_to_exclude.size () > 0)
            spdlog::info ("Info about un-fsynced bytes from some files was excluded.");

        spdlog::info ("----------------------------------------------------");
        if (THREAD_ID)
            spdlog::set_pattern ("[thread: %t] %+");
    }
}

void LazyFS::command_fault_clear_cache (bool lock_needed) {

    if (lock_needed)
        std::unique_lock<std::shared_mutex> lock (cache_command_lock);

    spdlog::warn ("[lazyfs.cmds]: clear cache request submitted...");

    //Taking snapshots
    if (this->snapshot_counter.load () >= 0)
        command_snapshot_files (regex(FSConfig->SNAPSHOT_FILES), FSConfig->SNAPSHOT_SAVE, lock_needed);

    FSCache->clear_all_cache ();

    if (this->snapshot_counter.load () >= 0)
        command_snapshot_files (regex(FSConfig->SNAPSHOT_FILES), FSConfig->SNAPSHOT_SAVE, lock_needed);

    spdlog::warn ("[lazyfs.cmds]: cache is cleared.");
}

void LazyFS::command_fault_sync_pages (faults::SyncPagesF &sync_pages) {

    std::unique_lock<std::shared_mutex> lock (cache_command_lock);

    spdlog::warn ("[lazyfs.cmds]: sync pages request submitted...");

    trigger_sync_pages_fault(sync_pages);

    spdlog::warn ("[lazyfs.cmds]: sync pages requested finished.");

}

void LazyFS::command_display_cache_usage (bool lock_needed) {

    if (lock_needed)
        std::unique_lock<std::shared_mutex> lock (cache_command_lock);

    spdlog::warn ("[lazyfs.cmds]: cache usage (\%pages) is {}%", FSCache->get_cache_usage ());
}

void LazyFS::command_checkpoint (bool lock_needed) {

    if (lock_needed)
        std::unique_lock<std::shared_mutex> lock (cache_command_lock);

    spdlog::warn ("[lazyfs.cmds]: cache checkpoint request submitted...");

    FSCache->full_checkpoint ();

    spdlog::warn ("[lazyfs.cmds]: checkpoint is done.");
}

} // namespace lazyfs
