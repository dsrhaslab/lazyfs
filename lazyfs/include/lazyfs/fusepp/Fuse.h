//==================================================================
/**
 *  FuseApp -- A simple C++ wrapper for the FUSE filesystem
 *
 *  Copyright (C) 2021 by James A. Chappell (rlrrlrll@gmail.com)
 *
 *  Permission is hereby granted, free of charge, to any person
 *  obtaining a copy of this software and associated documentation
 *  files (the "Software"), to deal in the Software without
 *  restriction, including without limitation the rights to use,
 *  copy, modify, merge, publish, distribute, sublicense, and/or
 *  sell copies of the Software, and to permit persons to whom the
 *  Software is furnished to do so, subject to the following
 *  condition:
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *  OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *  HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 *  OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __FUSE_APP_H__
#define __FUSE_APP_H__

#ifndef FUSE_USE_VERSION
#define FUSE_USE_VERSION 35
#endif

#include <cache/cache.hpp>
#include <cache/engine/backends/custom/custom_cache.hpp>
#include <cstring>
#include <fuse3/fuse.h>

using namespace cache;
using namespace cache::engine::backends::custom;

namespace Fusepp {

typedef int (*t_getattr) (const char*, struct stat*, struct fuse_file_info*);
typedef int (*t_readlink) (const char*, char*, size_t);
typedef int (*t_mknod) (const char*, mode_t, dev_t);
typedef int (*t_mkdir) (const char*, mode_t);
typedef int (*t_unlink) (const char*);
typedef int (*t_rmdir) (const char*);
typedef int (*t_symlink) (const char*, const char*);
typedef int (*t_rename) (const char*, const char*, unsigned int);
typedef int (*t_link) (const char*, const char*);
typedef int (*t_chmod) (const char*, mode_t, struct fuse_file_info*);
typedef int (*t_chown) (const char*, uid_t, gid_t, fuse_file_info*);
typedef int (*t_truncate) (const char*, off_t, fuse_file_info*);
typedef int (*t_open) (const char*, struct fuse_file_info*);
typedef int (*t_read) (const char*, char*, size_t, off_t, struct fuse_file_info*);
typedef int (*t_write) (const char*, const char*, size_t, off_t, struct fuse_file_info*);
typedef int (*t_statfs) (const char*, struct statvfs*);
typedef int (*t_flush) (const char*, struct fuse_file_info*);
typedef int (*t_release) (const char*, struct fuse_file_info*);
typedef int (*t_fsync) (const char*, int, struct fuse_file_info*);
typedef int (*t_setxattr) (const char*, const char*, const char*, size_t, int);
typedef int (*t_getxattr) (const char*, const char*, char*, size_t);
typedef int (*t_listxattr) (const char*, char*, size_t);
typedef int (*t_removexattr) (const char*, const char*);
typedef int (*t_opendir) (const char*, struct fuse_file_info*);
typedef int (*t_readdir) (const char*,
                          void*,
                          fuse_fill_dir_t,
                          off_t,
                          struct fuse_file_info*,
                          enum fuse_readdir_flags);
typedef int (*t_releasedir) (const char*, struct fuse_file_info*);
typedef int (*t_fsyncdir) (const char*, int, struct fuse_file_info*);
typedef void* (*t_init) (struct fuse_conn_info*, struct fuse_config* cfg);
typedef void (*t_destroy) (void*);
typedef int (*t_access) (const char*, int);
typedef int (*t_create) (const char*, mode_t, struct fuse_file_info*);
typedef int (*t_lock) (const char*, struct fuse_file_info*, int cmd, struct flock*);
typedef int (*t_utimens) (const char*, const struct timespec tv[2], struct fuse_file_info* fi);
typedef int (*t_bmap) (const char*, size_t blocksize, uint64_t* idx);

#if FUSE_USE_VERSION < 35
typedef int (*t_ioctl) (const char*,
                        int cmd,
                        void* arg,
#else
typedef int (*t_ioctl) (const char*,
                        unsigned int cmd,
                        void* arg,
#endif
                        struct fuse_file_info*,
                        unsigned int flags,
                        void* data);
typedef int (*t_poll) (const char*,
                       struct fuse_file_info*,
                       struct fuse_pollhandle* ph,
                       unsigned* reventsp);
typedef int (*t_write_buf) (const char*,
                            struct fuse_bufvec* buf,
                            off_t off,
                            struct fuse_file_info*);
typedef int (*t_read_buf) (const char*,
                           struct fuse_bufvec** bufp,
                           size_t size,
                           off_t off,
                           struct fuse_file_info*);
typedef int (*t_flock) (const char*, struct fuse_file_info*, int op);
typedef int (*t_fallocate) (const char*, int, off_t, off_t, struct fuse_file_info*);

template<class T> class Fuse {

  public:
    Fuse () {
        memset (&T::operations_, 0, sizeof (struct fuse_operations));
        load_operations_ ();
    }

    // no copy
    Fuse (const Fuse&) = delete;
    Fuse& operator= (const Fuse&) = delete;
    Fuse& operator= (Fuse&&) = delete;

    ~Fuse () = default;

    auto run (int argc, char** argv) { return fuse_main (argc, argv, Operations (), this); }

    auto Operations () { return &operations_; }

    static auto this_ () { return static_cast<T*> (fuse_get_context ()->private_data); }

  private:
    static void load_operations_ () {
        operations_.getattr     = T::lfs_getattr;
        operations_.readlink    = T::lfs_readlink;
        operations_.mknod       = T::lfs_mknod;
        operations_.mkdir       = T::lfs_mkdir;
        operations_.unlink      = T::lfs_unlink;
        operations_.rmdir       = T::lfs_rmdir;
        operations_.symlink     = T::lfs_symlink;
        operations_.rename      = T::lfs_rename;
        operations_.link        = T::lfs_link;
        operations_.chmod       = T::lfs_chmod;
        operations_.chown       = T::lfs_chown;
        operations_.truncate    = T::lfs_truncate;
        operations_.open        = T::lfs_open;
        operations_.read        = T::lfs_read;
        operations_.write       = T::lfs_write;
        operations_.statfs      = T::lfs_statfs;
        operations_.flush       = T::lfs_flush;
        operations_.release     = T::lfs_release;
        operations_.fsync       = T::lfs_fsync;
        operations_.setxattr    = T::lfs_setxattr;
        operations_.getxattr    = T::lfs_getxattr;
        operations_.listxattr   = T::lfs_listxattr;
        operations_.removexattr = T::lfs_removexattr;
        operations_.opendir     = T::lfs_opendir;
        operations_.readdir     = T::lfs_readdir;
        operations_.releasedir  = T::lfs_releasedir;
        operations_.fsyncdir    = T::lfs_fsyncdir;
        operations_.init        = T::lfs_init;
        operations_.destroy     = T::lfs_destroy;
        operations_.access      = T::lfs_access;
        operations_.create      = T::lfs_create;
        operations_.lock        = T::lfs_lock;
        operations_.utimens     = T::lfs_utimens;
        operations_.bmap        = T::lfs_bmap;
        operations_.ioctl       = T::lfs_ioctl;
        operations_.poll        = T::lfs_poll;
        operations_.write_buf   = T::lfs_write_buf;
        operations_.read_buf    = T::lfs_read_buf;
        operations_.flock       = T::lfs_flock;
        operations_.fallocate   = T::lfs_fallocate;
    }

    static struct fuse_operations operations_;

    static t_getattr lfs_getattr;
    static t_readlink lfs_readlink;
    static t_mknod lfs_mknod;
    static t_mkdir lfs_mkdir;
    static t_unlink lfs_unlink;
    static t_rmdir lfs_rmdir;
    static t_symlink lfs_symlink;
    static t_rename lfs_rename;
    static t_link lfs_link;
    static t_chmod lfs_chmod;
    static t_chown lfs_chown;
    static t_truncate lfs_truncate;
    static t_open lfs_open;
    static t_read lfs_read;
    static t_write lfs_write;
    static t_statfs lfs_statfs;
    static t_flush lfs_flush;
    static t_release lfs_release;
    static t_fsync lfs_fsync;
    static t_setxattr lfs_setxattr;
    static t_getxattr lfs_getxattr;
    static t_listxattr lfs_listxattr;
    static t_removexattr lfs_removexattr;
    static t_opendir lfs_opendir;
    static t_readdir lfs_readdir;
    static t_releasedir lfs_releasedir;
    static t_fsyncdir lfs_fsyncdir;
    static t_init lfs_init;
    static t_destroy lfs_destroy;
    static t_access lfs_access;
    static t_create lfs_create;
    static t_lock lfs_lock;
    static t_utimens lfs_utimens;
    static t_bmap lfs_bmap;
    static t_ioctl lfs_ioctl;
    static t_poll lfs_poll;
    static t_write_buf lfs_write_buf;
    static t_read_buf lfs_read_buf;
    static t_flock lfs_flock;
    static t_fallocate lfs_fallocate;
};
}; // namespace Fusepp

#endif
