// Hello filesystem class definition

#ifndef _LFS_HPP_
#define _LFS_HPP_

#include <cache/cache.hpp>
#include <cache/config/config.hpp>
#include <cache/engine/backends/custom/custom_cache.hpp>
#include <lazyfs/fusepp/Fuse-impl.h>
#include <lazyfs/fusepp/Fuse.h>
#include <thread>

using namespace cache;

namespace lazyfs {

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

  public:
    /**
     * @brief Construct a new LazyFS object
     *
     */
    LazyFS ();

    /**
     * @brief Construct a new LazyFS object
     *
     * @param cache Cache object
     * @param config LazyFS configuration
     * @param faults_handler_thread A thread to handle fault requests
     * @param fht_worker The faults worker logic method
     */
    LazyFS (Cache* cache,
            cache::config::Config* config,
            std::thread* faults_handler_thread,
            void (*fht_worker) (LazyFS* filesystem));

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
};

} // namespace lazyfs

#endif // _LFS_HPP_
