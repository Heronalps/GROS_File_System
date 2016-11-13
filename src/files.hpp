/**
 * files.hpp
 */

#ifndef __FILES_HPP_INCLUDED__   // if files.hpp hasn't been included yet...
#define __FILES_HPP_INCLUDED__   //   #define this so the compiler knows it has been included

//#include "grosfs.hpp"
#include "../include/catch.hpp"
#include <cstring>

#define FILENAME_MAX_LENGTH 255

typedef struct _direntry {
  int inode_num;  /* inode of file */
  char has_next;  /* 0 if this is the last entry, 1 if there are more entries */
  char filename[FILENAME_MAX_LENGTH]; /* the filename */
} DirEntry;


/**
 * Creates the primordial directory for the file system (i.e. root "/")
 *  The root of the file system will always be inode #0.
 *
 *  @param Disk* disk   Disk containing the file system
 */
void mkroot( Disk * disk );

/**
 * Returns the inode number of the file corresponding to the given path
 *
 * @param Disk* disk  Disk containing the file system
 * @param char* path  Path to the file, starting from root "/"
 */
int namei(Disk * disk, const char* path);

/**
 * Creates a directory with two entries (. and ..)
 *
 * @param Disk*  disk     Disk containing the file system
 * @param Inode* inode    Inode of directory in which to place new directory
 * @param char*  dirname  Name of new directory
 */
int i_mkdir(Disk *disk, Inode *inode, const char* dirname);
/* @param char*  path     FULL path (from root "/") to place the new directory */
int mkdir(Disk * disk, const char* path);

/**
 * Removes a directory and decrements all link counts for all files in directory
 *  If files then have 0 links, those files will be deleted/freed.
 *  TODO: Consider forcing vs not forcing recursive delete (error under latter)
 *
 * @param Disk*  disk     Disk containing the file system
 * @param Inode* inode    Inode of directory containing directory to delete
 * @param char*  dirname  Name of directory to delete
 */
int i_rmdir(Disk *disk, Inode *inode, const char* dirname);
/* @param char*  path     FULL path (from root "/") to directory the remove */
int rmdir(Disk * disk, const char* path);

/**
 * Removes a file or directory. If the file is a directory, calls rmdir.
 *
 * @param Disk*  disk       Disk containing the file system
 * @param Inode* inode      Inode of directory containing file to delete
 * @param char*  filename   Name of file to delete
 */
int i_unlink(Disk * disk, Inode *inode, const char* filename);
/* @param char*  path       FULL path (from root "/") to the file */
int unlink(Disk * disk, const char* path);

/**
 * Renames a file or directory
 *
 * @param Disk*  disk       Disk containing the file system
 * @param Inode* inode      Inode of directory containing file to rename
 * @param char*  oldname    Name of file to rename
 * @param char*  oldname    New name for file
 */
int i_rename(Disk * disk, Inode *inode, const char* oldname, const char* newname);
/* @param char*  path       FULL path (from root "/") to the file */
int rename(Disk * disk, const char* path, const char* newname);

/**
 * Reads `size` bytes at `offset` offset bytes from file corresponding
 *  to given Inode on the given disk into given buffer
 *
 * @param Disk*  disk     Disk containing the file system
 * @param Inode* inode    Inode corresponding to the file to read
 * @param char*  buf      Buffer to read into (must be allocated to at
 *                          least `size` bytes)
 * @param int    size     Number of bytes to read
 * @param int    offset   Offset into the file to start reading from
 */
int i_read(Disk * disk, Inode *inode, char *buf, int size, int offset);
/* @param char*  path     FULL path (from root "/") to the file */
int read(Disk * disk, const char* path, char *buf, int size, int offset);

/**
 * Writes `size` bytes (at `offset` bytes from 0) into file
 *  corresponding to given Inode on the given disk from given buffer
 *
 * @param Disk*  disk     Disk containing the file system
 * @param Inode* inode    Inode corresponding to the file to write to
 * @param char*  buf      Buffer to write to file (must be at least
 *                           `size` bytes)
 * @param int    size     Number of bytes to write
 * @param int    offset   Offset into the file to start writing to
 */
int i_write(Disk * disk, Inode *inode, char *buf, int size, int offset);
/* @param char*  path     FULL path (from root "/") to the file */
int write(Disk * disk, const char* path, char *buf, int size, int offset);

/**
 * Truncates or extends the file to a specified length
 *
 * @param Disk*  disk     Disk containing the file system
 * @param Inode* inode    Inode corresponding to the file to resize
 * @param int    size     Desired file size
 */
int i_truncate(Disk * disk, Inode *inode, int size);
/* @param char*  path     FULL path (from root "/") to the file */
int truncate(Disk * disk, const char* path, int size);

/*
    namei: get path to filename
    mkdir: makes a directory
    mknod: makes a file
    readdir: reads a directory
    unlink: removes a file or directory
    open/close: opens/closes a file
    read/write: reads/writes a file
    truncate: truncate or extend a file to a specified length
    chown: change file owner and group
    chmod: change file modes or Access Control Lists
*/

#endif
