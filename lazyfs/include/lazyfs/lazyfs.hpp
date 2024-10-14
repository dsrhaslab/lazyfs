
/**
 * @file lazyfs.hpp
 * @author João Azevedo joao.azevedo@inesctec.pt
 *
 * @copyright Copyright (c) 2020-2022 INESC TEC.
 *
 */

#ifndef _LFS_HPP_
#define _LFS_HPP_
#define THREAD_ID 1

#include <cache/cache.hpp>
#include <cache/config/config.hpp>
#include <cache/engine/backends/custom/custom_cache.hpp>
#include <lazyfs/fusepp/Fuse-impl.h>
#include <lazyfs/fusepp/Fuse.h>
#include <regex>
#include <thread>
#include <vector>

using namespace cache;

namespace lazyfs {

/**
 * @brief To store writes.
 */
class Write {
  public:
    /**
     * @brief The file path.
     */
    char* path;

    /**
     * @brief The content of the write.
     */
    char* buf;

    /**
     * @brief The number of bytes to write.
     */
    size_t size;

    /**
     * @brief The offset in the file
     */
    off_t offset;

    /**
     * @brief Construct a new Write object.
     */
    Write ();

    /**
     * @brief Construct a new Write object.
     */
    Write (const char* path,
           const char* buf,
           size_t size,
           off_t offset);

    /**
     * @brief Destroy the Write object
     *
     */   
    ~Write ();

};

/**
 * @brief The LazyFS implementation in C++.
 *
 */
class LazyFS : public Fusepp::Fuse<LazyFS> {

  private:
    /**
     * @brief The Cache interface
     *
     */
    Cache* FSCache;

    /**
     * @brief The LazyFS configuration
     *
     */
    cache::config::Config* FSConfig;

    /**
     * @brief Faults handler thread / worker.
     */
    std::thread* faults_handler_thread;

    /**
     * @brief Faults handler method to run inside the thread.
     *
     */
    void (*fht_worker) (LazyFS* filesystem);

    /**
     * @brief Faults programmed in the configuration file.
     */
    unordered_map<string,vector<faults::Fault*>>* faults;


    /**
     * @brief Faults of LazyFS crash injected during runtime.
     *
     * obsolete!
     */
    std::unordered_map<string, unordered_set<string>> crash_faults;

    /**
     * @brief Write that was put on hold and may or may not be persisted.
     */
    Write *pending_write;

    /**
     * @brief Lock for pending write.
    */
    std::mutex write_lock;

    /**
     * @brief Current faults being injected.
    */
    vector<string> injecting_fault;

    /**
     * @brief Lock for current injected faults.
    */
    std::mutex injecting_fault_lock;

    /**
     * @brief Indicates if we need to kill LazyFS (because of fault after return).
    */
    atomic<bool> kill_before;

  public:

    /**
     * @brief Map of faults associated with each filesystem operation
     *
     */
    // operation -> [((from_rgx, to_rgx), ...]
    std::unordered_map<string, vector<pair<std::regex, string>>> crash_faults_before_map;
    std::unordered_map<string, vector<pair<std::regex, string>>> crash_faults_after_map;

    /**
     * @brief Map of allowed operations to have a crash fault
     *
     */
    /*std::unordered_set<string> allow_crash_fs_operations = {"unlink",
                                                            "truncate",
                                                            "fsync",
                                                            "write",
                                                            "create",
                                                            "access",
                                                            "open",
                                                            "read",
                                                            "rename",
                                                            "link",
                                                            "symlink"};
    */
   
    /**
     * @brief Map of operations that have two paths
     *
     */
    //std::unordered_set<string> fs_op_multi_path = {"rename", "link", "symlink"};

    /**
     * @brief Construct a new LazyFS object.
     *
     */
    LazyFS ();

    /**
     * @brief Construct a new LazyFS object.
     *
     * @param cache Cache object
     * @param config LazyFS configuration
     * @param faults_handler_thread A thread to handle fault requests
     * @param fht_worker The faults worker logic method
     */
    LazyFS (Cache* cache,
            cache::config::Config* config,
            std::thread* faults_handler_thread,
            void (*fht_worker) (LazyFS* filesystem),
            unordered_map<string,vector<faults::Fault*>>* faults);

    /**
     * @brief Destroy the LazyFS object
     *
     */
    ~LazyFS ();

    /**
     * @brief Get faults currently being injected.
    */
    vector<string> get_injecting_fault();

    /**
     * @brief Fifo: (fault) Clear the cached contents
     *
     */
    void command_fault_clear_cache (bool lock_needed = true);

    /**
     * @brief Fifo: (fault) Persist the cached pages requested.
     * Only use after obtaining lock on cache_command_lock.
     * 
     * @param path Path of the file
     * @param parts Pages to be persisted
     *
     */
    void command_fault_sync_page (string path, string parts, bool sync_other_files, bool lock_needed = true);

    /**
     * @brief Fifo: (info) Display the cache usage
     *
     */
    void command_display_cache_usage (bool lock_needed = true);

    /**
     * @brief Fifo: (sync) Sync all cached data with the underlying FS
     *
     */
    void command_checkpoint (bool lock_needed = true);

    /**
     * @brief Fifo: Reports which files have unsynced data.
     * @param paths_to_exclude Paths to be excluded from the report.
     *
     */
    void command_unsynced_data_report (vector<string> paths_to_exclude);

    /**
     * @brief Checks if a programmed reorder fault for the given path and operation exists. If so, updates the counter and returns the fault.
     * @param path Path of the file 
     * @param op Operation ('write','fsync',...)
     * @return Pointer to the ReorderF object
    */
    faults::ReorderF* get_and_update_reorder_fault(string path, string op);

    /**
     * @brief Persists a write if a there is a programmed reorder fault for write in the given path and if the counter matches one of the writes to persist.
     * @param path Path of the file
     * @param buf Buffer with what is to be written
     * @param size Bytes to be written
     * @param offset Offset inside the file to start to write
     * @return true if a crash point was added, false otherwise
     */
    bool persist_write(const char* path, const char* buf, size_t size, off_t offset);

    /**
     * @brief Restarts the counter for some operation of a certain path.
     * @param path Path of the file
     * @param op Operation ('write','fsync',...)
    */
    void restart_counter(string path, string op);

    /**
     * @brief Checks the existence of a pending write (a write that could be persisted if it is followed by another one) and deletes it if it exists. 
     * @param path Path of the file
     */
    bool check_and_delete_pendingwrite(const char* path);

    /**
     * @brief Splits and persists part of a write if a there is a programmed split_write fault for the given path and if the occurrence matches the counter.
     * @param path Path of the file
     * @param buf Buffer with what is to be written
     * @param size Bytes to be written
     * @param offset Offset inside the file to start to write
     * @return true if a crash point was added, false otherwise
     */
    bool split_write(const char* path, const char* buf, size_t size, off_t offset);

    /**
     * @brief Checks whether a directory is empty of not by counting the number of entries.
     *        It must be <= 2 to be empty, only containing "." and ".."
     *
     * @param dirname the path for the directory
     * @return int 1 if is empty, 0 if not empty and -1 if an error occurred
     */
    static int lfs_is_dir_empty (const char* dirname);

    /**
     * @brief Returns a vector with the list of regular file names of all childs of a directory
     *
     * @param dirname the directory path
     * @param result list of filenames
     */
    static void lfs_get_dir_filenames (const char* dirname, std::vector<string>* result);

    /**
     * @brief Calls rename recursively to all child files and directories
     *
     * @param from the current subpath
     * @param to desired path
     * @param flags fuse rename flags
     * @return int
     */
    static int lfs_recursive_rename (const char* from, const char* to, unsigned int flags);

    static void* lfs_init (struct fuse_conn_info*, struct fuse_config* cfg);

    static void lfs_destroy (void*);

    static int lfs_symlink (const char* from, const char* to);

    static int lfs_release (const char* path, struct fuse_file_info* fi);

    static int lfs_link (const char* from, const char* to);

    static int lfs_readlink (const char* path, char* buf, size_t size);

    static int lfs_unlink (const char* path);

    static int lfs_rename (const char* from, const char* to, unsigned int flags);

    static int lfs_truncate (const char* path, off_t size, struct fuse_file_info* fi);

    off_t lfs_lseek (const char* path, off_t off, int whence, struct fuse_file_info* fi);

    //#ifdef HAVE_UTIMENSAT

    static int
    lfs_utimens (const char* path, const struct timespec ts[2], struct fuse_file_info* fi);

    // #endif

#ifdef HAVE_SETXATTR

    static int lfs_removexattr (const char* path, const char* name);

    static int lfs_listxattr (const char* path, char* list, size_t size);

    static int lfs_getxattr (const char* path, const char* name, char* value, size_t size);

    static int
    lfs_setxattr (const char* path, const char* name, const char* value, size_t size, int flags);

#endif /* HAVE_SETXATTR */

    static int lfs_fsync (const char* path, int isdatasync, struct fuse_file_info* fi);

    static int lfs_write (const char* path,
                          const char* buf,
                          size_t size,
                          off_t offset,
                          struct fuse_file_info* fi);

    static int lfs_create (const char* path, mode_t mode, struct fuse_file_info* fi);

    static int lfs_rmdir (const char* path);

    static int lfs_mkdir (const char* path, mode_t mode);

    static int lfs_access (const char* path, int mask);

    static int lfs_getattr (const char*, struct stat*, struct fuse_file_info*);

    static int lfs_readdir (const char* path,
                            void* buf,
                            fuse_fill_dir_t filler,
                            off_t offset,
                            struct fuse_file_info* fi,
                            enum fuse_readdir_flags);

    static int lfs_open (const char* path, struct fuse_file_info* fi);

    static int
    lfs_read (const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi);

    static int lfs_chmod (const char*, mode_t, struct fuse_file_info*);
    static int lfs_chown (const char*, uid_t, gid_t, fuse_file_info*);

    /**
     * @brief Adds a crash fault to the faults map
     *
     * @param crash_timing 'before' or 'after'
     * @param crash_operation one of 'allow_crash_fs_operations'
     * @param crash_regex_from a regex indicating the source fault path
     * @param crash_regex_to a regex indicating the destination fault path (for some operations like
     * rename, link...)
     */
    void add_crash_fault (string crash_timing,
                          string crash_operation,
                          string crash_regex_from,
                          string crash_regex_to);

    /**
     * @brief Adds a torn-seq fault to the faults map. Returns a vector with errors if any.
     * 
     * @param path path of the fault
     * @param op system call 
     * @param persist which parts of the write to persist
     * @param ret_ if the current system call is finished before crashing
     * @return errors
    */
    vector<string> add_torn_seq_fault(string path, string op, string persist, string ret_);

    /**
     * @brief Adds a torn-op fault to the faults map. Returns a vector with errors if any.
     * 
     * @param path path of the fault
     * @param parts which parts of the write to persist
     * @param parts_bytes division of the write in bytes
     * @param persist which parts of the write to persist
     * @param ret_ if the current system call is finished before crashing
     * @return errors
    */
    vector<string> add_torn_op_fault(string path, string parts, string parts_bytes, string persist, string ret_);

    /**
     * @brief Kills lazyfs with SIGKILL if any fault condition verifies
     *
     * @param opname one of 'allow_crash_fs_operations'
     * @param optiming timing for triggering fault operation ('before' or 'after' a given system call)
     * @param from_op_path source path specified in the operation
     * @param dest_op_path destination path specified in the operation
     */
    bool trigger_crash_fault (string opname, string optiming, string from_op_path, string to_op_path, bool lock_needed = true);

    /**
     * @brief Triggers a clear-cache fault if condition is verified.
     * (Important) Assumes that a lock of the cache is obtained. This is because this function is called by system calls that already have a lock.
     *
     * @param opname operation name
     * @param optiming timing for triggering fault operation ('before' or 'after')
     * @param from_op_path source path specified in the operation
     * @param dest_op_path destination path specified in the operation
     */
    bool trigger_configured_clear_fault (string opname, string optiming, string from_path, string to_path, bool lock_needed = true);

    /**
     * @brief Kills lazyfs with SIGKILL if kill_before is true.
     */
    void check_kill_before();

    void print_faults ();
};

} // namespace lazyfs

#endif // _LFS_HPP_
