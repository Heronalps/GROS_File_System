/**
 * disk.cpp
 */

#include "disk.hpp"
#include <cstring>

/**
 * Returns a new instance of a disk emulator residing in memory
 *  Disk size will be EMULATOR_SIZE defined in "grosfs.h"
 *  Disk *mem will be a char array of EMULATOR_SIZE items
 */
Disk * open_disk() {
    Disk * disk = new Disk();
    disk -> size = EMULATOR_SIZE;
    disk -> mem = new char[EMULATOR_SIZE];
    return disk;
}

/**
 * Effectively closes a connection to the disk emulator, deleting
 * memory in disk->mem and deleting the Disk object.
 *
 * @param Disk *disk    The pointer to the disk to close
 */
void close_disk( Disk * disk ) {
    delete[] disk -> mem;
    delete disk;
}

/**
 * Read a block from the disk into a provided buffer.
 *
 * @param Disk *disk      The pointer to the disk to read from
 * @param int block_num   Index of block to read
 * @param char *buf       Pointer to the destination for the read data
 *                        * Must be allocated to be size BLOCK_SIZE
 */
int read_block( Disk * disk, int block_num, char * buf ) {
    if( block_num < 0 ) {
        return -1;
    }
    int byte_offset = block_num * BLOCK_SIZE;
    if( ( byte_offset + BLOCK_SIZE ) > disk -> size ) {
        return -1;
    }
    std::memcpy( buf, ( disk -> mem + byte_offset ), BLOCK_SIZE );
    return 0;
}

/**
 * Write a block from a provided buffer to the disk.
 *
 * @param Disk *disk      The pointer to the disk to write to
 * @param int block_num   Index of block to write
 * @param char *buf       Pointer to the data to write
 *                        * Must be allocated to be size BLOCK_SIZE
 */
int write_block( Disk * disk, int block_num, char * buf ) {
    if( block_num < 0 ) {
        return -1;
    }
    int byte_offset = block_num * BLOCK_SIZE;
    if( ( byte_offset + BLOCK_SIZE ) > disk -> size ) {
        return -1;
    }
    std::memcpy( ( disk -> mem + byte_offset ), buf, BLOCK_SIZE );
    return 0;
}

