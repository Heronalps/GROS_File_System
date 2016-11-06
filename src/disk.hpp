/**
 * disk.h
 */

#ifndef __DISK_H_INCLUDED__   // if disk.h hasn't been included yet...
#define __DISK_H_INCLUDED__   //   #define this so the compiler knows it has been included

//#include "grosfs.hpp"
#include "../include/catch.hpp"

#define EMULATOR_SIZE 65536     // 65 kb
#define BLOCK_SIZE    512       // 512 b

#define DATA_BLOCKS   0.9       // 90% data blocks
#define INODE_BLOCKS  0.1       // 10% inode blocks

typedef struct _disk {
    int size;
    FILE * fp;
    char * mem;
} Disk;

/**
 * Returns a new instance of a disk emulator residing in memory
 *  Disk size will be EMULATOR_SIZE defined in "grosfs.h"
 *  Disk *mem will be a char array of EMULATOR_SIZE items
 */
Disk * open_disk();

/**
 * Effectively closes a connection to the disk emulator, deleting
 * memory in disk->mem and deleting the Disk object.
 *
 * @param Disk *disk    The pointer to the disk to close
 */
void close_disk( Disk * disk );

/**
 * Read a block from the disk into a provided buffer.
 *
 * @param Disk *disk      The pointer to the disk to read from
 * @param int block_num   Index of block to read
 * @param char *buf       Pointer to the destination for the read data
 *                        * Must be allocated to be size BLOCK_SIZE
 */
int read_block( Disk * disk, int block_num, char * buf );

/**
 * Write a block from a provided buffer to the disk.
 *
 * @param Disk *disk      The pointer to the disk to write to
 * @param int block_num   Index of block to write
 * @param char *buf       Pointer to the data to write
 *                        * Must be allocated to be size BLOCK_SIZE
 */
int write_block( Disk * disk, int block_num, char * buf );



#endif 
