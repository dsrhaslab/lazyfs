
/**
 * @file lazyfs.hpp
 * @author João Azevedo joao.azevedo@inesctec.pt
 *
 * @copyright Copyright (c) 2020-2022 INESC TEC.
 *
 */

#ifndef _LFS_HPP_
#define _LFS_HPP_

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
    unordered_map<string,vector<cache::config::Fault*>>* faults;

    /**
     * @brief Faults of LazyFS crash injected during runtime.
     *
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

  public:
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
            unordered_map<string,vector<cache::config::Fault*>>* faults);

    /**
     * @brief Destroy the LazyFS object
     *
     */
    ~LazyFS ();

    /**
     * @brief Fifo: (fault) Clear the cached contents
     *
     */
    void command_fault_clear_cache ();

    /**
     * @brief Fifo: (info) Display the cache usage
     *
     */
    void command_display_cache_usage ();

    /**
     * @brief Fifo: (sync) Sync all cached data with the underlying FS
     *
     */
    void command_checkpoint ();

    /**
     * @brief Fifo: Reports which files have unsynced data.
     *
     */
    void command_unsynced_data_report ();

    /**
     * @brief Checks if a programmed reorder fault for the given path and operation exists. If so, updates the counter and returns the fault.
     * @param path Path of the file 
     * @param op Operation ('write','fsync',...)
     * @return Pointer to the ReorderF object
    */
    cache::config::ReorderF* get_and_update_reorder_fault(string path, string op);

    /**
     * @brief Persists a write if a there is a programmed reorder fault for write in the given path and if the counter matches one of the writes to persist.
     * @param path Path of the file
     * @param buf Buffer with what is to be written
     * @param size Bytes to be written
     * @param offset Offset inside the file to start to write
     */
    void persist_write(const char* path, const char* buf, size_t size, off_t offset);

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
     * @brief Splits and persists part of a write if a there is a programmed split_write fault for the given path and if the ocurrence matches the counter.
     * @param path Path of the file
     * @param buf Buffer with what is to be written
     * @param size Bytes to be written
     * @param offset Offset inside the file to start to write
     */
    void split_write(const char* path, const char* buf, size_t size, off_t offset);

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
     * @brief Map of faults associated with each filesystem operation
     *
     */
    // operation -> [((from_rgx, to_rgx), before?true:false), ...]
    std::unordered_map<string, vector<pair<std::regex, string>>> crash_faults_before_map;
    std::unordered_map<string, vector<pair<std::regex, string>>> crash_faults_after_map;

    /**
     * @brief Map of allowed operations to have a crash fault
     *
     */
    std::unordered_set<string> allow_crash_fs_operations = {"unlink",
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

    /**
     * @brief Map of operations that have two paths
     *
     */
    std::unordered_set<string> fs_op_multi_path = {"rename", "link", "symlink"};

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
     * @brief kills lazyfs with SIGINT if any fault condition verifies
     *
     * @param opname operation to check
     * @param optiming one of 'allow_crash_fs_operations'
     * @param from_op_path source path specified in the operation
     * @param dest_op_path destination path specified in the operation
     */
    void
    trigger_crash_fault (string opname, string optiming, string from_op_path, string to_op_path);
};

} // namespace lazyfs

#endif // _LFS_HPP_
