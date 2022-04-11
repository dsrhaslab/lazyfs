//==================================================================
/**
 *  FuseApp -- A simple C++ wrapper for the FUSE filesystem
 *
 *  Copyright (C) 2017 by James A. Chappell (rlrrlrll@gmail.com)
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

#include <lazyfs/fusepp/Fuse.h>

template<class T> Fusepp::t_getattr Fusepp::Fuse<T>::lfs_getattr         = nullptr;
template<class T> Fusepp::t_readlink Fusepp::Fuse<T>::lfs_readlink       = nullptr;
template<class T> Fusepp::t_mknod Fusepp::Fuse<T>::lfs_mknod             = nullptr;
template<class T> Fusepp::t_mkdir Fusepp::Fuse<T>::lfs_mkdir             = nullptr;
template<class T> Fusepp::t_unlink Fusepp::Fuse<T>::lfs_unlink           = nullptr;
template<class T> Fusepp::t_rmdir Fusepp::Fuse<T>::lfs_rmdir             = nullptr;
template<class T> Fusepp::t_symlink Fusepp::Fuse<T>::lfs_symlink         = nullptr;
template<class T> Fusepp::t_rename Fusepp::Fuse<T>::lfs_rename           = nullptr;
template<class T> Fusepp::t_link Fusepp::Fuse<T>::lfs_link               = nullptr;
template<class T> Fusepp::t_chmod Fusepp::Fuse<T>::lfs_chmod             = nullptr;
template<class T> Fusepp::t_chown Fusepp::Fuse<T>::lfs_chown             = nullptr;
template<class T> Fusepp::t_truncate Fusepp::Fuse<T>::lfs_truncate       = nullptr;
template<class T> Fusepp::t_open Fusepp::Fuse<T>::lfs_open               = nullptr;
template<class T> Fusepp::t_read Fusepp::Fuse<T>::lfs_read               = nullptr;
template<class T> Fusepp::t_write Fusepp::Fuse<T>::lfs_write             = nullptr;
template<class T> Fusepp::t_statfs Fusepp::Fuse<T>::lfs_statfs           = nullptr;
template<class T> Fusepp::t_flush Fusepp::Fuse<T>::lfs_flush             = nullptr;
template<class T> Fusepp::t_release Fusepp::Fuse<T>::lfs_release         = nullptr;
template<class T> Fusepp::t_fsync Fusepp::Fuse<T>::lfs_fsync             = nullptr;
template<class T> Fusepp::t_setxattr Fusepp::Fuse<T>::lfs_setxattr       = nullptr;
template<class T> Fusepp::t_getxattr Fusepp::Fuse<T>::lfs_getxattr       = nullptr;
template<class T> Fusepp::t_listxattr Fusepp::Fuse<T>::lfs_listxattr     = nullptr;
template<class T> Fusepp::t_removexattr Fusepp::Fuse<T>::lfs_removexattr = nullptr;
template<class T> Fusepp::t_opendir Fusepp::Fuse<T>::lfs_opendir         = nullptr;
template<class T> Fusepp::t_readdir Fusepp::Fuse<T>::lfs_readdir         = nullptr;
template<class T> Fusepp::t_releasedir Fusepp::Fuse<T>::lfs_releasedir   = nullptr;
template<class T> Fusepp::t_fsyncdir Fusepp::Fuse<T>::lfs_fsyncdir       = nullptr;
template<class T> Fusepp::t_init Fusepp::Fuse<T>::lfs_init               = nullptr;
template<class T> Fusepp::t_destroy Fusepp::Fuse<T>::lfs_destroy         = nullptr;
template<class T> Fusepp::t_access Fusepp::Fuse<T>::lfs_access           = nullptr;
template<class T> Fusepp::t_create Fusepp::Fuse<T>::lfs_create           = nullptr;
template<class T> Fusepp::t_lock Fusepp::Fuse<T>::lfs_lock               = nullptr;
template<class T> Fusepp::t_utimens Fusepp::Fuse<T>::lfs_utimens         = nullptr;
template<class T> Fusepp::t_bmap Fusepp::Fuse<T>::lfs_bmap               = nullptr;
template<class T> Fusepp::t_ioctl Fusepp::Fuse<T>::lfs_ioctl             = nullptr;
template<class T> Fusepp::t_poll Fusepp::Fuse<T>::lfs_poll               = nullptr;
template<class T> Fusepp::t_write_buf Fusepp::Fuse<T>::lfs_write_buf     = nullptr;
template<class T> Fusepp::t_read_buf Fusepp::Fuse<T>::lfs_read_buf       = nullptr;
template<class T> Fusepp::t_flock Fusepp::Fuse<T>::lfs_flock             = nullptr;
template<class T> Fusepp::t_fallocate Fusepp::Fuse<T>::lfs_fallocate     = nullptr;

template<class T> struct fuse_operations Fusepp::Fuse<T>::operations_;