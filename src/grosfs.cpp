#include "disk.hpp"

/*
 * creates the superblock, free inode list, and free block list on disk
 * */
void make_fs( Disk * disk ) {
//    cast to / from ( Superblock * )
    Superblock * superblock             = new Superblock();
    superblock -> fs_disk_size          = disk -> size;
    superblock -> fs_block_size         = BLOCK_SIZE;
    superblock -> fs_inode_size         = sizeof( Inode );
    int num_blocks                      = superblock -> fs_disk_size / superblock -> fs_block_size;             // 128 total blocks
    int num_inode_blocks                = ceil( num_blocks * INODE_BLOCKS );                                    // 13 total blocks
    int inode_per_block                 = floor( superblock -> fs_block_size / superblock -> fs_inode_size );
    superblock -> fs_num_blocks         = floor( num_blocks * DATA_BLOCKS );                                    // # blocks for data ( 115 )
    superblock -> fs_num_inodes         = num_inode_blocks * inode_per_block;                                   // # blocks for inodes ( 13 ) * # inodes per block ( 5 ) = 65 inodes
    superblock -> fs_num_block_groups   = ceil( superblock -> fs_num_blocks / superblock -> fs_block_size );    // 1 block group ( 512 - 115 = 397 unused bits )
    superblock -> fs_num_used_inodes    = 0;
    superblock -> fs_num_used_blocks    = 0;

    superblock -> free_data_list        = ( superblock -> fs_block_size ) * num_inode_blocks;                   // pointer for first free block list ( first block after inodes )
//    superblock -> free_inode_list       = 0; /* red-black tree */
}

/*
 * allocate new inode
 * */
Inode new_inode( Disk * disk ) {
//    for(int i = free_ilist; i < disk -> )
}

int has_links( Inode * inode ) {
    return inode -> f_acl > 1 ? 1 : 0;
}

/*
 * search free inode list for available inode
 * */
Inode * find_free_inode( Disk * disk ) {
    char            buf[ BLOCK_SIZE ];
    Superblock *    superblock;
    int             addr;
    Inode *         free_inode;

    read_block( disk, 0, buf );
    superblock = ( Superblock * ) buf;
    addr = superblock -> free_inode_list;

    while( ( free_inode = disk[ addr ] ) > 0 ) {
        addr += sizeof( Inode );
    }

    return free_inode;
}

// allocate inode  - single empty data block + size = 0
// free inode      - deallocate data blocks, put inode on free list
// read / write    - implicit when read / write to file
//                      access times, file grows, change ownership / access
//                      new directory path to file

// buffer allocate - new data block.. new / extending file
// buffer free     - deleting file (shortening end of file ?)
// read buffer     - determined by inode pointers ?
