#include "grosfs.hpp"


/**
 * creates the superblock, free inode list, and free block list on disk
 *
 * @param Disk* disk    The disk for the new file system
 */
void make_fs( Disk * disk ) {
    int i, j, k;
//    cast to / from ( Superblock * )
    Superblock * superblock             = new Superblock();
    superblock -> fs_disk_size          = disk -> size;
    superblock -> fs_block_size         = BLOCK_SIZE;
    superblock -> fs_inode_size         = sizeof( Inode );
    int num_blocks                      = superblock -> fs_disk_size / superblock -> fs_block_size;             // 128 total blocks
    int num_inode_blocks                = ceil( num_blocks * INODE_BLOCKS );                                    // 13 total blocks
    int inode_per_block                 = floor( superblock -> fs_block_size / superblock -> fs_inode_size );
    superblock -> fs_num_blocks         = floor( num_blocks * DATA_BLOCKS ); // # blocks for data ( 115 )
    superblock -> fs_num_inodes         = num_inode_blocks * inode_per_block; // # blocks for inodes ( 13 ) * # inodes per block ( 5 ) = 65 inodes
    superblock -> fs_num_block_groups   = ceil( superblock -> fs_num_blocks / superblock -> fs_block_size );    // 1 block group ( 512 - 115 = 397 unused bits )
    superblock -> fs_num_used_inodes    = 0;
    superblock -> fs_num_used_blocks    = 0;

    // the free_data_list starts at the first block after the superblock and all of the inodes
    superblock -> first_data_block      = 1 /* Superblock */ + num_inode_blocks /* number of blocks dedicated to inodes */;

    // set up each of the inodes to *be* an inode
    int inode_count = 0;
    int rel_inode_index;
    char ibuf[BLOCK_SIZE];
    Inode *tmp = new Inode();
    tmp->f_links = 0; // the inode is free
    for (k=0; k < 15; k++) {
      tmp->f_block[k] = -1; // no data block allocated
    }

    // inodes are in blocks 1 through `num_inode_blocks`
    for (i=1; i <= num_inode_blocks; i++) {
      // read in the block
      read_block( disk, i, (char *) ibuf );
      for (j=0; j < inode_per_block; j++) {
        tmp->f_inode_num = inode_count++;
        rel_inode_index = j % inode_per_block;
        std::memcpy(((Inode*)ibuf) + rel_inode_index, tmp, sizeof(Inode));
      }
      write_block( disk, i, (char *) ibuf );
    }

    delete tmp;

    //superblock -> free_inode_list       = &disk -> mem[ sizeof( Superblock ) ];
    for (i=0; i<SB_ILIST_SIZE; i++) {
      superblock->free_inodes[i] = i;
    }

    int block_group_count               = 0; //var to get location of each new bitmap
    int block_num;
    char *buf;
    for ( i = 0; i < superblock -> fs_num_block_groups; i++ ) {
        block_num = superblock->first_data_block + i * BLOCK_SIZE;
        buf = (char *)malloc(BLOCK_SIZE * sizeof(char));
        Bitmap *bitmap = init_bitmap( superblock -> fs_block_size, buf); //superblock -> free_data_list + block_group_count);
        set_bit( bitmap, 0 ); //first block is bitmap
        block_group_count += superblock -> fs_block_size * superblock -> fs_block_size;
        write_block(disk, block_num, bitmap->buf);
        free(buf);
    }

    write_block(disk, 0, (char*) superblock);

    mkroot(disk); // set up the root directory
}


/**
 * Verifies and corrects all file system information
 *  - Ensures all data blocks marked as "used" appear in an inode
 *  - Those that do not will be freed (or put into /lost+found)
 *  - Also repopulates the inode free list
 *  - Counts the number of used inodes and data blocks
 *
 * @param Disk* disk    The disk that contains the file system
 */
void fsck( Disk * disk ) {
  // TODO
}


/**
 * Returns the first free inode from the free inode list in the superblock
 *
 * @param Disk* disk    The disk that contains the file system
 */
Inode * find_free_inode( Disk * disk ) {
    int i;
    char            buf[ BLOCK_SIZE ];
    Superblock *    superblock;
    int free_inode_index = -1;
    Inode *         free_inode;

    read_block( disk, 0, buf );
    superblock = ( Superblock * ) buf;

    // loop through superblock->free_inodes, find first non-negative

    for (i=0; i<SB_ILIST_SIZE; i++) {
      if (superblock->free_inodes[i] != -1) {
        free_inode_index = superblock->free_inodes[i];
        superblock->free_inodes[i] = -1;
      }
    }

    // if we get to the end without finding any free inodes, fetch some more from the inode datablocks
    //    and put a bunch back into superblock->free_inodes

    if (free_inode_index == -1) {
      //ERROR--go fetch some free inodes and put them back on the list
      // TODO: Write and call that function
      return NULL;
    }

    // save the -1 we just set for the inode we just found
    write_block(disk, 0, (char*)superblock);

    // otherwise, return that inode.

    return get_inode( disk, free_inode_index );
}


/**
 * Returns a new allocated inode given first free inode number from find_free_inode
 *
 * @param Disk* disk    The disk that contains the file system
 */
Inode * new_inode( Disk * disk ) {
    Inode * inode                       = find_free_inode( disk );
    inode -> f_size                     = 0;
    inode -> f_uid                      = 0;    //through system call??
    inode -> f_gid                      = 0;    //through system call??
    inode -> f_acl                      = 0;    //through system call??
    inode -> f_ctime                    = time(NULL);
    inode -> f_mtime                    = time(NULL);
    inode -> f_atime                    = time(NULL);
    inode -> f_links                    = 0; // set to 1 in mknod
    inode -> f_block[0]                 = allocate_data_block(disk);
    return inode;
}


/**
 * Returns the Inode corresponding to the given inode index
 *
 * @param Disk* disk        The disk containing the file system
 * @param int   inode_num   The inode index
 */
Inode * get_inode( Disk * disk, int inode_num ) {
    // return inode number `free_inode_index`.
    int inodes_per_block, block_num, rel_inode_index;
    char buf[BLOCK_SIZE];
    Superblock *superblock;

    read_block( disk, 0, (char *) buf );
    superblock = (Superblock *) buf;
    inodes_per_block = floor(superblock->fs_block_size / superblock->fs_inode_size);
    block_num = inode_num / inodes_per_block;
    rel_inode_index = inode_num % inodes_per_block;

    read_block(disk, block_num, buf);
    Inode *block_inodes = (Inode*)buf;
    return &(block_inodes[rel_inode_index]);
}


/**
 * Deallocates an inode and frees up all the resources owned by it
 *
 * @param Disk*  disk   The disk containing the file system
 * @param Inode* inode  The inode to deallocate
 */
void free_inode( Disk * disk, Inode *inode) {
    char sbuf[ BLOCK_SIZE ];
    char dbuf[ BLOCK_SIZE ];
    char tbuf[ BLOCK_SIZE ];
    int i, j, block, done = 0;

    int num_blocks = ceil(inode->f_size / BLOCK_SIZE);
    int direct_blocks = std::min(12, num_blocks);

    // deallocate the direct blocks
    done = free_blocks_list(disk, (int*)inode->f_block, direct_blocks);

    // deallocate the single indirect blocks
    if (done == 0) {
      read_block(disk, (long)inode->f_block[12], sbuf); // the block of redirects
      done = free_blocks_list(disk, (int*)sbuf, BLOCK_SIZE/sizeof(int));
    }

    // deallocate the double indirect blocks
    if (done == 0) {
      read_block(disk, (long)inode->f_block[13], dbuf); // the block of double redirects
      for (i=0; i<BLOCK_SIZE/sizeof(int); i++) {
        read_block(disk, (int)dbuf[i], sbuf); // the block of single redirects
        done = free_blocks_list(disk, (int*)sbuf, BLOCK_SIZE/sizeof(int));
        if (done == 1) { break; }
      }
    }

    // deallocate the triple indirect blocks
    if (done == 0) {
      read_block(disk, (long)inode->f_block[14], tbuf); // the block of triple redirects
      for (i=0; i<BLOCK_SIZE/sizeof(int); i++) {
        read_block(disk, tbuf[i], dbuf); // the block of double redirects
        for (j=0; j<BLOCK_SIZE/sizeof(int); j++) {
          read_block(disk, dbuf[i], sbuf); // the block of single redirects
          done = free_blocks_list(disk, (int*)sbuf, BLOCK_SIZE/sizeof(int));
          if (done == 1) { break; }
        }
        if (done == 1) { break; }
      }
    }

    // TODO: Put inode->f_inode_num onto superblock free list
}


/**
 * Checks if an Inode has any hard links, else it's free/available for allocation
 *
 * @param Inode* inode    The Inode to check
 */
int has_links( Inode * inode ) {
    return inode -> f_links >= 1 ? 1 : 0;
}


/**
 * Allocates data block from free data list
 *  Returns integer corresponding to block number of allocated data block
 *  Returns -1 if there are no blocks available
 *
 * @param Disk* disk    The disk containing the file system
 */
int allocate_data_block( Disk *disk ) {
    char            buf[BLOCK_SIZE];
    char            sbbuf[BLOCK_SIZE];
    int             i, bitmap_index, block_num;
    Superblock *    superblock;

    read_block( disk, 0, (char *) sbbuf );
    superblock = (Superblock *) sbbuf;

    for ( i = 0; i < superblock -> fs_num_block_groups; i++ ) {
        // block num for block group free list
        block_num = superblock->first_data_block + i * BLOCK_SIZE;
        read_block( disk, block_num, buf );
        Bitmap *bitmap = init_bitmap( superblock -> fs_block_size, buf);
        // if there is a free block in this block group
        if ( ( bitmap_index = first_unset_bit(bitmap) ) != -1 ) {
            // mark the data block as not free
            set_bit( bitmap, bitmap_index );
            write_block( disk, block_num, buf );
            return block_num + bitmap_index; // block num for free block
        }
    }
    return -1;    //no blocks available
}


/**
 * Deallocates a data block
 *
 * @param Disk* disk         The disk containing the file system
 * @param int   block_index  The block number of the block to deallocate
 */
void free_data_block( Disk *disk, int block_index ) {
    char buf[ BLOCK_SIZE ];
    int relative_index, block_group, offset;
    Bitmap *bm;
    Superblock *superblock;

    // write zeroes to block
    write_block( disk, block_index, buf);

    // decrement number of used datablocks for the superblock
    read_block( disk, 0, buf );
    superblock = ( Superblock * ) buf;
    superblock->fs_num_used_blocks--;
    write_block( disk, 0, buf );

    // calculate which block group this block is in
    relative_index = block_index - superblock->first_data_block;
    block_group = relative_index / BLOCK_SIZE;
    offset = relative_index % BLOCK_SIZE;

    // mark the block as unused in its block group leader
    read_block( disk, superblock->first_data_block + block_group * BLOCK_SIZE, buf );
    bm = init_bitmap(BLOCK_SIZE, buf);
    unset_bit(bm, offset);
    write_block( disk, superblock->first_data_block + block_group * BLOCK_SIZE, buf);
}


/**
 *  Given an array of `n` block numbers, deallocate each one.
 *
 *  @param Disk* disk   The disk containing the file system
 *  @param int*  list   The array of block numbers
 *  @param int   n      The number of block numbers in `list`
 */
int free_blocks_list( Disk *disk, int *list, int n) {
  int i, block, done = 0;
  for (i=0; i<n; i++) {
    block = list[i];
    if ( block != -1 ) {
      free_data_block(disk, block);
    } else {
      done = 1;
      break;
    }
  }
  return done;
}


// allocate inode  - single empty data block + size = 0
// free inode      - deallocate data blocks, put inode on free list
// read / write    - implicit when read / write to file
//                      access times, file grows, change ownership / access
//                      new directory path to file

// buffer allocate - new data block.. new / extending file
// buffer free     - deleting file (shortening end of file ?)
// read buffer     - determined by inode pointers ?
