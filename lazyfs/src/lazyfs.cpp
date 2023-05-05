
/**
 * @file lazyfs.cpp
 * @author João Azevedo joao.azevedo@inesctec.pt
 *
 * @copyright Copyright (c) 2020-2022 INESC TEC.
 *
 */

#include <chrono>
#include <cmath>
#include <ctime>
#include <dirent.h>
#include <iostream>
#include <lazyfs/lazyfs.hpp>
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

// LazyFS specific imports
#include <cache/cache.hpp>
#include <cache/config/config.hpp>
#include <cache/constants/constants.hpp>
#include <cache/engine/backends/custom/custom_cache.hpp>
#include <lazyfs/fusepp/Fuse-impl.h>

using namespace std;
using namespace cache;
using namespace cache::engine::backends::custom;

std::shared_mutex cache_command_lock;

namespace lazyfs {
//cache::config::Fault& operator=(cache::config::Fault const&);
Write::Write() {
    this->path = this->buf = NULL;
}

Write::Write(const char* path, const char* buf, size_t size, off_t offset) {
    this->path = strdup(path);
    this->buf = (char *) malloc(sizeof(char) * size);
    int i;
    for(i=0; i<size; i++) {
        this->buf[i] = buf[i];
    }
    this->size = size;
    this->offset = offset;
}

Write::~Write() {
    free((char*)this->path);
    free((char*)this->buf);
    this->path = NULL;
    this->buf = NULL;
}

LazyFS::LazyFS () {}

LazyFS::LazyFS (Cache* cache,
                cache::config::Config* config,
                std::thread* faults_handler_thread,
                void (*fht_worker) (LazyFS* filesystem),
                unordered_map<string,vector<cache::config::Fault*>>* faults) {

    this->FSConfig              = config;
    this->FSCache               = cache;
    this->faults_handler_thread = faults_handler_thread;
    this->fht_worker            = fht_worker;
    this->faults                = faults;
    this->pending_write = NULL;

    std::mutex write_lock;    

    for (auto const& it : this->allow_crash_fs_operations) {
        this->crash_faults_before_map.insert ({it, {}});
        this->crash_faults_after_map.insert ({it, {}});
    }
}

LazyFS::~LazyFS () {}

void LazyFS::trigger_crash_fault (string opname,
                                  string optiming,
                                  string from_op_path,
                                  string to_op_path) {

    vector<pair<std::regex, string>> opfaults;
    const int length = from_op_path.length();
 /*
    // declaring character array (+1 for null terminator)
    char* char_array = new char[length + 1];
 
    // copying the contents of the
    // string to char array
    strcpy(char_array, from_op_path.c_str());
    struct stat buffer;
    int status;

    status = lstat(char_array, &buffer);
*/

    if (optiming == "before") {
        opfaults = this->crash_faults_before_map.at (opname);
    } else {
        opfaults = this->crash_faults_after_map.at (opname);
    }

    if (opfaults.size () > 0) {

        bool is_multi_path = false;
        if (this_ ()->fs_op_multi_path.find (opname) != this_ ()->fs_op_multi_path.end ()) {
            is_multi_path = true;
        }

        for (auto const& it : opfaults) {

            bool regex_match = true;

            auto const& from_rgx = it.first;

            if (is_multi_path) {

                string to = it.second;
                std::regex to_rgx (".*" + to + ".*");

                if (std::regex_match ("none", from_rgx))
                    regex_match = std::regex_match (from_op_path, from_rgx);
                if (to != "none")
                    regex_match = regex_match && std::regex_match (to_op_path, to_rgx);

            } else {

                regex_match = std::regex_match (from_op_path, from_rgx);
            }

            if (regex_match) {

                spdlog::critical ("Triggered fault condition (op={},timing={})", opname, optiming);

                this_ ()->command_unsynced_data_report ();

                pid_t lazyfs_pid = getpid ();

                spdlog::critical ("Killing LazyFS pid {}!", lazyfs_pid);

                kill (lazyfs_pid, SIGKILL);
            }
        }
    }
}

void LazyFS::add_crash_fault (string crash_timing,
                              string crash_operation,
                              string crash_regex_from,
                              string crash_regex_to) {

    if (crash_timing == "before") {

        auto& crash_regex_list = crash_faults_before_map.at (crash_operation);
        std::regex from_rgx (".*" + crash_regex_from + ".*");
        crash_regex_list.push_back ({from_rgx, crash_regex_to});

    } else {

        auto& crash_regex_list = crash_faults_after_map.at (crash_operation);
        std::regex from_rgx (".*" + crash_regex_from + ".*");
        crash_regex_list.push_back ({from_rgx, crash_regex_to});
    }
}

void LazyFS::command_unsynced_data_report () {

    spdlog::warn ("[lazyfs.cmds]: report request submitted...");

    auto const& res = FSCache->report_unsynced_data ();

    spdlog::warn ("[lazyfs.cmds]: report generated.");

    if (res.empty ()) {

        spdlog::info ("[lazyfs.cmds]: report: all data blocks are synced!");

    } else {
        size_t total_bytes_unsynced = 0;

        for (auto const& it : res) {

            string ino        = string (get<0> (it).c_str ());
            auto files_mapped = FSCache->find_files_mapped_to_inode (ino);

            if (std::get<2> (it).size () > 0 && files_mapped.size () > 0) {

                spdlog::info ("[lazyfs.cmds]: report: [inode {}] is not fully synced, info:", ino);

                int last_block_index     = -1;
                int last_block_off_start = -1;
                int first_block_id       = -1;

                for (auto block_it = std::get<2> (it).begin (); block_it != std::get<2> (it).end ();
                     block_it++) {

                    int block_id = get<0> (*block_it);
                    auto offs    = get<1> (*block_it);

                    if (last_block_index < 0) {
                        last_block_index     = block_id;
                        last_block_off_start = offs.first;
                        first_block_id       = block_id;
                    }

                    if (block_id != (get<0> (*std::next (block_it)) - 1)) {

                        spdlog::info (
                            "[lazyfs.cmds]: report: [inode {}] info: (block {}) to (block "
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

                spdlog::info ("[lazyfs.cmds]: report: [inode {}] files mapped to this inode:", ino);

                for (auto it = files_mapped.begin (); it != files_mapped.end (); ++it)
                    spdlog::info ("[lazyfs.cmds]: report: [inode {}] => file: '{}'",
                                  ino,
                                  *it.base ());
            }
        }

        spdlog::info ("[lazyfs.cmds]: report: total number of bytes un-fsynced: {} bytes.\n",
                      total_bytes_unsynced);
    }
}

void LazyFS::command_fault_clear_cache () {

    std::unique_lock<std::shared_mutex> lock (cache_command_lock);

    spdlog::warn ("[lazyfs.cmds]: clear cache request submitted...");

    FSCache->clear_all_cache ();

    spdlog::warn ("[lazyfs.cmds]: cache is cleared.");
}

void LazyFS::command_display_cache_usage () {

    std::unique_lock<std::shared_mutex> lock (cache_command_lock);

    spdlog::warn ("[lazyfs.cmds]: cache usage (\%pages) is {}%", FSCache->get_cache_usage ());
}

void LazyFS::command_checkpoint () {

    std::unique_lock<std::shared_mutex> lock (cache_command_lock);

    spdlog::warn ("[lazyfs.cmds]: cache checkpoint request submitted...");

    FSCache->full_checkpoint ();

    spdlog::warn ("[lazyfs.cmds]: checkpoint is done.");
}

void LazyFS::restart_counter(string path, string op) {
    auto it = faults->find(path);
    if (it != faults->end()) {
        auto& v_faults = it->second;
        for (auto fault : v_faults) {
            if (fault->op == op) fault->counter.store(0);
        }
    }
}

bool LazyFS::check_pendingwrite(const char* path) {
    bool res = false;
    (this->write_lock).lock();
    if (this->pending_write && (res = (strcmp(this->pending_write->path,path) == 0))) {
        delete(this->pending_write); 
        this->pending_write = NULL;
    }
    (this->write_lock).unlock();
    return res;
}

cache::config::Fault* LazyFS::get_and_update_fault(string path, string op) {
    cache::config::Fault* fault_r = NULL;
    auto it = faults->find(path);
    if (it != faults->end()) {
        cout << ">>>>> Found fault!!" << endl;
        auto& v_faults = it->second;
        for (auto fault : v_faults) {
            if (fault->op == op) {fault->counter +=1; fault_r = fault;}
        }
    } else cout << ">>>>> Not found!!" << endl;
    return fault_r;
}

void LazyFS::persist_write(int fd, const char* path, const char* buf, size_t size, off_t offset) {
    string path_str(path);
    int pw,fd1;
    cache::config::Fault* fault = get_and_update_fault(path_str,"write");
    
    if (fault) { //Fault for path found
        (this->write_lock).lock();
        if (this->pending_write!=NULL) {
            fd1 = open (this->pending_write->path, O_CREAT|O_RDWR, 0666);
            pw = pwrite(fd1,this->pending_write->buf,this->pending_write->size,this->pending_write->offset);

            delete(this->pending_write); this->pending_write = NULL;    
            close(fd1);

            if ((fault->persist).size()==1) {
                this_ ()->add_crash_fault ("after", "write", path_str, "none");            
                spdlog::critical ("[lazyfs.faults]: Added crash fault ");
                spdlog::critical ("[lazyfs.faults]: => crash: timing = after");
                spdlog::critical ("[lazyfs.faults.worker]: => crash: operation = write");
                spdlog::critical ("[lazyfs.faults.worker]: => crash: from regex path = {}",path_str);     
                return;
            }
        }
        (this->write_lock).unlock();


        if (find((fault->persist).begin(), (fault->persist).end(), fault->counter) != (fault->persist).end()) { //If count is in vector persist
            if (fault->counter==1) { //If array persist has 1st write, we need to store this write until we know another write will happen
                if ((fault->count_ocurrence).load()==fault->ocurrence) {
                    (this->write_lock).lock();
                    this->pending_write = new Write(path,buf,size,offset);
                    (this->write_lock).unlock();
                }
            } else {
                //if ((fault->count_ocurrence).load() == this->fault->ocurrence) {
                    pw = pwrite(fd,buf,size,offset);

                    int max = *max_element(fault->persist.begin(), fault->persist.end());
                    if (max == fault->counter) {
                        this_ ()->add_crash_fault ("after", "write", path_str, "none"); 
                        spdlog::critical ("[lazyfs.faults]: Added crash fault ");
                        spdlog::critical ("[lazyfs.faults]: => crash: timing = after");
                        spdlog::critical ("[lazyfs.faults.worker]: => crash: operation = write");
                        spdlog::critical ("[lazyfs.faults.worker]: => crash: from regex path = {}",path_str);            
                    }
                //} else 
                  //  (fault->count_ocurrence).store(0);
            }
        } 
    } 
}



void* LazyFS::lfs_init (struct fuse_conn_info* conn, struct fuse_config* cfg) {

    (void)conn;

    cfg->entry_timeout    = 0;
    cfg->attr_timeout     = 0;
    cfg->negative_timeout = 0;
    cfg->use_ino          = 1;
    // cfg->direct_io        = 1;

    new (this_ ()->faults_handler_thread) std::thread (this_ ()->fht_worker, this_ ());

    return this_ ();
}

void LazyFS::lfs_destroy (void*) { spdlog::info ("[lazyfs]: stopping LazyFS..."); }

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

int LazyFS::lfs_open (const char* path, struct fuse_file_info* fi) {

    this_ ()->trigger_crash_fault ("open", "before", path, "");

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

    this_ ()->trigger_crash_fault ("open", "after", path, "");

    return 0;
}

int LazyFS::lfs_create (const char* path, mode_t mode, struct fuse_file_info* fi) {

    this_ ()->trigger_crash_fault ("create", "before", path, "");

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

    this_ ()->trigger_crash_fault ("create", "after", path, "");

    return 0;
}

int LazyFS::lfs_write (const char* path,
                       const char* buf,
                       size_t size,
                       off_t offset,
                       struct fuse_file_info* fi) {

    this_ ()->trigger_crash_fault ("write", "before", path, "");

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

        res = pwrite (fd, buf, size, offset);

    } else {

        // ----------------------------------------------------------------------------------

        bool locked_this = this_ ()->FSCache->lockItemCheckExists (inode);

        bool cache_had_owner   = this_ ()->FSCache->has_content_cached (inode);
        off_t FILE_SIZE_BEFORE = 0;

        if (!locked_this) {

            FILE_SIZE_BEFORE = stats.st_size;

        } else {

            Metadata* meta_now = this_ ()->FSCache->get_content_metadata (inode);

            if (meta_now != nullptr)
                FILE_SIZE_BEFORE = this_ ()->FSCache->get_content_metadata (inode)->size;

            this_ ()->FSCache->unlockItem (inode);
        }

        // std::printf ("\twrite: file size before is %d bytes\n", (int)FILE_SIZE_BEFORE);

        // ----------------------------------------------------------------------------------

        if (fi != NULL) {

            if (FILE_SIZE_BEFORE < offset) {

                // fill sparse write with zeros, how?
                // [file_size_offset, offset - 1] = zeros

                std::printf ("calling a sparse write...\n");

                off_t size_to_fill = offset - std::max (FILE_SIZE_BEFORE, (off_t)0);

                if (size_to_fill > 0) {

                    char* fill_zeros = (char*)calloc (size_to_fill, 1);
                    lfs_write (path, fill_zeros, size_to_fill, FILE_SIZE_BEFORE, NULL);
                    free (fill_zeros);
                }
            }
        }

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

        bool cache_full = (this_ ()->FSCache->get_cache_usage () == 100) &&
                          !this_ ()->FSConfig->APPLY_LRU_EVICTION;

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

        bool locked = this_ ()->FSCache->lockItemCheckExists (inode);

        if (locked) {

            off_t was_written_until_offset = offset + size;

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

                meta.ctim = modify_time;
                values_to_change.push_back ("ctime");
            }

            this_ ()->FSCache->update_content_metadata (inode, meta, values_to_change);

            // std::printf ("\twrite: file size now is %d bytes\n", (int)meta.size);

            this_ ()->FSCache->unlockItem (inode);
        }

        close (fd_caching);
    }

    // ----------------------------------------------------------------------------------
    
    this_ () -> persist_write(fd,path,buf,size,offset);

    // res should be = actual bytes written as pwrite could fail...
    res = size;

    // std::printf ("\twrite: returning %d bytes\n", res);

    if (res == -1)
        res = -errno;

    if (fi == NULL)
        close (fd);

    

    this_ ()->trigger_crash_fault ("write", "after", path, "");

    return res;
}

int LazyFS::lfs_read (const char* path,
                      char* buf,
                      size_t size,
                      off_t offset,
                      struct fuse_file_info* fi) {

    this_ ()->trigger_crash_fault ("read", "before", path, "");

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

    bool cache_had_owner = this_ ()->FSCache->has_content_cached (inode);

    Metadata meta;
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

    this_ ()->trigger_crash_fault ("read", "after", path, "");

    return res;
}

int LazyFS::lfs_fsync (const char* path, int isdatasync, struct fuse_file_info* fi) {

    this_ ()->trigger_crash_fault ("fsync", "before", path, "");

    std::shared_lock<std::shared_mutex> lock (cache_command_lock);

    struct stat stats;
    lfs_getattr (path, &stats, fi);

    if (this_ ()->FSConfig->log_all_operations) {
        spdlog::info ("[lazyfs.ops]: {}(path={},isdatasync={})", __FUNCTION__, path, isdatasync);
    }
    
    this_ ()->restart_counter(path,"write"); //Reset counter of op write for this path
    this_ ()->check_pendingwrite(path); //If there's a pending write, remove it

    string owner (path);
    string inode = this_ ()->FSCache->get_original_inode (owner);

    bool is_owner_cached = this_ ()->FSCache->has_content_cached (inode);

    int res;

    if (isdatasync)
        res = is_owner_cached ? this_ ()->FSCache->sync_owner (inode, true, (char*)path)
                              : fdatasync (fi->fh);
    else
        res = is_owner_cached ? this_ ()->FSCache->sync_owner (inode, false, (char*)path)
                              : fsync (fi->fh);

    this_ ()->trigger_crash_fault ("fsync", "after", path, "");

    return res;
}

int LazyFS::lfs_is_dir_empty (const char* dirname) {

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

    this_ ()->trigger_crash_fault ("rename", "before", from, to);

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

    this_ ()->trigger_crash_fault ("rename", "after", from, to);

    return 0;
}

int LazyFS::lfs_truncate (const char* path, off_t truncate_size, struct fuse_file_info* fi) {
    cout << ">> FTRUNCATE" << endl;

    this_ ()->trigger_crash_fault ("truncate", "before", path, "");

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

    this_ ()->trigger_crash_fault ("truncate", "after", path, "");

    return 0;
}

int LazyFS::lfs_symlink (const char* from, const char* to) {

    this_ ()->trigger_crash_fault ("symlink", "before", from, to);

    std::shared_lock<std::shared_mutex> lock (cache_command_lock);

    if (this_ ()->FSConfig->log_all_operations) {
        spdlog::info ("[lazyfs.ops]: {}(from={},to={})", __FUNCTION__, from, to);
    }

    int res;

    res = symlink (from, to);
    if (res == -1)
        return -errno;

    this_ ()->trigger_crash_fault ("symlink", "after", from, to);

    return 0;
}

int LazyFS::lfs_access (const char* path, int mask) {

    this_ ()->trigger_crash_fault ("access", "before", path, "");

    std::shared_lock<std::shared_mutex> lock (cache_command_lock);

    if (this_ ()->FSConfig->log_all_operations) {
        spdlog::info ("[lazyfs.ops]: {}(path={})", __FUNCTION__, path);
    }

    int res;

    res = access (path, mask);
    if (res == -1)
        return -errno;

    this_ ()->trigger_crash_fault ("access", "after", path, "");

    return 0;
}

int LazyFS::lfs_mkdir (const char* path, mode_t mode) {

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

int LazyFS::lfs_link (const char* from, const char* to) {

    this_ ()->trigger_crash_fault ("link", "before", from, to);

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

    this_ ()->trigger_crash_fault ("link", "after", from, to);

    return 0;
}

int LazyFS::lfs_readlink (const char* path, char* buf, size_t size) {

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

    std::shared_lock<std::shared_mutex> lock (cache_command_lock);

    if (this_ ()->FSConfig->log_all_operations) {
        spdlog::info ("[lazyfs.ops]: {}(path={})", __FUNCTION__, path);
    }

    (void)path;

    close (fi->fh);

    return 0;
}

int LazyFS::lfs_rmdir (const char* path) {

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

int LazyFS::lfs_chmod (const char* path, mode_t mode, struct fuse_file_info* fi) {

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

    this_ ()->trigger_crash_fault ("unlink", "before", path, "");

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

    this_ ()->trigger_crash_fault ("unlink", "after", path, "");

    return 0;
}

} // namespace lazyfs
