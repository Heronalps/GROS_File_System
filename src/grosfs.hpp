/**
 * grosfs.h
 */
#include <time.h>
#include <math.h>
#include <cstring>
#include <iostream>

#ifndef __GROSFS_H_INCLUDED__   // if grosfs.h hasn't been included yet...
#define __GROSFS_H_INCLUDED__   //   #define this so the compiler knows it has been included


#define EMULATOR_SIZE 4194304   // 4 mb
#define BLOCK_SIZE    4096      // 4 kb

#define DATA_BLOCKS   0.9       // 90% data blocks
#define INODE_BLOCKS  0.1       // 10% inode blocks

#define SINGLE_INDRCT 12        // index for single indirect data block
#define DOUBLE_INDRCT 13        // index for double indirect data block
#define TRIPLE_INDRCT 14        // index for triple indirect data block

// the space at the end of the superblock data up until the end of the block
#define SB_ILIST_SIZE ( BLOCK_SIZE - 9 * sizeof( int ) )

#define DEBUG
#ifdef DEBUG
#define pdebug std::cout << __FILE__ << "(" << __LINE__ << ") DEBUG: "
#else
#define pdebug 0 && std::cout
#endif

#include "bitmap.hpp"
#include "disk.hpp"
//#include "files.hpp"
#include "../include/catch.hpp"

typedef struct _superblock {
    int fs_disk_size;        /* total size of disk, in bytes */
    int fs_block_size;       /* size of disk blocks, in bytes */
    int fs_inode_size;       /* size of inode structure, in bytes */
    int fs_num_blocks;       /* total number of data blocks */
    int fs_num_inodes;       /* total number of inodes */
    int fs_num_used_inodes;  /* number of used inodes */
    int fs_num_used_blocks;  /* number of used blocks */
    int fs_num_block_groups; /* number of block groups */
    int first_data_block;    /* pointer to first data block */
    int free_inodes[ SB_ILIST_SIZE ]; /* bitmap of free inodes */
} Superblock;


typedef struct _inode { // 106 bytes
    int     f_inode_num; /* the inode number */
    int     f_size;      /* file size, in bytes */
    int     f_uid;       /* uid of owner */
    int     f_gid;       /* gid of owner group */
    /**
     * File ACLs
     *    bits 0,1:    file type
     *                    00 -> regular file
     *                    01 -> directory
     *                    10 -> special device
     *                    11 -> symlink
     *    bits 2,3,4:  owner permissions (r/w/x)
     *    bits 5,6,7:  group permissions (r/w/x)
     *    bits 8,9,10: universal permissions (r/w/x)
     */
    short   f_acl;
    time_t  f_ctime;    /* time inode last modified */
    time_t  f_mtime;    /* time file last modified */
    time_t  f_atime;    /* time file last accessed */
    int     f_links;    /* number of hard links to this file */
    /**
     * Data blocks for file
     *    f_block[ 0 - 11 ] = direct data blocks
     *    f_block[ 12 ]     = singly indirect data blocks
     *    f_block[ 13 ]     = doubly indirect data blocks
     *    f_block[ 14 ]     = triply indirect data blocks
     */
    int     f_block[ 15 ];
} Inode;


/**
 * creates the superblock, free inode list, and free block list on disk
 *
 * @param Disk * disk    The disk for the new file system
 */
void gros_make_fs( Disk * disk ); // initialize the file system


/**
 * Initializes inodes on disk
 *
 * @param Disk * disk             The disk containing the file system
 * @param int    num_inode_blocks The number of blocks allocated for inodes
 * @param int    inode_per_block  The number of inodes per block on disk
 */
void gros_init_inodes( Disk * disk, int num_inode_blocks, int inode_per_block );


/**
 * Verifies and corrects all file system information
 *  - Ensures all data blocks marked as "used" appear in an inode
 *  - Those that do not will be freed (or put into /lost+found)
 *  - Also repopulates the inode free list
 *  - Counts the number of used inodes and data blocks
 *
 * @param Disk * disk    The disk that contains the file system
 */
void gros_fsck( Disk * disk );    // recover the file system


/**
 * Finds path from root to inode, backwards
 *
 * @param   Disk        * disk        The disk containing the file system
 * @param   Inode       * parent_dir  The parent directory to traverse
 * @param   char        * filename    The file name to get path for
 * @return  const char  *             String path to inode number
 */
const char * gros_pwd( Disk * disk, Inode * parent_dir, const char * filename );


/**
 * Recursively traverse path back to root
 *
 * @param   Disk  * disk   The disk containing the file system
 * @param   char  * path   The current filepath
 * @param   Inode * dir    The directory to traverse
 * @return  char  *        Directory name
 */
const char * gros_get_path_to_root( Disk * disk, char * path, Inode * dir );


/**
 * Checks for inode number in parent inode
 *
 * @param   Disk * disk          The disk containing the file system
 * @param   int    parent_num    The parent inode to check
 * @param   int    inode_num     The inode number to search for
 * @return  int                  1 success, 0 failure
 */
int gros_check_parent( Disk * disk, int parent_num, int inode_num );


/**
 * Traverses directory to recursively search for inode number
 *
 * @param  Disk  * disk           The disk containing the file system
 * @param  Inode * dir            The directory to traverse
 * @param  int     inode_num      The inode number to count links for
 * @param  int     links          The current number of links
 */
int gros_count_links( Disk * disk, Inode * dir, int inode_num, int links );


/**
 * Checks list of allocated data blocks for valid block number and duplicates,
 *  appends block number to list if valid and not dup
 *
 * @param Disk *  disk           The disk containing the file system
 * @param int  *  allocd_blocks  The list of allocated data blocks
 * @param int     block_num      The block to check against list
 */
int gros_check_blocks( Disk * disk, int * allocd_blocks, int block_num );


/**
 * Returns the first free inode from the free inode list in the superblock
 *
 * @param Disk * disk    The disk that contains the file system
 */
Inode * gros_find_free_inode( Disk * disk );


/**
 * Saves an Inode back to disk
 *
 * @param Disk  * disk        The disk containing the file system
 * @param Inode * inode_num   The inode to save
 */
int gros_save_inode( Disk * disk, Inode * inode );


/**
* Scan inode blocks for more free inode numbers for free ilist
*
* @param Disk * disk         The disk that contains the file system
* @param int    inode_index  The
*/
void gros_repopulate_ilist( Disk * disk, int inode_index );


/**
 * Returns a new allocated inode given first free inode number from find_free_inode
 *
 * @param  Disk * disk   The disk that contains the file system
 */
Inode * gros_new_inode( Disk * disk );


/**
 * Return inode from disk
 *
 * @param  Disk * disk      The disk that contains the file system
 * @param  int    inode_num The inode number to retrieve from disk
 */
Inode * gros_get_inode( Disk * disk, int inode_num );


/**
 * Deallocates an inode and frees up all the resources owned by it
 *
 * @param Disk  *  disk   The disk containing the file system
 * @param Inode *  inode  The inode to deallocate
 */
void gros_free_inode( Disk * disk, Inode * inode );


/**
 * Updates free inode list in superblock
 *
 * @param Disk * disk       The disk containing the file system
 * @param int    inode_num  The inode number to put on free list
 */
void gros_update_free_list( Disk * disk, int inode_num );


/**
 * Deallocates a data block
 *
 * @param Disk * disk         The disk containing the file system
 * @param int    block_index  The block number of the block to deallocate
 */
void gros_free_data_block( Disk * disk, int block_index );


/**
 * Allocates data block from free data list
 *  Returns integer corresponding to block number of allocated data block
 *  Returns -1 if there are no blocks available
 *
 * @param Disk * disk    The disk containing the file system
 */
int gros_allocate_data_block( Disk * disk );


/**
 *  Given an array of `n` block numbers, deallocate each one.
 *
 *  @param Disk * disk   The disk containing the file system
 *  @param int  * list   The array of block numbers
 *  @param int    n      The number of block numbers in `list`
 *  @return              1 upon successful deallocation
 */
int gros_free_blocks_list( Disk * disk, int * block_list, int n );


int gros_is_file( short acl );


int gros_is_dir( short acl );


#endif
