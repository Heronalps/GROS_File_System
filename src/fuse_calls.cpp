/**
 * fuse_calls.cpp
 */

#include "fuse_calls.hpp"

// Initialize the filesystem. This function can often be left unimplemented,
// but it can be a handy way to perform one-time setup such as allocating
// variable-sized data structures or initializing a new filesystem.
// The fuse_conn_info structure gives information about what features are
// supported by FUSE, and can be used to request certain capabilities (see
// below for more information). The return value of this function is available
// to all file operations in the private_data field of fuse_context. It is also
// passed as a parameter to the destroy() method. (Note: see the warning under
// Other Options below, regarding relative pathnames.)
void * grosfs_init( struct fuse_conn_info * conn ) {
    pdebug << "in grosfs_init" << std::endl;
    struct fusedata * mydata = new struct fusedata();
    mydata->disk = gros_open_disk();
    if (mydata->disk->isnew) {
        gros_make_fs(mydata->disk);
    }
    return mydata;
}

// Called when the filesystem exits. The private_data comes from the return value of init.
void grosfs_destroy( void * private_data ) {
    pdebug << "in grosfs_destroy" << std::endl;
    struct fusedata * mydata = (struct fusedata *) private_data;
    gros_close_disk( mydata->disk );
    return;
}

// Return file attributes. The "stat" structure is described in detail in the
// stat(2) manual page. For the given pathname, this should fill in the elements
// of the "stat" structure. If a field is meaningless or semi-meaningless (e.g., st_ino)
// then it should be set to 0 or given a "reasonable" value. This call is pretty
// much required for a usable filesystem.
int grosfs_getattr( const char * path, struct stat * stbuf ) {
    pdebug << "in grosfs_getattr ( \"" << path << "\" )" << std::endl;
    struct fuse_context * ctxt;
    int                   inode_num;
    ctxt = fuse_get_context();
    struct fusedata * mydata = ( struct fusedata * ) ctxt->private_data;

    std::memset(stbuf, 0, sizeof(struct stat));

    inode_num = gros_namei( mydata->disk, path );
    if( inode_num < 0 ) return -ENOENT;

    return gros_i_stat( mydata->disk, inode_num, stbuf );
}

// As getattr, but called when fgetattr(2) is invoked by the user program.
int grosfs_fgetattr( const char * path, struct stat * stbuf, struct fuse_file_info *fi ) {
    pdebug << "in grosfs_fgetattr ( \"" << path << "\" )" << std::endl;
    return grosfs_getattr( path, stbuf );
}

// This is the same as the access(2) system call. It returns -ENOENT if the path
// doesn't exist, -EACCESS if the requested permission isn't available, or 0 for
// success. Note that it can be called on files, directories, or any other object
// that appears in the filesystem. This call is not required but is highly recommended.
int grosfs_access( const char * path, int mask ) {
    pdebug << "in grosfs_access ( \"" << path << "\", " << mask << " ) " << std::endl;
    return 0;

    struct fuse_context * ctxt;
    int inode_num;
    short r_ok, w_ok, x_ok;
    short usr, grp, uni;
    Inode * inode;
    ctxt = fuse_get_context();
    struct fusedata * mydata = ( struct fusedata * ) ctxt->private_data;

    inode_num = gros_namei( mydata->disk, path );
    if( inode_num < 0 ) return -ENOENT;

    inode = gros_get_inode( mydata->disk, inode_num );
    usr = ( short ) ( inode->f_acl & 0x7 );
    grp = ( short ) ( ( inode->f_acl >> 3 ) & 0x7 );
    uni = ( short ) ( ( inode->f_acl >> 6 ) & 0x7 );

    r_ok = ( uni & 0x4 ) ||
           ( ( grp & 0x4 ) && ctxt->gid == inode->f_gid ) ||
           ( ( usr & 0x4 ) && ctxt->uid == inode->f_uid );
    w_ok = ( uni & 0x2 ) ||
           ( ( grp & 0x2 ) && ctxt->gid == inode->f_gid ) ||
           ( ( usr & 0x2 ) && ctxt->uid == inode->f_uid );
    x_ok = ( uni & 0x1 ) ||
           ( ( grp & 0x1 ) && ctxt->gid == inode->f_gid ) ||
           ( ( usr & 0x1 ) && ctxt->uid == inode->f_uid );

    if( ( mask & R_OK && !r_ok ) ||
        ( mask & W_OK && !w_ok ) ||
        ( mask & X_OK && !x_ok ) ) {
        return -EACCES;
    }

    return 0;
}

// If path is a symbolic link, fill buf with its target, up to size. See
// readlink(2) for how to handle a too-small buffer and for error codes. Not
// required if you don't support symbolic links. NOTE: Symbolic-link support
// requires only readlink and symlink. FUSE itself will take care of tracking
// symbolic links in paths, so your path-evaluation code doesn't need to worry about it.
int grosfs_readlink( const char * path, char * buf, size_t size ) {
    pdebug << "in grosfs_readlink" << std::endl;
    struct fusedata * mydata = ( struct fusedata * ) fuse_get_context()->private_data;
    Inode    * inode = gros_get_inode( mydata->disk, gros_namei( mydata->disk, path ) );
    int first_bit  = inode->f_acl & 1;
    int second_bit = ( ( inode->f_acl & 2 ) >> 1 );
    if ( first_bit == 0 || second_bit == 0 || size < 1 )
    	return -EINVAL;
    if( ( inode == NULL || inode->f_links == 0 ) ) 
    	return -ENOENT;
    gros_i_read( mydata->disk, inode, buf, std::min( size, ( size_t )  inode->f_size ), 0 );
    buf[ inode->f_size ] = '\0';
//    return std::min( ( int ) size, inode->f_size );
    return 0;
}

// Open a directory for reading.
int grosfs_opendir( const char * path, struct fuse_file_info * fi ) {
    pdebug << "in grosfs_opendir" << std::endl;
    struct fusedata * mydata = ( struct fusedata * ) fuse_get_context()->private_data;
    Inode    * inode = gros_get_inode( mydata->disk, gros_namei( mydata->disk, path ) );
    if ( inode == NULL || inode->f_links == 0) 
    	return -ENOENT;
    if ( !gros_is_dir( inode->f_acl ) )
    	return -ENOTDIR;
    return 0;
}

// Return one or more directory entries (struct dirent) to the caller.
// This is one of the most complex FUSE functions. It is related to, but not
// identical to, the readdir(2) and getdents(2) system calls, and the readdir(3)
// library function. Because of its complexity, it is described separately below.
// Required for essentially any filesystem, since it's what makes ls and a whole
// bunch of other things work.
int grosfs_readdir( const char * path, void * buf, fuse_fill_dir_t filler,
                  off_t offset, struct fuse_file_info * fi ) {
    pdebug << "in grosfs_readdir ( \"" << path << "\", " << offset << " )" << std::endl;

    struct fuse_context * ctxt = fuse_get_context();
    struct fusedata *mydata = (struct fusedata *)ctxt->private_data;
    struct stat *stbuf;
    int full = 0;
    size_t off = 0;
    int inode_num = gros_namei(mydata->disk, path);
    // if we couldn't find the directory, error
    if (inode_num < 0) {
        return -ENOENT;
    }
    Inode *inode = gros_get_inode(mydata->disk, inode_num);
    DirEntry * ent = NULL; // the current direntry
    DirEntry * tmp = NULL; // the next direntry
    // get the first direntry
    gros_readdir_r(mydata->disk, inode, NULL, &ent);

    while (ent != NULL && full != 1) {
        // get the next direntry and put it into tmp
        gros_readdir_r(mydata->disk, inode, ent, &tmp);
        // if there are no more direntries, off = 0, else offset is to next
        off = tmp == NULL ? 0 : ((24+strlen(ent->filename)+7)&~7);
        if (off < offset) {
            ent = tmp;
            continue;
        }
        stbuf = new struct stat();
        gros_i_stat( mydata->disk, ent->inode_num, stbuf );
        // full will be 1 if the buffer is full
        full = filler( buf, ent->filename, stbuf, ( off_t ) off );
        // tmp is the new current entry
        ent = tmp;
    }

    return 0;
}

// Make a special (device) file, FIFO, or socket. See mknod(2) for details.
// This function is rarely needed, since it's uncommon to make these objects
// inside special-purpose filesystems.
int grosfs_mknod( const char * path, mode_t mode, dev_t rdev ) {
    pdebug << "in grosfs_mknod ( \"" << path << "\", " << mode << ", " << rdev << " ) " << std::endl;
    struct fusedata * mydata = ( struct fusedata * ) fuse_get_context()->private_data;
    if( grosfs_access( path, mode ) < 0 ) //TODO: do we need this?
    	return -EACCES;

    int inode_num = gros_mknod(mydata->disk, path);
    if (inode_num < 0)
        return inode_num;
    Inode * inode = gros_get_inode(mydata->disk, inode_num);
    inode->f_acl = 0; // regular file

    gros_i_chmod( mydata->disk, inode, mode );

    inode->f_atime = time(NULL);
    inode->f_ctime = time(NULL);
    inode->f_mtime = time(NULL);

    gros_save_inode(mydata->disk, inode);
    return 0;
}

// Create a directory with the given name. The directory permissions are encoded
// in mode. See mkdir(2) for details. This function is needed for any reasonable
// read/write filesystem.
int grosfs_mkdir( const char * path, mode_t mode ) {
    pdebug << "in grosfs_mkdir ( \"" << path << "\", " << mode << " ) " << std::endl;
    struct fusedata * mydata = ( struct fusedata * ) fuse_get_context()->private_data;
    if( grosfs_access( path, mode ) < 0 ) //TODO: do we need this?
    	return -EACCES;
    return gros_mkdir( mydata->disk, path ) > 0 ? 0 : -1;
}

// Remove (delete) the given file, symbolic link, hard link, or special node.
// Note that if you support hard links, unlink only deletes the data when the
// last hard link is removed. See unlink(2) for details.
int grosfs_unlink( const char * path ) {
    pdebug << "in grosfs_unlink ( \"" << path << "\" ) " << std::endl;
    struct fusedata * mydata = ( struct fusedata * ) fuse_get_context()->private_data;
    return gros_unlink( mydata->disk, path );
}

// Remove the given directory. This should succeed only if the directory is empty
// (except for "." and ".."). See rmdir(2) for details.
int grosfs_rmdir( const char * path ) {
    pdebug << "in grosfs_rmdir ( \"" << path << "\" ) " << std::endl;
    struct fusedata * mydata = ( struct fusedata * ) fuse_get_context()->private_data;
    return gros_rmdir( mydata->disk, path );
}

// Create a symbolic link named "from" which, when evaluated, will lead to "to".
// Not required if you don't support symbolic links. NOTE: Symbolic-link support
// requires only readlink and symlink. FUSE itself will take care of tracking
// symbolic links in paths, so your path-evaluation code doesn't need to worry about it.
int grosfs_symlink( const char * to, const char * from ) {
    pdebug << "in grosfs_symlink ( " << to << ", " << from << " ) " << std::endl;
    const char * filename = strrchr( from, '/' ) + 1;
    int          length   = ( int ) ( strlen( from ) - strlen( filename ) );
    char       * dirname  = new char[ length ];
    struct fusedata * mydata = ( struct fusedata * ) fuse_get_context()->private_data;

    strncpy( dirname, from, ( size_t ) length + 1 );
    dirname[length-1] = '\0';

    Inode    * from_dir = gros_get_inode( mydata->disk, gros_namei( mydata->disk, dirname ) );
    Inode    * inode    = gros_new_inode( mydata->disk );
    inode->f_acl        = 0x7ff; // 11 111 111 111
    inode->f_links      = 1;
    DirEntry * direntry = new DirEntry();

    direntry->inode_num = inode->f_inode_num;
    strcpy( direntry->filename, filename );

    gros_save_inode( mydata->disk, inode );
    gros_i_write( mydata->disk, from_dir, ( char * ) direntry, sizeof( DirEntry ),
                  from_dir->f_size );
    gros_i_write( mydata->disk, inode, ( char * ) to, ( int ) strlen( to ), 0 );

    delete    direntry;
    delete [] dirname;

    return 0;
}


// Rename the file, directory, or other object "from" to the target "to". Note
// that the source and target don't have to be in the same directory, so it
// may be necessary to move the source to an entirely new directory. See rename(2)
// for full details.
int grosfs_rename( const char * from, const char * to ) {
    pdebug << "in grosfs_rename ( " << from << ", " << to << " ) " << std::endl;
    struct fusedata * mydata = ( struct fusedata * ) fuse_get_context()->private_data;
    return gros_frename( mydata->disk, from, to );
}


// Create a hard link between "from" and "to". Hard links aren't required for a
// working filesystem, and many successful filesystems don't support them.
// If you do implement hard links, be aware that they have an effect on how
// unlink works. See link(2) for details.
int grosfs_link( const char * from, const char * to ) {
    pdebug << "in grosfs_link ( " << from << ", " << to << " ) " << std::endl;
    struct fusedata * mydata = ( struct fusedata * ) fuse_get_context()->private_data;
    return gros_copy( mydata->disk, from, to );
}


// Change the mode (permissions) of the given object to the given new permissions.
// Only the permissions bits of mode should be examined. See chmod(2) for details.
int grosfs_chmod( const char * path, mode_t mode ) {
    pdebug << "in grosfs_chmod ( \"" << path << "\", " << mode << " ) " << std::endl;
    struct fusedata * mydata = ( struct fusedata * ) fuse_get_context()->private_data;
    int inode_num = gros_namei( mydata->disk, path );
    if (inode_num < 0) {
        return -ENOENT;
    }
    Inode * inode = gros_get_inode( mydata->disk, inode_num );
    gros_i_chmod( mydata->disk, inode, mode );
    gros_save_inode( mydata->disk, inode );
    return 0; // leave unimplemented
}


// Change the given object's owner and group to the provided values.
// See chown(2) for details. NOTE: FUSE doesn't deal particularly well with file
// ownership, since it usually runs as an unprivileged user and this call is
// restricted to the superuser. It's often easier to pretend that all files are
// owned by the user who mounted the filesystem, and to skip implementing this function.
int grosfs_chown( const char * path, uid_t uid, gid_t gid ) {
    pdebug << "in grosfs_chown ( \"" << path << "\", " << uid << ", " << gid << " ) " << std::endl;
    struct fusedata * mydata = ( struct fusedata * ) fuse_get_context()->private_data;
    int inode_num = gros_namei( mydata->disk, path );
    if (inode_num < 0) {
        return -ENOENT;
    }
    Inode * inode = gros_get_inode( mydata->disk, inode_num );
    inode->f_uid = uid;
    inode->f_gid = gid;
    gros_save_inode( mydata->disk, inode );
    return 0; // leave unimplemented
}


// Truncate or extend the given file so that it is precisely size bytes long.
// See truncate(2) for details. This call is required for read/write filesystems,
// because recreating a file will first truncate it.
int grosfs_truncate( const char * path, off_t size ) {
    pdebug << "in grosfs_truncate ( \"" << path << "\", " << size << " ) " << std::endl;
    struct fusedata * mydata = ( struct fusedata * ) fuse_get_context()->private_data;
    return gros_truncate( mydata->disk, path, size );
}


// As truncate, but called when ftruncate(2) is called by the user program.
int grosfs_ftruncate( const char * path, off_t size, struct fuse_file_info *fi ) {
    pdebug << "in grosfs_ftruncate ( \"" << path << "\", " << size << " ) " << std::endl;
    return grosfs_truncate( path, size );
}


// Update the last access time of the given object from ts[0] and the last modification
// time from ts[1]. Both time specifications are given to nanosecond resolution,
// but your filesystem doesn't have to be that precise; see utimensat(2) for full details.
// Note that the time specifications are allowed to have certain special values;
// however, I don't know if FUSE functions have to support them. This function
// isn't necessary but is nice to have in a fully functional filesystem.
int grosfs_utimens( const char * path, const struct timespec ts[ 2 ] ) {
    pdebug << "in grosfs_utimens ( \"" << path << "\" ) " << std::endl;
    struct fusedata * mydata = ( struct fusedata * ) fuse_get_context()->private_data;
    int      inode_num = gros_namei( mydata->disk, path );
    Inode * inode      = gros_get_inode( mydata->disk, inode_num );
    inode->f_atime     = ts[ 0 ].tv_sec * 1000 + ts[ 0 ].tv_nsec * 1000000;
    inode->f_mtime     = ts[ 1 ].tv_sec * 1000 + ts[ 1 ].tv_nsec * 1000000;
    gros_save_inode( mydata->disk, inode );

    delete inode;
    return 0;
}


// Open a file. If you aren't using file handles, this function should just check
// for existence and permissions and return either success or an error code.
// If you use file handles, you should also allocate any necessary structures
// and set fi->fh. In addition, fi has some other fields that an advanced filesystem
// might find useful; see the structure definition in fuse_common.h for very brief commentary.
int grosfs_open( const char * path, struct fuse_file_info * fi ) {
    pdebug << "in grosfs_open ( \"" << path << "\" ) " << std::endl;
    struct fusedata * mydata = ( struct fusedata * ) fuse_get_context()->private_data;

    int     inode_num;
    int     mode  = 0;
    Inode * inode = NULL;

    inode_num = gros_namei( mydata->disk, path );
    if( inode_num > 0 )
        inode = gros_get_inode( mydata->disk, inode_num );

    if( ( inode != NULL && inode->f_links > 0 )
        && fi->flags & ( O_CREAT | O_EXCL ) )
        return -EEXIST;
    else if( ( inode == NULL || inode->f_links == 0 )
             && !( fi->flags & O_CREAT ) )
        return -ENOENT;
    else if( ( inode == NULL || inode->f_links == 0 )
               && fi->flags & O_CREAT )
        inode = gros_new_inode( mydata->disk );


    if( fi->flags & O_RDONLY || fi->flags & O_RDWR )
        mode |= R_OK;
    if( fi->flags & O_WRONLY || fi->flags & O_TRUNC || fi->flags & O_RDWR )
        mode |= W_OK;
    if( grosfs_access( path, mode ) < 0 )
        return -EACCES;
    if( fi->flags & O_TRUNC )
        gros_i_truncate( mydata->disk, inode, 0 );

    fi->fh = ( uint64_t ) inode_num;
    return 0;
}


// Read size bytes from the given file into the buffer buf, beginning offset
// bytes into the file. See read(2) for full details. Returns the number of
// bytes transferred, or 0 if offset was at or beyond the end of the file.
// Required for any sensible filesystem.
int grosfs_read( const char * path, char * buf, size_t size, off_t offset,
                 struct fuse_file_info * fi ) {

    pdebug << "in grosfs_read" << std::endl;
    pdebug << "reading " << size << " bytes from offset " << offset << " into file " << path << std::endl;
    struct fusedata * mydata = ( struct fusedata * ) fuse_get_context()->private_data;

    if( fi->fh == 0 )
        fi->fh = ( uint64_t ) gros_namei( mydata->disk, path );

    return gros_i_read( mydata->disk, gros_get_inode( mydata->disk, ( int ) fi->fh ), buf,
                        ( int ) size, ( int ) offset );
}


// As for read above, except that it can't return 0.
int grosfs_write( const char * path, const char * buf, size_t size, off_t offset,
                  struct fuse_file_info * fi ) {
    pdebug << "in grosfs_write" << std::endl;
    pdebug << "writing " << size << " bytes to offset " << offset << " into file " << path << std::endl;
    struct fusedata * mydata = ( struct fusedata * ) fuse_get_context()->private_data;
    if( fi->fh == 0 )
        fi->fh = ( uint64_t ) gros_namei( mydata->disk, path );

    return gros_i_write( mydata->disk, gros_get_inode( mydata->disk, ( int ) fi->fh ),
                         ( char * ) buf, ( int ) size, ( int ) offset );
}


// Return statistics about the filesystem. See statvfs(2) for a description of
// the structure contents. Usually, you can ignore the path. Not required, but handy
// for read/write filesystems since this is how programs like df determine the free space.
int grosfs_statfs( const char * path, struct statvfs * stbuf ) {
    pdebug << "in grosfs_statfs ( \"" << path << "\" ) " << std::endl;
    struct fusedata * mydata = ( struct fusedata * ) fuse_get_context()->private_data;
    Superblock  * sb  = new Superblock();
    gros_read_block( mydata->disk, 0, ( char * ) sb );

    stbuf->f_bsize   = ( unsigned long ) sb->fs_block_size;          /* file system block size */
    stbuf->f_frsize  = 0;                                            /* fragment size */
    stbuf->f_blocks  = ( fsblkcnt_t ) sb->fs_num_blocks;             /* size of fs in f_frsize units */
    stbuf->f_bfree   = ( fsblkcnt_t ) ( sb->fs_num_blocks - sb->fs_num_used_blocks );   /* # free blocks */
    stbuf->f_bavail  = ( fsblkcnt_t ) ( sb->fs_num_blocks - sb->fs_num_used_blocks );   /* # free blocks for unprivileged users */
    stbuf->f_files   = ( fsfilcnt_t ) sb->fs_num_inodes;             /* # inodes */
    stbuf->f_ffree   = ( fsfilcnt_t ) ( sb->fs_num_inodes - sb->fs_num_used_inodes );   /* # free inodes */
    stbuf->f_favail  = ( fsfilcnt_t ) ( sb->fs_num_inodes - sb->fs_num_used_inodes );   /* # free inodes for unprivileged users */
    stbuf->f_fsid    = 0;                                            /* file system ID */
    stbuf->f_flag    = 0;                                            /* mount flags */
    stbuf->f_namemax = FILENAME_MAX_LENGTH;                          /* maximum filename length */

    delete sb;
    return 0;
}


// This is the only FUSE function that doesn't have a directly corresponding system call,
// although close(2) is related. Release is called when FUSE is completely done with a file;
// at that point, you can free up any temporarily allocated data structures.
// The IBM document claims that there is exactly one release per open,
// but I don't know if that is true.
int grosfs_release( const char * path, struct fuse_file_info * fi ) {
    pdebug << "in grosfs_release ( \"" << path << "\" ) " << std::endl;
    return 0; // leave unimplemented
}


// This is like release, except for directories.
int grosfs_releasedir( const char * path, struct fuse_file_info * fi ) {
    pdebug << "in grosfs_releasedir ( \"" << path << "\" ) " << std::endl;
    return 0; // leave unimplemented
}


// Flush any dirty information about the file to disk. If isdatasync is nonzero,
// only data, not metadata, needs to be flushed. When this call returns,
// all file data should be on stable storage. Many filesystems leave this call
// unimplemented, although technically that's a Bad Thing since it risks losing data.
// If you store your filesystem inside a plain file on another filesystem, you can
// implement this by calling fsync(2) on that file, which will flush too much data
// (slowing performance) but achieve the desired guarantee.
int grosfs_fsync( const char * path, int isdatasync, struct fuse_file_info * fi ) {
    pdebug << "in grosfs_fsync ( \"" << path << "\", " << isdatasync << " ) " << std::endl;
    fsync(( ( struct fusedata * ) fuse_get_context()->private_data )->disk->fd);
    return 0; // leave unimplemented
}

int grosfs_setxattr(const char* path, const char* name, const char* value, size_t size, int flags) {
    pdebug << "in grosfs_setxattr" << std::endl;
    return -ENOSYS; // leave unimplemented
}
int grosfs_getxattr(const char* path, const char* name, char* value, size_t size) {
    pdebug << "in grosfs_getxattr" << std::endl;
    return -ENOSYS; // leave unimplemented
}
int grosfs_listxattr(const char* path, char* list, size_t size) {
    pdebug << "in grosfs_listxattr" << std::endl;
    return -ENOSYS; // leave unimplemented
}
int grosfs_removexattr(const char* path, const char* name) {
    pdebug << "in grosfs_removexattr " << std::endl;
    return -ENOSYS; // leave unimplemented
}


// Like fsync, but for directories.
int grosfs_fsyncdir( const char * path, int isdatasync, struct fuse_file_info * fi ) {
    pdebug << "in grosfs_fsyncdir ( \"" << path << "\", " << isdatasync << " ) " << std::endl;
    return 0; // leave unimplemented
}


// Called on each close so that the filesystem has a chance to report delayed errors.
// Important: there may be more than one flush call for each open.
// Note: There is no guarantee that flush will ever be called at all!
int grosfs_flush( const char * path, struct fuse_file_info * fi ) {
    pdebug << "in grosfs_flush ( \"" << path << "\" ) " << std::endl;
    return 0; // leave unimplemented
}


// Perform a POSIX file-locking operation. See details below.
int grosfs_lock( const char * path, struct fuse_file_info * fi, int cmd,
                 struct flock * locks ) {
    pdebug << "in grosfs_lock ( \"" << path << "\" ) " << std::endl;
    return 0; // leave unimplemented
}


// This function is similar to bmap(9). If the filesystem is backed by a block device,
// it converts blockno from a file-relative block number to a device-relative block.
// It isn't entirely clear how the blocksize parameter is intended to be used.
int grosfs_bmap( const char * path, size_t blocksize, uint64_t * blockno ) {
    pdebug << "in grosfs_bmap ( \"" << path << "\", " << blocksize << " ) " << std::endl;
    struct fusedata * mydata = ( struct fusedata * ) fuse_get_context()->private_data;
    int          block_size;         /* fs block size */
    int          n_indirects;        /* how many ints can fit in a block */
    int          n_indirects_sq;     /* n_indirects * n_indirects */
    int          block_to_read;      /* current block (relative to fs) to gros_read */
    int          cur_si     = -1;    /* id of blocks last gros_read into siblock */
    int          cur_di     = -1;    /* id of blocks last gros_read into diblock */
    int          si         = -1;    /* id of siblock that we need */
    int          di         = -1;    /* id of diblock that we need */
    int        * siblock    = NULL;  /* buffer to store indirects */
    int        * diblock    = NULL;  /* buffer to store indirects */
    int        * tiblock    = NULL;  /* buffer to store indirects */
    int          inode_num  = gros_namei( mydata->disk, path );
    Superblock * sb         = new Superblock();
    Inode      * inode      = gros_get_inode( mydata->disk, inode_num );
    gros_read_block( mydata->disk, 0, ( char * ) sb );

    block_size     = sb->fs_block_size;
    n_indirects    = block_size / sizeof( int );
    n_indirects_sq = n_indirects * n_indirects;
    block_to_read  = ( int ) * blockno;

    // by default, the double indirect block we gros_read from is the one given in
    // the inode. this will change if we are in the triple indirect block
    di = inode->f_block[ DOUBLE_INDRCT ];
    // by default, the single indirect block we gros_read from is the one given in
    // the inode. this will change if we are in the double indirect block
    si = inode->f_block[ SINGLE_INDRCT ];

    // in a triple indirect block
    if( block_to_read >= ( n_indirects_sq + SINGLE_INDRCT ) ) {
        // if we haven't fetched the triple indirect block yet, do so now
        if( tiblock == NULL ) {
            tiblock = new int[ n_indirects ];
            gros_read_block( mydata->disk,
                             inode->f_block[ TRIPLE_INDRCT ],
                             ( char * ) tiblock );
        }

        // subtracting n^2+n+12 to obviate lower layers of indirection
        int pos =
                ( block_to_read - ( n_indirects_sq
                                    + n_indirects
                                    + SINGLE_INDRCT ) )
                / n_indirects_sq;
        // get the double indirect block that contains the block we need to gros_read
        di = tiblock[ pos ];
        // since we're moving onto double indirect addressing, subtract all
        // triple indirect related index information
        block_to_read -= pos * n_indirects_sq; /* still >= n+12 */
    }

    // in a double indirect block
    if( block_to_read >= ( n_indirects + SINGLE_INDRCT ) ) {
        diblock = diblock == NULL ? ( new int[ n_indirects ] ) : diblock;
        if( cur_di != di ) {
            cur_di = di;
            gros_read_block( mydata->disk, di, ( char * ) diblock );
        }
        // subtracting n+12 to obviate lower layers of indirection
        int pos = ( block_to_read - ( n_indirects + SINGLE_INDRCT ) ) /
                  n_indirects;
        // get the single indirect block that contains the block we need to gros_read
        si = diblock[ pos ];
        // since we're moving onto single indirect addressing, subtract all
        // double indirect related index information
        block_to_read -= pos * n_indirects; /* still >= 12 */
    }

    // in a single indirect block
    if( block_to_read >= SINGLE_INDRCT ) {
        siblock = siblock == NULL ? ( new int[ n_indirects ] ) : siblock;
        // if we dont' already have the single indirects loaded into memory, load it
        if( cur_si != si ) {
            cur_si = si;
            gros_read_block( mydata->disk, si, ( char * ) siblock );
        }
        // relative index into single indirects
        block_to_read = siblock[ block_to_read - SINGLE_INDRCT ];
    }

    // in a direct block
    if( * blockno < SINGLE_INDRCT )
        block_to_read = inode->f_block[ block_to_read ];

    // free up the resources we allocated
    if( siblock != NULL ) delete [] siblock;
    if( diblock != NULL ) delete [] diblock;
    if( tiblock != NULL ) delete [] tiblock;

    return block_to_read;
}


// Support the ioctl(2) system call. As such, almost everything is up to the filesystem.
// On a 64-bit machine, FUSE_IOCTL_COMPAT will be set for 32-bit ioctls.
// The size and direction of data is determined by _IOC_*() decoding of cmd.
// For _IOC_NONE, data will be NULL; for _IOC_WRITE data is being written by the user;
// for _IOC_READ it is being read, and if both are set the data is bidirectional.
// In all non-NULL cases, the area is _IOC_SIZE(cmd) bytes in size.
int grosfs_ioctl( const char * path, int cmd, void * arg,
                  struct fuse_file_info * fi, unsigned int flags, void * data ) {
    pdebug << "in grosfs_ioctl ( \"" << path << "\", " << cmd << ", " << flags << " ) " << std::endl;
//    int size = IOCPARM_LEN( cmd );
//    long dir  = IOCBASECMD( cmd );


    if (flags & FUSE_IOCTL_COMPAT)
        return -ENOSYS;

/*
    switch( dir ) {
        case _IOR('E', 0, size_t):
            grosfs_read( path, ( char * ) data, ( size_t ) size, 0, fi );
        case _IOW('E', 1, size_t):
            grosfs_write( path, ( char * ) data, ( size_t ) size, 0, fi );
//        case _IOWR:
//            // TODO what to do here?
        default:
            return -EINVAL;
    }
*/

    return 0;
}


// Poll for I/O readiness. If ph is non-NULL, when the filesystem is ready for I/O
// it should call fuse_notify_poll (possibly asynchronously) with the specified ph;
// this will clear all pending polls. The callee is responsible for destroying ph
// with fuse_pollhandle_destroy() when ph is no longer needed.
int grosfs_poll( const char * path, struct fuse_file_info * fi,
                 struct fuse_pollhandle * ph, unsigned * reventsp ) {
    pdebug << "in grosfs_poll ( \"" << path << "\" ) " << std::endl;
    return 0; // leave unimplemented
}


int grosfs_create( char const * path, mode_t mode, struct fuse_file_info * fi ) {
    pdebug << "in grosfs_create ( \"" << path << "\", " << mode << " ) " << std::endl;
    return grosfs_mknod(path, mode, 0);
}


struct fuse_operations initfuseops() {
    struct fuse_operations grosfs_oper;
	grosfs_oper.getattr     = grosfs_getattr;
	grosfs_oper.readlink    = grosfs_readlink;
	grosfs_oper.mknod       = grosfs_mknod;
	grosfs_oper.mkdir       = grosfs_mkdir;
	grosfs_oper.unlink      = grosfs_unlink;
	grosfs_oper.rmdir       = grosfs_rmdir;
	grosfs_oper.symlink     = grosfs_symlink;
	grosfs_oper.rename      = grosfs_rename;
	grosfs_oper.link        = grosfs_link;
	grosfs_oper.chmod       = grosfs_chmod;
	grosfs_oper.chown       = grosfs_chown;
	grosfs_oper.truncate    = grosfs_truncate;
	grosfs_oper.open        = grosfs_open;
	grosfs_oper.read        = grosfs_read;
	grosfs_oper.write       = grosfs_write;
	grosfs_oper.statfs      = grosfs_statfs;
	grosfs_oper.flush       = grosfs_flush;
	grosfs_oper.release     = grosfs_release;
	grosfs_oper.fsync       = grosfs_fsync;
	grosfs_oper.setxattr    = grosfs_setxattr;
	grosfs_oper.getxattr    = grosfs_getxattr;
	grosfs_oper.listxattr   = grosfs_listxattr;
	grosfs_oper.removexattr = grosfs_removexattr;
	grosfs_oper.opendir     = grosfs_opendir;
	grosfs_oper.readdir     = grosfs_readdir;
	grosfs_oper.releasedir  = grosfs_releasedir;
	grosfs_oper.fsyncdir    = grosfs_fsyncdir;
	grosfs_oper.init        = grosfs_init;
	grosfs_oper.destroy     = grosfs_destroy;
	grosfs_oper.access      = grosfs_access;
	grosfs_oper.create      = grosfs_create;
	grosfs_oper.lock        = grosfs_lock;
	grosfs_oper.utimens     = grosfs_utimens;
	grosfs_oper.bmap        = grosfs_bmap;
//	grosfs_oper.ioctl       = grosfs_ioctl;
	grosfs_oper.poll        = grosfs_poll;

	grosfs_oper.fgetattr    = grosfs_fgetattr;
	grosfs_oper.ftruncate   = grosfs_ftruncate;
	grosfs_oper.flag_nullpath_ok = 0;                /* See below */
    return grosfs_oper;
}
