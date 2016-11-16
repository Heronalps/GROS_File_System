#include "grosfs.hpp"
#include "files.hpp"


/**
 * creates the superblock, free inode list, and free block list on disk
 *
 * @param Disk * disk    The disk for the new file system
 */
void make_fs( Disk * disk ) {
    int i;

    Superblock * superblock           = new Superblock();
    superblock -> fs_disk_size        = disk -> size;
    superblock -> fs_block_size       = BLOCK_SIZE;
    superblock -> fs_inode_size       = sizeof( Inode );
    int num_blocks                    = superblock -> fs_disk_size /
                                        superblock -> fs_block_size;
    int num_inode_blocks              = ( int ) ceil( num_blocks * INODE_BLOCKS );
    int inode_per_block               = ( int ) floor( superblock -> fs_block_size
                                                       / superblock -> fs_inode_size );
    superblock -> fs_num_blocks       = ( int ) floor( num_blocks * DATA_BLOCKS );
    superblock -> fs_num_inodes       = num_inode_blocks * inode_per_block;
    superblock -> fs_num_block_groups = ( int ) ceil( superblock -> fs_num_blocks
                                                      / superblock -> fs_block_size );
    superblock -> fs_num_used_inodes  = 0;
    superblock -> fs_num_used_blocks  = 0;

    // the free_data_list starts at the first block after the superblock and all of the inodes
    superblock -> first_data_block    = 1 + num_inode_blocks;

    // initialize inodes on disk
    init_inodes( disk, num_inode_blocks, inode_per_block );

    // initialize free ilist with initial inode numbers
    for( i = 0; i < SB_ILIST_SIZE; i++ )
        superblock -> free_inodes[ i ] = i;

    // initialize block groups w/ bitmaps
    int     block_group_count = 0; // var to get location of each new bitmap
    int     block_num;
    char *  buf;

    for( i = 0; i < superblock -> fs_num_block_groups; i++ ) {
        block_num = superblock -> first_data_block + i * BLOCK_SIZE;
        buf       = ( char * ) malloc( BLOCK_SIZE * sizeof( char ) );

        Bitmap * bitmap = init_bitmap( superblock -> fs_block_size, buf );
        set_bit( bitmap, 0 ); // set first block to used bc it's the bitmap
        block_group_count += superblock -> fs_block_size
                             * superblock -> fs_block_size;
        write_block( disk, block_num, bitmap -> buf );
        free( buf );
    }

    write_block( disk, 0, ( char * ) superblock );

    mkroot( disk ); // set up the root directory
}


/**
 * Initializes inodes on disk
 *
 * @param Disk * disk             The disk containing the file system
 * @param int    num_inode_blocks The number of blocks allocated for inodes
 * @param int    inodes_per_block The number of inodes per block on disk
 */
void init_inodes( Disk * disk, int num_inode_blocks, int inodes_per_block ) {
    int     i;
    int     j;
    int     rel_inode_index;
    char    ibuf[ BLOCK_SIZE ];
    int     inode_num;
    Inode * tmp;

    inode_num            = 0;
    tmp            = new Inode();
    tmp -> f_links = 0;

    for( i = 0; i < 15; i++ )
        tmp -> f_block[ i ] = -1;   // null data block pointers

    // inodes are in blocks 1 through `num_inode_blocks`
    for( i = 1; i <= num_inode_blocks; i++ ) {
        // read in the block to fill with inodes
        read_block( disk, i, ( char * ) ibuf );

        for( j = 0; j < inodes_per_block; j++ ) {
            tmp -> f_inode_num = inode_num++;
            rel_inode_index = j % inodes_per_block;
            std::memcpy( ( ( Inode * ) ibuf ) + rel_inode_index,
                         tmp,
                         sizeof( Inode ) );
        }
        write_block( disk, i, ( char * ) ibuf );
    }
    delete tmp;
}


// TODO test function
/**
 * Verifies and corrects all file system information
 *  - Ensures all data blocks marked as "used" appear in an inode
 *  - Those that do not will be freed (or put into / lost + found)
 *  - Vice versa, check free blocks are not claimed by any files
 *  - Also updates, repopulates the inode free list
 *  - Counts the number of used, free inodes and data blocks
 *
 * @param  Disk * disk    The disk that contains the file system
 * @return # files, # used fragments, # free fragments
 *         ( # free non-block fragments, # unused full blocks, % fragmentation )
 */
void fsck( Disk * disk ) {
    char         buf [ BLOCK_SIZE ];
    char         sbuf[ BLOCK_SIZE ];
    char         dbuf[ BLOCK_SIZE ];
    char         tbuf[ BLOCK_SIZE ];
    int          size;
    int          valid;
    int          links;
    int          i;
    int          j;
    int          k;
    int          l;
    int          m;
    int          n;
    int          inodes_per_block;
    int          num_inode_blocks;
    int          num_free_blocks;
    int          num_free_inodes;
    int          n_indirects;       // total indirect block # entries per block
    int        * allocd_blocks;
    Inode      * inode;
    Superblock * superblock;

    read_block( disk, 0, buf );
    n_indirects  = BLOCK_SIZE / sizeof( int );
    superblock   = ( Superblock * ) buf;
    allocd_blocks = ( int * ) calloc( superblock -> fs_num_blocks, sizeof( int ) );

    num_free_blocks  = 0;
    num_free_inodes  = 0;
    num_inode_blocks = ( int ) ceil( ( superblock -> fs_disk_size
                                       / superblock -> fs_block_size )
                                     * INODE_BLOCKS );
    inodes_per_block = ( int ) floor( superblock -> fs_block_size
                                      / superblock -> fs_inode_size );

    // check file system size within bounds
    if( superblock -> fs_num_blocks * superblock -> fs_block_size
        + superblock -> fs_num_inodes * sizeof( Inode ) + 1
        > superblock -> fs_disk_size ) {
        perror( "Corrupt file system size" );
        // exit or request alternate superblock
        exit( -1 );
    }

    // scan inode blocks
    for( i = 1; i <= num_inode_blocks; i++ ) {
        read_block( disk, i, ( char * ) buf );

        // scan individual inodes in block
        for( j = 0; j < inodes_per_block; j++ ) {
            inode = ( ( Inode * ) buf ) + j;
            links = count_links( disk, inode -> f_inode_num );
            k     = 0;
            size  = 0;
            valid = 1;

            // check inode number in bounds, correct link count
            if( inode -> f_inode_num < 1
                || inode -> f_inode_num > superblock -> fs_num_inodes
                || ( links < 1 && inode -> f_links > 0 ) ) {
                perror( "Corrupt inode" );
                free_inode( disk, inode );
                valid = 0;
            }
            else if( links != inode -> f_links ) // update if wrong count
                inode -> f_links = links;

            // increment free inode counter if no links
            if( links < 1 )
                num_free_inodes++;
            else if( valid ) {
                if( is_file( inode -> f_acl ) ) {
                    // check for valid data block #s, duplicate allocated blocks
                    while( k < 15 && valid ) {
                        if( k < SINGLE_INDRCT ) {
                            valid = check_blocks( disk,
                                                  allocd_blocks,
                                                  inode -> f_block[ k++ ] );
                            size += valid;
                        }
                        else if( k == SINGLE_INDRCT ) {
                            read_block( disk, inode -> f_block[ k++ ], sbuf );
                            l = 0;
                            while( l < n_indirects && valid ) {
                                valid = check_blocks( disk,
                                                      allocd_blocks,
                                                      sbuf[ l++ ] );
                                size += valid;
                            }
                        }
                        else if( k == DOUBLE_INDRCT ) {
                            read_block( disk, inode -> f_block[ k++ ], dbuf );
                            l = 0;
                            while( l < n_indirects && valid ) {
                                read_block( disk, ( int ) dbuf[ l++ ], sbuf );
                                m = 0;
                                while( m < n_indirects && valid ) {
                                    valid = check_blocks( disk,
                                                          allocd_blocks,
                                                          sbuf[ m++ ] );
                                    size += valid;
                                }
                            }
                        }
                        else if( k == TRIPLE_INDRCT ) {
                            read_block( disk, inode -> f_block[ k++ ], tbuf );
                            l = 0;
                            while( l < n_indirects && valid ) {
                                read_block( disk, ( int ) tbuf[ l++ ], dbuf );
                                m = 0;
                                while( m < n_indirects && valid ) {
                                    read_block( disk,
                                                ( int ) dbuf[ m++ ],
                                                sbuf );
                                    n = 0;
                                    while( n < n_indirects && valid ) {
                                        valid = check_blocks( disk,
                                                              allocd_blocks,
                                                              sbuf[ n++ ] );
                                        size += valid;
                                    }
                                }
                            }
                        }
                    } // done scanning all data blocks in inode
                }
                else if( is_dir( inode -> f_acl ) ) {
                    int inode_num;
                    int parent_num;

                    parent_num = 0;
                    valid      = 1;

//                    TODO check correct parent inode number
                    // check first entry is own inode num, second is parent,
                    //  and there exists a path to it in the file system
                    if( readdir( inode ) -> inode_num != inode -> f_inode_num
                        || readdir( inode ) -> inode_num != parent_num
                        || ! count_links( disk, inode -> f_inode_num ) )
                        valid = 0;

                    // check for valid inodes in entries
                    while( ( inode_num = readdir( inode ) -> inode_num ) && valid ) {
                        valid = ( inode_num > 0
                                  && inode_num < superblock -> fs_num_inodes
                                  && get_inode( disk, inode_num ) -> f_links > 0 );
                    }
                }
            }
            // check file size matches # bytes in inode
            if( inode -> f_size != size )
                inode -> f_size = size;
        } // done scanning all inodes in block
    } // done scanning all inode blocks

    // check total inodes less than allowable amount in system
    if( superblock -> fs_num_used_inodes + num_free_inodes
        > superblock -> fs_num_inodes ) {
        perror( "Corrupt inodes in file system" );
        exit( -1 );
    }

    // check all blocks properly accounted for
    while( num_free_blocks < superblock -> fs_num_blocks
           && allocd_blocks[ num_free_blocks++ ] > 0 );
    if( superblock -> fs_num_used_blocks + num_free_blocks
        > superblock -> fs_num_blocks ) {
        perror( "Corrupt data blocks in file system" );
        exit( -1 );
    }
}


// TODO test function ( + recursive helper )
/**
 * Checks for inode number in directory tree
 *
 * @param  Disk *  disk           The disk containing the file system
 * @param  int     inode_num      The inode number to count links for
 * @param  int                    The number of links in tree
 */
int count_links( Disk * disk, int inode_num ) {
    int         links = 0;
    char        buf [ BLOCK_SIZE ];
    Inode    *  dir;

    // traverse tree starting at root
    read_block( disk, 1, ( char * ) buf );
    dir = ( Inode * ) buf;

//    TODO check recursion
    links += traverse_dir( disk, dir, inode_num );

    return links;
}


/**
 * Traverses directory to recursively search for inode number
 *
 * @param  Disk  * disk           The disk containing the file system
 * @param  Inode * dir            The directory to traverse
 * @param  int     inode_num      The inode number to count links for
 */
int traverse_dir( Disk * disk, Inode * dir, int inode_num ) {
    int     dir_inode_num;
    Inode * inode;

    while( ( dir_inode_num = readdir( dir ) -> inode_num ) ) {
        inode = get_inode( disk, dir_inode_num  );
        if( is_dir( inode -> f_acl ) )
            traverse_dir( disk, inode, inode_num );

        if( dir_inode_num == inode_num )
            return 1;
    }
    return 0;
}


// TODO test function
/**
 * Checks for valid block number and against list of allocated data blocks for
 *  duplicates, appends block number to allocated list if valid and not dup
 *
 * @param  Disk *  disk           The disk containing the file system
 * @param  int  *  allocd_blocks  The list of allocated data blocks
 * @param  int     block_num      The block to check against list
 * @return int                    block size upon success, 0 upon failure
 */
int check_blocks( Disk * disk, int * allocd_blocks, int block_num ) {
    char         buf [ BLOCK_SIZE ];
    char         bbuf[ BLOCK_SIZE ];
    Superblock * superblock;
    int          i     = 0;
    int          valid = 0;
    int          size  = 0;

    read_block( disk, 0, buf );
    superblock = ( Superblock * ) buf;

    // check block number is valid
    if( block_num > 0 || block_num < superblock -> fs_num_blocks ) {
        // check if data block is marked used in bitmap
        read_block( disk, block_num % superblock -> fs_num_block_groups, bbuf );
        valid = is_bit_set( ( Bitmap * ) bbuf, block_num );

        // loop until end of list or duplicate is found or add to list
        while( i < superblock -> fs_num_blocks - 1
               && allocd_blocks[ i ] > 0
               && ( valid = allocd_blocks[ i ] != block_num ) )
            i++;

        // add to list if not duplicate and calculate size
        if( valid ) {
            allocd_blocks[ i ] = block_num;
            read_block( disk, block_num, bbuf );

            // calculate size of block
//            TODO check properly detects end of file
            while( size < BLOCK_SIZE && bbuf[ size / sizeof( char ) ] > 0 )
                size += sizeof( char );
        }
    }
    // return 0 if invalid, else return size
    return valid * size;
}


/**
 * Returns the first free inode from the free inode list in the superblock
 *
 * @param Disk * disk    The disk that contains the file system
 */
Inode * find_free_inode( Disk * disk ) {
    int          i                = 0;
    int          free_inode_index = -1;
    Superblock * superblock       = new Superblock();

    read_block( disk, 0, ( char * ) superblock );

    // check if any inodes available for allocation
    if( superblock -> fs_num_used_inodes >= superblock -> fs_num_inodes ) {
        perror( "Not enough disk space to create file" );
        return NULL;
    }

    // loop through superblock free inode bitmap to find first non-negative
    while( i < SB_ILIST_SIZE && superblock -> free_inodes[ i ] < 0 )
        i++;

    if( i < SB_ILIST_SIZE ) {
        free_inode_index = superblock -> free_inodes[ i ];
        superblock -> free_inodes[ i ] = -1;
    }
    else {
        // fetch more inodes from the datablocks to update free list
        repopulate_ilist( disk, free_inode_index );
        find_free_inode( disk );
    }

    // save the superblock to disk with updated free ilist
    write_block( disk, 0, ( char * ) superblock );

    // otherwise, return that inode.
    return get_inode( disk, free_inode_index );
}


/**
* Scan inode blocks for more free inode numbers for free ilist
*
* @param Disk * disk         The disk that contains the file system
* @param int    inode_index  The
*/
void repopulate_ilist( Disk * disk, int inode_index ) {
    int           i;
    int           j;
    int           rel_inode_index;
    char          buf[ BLOCK_SIZE ];
    Superblock  * superblock  = new Superblock();
    Inode       * tmp         = new Inode();
    int           ilist_count = 0;

    read_block( disk, 0, ( char * ) superblock );

    int num_blocks       = superblock -> fs_disk_size / superblock -> fs_block_size;
    int num_inode_blocks = ( int ) ceil( num_blocks * INODE_BLOCKS );
    int inode_per_block  = ( int ) floor( superblock -> fs_block_size
                                          / superblock -> fs_inode_size );
    int starting_block   = inode_index / inode_per_block;

    for( i = starting_block; i <= num_inode_blocks; i++ ) {
        read_block( disk, i, ( char * ) buf );

        for( j = 0; j < inode_per_block; j++ ) {
            rel_inode_index = j % inode_per_block;
            std::memcpy( tmp,
                         ( ( Inode * ) buf ) + rel_inode_index,
                         sizeof( Inode ) );
            if( tmp -> f_links == 0 ) {
                superblock -> free_inodes[ ilist_count++ ] = tmp -> f_inode_num;
                if( ilist_count == SB_ILIST_SIZE ) {
                    write_block( disk, 0, ( char * ) superblock );
                    return;
                }
            }
        }
    }
}


/**
 * Returns a new allocated inode given first free inode number from find_free_inode
 *
 * @param  Disk * disk    The disk that contains the file system
 */
Inode * new_inode( Disk * disk ) {
    Inode * inode           = find_free_inode( disk );
    inode -> f_size         = 0;
    inode -> f_uid          = 0;            //through system call??
    inode -> f_gid          = 0;            //through system call??
    inode -> f_acl          = 0;            //through system call??
    inode -> f_ctime        = time( NULL );
    inode -> f_mtime        = time( NULL );
    inode -> f_atime        = time( NULL );
    inode -> f_links        = 0;            // set to 1 in mknod
    inode -> f_block[ 0 ]   = allocate_data_block( disk );
    return inode;
}


/**
 * Returns the Inode corresponding to the given inode index
 *
 * @param Disk * disk       The disk containing the file system
 * @param int    inode_num  The inode index to retrieve
*/
Inode * get_inode( Disk * disk, int inode_num ) {
    int           inodes_per_block;
    int           block_num;
    int           rel_inode_index;
    char          buf[ BLOCK_SIZE ];
    Superblock  * superblock;
    Inode       * ret_inode = new Inode();

    read_block( disk, 0, ( char * ) buf );
    superblock       = ( Superblock * ) buf;
    inodes_per_block = ( int ) floor( superblock -> fs_block_size
                                      / superblock -> fs_inode_size );
    block_num        = inode_num / inodes_per_block;
    rel_inode_index  = inode_num % inodes_per_block;

    read_block( disk, block_num, buf );
    Inode * block_inodes = ( Inode * ) buf;
    std::memcpy( ret_inode,
                 &( block_inodes[ rel_inode_index ] ),
                 sizeof( Inode ) );
    return ret_inode;
}


/**
 * Saves an Inode back to disk
 *
 * @param Disk  * disk    The disk containing the file system
 * @param Inode * inode   The inode to save
 */
int save_inode( Disk * disk, Inode * inode ) {
    int          block_num;
    int          inodes_per_block;
    int          inode_num;
    int          rel_inode_index;
    char         buf[ BLOCK_SIZE ];
    Superblock * superblock;

    // get data from superblock to calculate where inode should be
    read_block( disk, 0, ( char * ) buf );
    superblock       = ( Superblock * ) buf;
    inode_num        = inode -> f_inode_num;
    inodes_per_block = ( int ) floor( superblock -> fs_block_size
                                      / superblock -> fs_inode_size );
    block_num        = inode_num / inodes_per_block;
    rel_inode_index  = inode_num % inodes_per_block;

    // save the inode to disk
    read_block( disk, block_num, buf );
    std::memcpy( ( & ( ( Inode * ) buf )[ rel_inode_index ] ),
                 inode,
                 sizeof( Inode ) );
    write_block( disk, block_num, buf );
    return inode_num;
}


/**
 * Deallocates an inode and frees up all the resources owned by it
 *
 * @param Disk  *  disk   The disk containing the file system
 * @param Inode *  inode  The inode to deallocate
 */
void free_inode( Disk * disk, Inode * inode ) {
    char sbuf[ BLOCK_SIZE ];
    char dbuf[ BLOCK_SIZE ];
    char tbuf[ BLOCK_SIZE ];
    int  i;
    int  j;
    int  direct_blocks;
    int  done;
    int  num_blocks;
    int  n_indirects;

    num_blocks     = ( int ) ceil( inode -> f_size / BLOCK_SIZE );
    direct_blocks  = std::min( SINGLE_INDRCT, num_blocks );
    n_indirects    = BLOCK_SIZE / sizeof( int );

    // deallocate the direct blocks
    done = free_blocks_list( disk, ( int * ) inode -> f_block, direct_blocks );

    // deallocate the single indirect blocks
    if( ! done ) {
        // read in the block of redirects to buffer
        read_block( disk, inode -> f_block[ SINGLE_INDRCT ], sbuf );
        done = free_blocks_list( disk, ( int * ) sbuf, n_indirects );
    }

    // deallocate the double indirect blocks
    if( ! done ) {
        // read in the block of double redirects to buffer
        read_block( disk, inode -> f_block[ DOUBLE_INDRCT ], dbuf );

        for( i = 0; i < BLOCK_SIZE / sizeof( int ); i++ ) {
            read_block( disk, ( int ) dbuf[ i ], sbuf ); // single indirects
            done = free_blocks_list( disk, ( int * ) sbuf, n_indirects );
            if( done ) break;
        }
    }

    // deallocate the triple indirect blocks
    if( ! done ) {
        read_block( disk, inode -> f_block[ TRIPLE_INDRCT ], tbuf ); // triple

        for( i = 0; i < n_indirects; i++ ) {
            read_block( disk, tbuf[ i ], dbuf ); // double indirects

            for( j = 0; j < n_indirects; j++ ) {
                read_block( disk, dbuf[ i ], sbuf ); // single indirects
                done = free_blocks_list( disk, ( int * ) sbuf, n_indirects );
                if( done ) break;
            }
            if( done ) break;
        }
    }
    // try to put inode number on free list
    update_free_list( disk, inode -> f_inode_num );
}


// TODO test function
/**
 * Update free list with freed inode number, if space or higher inode numbers
 *
 * @param Disk  *  disk       The disk containing the file system
 * @param Inode *  inode_num  The inode number to try to add to free list
 */
void update_free_list( Disk * disk, int inode_num ) {
    char         buf[ BLOCK_SIZE ];
    Superblock * superblock;

    // start at second to last to leave last entry as starting index for repopulation
    int          i = SB_ILIST_SIZE - 2;

    read_block( disk, 0, buf );
    superblock = ( Superblock * ) buf;
    // scan free inode list backward for -1 or indices greater than inode's
    while( i >= 0 && superblock -> free_inodes[ i ] > 0 ) {
        // if entry is less than inode_num, replace w/ with inode num if empty
        //  or next entry if just lower inode number
        if( superblock -> free_inodes[ i ] < 0 ) {
            superblock -> free_inodes[ i ] = inode_num;
            i = 0;
        }
        else if( superblock -> free_inodes[ i ] < inode_num ) {
            superblock -> free_inodes[ i + 1 ] = inode_num;
            i = 0;
        }
        i--;
    }
    superblock -> fs_num_used_inodes--;
    // update superblock on disk
    write_block( disk, 0, ( char * ) superblock );
}


/**
 *  Given an array of `n` block numbers, deallocate each one.
 *
 *  @param Disk * disk          The disk containing the file system
 *  @param int  * block_list    The array of block numbers
 *  @param int    n             The number of block numbers in `list`
 */
int free_blocks_list( Disk * disk, int * block_list, int n ) {
    int block_num;
    int i = 0;

    while( i < n ) {
        block_num = block_list[ i++ ];
        if( block_num > 0 )  // block is allocated
            free_data_block( disk, block_num );
        else
            return 1;
    }
    return 0;
}


/**
 * Deallocates a data block
 *
 * @param Disk * disk         The disk containing the file system
 * @param int    block_index  The block number of the block to deallocate
 */
void free_data_block( Disk *disk, int block_index ) {
    char buf[ BLOCK_SIZE ];
    int relative_index, block_group, offset;
    Bitmap * bm;
    Superblock * superblock = new Superblock();

    // write zeroes to block
    write_block( disk, block_index, buf );

    // decrement number of used datablocks for the superblock
    read_block( disk, 0, ( char * ) superblock );
    superblock -> fs_num_used_blocks--;
    write_block( disk, 0, ( char * ) superblock );

    // calculate which block group this block is in
    relative_index = block_index - superblock -> first_data_block;
    block_group = relative_index / BLOCK_SIZE;
    offset = relative_index % BLOCK_SIZE;

    // mark the block as unused in its block group leader
    read_block( disk,
                superblock -> first_data_block + block_group * BLOCK_SIZE,
                buf );
    bm = init_bitmap( BLOCK_SIZE, buf );
    unset_bit( bm, offset );
    write_block( disk,
                 superblock -> first_data_block + block_group * BLOCK_SIZE,
                 buf );
}


/**
 * Allocates data block from free data list
 *  Returns integer corresponding to block number of allocated data block
 *  Returns -1 if there are no blocks available
 *
 * @param Disk * disk    The disk containing the file system
 */
int allocate_data_block( Disk * disk ) {
    char         buf[ BLOCK_SIZE ];
    char         sbbuf[ BLOCK_SIZE ];
    int          i;
    int          bitmap_index;
    int          block_num;
    Superblock * superblock;

    read_block( disk, 0, ( char * ) sbbuf );
    superblock = ( Superblock * ) sbbuf;

    for( i = 0; i < superblock -> fs_num_block_groups; i++ ) {
        // block num for block group free list
        block_num = superblock -> first_data_block + i * BLOCK_SIZE;
        read_block( disk, block_num, buf );
        Bitmap * bitmap = init_bitmap( superblock -> fs_block_size, buf );

        // if there is a free block in this block group
        if( ( bitmap_index = first_unset_bit( bitmap ) ) != -1 ) {
            // mark the data block as not free
            set_bit( bitmap, bitmap_index );
            write_block( disk, block_num, buf );
            return block_num + bitmap_index; // block num for free block
        }
    }
    return -1;    // no blocks available
}


// TODO test function
int is_file( short acl ) {
    return ( ( acl << 13 ) & 0 );
}


// TODO test function
int is_dir( short acl ) {
    return ( ( acl << 13 ) & 1 );
}
