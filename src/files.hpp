/**
 * files.hpp
 */

#ifndef __FILES_HPP_INCLUDED__   // if files.hpp hasn't been included yet...
#define __FILES_HPP_INCLUDED__   //   #define this so the compiler knows it has been included

#include "../include/catch.hpp"
#include "grosfs.hpp"
#include "disk.hpp"
#include <cstring>
#include <cstdio>
#include <sys/stat.h>
#include <errno.h>

#define FILENAME_MAX_LENGTH 255

typedef struct _direntry {
    int  inode_num;                       /* inode of file */
    char filename[ FILENAME_MAX_LENGTH ]; /* the filename */
} DirEntry;


/**
 * Creates the primordial directory for the file system (i.e. root "/")
 *  The root of the file system will always be inode #0.
 *
 *  @param Disk * disk   Disk containing the file system
 */
void gros_mkroot( Disk * disk );


/**
* Returns the inode number of the file corresponding to the given path
*
* @param Disk * disk  Disk containing the file system
* @param char * path  Path to the file, starting from root "/"
*/
int gros_namei( Disk * disk, const char * path );


/**
 * Reads `size` bytes at `offset` offset bytes from file corresponding
 *  to given Inode on the given disk into given buffer
 *
* @param Disk  *  disk     Disk containing the file system
* @param Inode *  inode    Inode corresponding to the file to read
* @param char  *  buf      Buffer to read into (must be allocated to at
*                          least `size` bytes)
* @param int      size     Number of bytes to read
* @param int      offset   Offset into the file to start reading from
*/
int gros_i_read( Disk * disk, Inode * inode, char * buf, int size, int offset );


/* @param char*  path     FULL path (from root "/") to the file */
int gros_read( Disk * disk, const char * path, char * buf, int size,
               int offset );


/**
 * Writes `size` bytes (at `offset` bytes from 0) into file
 *  corresponding to given Inode on the given disk from given buffer
 *
* @param Disk  *  disk     Disk containing the file system
* @param Inode *  inode    Inode corresponding to the file to write to
* @param char  *  buf      Buffer to write to file (must be at least
*                           `size` bytes)
* @param int      size     Number of bytes to write
* @param int      offset   Offset into the file to start writing to
*/
int gros_i_write( Disk * disk, Inode * inode, char * buf, int size, int offset );


/* @param char*  path     FULL path (from root "/") to the file */
int gros_write( Disk * disk, const char * path, char * buf, int size,
                int offset );


/**
* Creates a file
*
* @param Disk  *  disk     Disk containing the file system
* @param Inode *  inode    Inode of directory in which to place new file
* @param char  *  filename  Name of new file
*/
int gros_i_mknod( Disk * disk, Inode * inode, const char * filename );


/* @param char*  path     FULL path (from root "/") to place the new file */
int gros_mknod( Disk * disk, const char * path );


/**
* Creates a directory with two entries (. and ..)
*
* @param Disk  *  disk     Disk containing the file system
* @param Inode *  inode    Inode of directory in which to place new directory
* @param char  *  dirname  Name of new directory
*/
int gros_i_mkdir( Disk * disk, Inode * inode, const char * dirname );


/* @param char*  path     FULL path (from root "/") to place the new directory */
int gros_mkdir( Disk * disk, const char * path );


/**
* Removes a directory and decrements all link counts for all files in directory
*  If files then have 0 links, those files will be deleted/freed.
*  TODO: Consider forcing vs not forcing recursive delete (error under latter)
*
* @param Disk  *  disk     Disk containing the file system
* @param Inode *  inode    Inode of directory containing directory to delete
* @param char*  path       FULL path (from root "/") to directory the remove
*/
int gros_i_rmdir( Disk * disk, Inode * inode, Inode * dir_inode );



/* @param char*  path     FULL path (from root "/") to directory the remove */
int gros_rmdir( Disk * disk, const char * path );


/**
* Removes a file or directory. If the file is a directory, calls rmdir.
*
* @param Disk  *  disk       Disk containing the file system
* @param Inode *  inode      Inode of directory containing file to delete
* @param char  *  filename   Name of file to delete
*/
int gros_i_unlink( Disk * disk, Inode * inode, const char * filename );


/* @param char*  path       FULL path (from root "/") to the file */
int gros_unlink( Disk * disk, const char * path );


/**
* Renames a file or directory
*
* @param Disk  *  disk       Disk containing the file system
* @param Inode *  inode      Inode of directory containing file to rename
* @param char  *  from       Name of file to rename
* @param char  *  to         New name for file
*/
int gros_frename( Disk * disk, const char * from, const char * to );

/**
* Truncates or extends the file to a specified length
*
* @param Disk  *  disk     Disk containing the file system
* @param Inode *  inode    Inode corresponding to the file to resize
* @param int      size     Desired file size
*/
int gros_i_truncate( Disk * disk, Inode * inode, int size );


/* @param char*  path     FULL path (from root "/") to the file */
int gros_truncate( Disk * disk, const char * path, int size );


/**
 * Readdir_r takes an inode corresponding to a directory file, a pointer to the
 *  caller's `current` direntry, and returns the next direntry in the out parameter
 *  `result`. If `current` is NULL, then this returns the first direntry into
 *  `result`. If there are no more direntries, then `result` will be NULL.
 *
 * @param Disk      * disk     The disk containing the file system
 * @param Inode     * dir      Directory instance
 * @param DirEntry  * current  Where to start traversing the directory from
 * @param DirEntry ** result   Out parameter for resulting direntry
 *
 * @returns int status
 */
int gros_readdir_r( Disk * disk, Inode * dir, DirEntry * current,
                    DirEntry ** result );


/* @param Inode * dir       directory instance */
DirEntry * gros_readdir( Disk * disk, Inode * dir );


/**
* Ensures that a file is at least `size` bytes long. If it is already
*  `size` bytes, nothing happens and this returns 0. Otherwise, the
*  file is allocated data blocks as necessary and zero-filled to be
*  `size` bytes long, returning the number of bytes extended.
*
* @param Disk  *  disk     Disk containing the file system
* @param Inode *  inode    Inode corresponding to the file to resize
* @param int      size     Desired file size
*/
int gros_i_ensure_size( Disk * disk, Inode * inode, int size );


/* @param char*  path     FULL path (from root "/") to the file */
int gros_ensure_size( Disk * disk, const char * path, int size );


/**
* Copies a file from one directory to another, incrementing the number of links
*
* @param Disk  *  disk     Disk containing the file system
* @param Inode *  from     Inode corresponding to the file to copy
* @param Inode *  todir    Inode corresponding to destination directory
* @param const char * name Desired new name for file
*/
int gros_i_copy( Disk * disk, Inode * from, Inode * todir, const char * name );

/* @param char*  to     FULL path (from root "/") to the new copied file */
int gros_copy( Disk * disk, const char * from, const char * to );

int gros_i_stat( Disk * disk, int inode_num, struct stat * stbuf );
int gros_i_chmod( Disk * disk, Inode * inode, mode_t mode );

/*
open/close: opens/closes a file
chown: change file owner and group
chmod: change file modes or Access Control Lists
*/

#endif
