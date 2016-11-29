/**
 * fuse_calls.hpp
 */


#ifndef __FUSE_CALLS_H_INCLUDED__   // if grosfs.h hasn't been included yet...
#define __FUSE_CALLS_H_INCLUDED__   //   #define this so the compiler knows it has been included

#define FUSE_USE_VERSION 30

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef linux
/* For pread()/pwrite()/utimensat() */
#define _XOPEN_SOURCE 700
#endif

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
//#include <errno.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/uio.h>

#include "grosfs.hpp"
#include "disk.hpp"
#include "files.hpp"

#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

struct fusedata {
    Disk * disk;
};

// Initialize the filesystem. This function can often be left unimplemented, but it can be a handy way to perform one-time setup such as allocating variable-sized data structures or initializing a new filesystem. The fuse_conn_info structure gives information about what features are supported by FUSE, and can be used to request certain capabilities (see below for more information). The return value of this function is available to all file operations in the private_data field of fuse_context. It is also passed as a parameter to the destroy() method. (Note: see the warning under Other Options below, regarding relative pathnames.)
void * grosfs_init( struct fuse_conn_info * conn );

// Called when the filesystem exits. The private_data comes from the return value of init.
void grosfs_destroy( void * private_data );

// Return file attributes. The "stat" structure is described in detail in the stat(2) manual page. For the given pathname, this should fill in the elements of the "stat" structure. If a field is meaningless or semi-meaningless (e.g., st_ino) then it should be set to 0 or given a "reasonable" value. This call is pretty much required for a usable filesystem.
int grosfs_getattr( const char * path, struct stat * stbuf );

// As getattr, but called when fgetattr(2) is invoked by the user program.
int grosfs_fgetattr( const char * path, struct stat * stbuf, struct fuse_file_info *fi );

// This is the same as the access(2) system call. It returns -ENOENT if the path doesn't exist, -EACCESS if the requested permission isn't available, or 0 for success. Note that it can be called on files, directories, or any other object that appears in the filesystem. This call is not required but is highly recommended.
int grosfs_access( const char * path, int mask );

// If path is a symbolic link, fill buf with its target, up to size. See readlink(2) for how to handle a too-small buffer and for error codes. Not required if you don't support symbolic links. NOTE: Symbolic-link support requires only readlink and symlink. FUSE itself will take care of tracking symbolic links in paths, so your path-evaluation code doesn't need to worry about it.
int grosfs_readlink( const char * path, char * buf, size_t size );

// Open a directory for reading.
int grosfs_opendir( const char * path, struct fuse_file_info * fi );

// Return one or more directory entries (struct dirent) to the caller. This is one of the most complex FUSE functions. It is related to, but not identical to, the readdir(2) and getdents(2) system calls, and the readdir(3) library function. Because of its complexity, it is described separately below. Required for essentially any filesystem, since it's what makes ls and a whole bunch of other things work.
int grosfs_readdir( const char * path, void * buf, fuse_fill_dir_t filler,
                    off_t offset, struct fuse_file_info * fi );

// Make a special (device) file, FIFO, or socket. See mknod(2) for details. This function is rarely needed, since it's uncommon to make these objects inside special-purpose filesystems.
int grosfs_mknod( const char * path, mode_t mode, dev_t rdev );

// Create a directory with the given name. The directory permissions are encoded in mode. See mkdir(2) for details. This function is needed for any reasonable read/write filesystem.
int grosfs_mkdir( const char * path, mode_t mode );

// Remove (delete) the given file, symbolic link, hard link, or special node. Note that if you support hard links, unlink only deletes the data when the last hard link is removed. See unlink(2) for details.
int grosfs_unlink( const char * path );

// Remove the given directory. This should succeed only if the directory is empty (except for "." and ".."). See rmdir(2) for details.
int grosfs_rmdir( const char * path );

// Create a symbolic link named "from" which, when evaluated, will lead to "to". Not required if you don't support symbolic links. NOTE: Symbolic-link support requires only readlink and symlink. FUSE itself will take care of tracking symbolic links in paths, so your path-evaluation code doesn't need to worry about it.
int grosfs_symlink( const char * to, const char * from );

// Rename the file, directory, or other object "from" to the target "to". Note that the source and target don't have to be in the same directory, so it may be necessary to move the source to an entirely new directory. See rename(2) for full details.
int grosfs_rename( const char * from, const char * to );

// Create a hard link between "from" and "to". Hard links aren't required for a working filesystem, and many successful filesystems don't support them. If you do implement hard links, be aware that they have an effect on how unlink works. See link(2) for details.
int grosfs_link( const char * from, const char * to );

// Change the mode (permissions) of the given object to the given new permissions. Only the permissions bits of mode should be examined. See chmod(2) for details.
int grosfs_chmod( const char * path, mode_t mode );

// Change the given object's owner and group to the provided values. See chown(2) for details. NOTE: FUSE doesn't deal particularly well with file ownership, since it usually runs as an unprivileged user and this call is restricted to the superuser. It's often easier to pretend that all files are owned by the user who mounted the filesystem, and to skip implementing this function.
int grosfs_chown( const char * path, uid_t uid, gid_t gid );

// Truncate or extend the given file so that it is precisely size bytes long. See truncate(2) for details. This call is required for read/write filesystems, because recreating a file will first truncate it.
int grosfs_truncate( const char * path, off_t size );

// As truncate, but called when ftruncate(2) is called by the user program.
int grosfs_ftruncate( const char * path, off_t size, struct fuse_file_info * fi );

// Update the last access time of the given object from ts[0] and the last modification time from ts[1]. Both time specifications are given to nanosecond resolution, but your filesystem doesn't have to be that precise; see utimensat(2) for full details. Note that the time specifications are allowed to have certain special values; however, I don't know if FUSE functions have to support them. This function isn't necessary but is nice to have in a fully functional filesystem.
int grosfs_utimens( const char * path, const struct timespec ts[2] );

/**
 * Create and open a file
 *
 * If the file does not exist, first create it with the specified
 * mode, and then open it.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the mknod() and open() methods
 * will be called instead.
 */
int grosfs_create( const char * path, mode_t mode, struct fuse_file_info * fi );

// Open a file. If you aren't using file handles, this function should just check for existence and permissions and return either success or an error code. If you use file handles, you should also allocate any necessary structures and set fi->fh. In addition, fi has some other fields that an advanced filesystem might find useful; see the structure definition in fuse_common.h for very brief commentary.
int grosfs_open( const char * path, struct fuse_file_info * fi );

// Read size bytes from the given file into the buffer buf, beginning offset bytes into the file. See read(2) for full details. Returns the number of bytes transferred, or 0 if offset was at or beyond the end of the file. Required for any sensible filesystem.
int grosfs_read( const char * path, char * buf, size_t size, off_t offset,
               struct fuse_file_info * fi );

// As for read above, except that it can't return 0.
int grosfs_write( const char * path, const char * buf, size_t size, off_t offset,
                struct fuse_file_info * fi );

// Return statistics about the filesystem. See statvfs(2) for a description of the structure contents. Usually, you can ignore the path. Not required, but handy for read/write filesystems since this is how programs like df determine the free space.
int grosfs_statfs( const char * path, struct statvfs * stbuf );

// This is the only FUSE function that doesn't have a directly corresponding system call, although close(2) is related. Release is called when FUSE is completely done with a file; at that point, you can free up any temporarily allocated data structures. The IBM document claims that there is exactly one release per open, but I don't know if that is true.
int grosfs_release( const char * path, struct fuse_file_info * fi );

// This is like release, except for directories.
int grosfs_releasedir( const char * path, struct fuse_file_info * fi );

// Flush any dirty information about the file to disk. If isdatasync is nonzero, only data, not metadata, needs to be flushed. When this call returns, all file data should be on stable storage. Many filesystems leave this call unimplemented, although technically that's a Bad Thing since it risks losing data. If you store your filesystem inside a plain file on another filesystem, you can implement this by calling fsync(2) on that file, which will flush too much data (slowing performance) but achieve the desired guarantee.
int grosfs_fsync( const char * path, int isdatasync, struct fuse_file_info * fi );

// Like fsync, but for directories.
int
grosfs_fsyncdir( const char * path, int isdatasync, struct fuse_file_info * fi );

// Called on each close so that the filesystem has a chance to report delayed errors. Important: there may be more than one flush call for each open. Note: There is no guarantee that flush will ever be called at all!
int grosfs_flush( const char * path, struct fuse_file_info * fi );

// Perform a POSIX file-locking operation. See details below.
int grosfs_lock( const char * path, struct fuse_file_info * fi, int cmd,
                 struct flock * locks );

// This function is similar to bmap(9). If the filesystem is backed by a block device, it converts blockno from a file-relative block number to a device-relative block. It isn't entirely clear how the blocksize parameter is intended to be used.
int grosfs_bmap( const char * path, size_t blocksize, uint64_t * blockno );

#ifdef HAVE_SETXATTR
// Set an extended attribute. See setxattr(2). This should be implemented only if HAVE_SETXATTR is true.
int grosfs_setxattr(const char* path, const char* name, const char* value, size_t size, int flags);

// Read an extended attribute. See getxattr(2). This should be implemented only if HAVE_SETXATTR is true.
int grosfs_getxattr(const char* path, const char* name, char* value, size_t size);

// List the names of all extended attributes. See listxattr(2). This should be implemented only if HAVE_SETXATTR is true.
int grosfs_listxattr(const char* path, const char* list, size_t size);
#endif

// Support the ioctl(2) system call. As such, almost everything is up to the filesystem. On a 64-bit machine, FUSE_IOCTL_COMPAT will be set for 32-bit ioctls. The size and direction of data is determined by _IOC_*() decoding of cmd. For _IOC_NONE, data will be NULL; for _IOC_WRITE data is being written by the user; for _IOC_READ it is being read, and if both are set the data is bidirectional. In all non-NULL cases, the area is _IOC_SIZE(cmd) bytes in size.
int
grosfs_ioctl( const char * path, int cmd, void * arg, struct fuse_file_info * fi,
            unsigned int flags, void * data );

// Poll for I/O readiness. If ph is non-NULL, when the filesystem is ready for I/O it should call fuse_notify_poll (possibly asynchronously) with the specified ph; this will clear all pending polls. The callee is responsible for destroying ph with fuse_pollhandle_destroy() when ph is no longer needed.
int grosfs_poll( const char * path, struct fuse_file_info * fi,
                 struct fuse_pollhandle * ph, unsigned * reventsp );

//static struct fuse_operations grosfs_oper;
struct fuse_operations initfuseops();

#endif
