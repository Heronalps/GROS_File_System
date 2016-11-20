/**
 * disk.cpp
 */

#include "disk.hpp"
#include <sys/types.h>
#include <sys/stat.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>

/**
 * Returns a new instance of a disk emulator residing in memory
 *  Disk size will be EMULATOR_SIZE defined in "grosfs.h"
 *  Disk * mem will be a char array of EMULATOR_SIZE items
 */
Disk * gros_open_disk() {
    int result;
    Disk * disk = new Disk();
    disk->size  = EMULATOR_SIZE;
    disk->fd    = open("grosfs.filesystem", O_RDWR | O_CREAT, (mode_t)0600);
    if (disk->fd == -1) {
        printf("Could not open device for file system..\n");
        exit(1);
    }
    result = lseek(disk->fd, EMULATOR_SIZE, SEEK_SET);
    if (result == -1) {
        close(disk->fd);
        printf("Could not extend file to desired file system size..\n");
        exit(1);
    }
    result = write(disk->fd, "", 1);
    if (result < 0) {
        close(disk->fd);
        printf("Could not write a byte to the end of the file..\n");
        exit(1);
    };
    return disk;
}

/**
 * Effectively closes a connection to the disk emulator, deleting
 * memory in disk->mem and deleting the Disk object.
 *
 * @param Disk * disk    The pointer to the disk to close
 */
void gros_close_disk( Disk * disk ) {
    close(disk->fd);
    delete disk;
}

/**
 * Read a block from the disk into a provided buffer.
 *
 * @param Disk * disk       The pointer to the disk to read from
 * @param int    block_num  Index of block to read
 * @param char * buf        Pointer to the destination for the read data
 *                        * Must be allocated to be size BLOCK_SIZE
 */
int gros_read_block( Disk * disk, int block_num, char * buf ) {
    if( block_num < 0 )
        return -1;

    int byte_offset = block_num * BLOCK_SIZE;
    if( ( byte_offset + BLOCK_SIZE ) > disk->size )
        return -1;

    lseek(disk->fd, byte_offset, SEEK_SET);
    read(disk->fd, buf, BLOCK_SIZE);

    return 0;
}

/**
 * Write a block from a provided buffer to the disk.
 *
 * @param Disk * disk       The pointer to the disk to write to
 * @param int    block_num  Index of block to write
 * @param char * buf        Pointer to the data to write
 *                        * Must be allocated to be size BLOCK_SIZE
 */
int gros_write_block( Disk * disk, int block_num, char * buf ) {
    if( block_num < 0 )
        return -1;

    int byte_offset = block_num * BLOCK_SIZE;
    if( ( byte_offset + BLOCK_SIZE ) > disk->size )
        return -1;

    lseek(disk->fd, byte_offset, SEEK_SET);
    write(disk->fd, buf, BLOCK_SIZE);

    return 0;
}

TEST_CASE( "Disk emulator can be accessed properly", "[disk]" ) {

    Disk * disk = gros_open_disk();
    char buf[BLOCK_SIZE];
    struct stat stbuf;

    REQUIRE( disk->size == EMULATOR_SIZE );
    REQUIRE( disk->fd != -1 );
    lseek(disk->fd, 0L, SEEK_END);
    fstat(disk->fd, &stbuf);
    REQUIRE( stbuf.st_size == disk->size+1 );

    SECTION( "gros_read_block will gros_read out BLOCK_SIZE bytes" ) {
        int ret = gros_read_block( disk, 0, buf );
        REQUIRE( ret == 0 );
    }

    SECTION( "gros_read_block will fail on a negative block number" ) {
        int ret = gros_read_block( disk, -1, buf );
        REQUIRE( ret != 0 );
    }

    SECTION( "gros_read_block will fail on a block number that is out of range" ) {
        int block_num = ( disk->size / BLOCK_SIZE ) + 1;
        int ret = gros_read_block( disk, block_num, buf );
        REQUIRE( ret != 0 );
    }

    SECTION( "gros_write_block will gros_write out BLOCK_SIZE bytes" ) {
        int ret = gros_write_block( disk, 0, buf );
        REQUIRE( ret == 0 );
    }

    SECTION( "gros_write_block will fail on a negative block number" ) {
        int ret = gros_write_block( disk, -1, buf );
        REQUIRE( ret != 0 );
    }

    SECTION( "gros_write_block will fail on a block number that is out of range" ) {
        int block_num = ( disk->size / BLOCK_SIZE ) + 1;
        int ret = gros_write_block( disk, block_num, buf );
        REQUIRE( ret != 0 );
    }
    gros_close_disk(disk);
}

TEST_CASE( "Testing Catch", "[test]" ) {
    REQUIRE( 1 == 1 );
    REQUIRE( 2 == 2 );
    //REQUIRE(3 == 2);
}
