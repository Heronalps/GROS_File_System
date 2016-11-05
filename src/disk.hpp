/**
 * disk.h
 */

#ifndef __DISK_H_INCLUDED__   // if disk.h hasn't been included yet...
#define __DISK_H_INCLUDED__   //   #define this so the compiler knows it has been included

#include "grosfs.hpp"
#include "../include/catch.hpp"

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


TEST_CASE( "Disk emulator can be accessed properly", "[disk]" ) {

    Disk * disk = open_disk();
    char buf[BLOCK_SIZE];

    REQUIRE( disk -> size == EMULATOR_SIZE );
    REQUIRE( disk -> mem != NULL );
    REQUIRE( disk -> fp == NULL );

    SECTION( "read_block will read out BLOCK_SIZE bytes" ) {
        int ret = read_block( disk, 0, buf );
        REQUIRE( ret == 0 );
    }

    SECTION( "read_block will fail on a negative block number" ) {
        int ret = read_block( disk, -1, buf );
        REQUIRE( ret != 0 );
    }

    SECTION( "read_block will fail on a block number that is out of range" ) {
        int block_num = ( disk -> size / BLOCK_SIZE ) + 1;
        int ret = read_block( disk, block_num, buf );
        REQUIRE( ret != 0 );
    }

    SECTION( "write_block will write out BLOCK_SIZE bytes" ) {
        int ret = write_block( disk, 0, buf );
        REQUIRE( ret == 0 );
    }

    SECTION( "write_block will fail on a negative block number" ) {
        int ret = write_block( disk, -1, buf );
        REQUIRE( ret != 0 );
    }

    SECTION( "write_block will fail on a block number that is out of range" ) {
        int block_num = ( disk -> size / BLOCK_SIZE ) + 1;
        int ret = write_block( disk, block_num, buf );
        REQUIRE( ret != 0 );
    }
}

TEST_CASE( "Testing Catch", "[test]" ) {
    REQUIRE( 1 == 1 );
    REQUIRE( 2 == 2 );
    //REQUIRE(3 == 2);
}

#endif 
