/**
 * grosfs.h
 */
#include <time.h>
#include <math.h>

#ifndef __GROSFS_H_INCLUDED__   // if grosfs.h hasn't been included yet...
#define __GROSFS_H_INCLUDED__   //   #define this so the compiler knows it has been included

#include "../include/catch.hpp"


//#define EMULATOR_SIZE 1048576   // 1 Mb
#define EMULATOR_SIZE 65536     // 65 kb
#define BLOCK_SIZE    512       // 512 b

#define DATA_BLOCKS   0.9       // 90% data blocks
#define INODE_BLOCKS  0.1       // 10% inode blocks


typedef struct _superblock {
    int     fs_disk_size;        /* total size of disk, in bytes */
    int     fs_block_size;       /* size of disk blocks, in bytes */
    int     fs_inode_size;       /* size of inode structure, in bytes */
    int     fs_num_blocks;       /* total number of data blocks */
    int     fs_num_inodes;       /* total number of inodes */
    int     fs_num_used_inodes;  /* number of used inodes */
    int     fs_num_used_blocks;  /* number of used blocks */
    int     fs_num_block_groups; /* number of block groups */

    int     free_data_list;      /* pointer to start of free data list */
    int     free_inode_list;     /* pointer to free inode list */
} Superblock;

typedef struct _inode { // 102 bytes
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

#endif 
