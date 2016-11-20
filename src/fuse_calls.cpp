/**
 * fuse_calls.cpp
 */

#include "fuse_calls.hpp"

// Initialize the filesystem. This function can often be left unimplemented, but it can be a handy way to perform one-time setup such as allocating variable-sized data structures or initializing a new filesystem. The fuse_conn_info structure gives information about what features are supported by FUSE, and can be used to request certain capabilities (see below for more information). The return value of this function is available to all file operations in the private_data field of fuse_context. It is also passed as a parameter to the destroy() method. (Note: see the warning under Other Options below, regarding relative pathnames.)
void * gros_init( struct fuse_conn_info * conn ) {
}

// Called when the filesystem exits. The private_data comes from the return value of init.
void gros_destroy( void * private_data ) {
}

// Return file attributes. The "stat" structure is described in detail in the stat(2) manual page. For the given pathname, this should fill in the elements of the "stat" structure. If a field is meaningless or semi-meaningless (e.g., st_ino) then it should be set to 0 or given a "reasonable" value. This call is pretty much required for a usable filesystem.
int gros_getattr( const char * path, struct stat * stbuf ) {
    struct fuse_context * ctxt;
    int inode_num;
    short usr, grp, uni;
    Inode * inode;
    ctxt = fuse_get_context();

    inode_num = gros_namei( disk, path );
    if( inode_num < 0 ) return -ENOENT;

    inode = gros_get_inode( disk, inode_num );

    usr = inode->acl & 0x7;
    grp = ( inode->acl >> 3 ) & 0x7;
    uni = ( inode->acl >> 6 ) & 0x7;

    stbuf->st_mode = 0;
    // user
    stbuf->st_mode = ( uid & 0x4 ) ? stbuf->st_mode | S_IRUSR : stbuf->st_mode;
    stbuf->st_mode = ( uid & 0x2 ) ? stbuf->st_mode | S_IWUSR : stbuf->st_mode;
    stbuf->st_mode = ( uid & 0x1 ) ? stbuf->st_mode | S_IXUSR : stbuf->st_mode;
    // group
    stbuf->st_mode = ( grp & 0x4 ) ? stbuf->st_mode | S_IRGRP : stbuf->st_mode;
    stbuf->st_mode = ( grp & 0x2 ) ? stbuf->st_mode | S_IWGRP : stbuf->st_mode;
    stbuf->st_mode = ( grp & 0x1 ) ? stbuf->st_mode | S_IXGRP : stbuf->st_mode;
    // universe
    stbuf->st_mode = ( uni & 0x4 ) ? stbuf->st_mode | S_IROTH : stbuf->st_mode;
    stbuf->st_mode = ( uni & 0x2 ) ? stbuf->st_mode | S_IWOTH : stbuf->st_mode;
    stbuf->st_mode = ( uni & 0x1 ) ? stbuf->st_mode | S_IXOTH : stbuf->st_mode;

    stbuf->st_dev = 0; // not used
    stbuf->st_rdev = 0; // not used;
    stbuf->st_ino = inode_num;
    stbuf->st_uid = inode->f_uid;
    stbuf->st_gid = inode->f_gid;
    stbuf->st_atimespec = inode->f_atime;
    stbuf->st_mtimespec = inode->f_mtime;
    stbuf->st_ctimespec = inode->f_ctime;
    stbuf->st_nlink = inode->f_links;
    stbuf->st_size = inode->f_size;
    stbuf->st_blocks = ( inode->f_size / BLOCK_SIZE ) + 1;
    stbuf->st_blksize = BLOCK_SIZE;

    return 0;
}

// As getattr, but called when fgetattr(2) is invoked by the user program.
int gros_fgetattr( const char * path, struct stat * stbuf ) {
    return gros_getattr( path, stbuf );
}

// This is the same as the access(2) system call. It returns -ENOENT if the path doesn't exist, -EACCESS if the requested permission isn't available, or 0 for success. Note that it can be called on files, directories, or any other object that appears in the filesystem. This call is not required but is highly recommended.
int gros_access( const char * path, int mask ) {
    struct fuse_context * ctxt;
    int inode_num;
    short r_ok, w_ok, x_ok;
    short usr, grp, uni;
    Inode * inode;
    ctxt = fuse_get_context();

    inode_num = gros_namei( disk, path );
    if( inode_num < 0 ) return -ENOENT;

    inode = gros_get_inode( disk, inode_num );
    usr = inode->acl & 0x7;
    grp = ( inode->acl >> 3 ) & 0x7;
    uni = ( inode->acl >> 6 ) & 0x7;

    r_ok = ( uni & 0x4 ) ||
           ( ( grp & 0x4 ) && ctxt->grp == inode->f_gid ) ||
           ( ( uid & 0x4 ) && ctxt->uid == inode->f_uid );
    w_ok = ( uni & 0x2 ) ||
           ( ( grp & 0x2 ) && ctxt->grp == inode->f_gid ) ||
           ( ( uid & 0x2 ) && ctxt->uid == inode->f_uid );
    x_ok = ( uni & 0x1 ) ||
           ( ( grp & 0x1 ) && ctxt->grp == inode->f_gid ) ||
           ( ( uid & 0x1 ) && ctxt->uid == inode->f_uid );

    if( ( mode & R_OK && !r_ok ) ||
        ( mode & W_OK && !w_ok ) ||
        ( mode & X_OK && !x_ok ) ) {
        return -EACCESS;
    }

    return 0;
}

// If path is a symbolic link, fill buf with its target, up to size. See readlink(2) for how to handle a too-small buffer and for error codes. Not required if you don't support symbolic links. NOTE: Symbolic-link support requires only readlink and symlink. FUSE itself will take care of tracking symbolic links in paths, so your path-evaluation code doesn't need to worry about it.
int gros_readlink( const char * path, char * buf, size_t size ) {
}

// Open a directory for reading.
int gros_opendir( const char * path, struct fuse_file_info * fi ) {
}

// Return one or more directory entries (struct dirent) to the caller. This is one of the most complex FUSE functions. It is related to, but not identical to, the readdir(2) and getdents(2) system calls, and the readdir(3) library function. Because of its complexity, it is described separately below. Required for essentially any filesystem, since it's what makes ls and a whole bunch of other things work.
int gros_readdir( const char * path, void * buf, fuse_fill_dir_t filler,
                  off_t offset, struct fuse_file_info * fi ) {
}

// Make a special (device) file, FIFO, or socket. See mknod(2) for details. This function is rarely needed, since it's uncommon to make these objects inside special-purpose filesystems.
int gros_mknod( const char * path, mode_t mode, dev_t rdev ) {
}

// Create a directory with the given name. The directory permissions are encoded in mode. See mkdir(2) for details. This function is needed for any reasonable read/write filesystem.
int gros_mkdir( const char * path, mode_t mode ) {
}

// Remove (delete) the given file, symbolic link, hard link, or special node. Note that if you support hard links, unlink only deletes the data when the last hard link is removed. See unlink(2) for details.
int gros_unlink( const char * path ) {
}

// Remove the given directory. This should succeed only if the directory is empty (except for "." and ".."). See rmdir(2) for details.
int gros_rmdir( const char * path ) {
}

// Create a symbolic link named "from" which, when evaluated, will lead to "to". Not required if you don't support symbolic links. NOTE: Symbolic-link support requires only readlink and symlink. FUSE itself will take care of tracking symbolic links in paths, so your path-evaluation code doesn't need to worry about it.
int gros_symlink( const char * to, const char * from ) {
}

// Rename the file, directory, or other object "from" to the target "to". Note that the source and target don't have to be in the same directory, so it may be necessary to move the source to an entirely new directory. See rename(2) for full details.
int gros_rename( const char * from, const char * to ) {
}

// Create a hard link between "from" and "to". Hard links aren't required for a working filesystem, and many successful filesystems don't support them. If you do implement hard links, be aware that they have an effect on how unlink works. See link(2) for details.
int gros_link( const char * from, const char * to ) {
}

// Change the mode (permissions) of the given object to the given new permissions. Only the permissions bits of mode should be examined. See chmod(2) for details.
int gros_chmod( const char * path, mode_t mode ) {
}

// Change the given object's owner and group to the provided values. See chown(2) for details. NOTE: FUSE doesn't deal particularly well with file ownership, since it usually runs as an unprivileged user and this call is restricted to the superuser. It's often easier to pretend that all files are owned by the user who mounted the filesystem, and to skip implementing this function.
int gros_chown( const char * path, uid_t uid, gid_t gid ) {
}

// Truncate or extend the given file so that it is precisely size bytes long. See truncate(2) for details. This call is required for read/write filesystems, because recreating a file will first truncate it.
int gros_truncate( const char * path, off_t size ) {
}

// As truncate, but called when ftruncate(2) is called by the user program.
int gros_ftruncate( const char * path, off_t size ) {
}

// Update the last access time of the given object from ts[0] and the last modification time from ts[1]. Both time specifications are given to nanosecond resolution, but your filesystem doesn't have to be that precise; see utimensat(2) for full details. Note that the time specifications are allowed to have certain special values; however, I don't know if FUSE functions have to support them. This function isn't necessary but is nice to have in a fully functional filesystem.
int gros_utimens( const char * path, const struct timespec ts[2] ) {
}

// Open a file. If you aren't using file handles, this function should just check for existence and permissions and return either success or an error code. If you use file handles, you should also allocate any necessary structures and set fi->fh. In addition, fi has some other fields that an advanced filesystem might find useful; see the structure definition in fuse_common.h for very brief commentary.
int gros_open( const char * path, struct fuse_file_info * fi ) {
    struct fuse_context * ctxt;
    int                   inode_num;
    int                   mode = 0;
    Inode               * inode = NULL;

    ctxt      = fuse_get_context();
    inode_num = gros_namei( disk, path );
    if( inode_num > 0 ) {
        inode = gros_get_inode( disk, inode_num );
    }

    if( ( inode != NULL && inode->f_links > 0 ) &&
        fi->flags & ( O_CREATE | O_EXCL ) ) {
        return -EEXIST;
    } else if( ( inode == NULL || inode->f_links == 0 ) &&
               !( fi->flags & O_CREAT ) ) {
        return -ENOENT;
    } else if( ( inode == NULL || inode->f_links == 0 ) &&
               fi->flags & O_CREAT ) {
        inode = gros_new_inode( disk );
    }

    if( fi->flags & O_RDONLY || fi->flags & O_RDWR ) {
        mode |= R_OK;
    }
    if( fi->flags & O_WRONLY || fi->flags & O_TRUNC || fi->flags & O_RDWR ) {
        mode |= W_OK;
    }
    if( gros_access( path, mode ) < 0 ) {
        return -EACCES;
    }

    if( fi->flags & O_TRUNC ) {
        gros_i_truncate( disk, inode, 0 );
    }

    fi->fh = inode_num;
    return 0;
}

// Read size bytes from the given file into the buffer buf, beginning offset bytes into the file. See read(2) for full details. Returns the number of bytes transferred, or 0 if offset was at or beyond the end of the file. Required for any sensible filesystem.
int gros_read( const char * path, char * buf, size_t size, off_t offset,
               struct fuse_file_info * fi ) {
}

// As for read above, except that it can't return 0.
int gros_write( const char * path, char * buf, size_t size, off_t offset,
                struct fuse_file_info * fi ) {
}

// Return statistics about the filesystem. See statvfs(2) for a description of the structure contents. Usually, you can ignore the path. Not required, but handy for read/write filesystems since this is how programs like df determine the free space.
int gros_statfs( const char * path, struct statvfs * stbuf ) {
}

// This is the only FUSE function that doesn't have a directly corresponding system call, although close(2) is related. Release is called when FUSE is completely done with a file; at that point, you can free up any temporarily allocated data structures. The IBM document claims that there is exactly one release per open, but I don't know if that is true.
int gros_release( const char * path, struct fuse_file_info * fi ) {
}

// This is like release, except for directories.
int gros_releasedir( const char * path, struct fuse_file_info * fi ) {
}

// Flush any dirty information about the file to disk. If isdatasync is nonzero, only data, not metadata, needs to be flushed. When this call returns, all file data should be on stable storage. Many filesystems leave this call unimplemented, although technically that's a Bad Thing since it risks losing data. If you store your filesystem inside a plain file on another filesystem, you can implement this by calling fsync(2) on that file, which will flush too much data (slowing performance) but achieve the desired guarantee.
int
gros_fsync( const char * path, int isdatasync, struct fuse_file_info * fi ) {
}

// Like fsync, but for directories.
int
gros_fsyncdir( const char * path, int isdatasync, struct fuse_file_info * fi ) {
}

// Called on each close so that the filesystem has a chance to report delayed errors. Important: there may be more than one flush call for each open. Note: There is no guarantee that flush will ever be called at all!
int gros_flush( const char * path, struct fuse_file_info * fi ) {
}

// Perform a POSIX file-locking operation. See details below.
int gros_lock( const char * path, struct fuse_file_info * fi, int cmd,
               struct flock * locks ) {
}

// This function is similar to bmap(9). If the filesystem is backed by a block device, it converts blockno from a file-relative block number to a device-relative block. It isn't entirely clear how the blocksize parameter is intended to be used.
int gros_bmap( const char * path, size_t blocksize, uint64_t * blockno ) {
}

#ifdef HAVE_SETXATTR
// Set an extended attribute. See setxattr(2). This should be implemented only if HAVE_SETXATTR is true.
int gros_setxattr(const char* path, const char* name, const char* value, size_t size, int flags) {
}

// Read an extended attribute. See getxattr(2). This should be implemented only if HAVE_SETXATTR is true.
int gros_getxattr(const char* path, const char* name, char* value, size_t size) {
}

// List the names of all extended attributes. See listxattr(2). This should be implemented only if HAVE_SETXATTR is true.
int gros_listxattr(const char* path, const char* list, size_t size) {
}
#endif

// Support the ioctl(2) system call. As such, almost everything is up to the filesystem. On a 64-bit machine, FUSE_IOCTL_COMPAT will be set for 32-bit ioctls. The size and direction of data is determined by _IOC_*() decoding of cmd. For _IOC_NONE, data will be NULL; for _IOC_WRITE data is being written by the user; for _IOC_READ it is being read, and if both are set the data is bidirectional. In all non-NULL cases, the area is _IOC_SIZE(cmd) bytes in size.
int
gros_ioctl( const char * path, int cmd, void * arg, struct fuse_file_info * fi,
            unsigned int flags, void * data ) {
}

// Poll for I/O readiness. If ph is non-NULL, when the filesystem is ready for I/O it should call fuse_notify_poll (possibly asynchronously) with the specified ph; this will clear all pending polls. The callee is responsible for destroying ph with fuse_pollhandle_destroy() when ph is no longer needed.
int gros_poll( const char * path, struct fuse_file_info * fi,
               struct fuse_pollhandle * ph, unsigned * reventsp ) {
}
