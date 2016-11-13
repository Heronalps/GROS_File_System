/**
 * grosfs.h
 */
#include <time.h>
#include <math.h>
#include <cstring>

#ifndef __GROSFS_H_INCLUDED__   // if grosfs.h hasn't been included yet...
#define __GROSFS_H_INCLUDED__   //   #define this so the compiler knows it has been included


#define EMULATOR_SIZE 4194304   // 4 mb
#define BLOCK_SIZE    4096      // 4 kb

#define DATA_BLOCKS   0.9       // 90% data blocks
#define INODE_BLOCKS  0.1       // 10% inode blocks

// the space at the end of the superblock data up until the end of the block
#define SB_ILIST_SIZE (BLOCK_SIZE - 9*sizeof(int))

#include "bitmap.hpp"
#include "disk.hpp"
#include "../include/catch.hpp"

typedef struct _superblock {
    int     fs_disk_size;        /* total size of disk, in bytes */
    int     fs_block_size;       /* size of disk blocks, in bytes */
    int     fs_inode_size;       /* size of inode structure, in bytes */
    int     fs_num_blocks;       /* total number of data blocks */
    int     fs_num_inodes;       /* total number of inodes */
    int     fs_num_used_inodes;  /* number of used inodes */
    int     fs_num_used_blocks;  /* number of used blocks */
    int     fs_num_block_groups; /* number of block groups */

    //char *     free_data_list;      /* pointer to start of free data list */
    int first_data_block;
    int free_inodes[SB_ILIST_SIZE];
    //char *     free_inode_list;     /* pointer to free inode list */
} Superblock;

typedef struct _inode { // 106 bytes
    int     f_inode_num; /* the inode number */
    int     f_size;     /* file size, in bytes */
    int     f_uid;      /* uid of owner */
    int     f_gid;      /* gid of owner group */
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
     *    f_block[0-11] = direct data blocks
     *    f_block[12]   = singly indirect data blocks
     *    f_block[13]   = doubly indirect data blocks
     *    f_block[14]   = triply indirect data blocks
     */
    int     f_block[15];
} Inode;


void make_fs( Disk * disk ); // initialize the file system
void fsck( Disk * disk ); // recover the file system

Inode * find_free_inode( Disk * disk );
Inode * new_inode( Disk * disk ); // allocate inode
Inode * get_inode( Disk * disk, int inode_num ); // allocate inode
void free_inode( Disk * disk, Inode * inode ); // deallocate_inode
int has_links( Inode * inode );

int allocate_data_block( Disk *disk ); // allocate data block
void free_data_block( Disk *disk, int block ); // deallocate a data block
int free_blocks_list( Disk *disk, int *list, int n); // deallocate a list of data blocks
// read/write data blocks through disk (read_block, write_block)

#endif
