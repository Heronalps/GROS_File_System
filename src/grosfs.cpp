#include "grosfs.hpp"
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

	superblock -> free_data_list 		= &disk -> mem[ sizeof( Superblock ) + ( superblock -> fs_block_size ) * num_inode_blocks ];	
	superblock -> free_inode_list 		= &disk -> mem[ sizeof( Superblock ) ];
	int i;
	int block_group_count = 0;																					//var to get location of each new bitmap
	for ( i = 0; i < superblock -> fs_num_block_groups; i++ ) {
		Bitmap *bitmap = init_bitmap( superblock -> fs_block_size, superblock -> free_data_list + block_group_count);
		set_bit( bitmap, 0 );																					//first block is bitmap
		block_group_count += superblock -> fs_block_size * superblock -> fs_block_size;
	}
	
  write_block(disk, 0, (char*) superblock);
}

/*
 * search free inode list for available inode
 * */
Inode * find_free_inode( Disk * disk ) {
    char            buf[ BLOCK_SIZE ];
    Superblock *    superblock;
    char *             addr;
    Inode *         free_inode;

    read_block( disk, 0, buf );
    superblock = ( Superblock * ) buf;
    addr = superblock -> free_inode_list;

    while( ( free_inode = ( Inode * ) addr ) > 0 ) {
        addr += sizeof( Inode );
    }

    return free_inode;
}

/*
 * Allocate inode from free data list
 * Return -1 if there are no blocks available
 * */
char * allocate_data_block( Disk *disk ) {
	char            buf[ BLOCK_SIZE ];
    Superblock *    superblock;
    
	read_block( disk, 0, buf );
    superblock = ( Superblock * ) buf;
    int i, bitmap_index;
	int block_group_count = 0;																					  //var to get location of each new bitmap
	for ( i = 0; i < superblock -> fs_num_block_groups; i++ ) {
		Bitmap *bitmap = ( Bitmap *) ( superblock -> free_data_list + block_group_count );
		if ( ( bitmap_index = first_unset_bit(bitmap) ) != -1 ) {
			set_bit( bitmap, bitmap_index );
			char * addr = superblock -> free_data_list + block_group_count + bitmap_index * superblock -> fs_block_size;  //address of data block 
			return addr;
		}
		block_group_count += superblock -> fs_block_size * superblock -> fs_block_size;
	}
	return NULL;	//no blocks available
}

/*
 * allocate new inode from list ( rb tree ? )
 * */
Inode * new_inode( Disk * disk ) {
	Inode * inode 						= new Inode();
	inode -> f_size						= 0;
	inode -> f_uid						= 0;	//through system call??
	inode -> f_gid						= 0;	//through system call??
	inode -> f_acl						= 0;	//through system call??
	inode -> f_ctime					= time(NULL);
	inode -> f_mtime					= time(NULL);
	inode -> f_atime					= time(NULL);
	inode -> f_links					= 1;
	inode -> f_block[0]					= allocate_data_block(disk);
	Inode * free_inode = find_free_inode( disk );
//	char * 	free_inode_addr = find_free_inode( disk );
  std::memcpy( free_inode, inode, sizeof( Inode ) );
	return inode;
}

/*
 * check if file has any hard links, else it's available for allocation
 * */
int has_links( Inode * inode ) {
    return inode -> f_links >= 1 ? 1 : 0;
}


/*
 * Deallocate inode from free data list
 * */
/*void free_data_block( Disk *disk, char * block ) {
	char            buf[ BLOCK_SIZE ];
    Superblock *    superblock;
    
	read_block( disk, 0, buf );
    superblock = ( Superblock * ) buf;
    int i, bitmap_index;
	int block_group_count = 0;																					  //var to get location of each new bitmap
	for ( i = 0; i < superblock -> fs_num_block_groups; i++ ) {
		Bitmap *bitmap = ( Bitmap * )( superblock -> free_data_list + block_group_count );
		char * next_bitmap_addr = superblock -> free_data_list + block_group_count + superblock -> fs_block_size * superblock -> fs_block_size;
		if ( block < next_bitmap_addr ) { 																		  //see if block lies within current block group
			memset( block, 0, superblock -> fs_block_size );
			int bitmap_index = ( block - bitmap ) / superblock -> fs_block_size;								  //get bitmap index of block 
			unset_bit(bitmap, bitmap_index);
		}
		block_group_count += superblock -> fs_block_size * superblock -> fs_block_size;
	}
} */

/*
 * Deallocate inode from free inode list
 * */
/*void free_inode( Disk * disk, Inode * inode) {
//  deallocate data blocks
	int i;
	int direct_blocks_empty = 0;									//0 if data blocks don't go past direct blocks
	for ( i = 0; i < 12; i++ ) {
		if ( inode -> f_block[i] != NULL ) {
			free_data_block( disk, inode -> f_block[i] );
			direct_blocks_empty = 1;
		}
		else {
			direct_blocks_empty = 0;
			break;
		}
	}
	//still need to loop through indirect blocks and free those
	memset( inode, 0, sizeof( Inode ) );	//inode or disk[ inode ]?
}
*/


// allocate inode  - single empty data block + size = 0
// free inode      - deallocate data blocks, put inode on free list
// read / write    - implicit when read / write to file
//                      access times, file grows, change ownership / access
//                      new directory path to file

// buffer allocate - new data block.. new / extending file
// buffer free     - deleting file (shortening end of file ?)
// read buffer     - determined by inode pointers ?
