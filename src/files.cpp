/**
 * files.cpp
 */

#include "files.hpp"
#include <cstring>

/**
 * Creates the primordial directory for the file system (i.e. root "/")
 *  The root of the file system will always be inode #0.
 *
 *  @param Disk * disk   The disk containing the file system
 */
void gros_mkroot( Disk * disk ) {
    Inode    * root_i;
    DirEntry   root[ 2 ];

    // get the zero-th inode to store the root dir in
    root_i          = gros_new_inode( disk ); // should be inode 0
    root_i->f_acl   = 0x3ed; // 01 111 100 100
    root_i->f_links = 2;

    root[ 0 ].inode_num = root_i->f_inode_num;
    strncpy( root[ 0 ].filename, ".", strlen( "." ) + 1 );

    root[ 1 ].inode_num = root_i->f_inode_num;
    strncpy( root[ 1 ].filename, "..", strlen( "." ) + 1 );

    gros_save_inode( disk, root_i );
    gros_i_write( disk, root_i, ( char * ) &( root[ 0 ] ), sizeof( DirEntry ), 0 );
    gros_i_write( disk, root_i, ( char * ) &( root[ 1 ] ), sizeof( DirEntry ),
                  sizeof( DirEntry ) );

    delete root_i;
}


/**
 * Returns the inode number of the file corresponding to the given path
 *
 * @param Disk * disk  Disk containing the file system
 * @param char * path  Path to the file, starting from root "/"
 */
int gros_namei( Disk * disk, const char * path ) {
    char     * filename;
    Inode    * dir;
    DirEntry * direntry = NULL;
    DirEntry * tmp = NULL;

    // check for valid path
    if ( ! strcmp( path, "/" ) || ! strcmp( path, "" ) )
        return 0;

    const char * target = strrchr( path, '/' ) + 1; // get filename this way

    // start at root
    dir      = gros_get_inode( disk, 0 );
    // parse name from path
    filename = strdup( path );
    filename = strtok( filename, "/" );

    gros_readdir_r( disk, dir, NULL, &direntry);
    // traverse directory for filename in path
    while( direntry != NULL && filename ) {
        gros_readdir_r( disk, dir, direntry, &tmp);
        if( ! strcmp( direntry -> filename, filename ) ) {
            dir      = gros_get_inode( disk, direntry->inode_num );
            filename = strtok( NULL, "/" );
            if (filename) {
                gros_readdir_r( disk, dir, NULL, &direntry );
            }
        } else {
            direntry = tmp;
        }
    }
    if ( ! direntry || ! strcmp( direntry->filename, target ) )
        return dir->f_inode_num;
    else
        return -1;
}


/**
 * Reads `size` bytes at `offset` offset bytes from file corresponding
 *  to given Inode on the given disk into given buffer
 *
 * @param Disk  * disk     Disk containing the file system
 * @param Inode * inode    Inode corresponding to the file to read
 * @param char  * buf      Buffer to read into (must be allocated to at
 *                          least `size` bytes)
 * @param int     size     Number of bytes to read
 * @param int     offset   Offset into the file to start reading from
 */
int gros_i_read( Disk * disk, Inode * inode, char * buf, int size, int offset ) {
    char         data[ BLOCK_SIZE ]; /* buffer to gros_read file contents into */
    int          block_size;         /* fs block size */
    int          file_size;          /* file size, i.e. inode->f_size */
    int          n_indirects;        /* how many ints can fit in a block */
    int          n_indirects_sq;     /* n_indirects * n_indirects */
    int          cur_block;          /* current block (relative to file) to gros_read */
    int          block_to_read;      /* current block (relative to fs) to gros_read */
    int          bytes_to_read;      /* bytes to gros_read from cur_block */
    int          bytes_read = 0;     /* number of bytes already gros_read into buf */
    int          cur_si     = -1;    /* id of blocks last gros_read into siblock */
    int          cur_di     = -1;    /* id of blocks last gros_read into diblock */
    int          si         = -1;    /* id of siblock that we need */
    int          di         = -1;    /* id of diblock that we need */
    int        * siblock = NULL;     /* buffer to store indirects */
    int        * diblock = NULL;     /* buffer to store indirects */
    int        * tiblock = NULL;     /* buffer to store indirects */
    Superblock * superblock = new Superblock(); /* reference to a superblock */
    int          is_first;

    file_size = inode->f_size;
    // by default, the double indirect block we gros_read from is the one given in
    // the inode. this will change if we are in the triple indirect block
    di = inode->f_block[ DOUBLE_INDRCT ];
    // by default, the single indirect block we gros_read from is the one given in
    // the inode. this will change if we are in the double indirect block
    si = inode->f_block[ SINGLE_INDRCT ];

    // if we don't have to read, don't gros_read. ¯\_(ツ)_/¯
    if( size <= 0 || offset >= file_size )
        return 0;

    // get the superblock so we can get the data we need about the file system
    gros_read_block( disk, 0, ( char * ) superblock );
    block_size      = superblock->fs_block_size;
    // the number of indirects a block can have
    n_indirects     = block_size / sizeof( int );
    n_indirects_sq  = n_indirects * n_indirects;
    // this is the file's n-th block that we will gros_read
    cur_block       = offset / block_size;
    is_first        = 1;

    // while we have more bytes in the file to read and have not gros_read the
    // requested amount of bytes
    while( ( offset + bytes_read ) < file_size && bytes_read < size ) {
        // for this block, we either finish reading or gros_read the entire block
        bytes_to_read = std::min( size - bytes_read, block_size );
        if ( is_first == 1 ) {
            bytes_to_read = std::min( size, block_size - (offset % block_size) );
        }
        // tmp var to store index into indirect blocks if necessary
        block_to_read = cur_block;

        // in a triple indirect block
        if( block_to_read >= ( n_indirects_sq + SINGLE_INDRCT ) ) {
            // if we haven't fetched the triple indirect block yet, do so now
            if( tiblock == NULL ) {
                tiblock = new int[ n_indirects ];
                gros_read_block( disk,
                                 inode->f_block[ TRIPLE_INDRCT ],
                                 ( char * ) tiblock );
            }

            // subtracting n^2+n+12 to obviate lower layers of indirection
            int pos =
                    ( block_to_read - ( n_indirects_sq
                                        + n_indirects
                                        + SINGLE_INDRCT ) )
                    / n_indirects_sq;
            // get the double indirect block that contains the block we need to gros_read
            di = tiblock[ pos ];
            // since we're moving onto double indirect addressing, subtract all
            // triple indirect related index information
            block_to_read -= pos * n_indirects_sq; /* still >= n+12 */
        }

        // in a double indirect block
        if( block_to_read >= ( n_indirects + SINGLE_INDRCT ) ) {
            diblock = diblock == NULL ? ( new int[ n_indirects ] ) : diblock;
            if( cur_di != di ) {
                cur_di = di;
                gros_read_block( disk, di, ( char * ) diblock );
            }
            // subtracting n+12 to obviate lower layers of indirection
            int pos = ( block_to_read - ( n_indirects + SINGLE_INDRCT ) ) /
                      n_indirects;
            // get the single indirect block that contains the block we need to gros_read
            si = diblock[ pos ];
            // since we're moving onto single indirect addressing, subtract all
            // double indirect related index information
            block_to_read -= pos * n_indirects; /* still >= 12 */
        }

        // in a single indirect block
        if( block_to_read >= SINGLE_INDRCT ) {
            siblock = siblock == NULL ? ( new int[ n_indirects ] ) : siblock;
            // if we dont' already have the single indirects loaded into memory, load it
            if( cur_si != si ) {
                cur_si = si;
                gros_read_block( disk, si, ( char * ) siblock );
            }
            // relative index into single indirects
            block_to_read = siblock[ block_to_read - SINGLE_INDRCT ];
        }

        // in a direct block
        if( cur_block < SINGLE_INDRCT ) {
            block_to_read = inode->f_block[ block_to_read ];
        }

        gros_read_block( disk, block_to_read, data );
        if ( is_first == 1 ) {
            std::memcpy( buf, data + (offset % block_size), bytes_to_read );
            is_first = 0;
        } else {
            std::memcpy( buf + bytes_read, data, bytes_to_read );
        }
        bytes_read += bytes_to_read;

        cur_block++;
    }

    // free up the resources we allocated
    if( siblock != NULL ) delete[] siblock;
    if( diblock != NULL ) delete[] diblock;
    if( tiblock != NULL ) delete[] tiblock;

    return bytes_read;
}

int gros_read( Disk * disk, const char * path, char * buf, int size,
               int offset ) {
    return gros_i_read( disk, gros_get_inode( disk, gros_namei( disk, path ) ),
                        buf, size, offset );
}


/**
 * Writes `size` bytes (at `offset` bytes from 0) into file
 *  corresponding to given Inode on the given disk from given buffer
 *
 * @param Disk  *  disk     Disk containing the file system
 * @param Inode *  inode    Inode corresponding to the file to write to
 * @param char  *  buf      Buffer to write to file (must be at least
 *                           `size` bytes)
 * @param int      size     Number of bytes to write
 * @param int      offset   Offset into the file to start writing to
 */
int gros_i_write( Disk * disk, Inode * inode, char * buf, int size, int offset ) {
    char         data[ BLOCK_SIZE ];   /* buffer to gros_read/gros_write file contents into/from */
    int          block_size;           /* copy of the fs block size */
    int          file_size;            /* copy of the file size, i.e. inode->f_size */
    int          n_indirects;          /* how many ints can fit in a block */
    int          n_indirects_sq;       /* n_indirects * n_indirects */
    int          cur_block;            /* the current block (relative to file) to gros_write */
    int          block_to_write;       /* the current block (relative to fs) to gros_write */
    int          bytes_to_write;       /* bytes to gros_write into cur_block */
    int          bytes_written = 0;    /* number of bytes already written from buf */
    int          cur_si        = -1;
    int          cur_di        = -1;   /* ids of blocks last gros_read into siblock & diblock */
    int          si            = -1;
    int          di            = -1;   /* id of siblock and diblock that we need */
    int          si_index      = -1;   /* index into siblock for data */
    int          di_index      = -1;   /* index into diblock for siblock */
    int          ti_index      = -1;   /* index into tiblock for diblock */
    int        * siblock = NULL;
    int        * diblock = NULL;
    int        * tiblock = NULL;       /* buffers to store indirects */
    Superblock * superblock = new Superblock(); /* reference to a superblock */
    Inode      * tosave;
    int          is_first;
    int          min_size;

    file_size = inode->f_size;
    // by default, the double indirect block we gros_write to is the one given in the
    // inode. this will change if we are in the triple indirect block
    di = inode->f_block[ DOUBLE_INDRCT ];
    // by default, the single indirect block we gros_write to is the one given in the
    // inode. this will change if we are in the double indirect block
    si = inode->f_block[ SINGLE_INDRCT ];

    // if we don't have to write, don't gros_write. ¯\_(ツ)_/¯
    if( size <= 0 ) return 0;

    // if we're writing to some offset, make sure it's that size
    gros_i_ensure_size(disk, inode, offset);

    // get the superblock so we can get the data we need about the file system
    gros_read_block( disk, 0, ( char * ) superblock );
    block_size     = superblock->fs_block_size;

    // the number of indirects a block can have
    n_indirects    = block_size / sizeof( int );
    n_indirects_sq = n_indirects * n_indirects;

    // this is the file's n-th block that we will gros_write to
    cur_block      = offset / block_size;
    is_first       = 1;

    // while we have more bytes to gros_write
    while( bytes_written < size ) {
        // for this block, we either finish writing or gros_write an entire block
        bytes_to_write = std::min( size - bytes_written, block_size );
        if ( is_first == 1) {
            bytes_to_write = std::min( size, block_size - (offset % block_size) );
        }
        // tmp var to store index into indirect blocks if necessary
        block_to_write = cur_block;

        // in a triple indirect block
        if( block_to_write >= ( n_indirects_sq + SINGLE_INDRCT ) ) {
            // if we haven't fetched the triple indirect block yet, do so now
            if( tiblock == NULL ) {
                tiblock = new int[ n_indirects ];
                // make sure all blocks before triple indirect block
                // are filled/allocated
                gros_i_ensure_size(
                        disk, inode,
                        ( n_indirects_sq + n_indirects + SINGLE_INDRCT ) * block_size
                );
                // if there is no triple indirect block yet, we need to allocate one
                if( inode->f_block[ TRIPLE_INDRCT ] == -1 ) {
                    inode->f_block[ TRIPLE_INDRCT ] = gros_allocate_data_block(
                            disk );
                    gros_save_inode( disk, inode );
                }
                // gros_read the triple indirect block into tiblock
                gros_read_block( disk, inode->f_block[ TRIPLE_INDRCT ],
                                 ( char * ) tiblock );
            }

            // subtracting n^2+n+12 to obviate lower layers of indirection
            ti_index = ( block_to_write
                         - ( n_indirects_sq + n_indirects + SINGLE_INDRCT ) )
                       / n_indirects_sq;
            // get the double indirect block that contains the block we need to gros_write to
            di = tiblock[ ti_index ];
            // since we're moving onto double indirect addressing, subtract all
            // triple indirect related index information
            block_to_write -= ti_index * n_indirects_sq; /* still >= n+12 */
        }

        // in a double indirect block
        if( block_to_write >= ( n_indirects + SINGLE_INDRCT ) ) {
            // if we're in the inode's double indirect block and it hasn't been allocated
            if( ti_index == -1 && di == -1 ) {
                // make sure all blocks before double indirect block
                // are filled/allocated
                gros_i_ensure_size(
                        disk, inode,
                        ( n_indirects + SINGLE_INDRCT ) * block_size
                );
                // allocate a data block and save the inode
                inode->f_block[ DOUBLE_INDRCT ] = gros_allocate_data_block( disk );
                di = inode->f_block[ DOUBLE_INDRCT ];
                gros_save_inode( disk, inode );
            }
            else if( ti_index != -1 && di == -1 ) {
                // if we're in a double indirect block from a triple indirect, but it
                // hasn't been allocated yet
                // make sure all blocks before <this> double indirect block
                // are filled/allocated
                gros_i_ensure_size( disk, inode,
                        // number of data blocks passed in triple indirects plus
                        // all inode double indirects plus
                        // all inode single indirects plus 12 (direct)
                                    ( ti_index * n_indirects_sq + n_indirects_sq
                                      + n_indirects + SINGLE_INDRCT )
                                    * block_size );
                // allocate a double indirect block and save the tiblock
                tiblock[ ti_index ] = gros_allocate_data_block( disk );
                di = tiblock[ ti_index ];
                gros_write_block( disk, inode->f_block[ TRIPLE_INDRCT ],
                                  ( char * ) tiblock );
            }

            diblock = diblock == NULL ? ( new int[ n_indirects ] ) : diblock;
            if( cur_di != di ) {
                cur_di = di;
                gros_read_block( disk, di, ( char * ) diblock );
            }
            // subtracting n+12 to obviate lower layers of indirection
            di_index = ( block_to_write - ( n_indirects + SINGLE_INDRCT ) )
                       / n_indirects;
            // get the single indirect block that contains the block we need to gros_read
            si = diblock[ di_index ];
            // since we're moving onto single indirect addressing, subtract all
            // double indirect related index information
            block_to_write -= di_index * n_indirects; /* still >= 12 */
        }

        // in a single indirect block
        if( block_to_write >= SINGLE_INDRCT ) {
            // if we're in the inode's single indirect block and it hasn't been allocated
            if( ti_index == -1 && di_index == -1 && si == -1 ) {
                // make sure all blocks before single indirect block are filled/allocated
                gros_i_ensure_size( disk, inode, SINGLE_INDRCT * block_size );
                // allocate a data block and save the inode
                inode->f_block[ SINGLE_INDRCT ] = gros_allocate_data_block(
                        disk );
                si = inode->f_block[ SINGLE_INDRCT ];
                gros_save_inode( disk, inode );
            } else if( ti_index == -1 && di_index != -1 && si == -1 ) {
                // if we're in a single indirect block from the inode's double indirect,
                // but the single indirect block hasn't been allocated yet
                // make sure all blocks before <this> single indirect block
                // are filled/allocated
                gros_i_ensure_size(
                        disk, inode,
                        // number of data blocks passed in double indirects plus
                        // all inode single indirects plus 12 (direct)
                        ( di_index * n_indirects + n_indirects +
                          SINGLE_INDRCT ) * block_size
                );
                // allocate a single indirect block and save the diblock
                diblock[ di_index ] = gros_allocate_data_block( disk );
                si = diblock[ di_index ];
                gros_write_block( disk, inode->f_block[ DOUBLE_INDRCT ],
                                  ( char * ) diblock );
            }
            else if( ti_index != -1 && di_index != -1 && si == -1 ) {
                // if we're in a single indirect block from a double indirect (itself from
                // a triple indirect), but it hasn't been allocated yet
                // make sure all blocks before <this> single indirect block
                // are filled/allocated
                gros_i_ensure_size(
                        disk, inode,
                        ( // number of data blocks passed in triple indirects plus
                                ti_index * n_indirects_sq +
                                // number of data blocks passed in current double indirect plus
                                di_index * n_indirects +
                                // all inode double indirects plus
                                n_indirects_sq +
                                // all inode single indirects plus 12 (direct)
                                n_indirects + SINGLE_INDRCT
                        ) * block_size
                );
                // allocate a single indirect block and save the diblock
                diblock[ di_index ] = gros_allocate_data_block( disk );
                si = diblock[ di_index ];
                gros_write_block( disk, di, ( char * ) diblock );
            }
            siblock = siblock == NULL ? ( new int[ n_indirects ] ) : siblock;
            // if we dont' already have the single indirects loaded into memory, load it
            if( cur_si != si ) {
                cur_si = si;
                gros_read_block( disk, si, ( char * ) siblock );
            }
            // relative index into single indirects
            si_index = block_to_write - SINGLE_INDRCT;
            block_to_write = siblock[ si_index ];
        }
        /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
         * since ti_index, di_index, si_index, inode->f_block[13], inode->f_block[12]
         * all start as -1 when unallocated, using max lets us do this calculation
         * for size and include them *only* if they have been set.
         *
         * we use min for cur_block and 12 because cur_block might be less than 12,
         * in which case we the rest of the calculation will evaluate to zeroes and
         * we don't need to be writing past the direct blocks.
         * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
         min_size = ( // number of data blocks passed in triple indirects plus
                        std::max( ti_index, 0 ) * n_indirects_sq +
                        // number of data blocks passed in current double indirect plus
                        std::max( di_index, 0 ) * n_indirects +
                        // number of data blocks passed in current single indirect plus
                        std::max( si_index, 0 ) +
                        // all inode double indirects plus
                        std::max( inode->f_block[ DOUBLE_INDRCT ], 0 ) *
                        n_indirects_sq +
                        // all inode single indirects plus
                        std::max( inode->f_block[ SINGLE_INDRCT ], 0 ) *
                        n_indirects +
                        // direct blocks
                        std::min<int>( std::max( cur_block, 0), SINGLE_INDRCT )
                ) * block_size;
        gros_i_ensure_size( disk, inode, min_size );
        tosave = gros_get_inode(disk, inode->f_inode_num);

        // we're in a single indirect block and the data block hasn't been allocated
        if( si_index != -1 && block_to_write == -1 ) {
            siblock[ si_index ] = gros_allocate_data_block( disk );
            block_to_write = siblock[ si_index ];
            gros_write_block( disk, si, ( char * ) siblock );
        }

        // in a direct block
        if( cur_block < SINGLE_INDRCT ) {
            block_to_write = inode->f_block[ cur_block ];
            if( tosave->f_block[ cur_block ] == -1 ) {
                tosave->f_block[ cur_block ] = gros_allocate_data_block( disk );
                block_to_write = tosave->f_block[ cur_block ];
            }
        }

        // if we are writing an entire block, we don't need to gros_read, since we're
        // overwriting it. Otherwise, we need to save what we're not writing over
        if( bytes_to_write < block_size )
            gros_read_block( disk, block_to_write, data );

        if ( is_first == 1 ) {
            std::memcpy( data + (offset % block_size), buf, bytes_to_write );
        } else {
            std::memcpy( data, buf + bytes_written, bytes_to_write );
        }
        gros_write_block( disk, block_to_write, data );
        bytes_written += bytes_to_write;

        tosave->f_size = std::max(
                // if we didn't gros_write to the end of the file
                tosave->f_size,
                // if we wrote past the end of the file
                min_size + bytes_to_write +
                (is_first == 1 ? (offset % block_size) : 0)
        );
        inode->f_size = tosave->f_size;
        gros_save_inode( disk, tosave );
        is_first = 0;
        cur_block++;
    }

    // free up the resources we allocated
    if( siblock != NULL ) delete [] siblock;
    if( diblock != NULL ) delete [] diblock;
    if( tiblock != NULL ) delete [] tiblock;

    return bytes_written;
}


int gros_write( Disk * disk, const char * path, char * buf, int size,
                int offset ) {
    return gros_i_write( disk, gros_get_inode( disk, gros_namei( disk, path ) ),
                         buf, size, offset );
}


/**
 * Ensures that a file is at least `size` bytes long. If it is already
 *  `size` bytes, nothing happens. Otherwise, the file is allocated
 *  data blocks as necessary and zero-filled to be `size` bytes long.
 *
 * @param Disk*  disk     Disk containing the file system
 * @param Inode* inode    Inode corresponding to the file to resize
 * @param int    size     Desired file size
 */
int gros_i_ensure_size( Disk * disk, Inode * inode, int size ) {
    int          file_size;         /* file size, i.e. inode->f_size */
    int          offset;            /* where to start allocating from */
    int          bytes_to_allocate; /* bytes to read from cur_block */
    char       * wrdata;            /* zero-filled data to write into file */

    file_size = inode->f_size;

    // if we don't have to extend, don't extend. ¯\_(ツ)_/¯
    if( file_size >= size )
      return 0;

    bytes_to_allocate = size - file_size;
    wrdata = ( char * ) calloc( bytes_to_allocate, sizeof( char ) );
    offset = file_size;
    gros_i_write( disk, inode, wrdata, bytes_to_allocate, offset );

    return bytes_to_allocate;
}

int gros_ensure_size( Disk * disk, char * path, int size ) {
    return gros_i_ensure_size( disk,
                               gros_get_inode( disk, gros_namei( disk, path ) ),
                               size );
}


/**
 * Creates a file from free inode, adds entry to parent directory
 *
 * @param  Disk  *  disk      Disk containing the file system
 * @param  Inode *  inode     Inode of parent directory
 * @param  char  *  filename  Name of new file
 * @return int                Inode number of new file
 */
int gros_i_mknod( Disk * disk, Inode * pdir, const char * filename ) {
    DirEntry * direntry = new DirEntry();
    Inode    * new_file = gros_new_inode( disk );
    int        status   = 0;
    if (!new_file)
    	return -1;

    new_file->f_links   = 1;
    direntry->inode_num = new_file->f_inode_num;
    strncpy( direntry->filename, filename, strlen( filename ) + 1 );

    if ( gros_save_inode( disk, new_file ) < 1 ) {
        gros_free_inode( disk, new_file );
        return -1;
    }
    gros_i_write( disk, pdir, ( char * ) direntry, sizeof( DirEntry ),
                  pdir->f_size );

    delete direntry;
    if (status != 0) return status;
    status = new_file->f_inode_num;
    delete new_file;
    return status;
}


int gros_mknod( Disk * disk, const char * path ) {
    char        * new_path = new char[ strlen( path ) + 1 ];
    const char  * file     = strrchr( path, '/' ); // get filename this way

    // invalid path
    if( ! file ) return -ENOENT;

    strncpy( new_path, path, strlen( path ) - strlen( file ) );
    file += 1; // skip over delimiter
    Inode * inode = gros_get_inode( disk, gros_namei( disk, new_path ) );

    delete [] new_path;
    return gros_i_mknod( disk, inode, file );
}


/**
 * Creates a directory with two entries (. and ..)
 *
 * @param Disk  * disk     Disk containing the file system
 * @param Inode * inode    Inode of directory in which to place new directory
 * @param char  * dirname  Name of new directory
 * @return int    status   0 on success, -1 on failure
 */
int gros_i_mkdir( Disk * disk, Inode * inode, const char * dirname ) {
    DirEntry   entries[ 2 ];
    DirEntry * direntry = new DirEntry();
    Inode    * new_dir  = gros_new_inode( disk );
    int        status   = 0;
    if (!new_dir)
    	return -1;

    direntry->inode_num = new_dir->f_inode_num;
    strcpy( direntry->filename, dirname );

    entries[ 0 ].inode_num = new_dir->f_inode_num;
    strcpy( entries[ 0 ].filename, "." );
    entries[ 1 ].inode_num = inode->f_inode_num;
    strcpy( entries[ 1 ].filename, ".." );

    new_dir->f_links = 2;
    new_dir->f_acl   = 0x3ed;
    inode->f_links  += 1;

    // save directories back to disk
    gros_save_inode( disk, inode ) < 0 ? status = -1 : status;
    if (gros_save_inode( disk, new_dir ) < 0) {
        gros_free_inode( disk, new_dir );
        return -1;
    }

    // add new direntry to current directory
    gros_i_write( disk, inode, ( char * ) direntry, sizeof( DirEntry ),
                  inode->f_size );

    // add first entries to new directory
    gros_i_write( disk, new_dir, ( char * ) entries, 2 * sizeof( DirEntry ), 0 );

    delete direntry;
    status = new_dir->f_inode_num;
    delete new_dir;
    return status;
}


int gros_mkdir( Disk * disk, const char * path ) {
    char       * new_path = new char[ strlen( path ) + 1 ];
    const char * dir      = strrchr( path, '/' ); // get filename

    // invalid path
    if( ! dir ) return -ENOENT;

    dir += 1;  // skip over delimiter

    strcpy( new_path, path );
    // add null terminator to end of filename
    new_path[ ( int ) ( dir - path ) - 1 ] = '\0';
    Inode * inode = gros_get_inode( disk, gros_namei( disk, new_path ) );

    delete [] new_path;
    return gros_i_mkdir( disk, inode, dir );
}


/**
* Removes a directory and decrements all link counts for all files in directory
*  If files then have 0 links, those files will be deleted/freed.
*  TODO: Consider forcing vs not forcing recursive delete (error under latter)
*
* @param  Disk  *  disk     Disk containing the file system
* @param  Inode *  pdir     Inode of parent directory
* @param  Inode *  dir      Name of directory to delete
* @return int      status   0 upon success, negative on failure
*/
int gros_i_rmdir( Disk * disk, Inode * pdir, Inode * dir ) {
    DirEntry * result;
    Inode    * child_inode;
    int        status           = 1;
    int        direntry_counter = 0;
    int        direntry_size    = sizeof( DirEntry );
    char       buf[ direntry_size ];

    // check first 2 entries are ".", ".."
    if( gros_readdir_r( disk, dir, NULL, &result )
        || gros_readdir_r( disk, dir, result, &result ) )
        return -EINVAL;
    gros_readdir_r( disk, dir, result, &result );

    // unlink entries, recursively remove nested directories
    while( result ) {
        if( gros_is_dir( gros_get_inode( disk, result->inode_num )->f_acl ) ) {
//            return -ENOTEMPTY;
            child_inode = gros_get_inode( disk, result->inode_num );
            gros_i_rmdir( disk, dir, child_inode );
        }
        else
            gros_i_unlink( disk, dir, result->filename );
        gros_readdir_r( disk, dir, result, &result );
    }

    // remove entry from parent directory
    if( gros_readdir_r( disk, pdir, NULL, &result )
        || gros_readdir_r( disk, pdir, result, &result ) )
        status = -EINVAL;
    gros_readdir_r( disk, pdir, result, &result );

    while( result && status ) {
        // loop until dir entry, replace with last entry in parent directory
        if( result->inode_num == dir->f_inode_num ) {
            gros_i_read( disk, pdir, buf, direntry_size,
                         pdir->f_size - direntry_size );
            gros_i_write( disk, pdir, buf, direntry_size,
                          direntry_counter * direntry_size );
            status = 0;
        }
        direntry_counter++;
        gros_readdir_r( disk, pdir, result, &result );
    }
    gros_i_truncate( disk, pdir, pdir->f_size - direntry_size );
    gros_free_inode( disk, dir );

    status ? status = -EINVAL : status;
    return status;
}


// TODO check for correct filename passed into fn
int gros_rmdir( Disk * disk, const char * path ) {
    const char * dirname     = strrchr( path, '/' ) + 1;
    int          length      = ( int ) ( strlen( path ) - strlen( dirname ) );
    char       * parent_path = new char[ length + 1 ];
    int          parent_num;
    int          inode_num;

    if( ( inode_num = gros_namei( disk, path ) ) < 0 )
        return 0;

    strncpy( parent_path, path, ( size_t ) length );
    parent_path[ length - 1 ] = '\0';

    if( ( parent_num = gros_namei( disk, parent_path ) ) < 0 )
        return 0;

/*    strncpy( parent_path, path, ( size_t ) length + 1 );
    if( ( inode_num = gros_namei( disk, path ) ) < 1
        || ( parent_num = gros_namei( disk, parent_path ) ) < 0 )
        return -ENOENT;*/

    delete [] parent_path;
    return gros_i_rmdir( disk, gros_get_inode( disk, parent_num ),
                         gros_get_inode( disk, inode_num ) );
}


/**
* Removes a file or directory. If the file is a directory, calls rmdir.
*
* @param Disk  *  disk       Disk containing the file system
* @param Inode *  inode      Inode of directory containing file to delete
* @param char  *  filename   Name of file to delete
*/
int gros_i_unlink( Disk * disk, Inode * inode, const char * filename ) {
    DirEntry * result;
    Inode    * child_inode;
    int        direntry_counter = 0;
    int        status           = -1;
    int        direntry_size    = sizeof( DirEntry );
    char       buf[ direntry_size ];

    gros_readdir_r( disk, inode, NULL, &result );
    while( result && status < 0 ) {
        if( strcmp( result->filename, filename ) == 0 ) {
            child_inode = gros_get_inode( disk, result->inode_num );
            child_inode->f_links--;

            if( child_inode->f_links == 0 ) {
                gros_free_inode( disk, child_inode );
            }
            gros_i_read( disk, inode, buf, direntry_size,
                         inode->f_size - direntry_size );
            gros_i_write( disk, inode, buf, direntry_size,
                          direntry_counter * direntry_size );
            status = 0;
        }
        direntry_counter++;
        gros_readdir_r( disk, inode, result, &result );
    }
    gros_i_truncate( disk, inode, inode->f_size - direntry_size );

    return status;
}


int gros_unlink( Disk * disk, const char * path ) {
    const char * filename    = strrchr( path, '/' ) + 1;
    int          length      = ( int ) ( strlen( path ) - strlen( filename ) );
    char       * parent_path = new char[ length + 1 ];
    int          parent_num;

    strncpy( parent_path, path, ( size_t ) length + 1 );
    parent_num = gros_namei( disk, parent_path );

    delete [] parent_path;
    return gros_i_unlink( disk, gros_get_inode( disk, parent_num ), filename );
}


int gros_frename( Disk * disk, const char * from, const char * to ) {
    gros_copy( disk, from, to );
    return gros_unlink( disk, from );
}


/**
* Truncates or extends the file to a specified length
*
* @param Disk  *  disk     Disk containing the file system
* @param Inode *  inode    Inode corresponding to the file to resize
* @param int      size     Desired file size
*/
int gros_i_truncate( Disk * disk, Inode * inode, int size ) {
    char         data[ BLOCK_SIZE ]; /* buffer to read file contents into */
    int          block_size;         /* fs block size */
    int          file_size;          /* file size, i.e. inode->f_size */
    int          n_indirects;        /* how many ints can fit in a block */
    int          n_indirects_sq;     /* n_indirects * n_indirects */
    int          cur_block;          /* current block (relative to file) to free */
    int          block_to_free;      /* current block (relative to fs) to free */
    int          bytes_to_dealloc;   /* bytes to free from cur_block */
    int          tmp;
    int          offset;
    int          done         = 0;   /* whether we are done truncating */
    int          last_of_file = 1;   /* whether this is last block of file */
    int          cur_si       = -1;  /* id of blocks last read into siblock */
    int          cur_di       = -1;  /* id of blocks last read into diblock */
    int          si           = -1;  /* id of siblock that we need */
    int          di           = -1;  /* id of diblock that we need */
    int          si_index     = -1;  /* index into siblock for data */
    int          di_index     = -1;  /* index into diblock for siblock */
    int          ti_index     = -1;  /* index into tiblock for diblock */
    int        * siblock = NULL;     /* buffer to store indirects */
    int        * diblock = NULL;     /* buffer to store indirects */
    int        * tiblock = NULL;     /* buffer to store indirects */
    Superblock * superblock = new Superblock(); /* reference to a superblock */

    file_size = inode->f_size;
    // by default, the double indirect block we read from is the one given in
    // the inode. this will change if we are in the triple indirect block
    di = inode->f_block[ DOUBLE_INDRCT ];
    // by default, the single indirect block we read from is the one given in
    // the inode. this will change if we are in the double indirect block
    si = inode->f_block[ SINGLE_INDRCT ];

    // handles extending case
    gros_i_ensure_size( disk, inode, size );
    // if we the file is already `size`, then return
    if( file_size == size )
        return 0;
    offset = size;

    // get the superblock so we can get the data we need about the file system
    gros_read_block( disk, 0, (char*) superblock );
    block_size      = superblock->fs_block_size;
    // the number of indirects a block can have
    n_indirects     = block_size / sizeof( int );
    n_indirects_sq  = n_indirects * n_indirects;
    // this is the file's n-th block that we will free
    cur_block       = offset / block_size;

    // while we have more blocks to free
    while( !done ) {
        // tmp var to store index into indirect blocks if necessary
        block_to_free = cur_block;

        // in a triple indirect block
        if( block_to_free >= ( n_indirects_sq + SINGLE_INDRCT ) ) {
            // if we haven't fetched the triple indirect block yet, do so now
            if( tiblock == NULL ) {
                tiblock = new int[ n_indirects ];
                gros_read_block( disk,
                            inode->f_block[ TRIPLE_INDRCT ],
                            ( char * ) tiblock );
            }

            // subtracting n^2+n+12 to obviate lower layers of indirection
            ti_index =
                    ( block_to_free - ( n_indirects_sq
                                        + n_indirects
                                        + SINGLE_INDRCT ) )
                    / n_indirects_sq;
            // get the double indirect block that contains the block we need to free
            di = tiblock[ ti_index ];
            // since we're moving onto double indirect addressing, subtract all
            // triple indirect related index information
            block_to_free -= ti_index * n_indirects_sq; /* still >= n+12 */
        }

        // in a double indirect block
        if( block_to_free >= ( n_indirects + SINGLE_INDRCT ) ) {
            diblock = diblock == NULL ? ( new int[ n_indirects ] ) : diblock;
            if( cur_di != di ) {
                cur_di = di;
                gros_read_block( disk, di, ( char * ) diblock );
            }
            // subtracting n+12 to obviate lower layers of indirection
            di_index = ( block_to_free - ( n_indirects + SINGLE_INDRCT ) ) /
                      n_indirects;
            // get the single indirect block that contains the block we need to free
            si = diblock[ di_index ];
            // since we're moving onto single indirect addressing, subtract all
            // double indirect related index information
            block_to_free -= di_index * n_indirects; /* still >= 12 */
        }

        // in a single indirect block
        if( block_to_free >= SINGLE_INDRCT ) {
            siblock = siblock == NULL ? ( new int[ n_indirects ] ) : siblock;
            // if we dont' already have the single indirects loaded into memory, load it
            if( cur_si != si ) {
                cur_si = si;
                gros_read_block( disk, si, ( char * ) siblock );
            }
            // relative index into single indirects
            si_index = block_to_free - SINGLE_INDRCT;
            block_to_free = siblock[ si_index ];
        }

        // in a direct block
        if( cur_block < SINGLE_INDRCT ) {
            tmp = block_to_free;
            block_to_free = inode->f_block[ block_to_free ];
        }

        // if this block is not allocated, we've reached the end of the original
        // file's length and we can stop free-ing
        if( block_to_free == -1 ) {
            done = 1;
        }
        else if( last_of_file == 1 ) { // if this block contains the new end of file
            bytes_to_dealloc = block_size - ( inode->f_size % block_size );
            gros_read_block( disk, block_to_free, data );
            // set zeros from the new end of the file to the end of the block
            std::memset( data + ( inode->f_size % block_size ), 0,
                         bytes_to_dealloc );
            // save the block back
            gros_write_block( disk, block_to_free, data );
            last_of_file = 0;
        }
        else { // just free this block
            gros_free_data_block( disk, block_to_free );

            if( si_index != -1 ) {
                siblock[ si_index ] = -1;
                gros_write_block( disk, si, ( char * ) siblock );

                if( si_index == ( n_indirects - 1 ) && di_index != -1 ) {
                    gros_free_data_block( disk, diblock[ di_index ] );
                    diblock[ di_index ] = -1;
                    gros_write_block( disk, di, ( char * ) diblock );

                    if( di_index == ( n_indirects - 1 ) && ti_index != -1 ) {
                        gros_free_data_block( disk, diblock[ di_index ] );
                        tiblock[ ti_index ] = -1;
                        gros_write_block( disk, inode->f_block[ TRIPLE_INDRCT ],
                                          ( char * ) tiblock );
                        if( ti_index == ( n_indirects - 1 ) ) {
                            gros_free_data_block( disk,
                                                  inode->f_block[ TRIPLE_INDRCT ] );
                            inode->f_block[ TRIPLE_INDRCT ] = -1;
                        }
                    }
                    else if( di_index == ( n_indirects - 1 )
                             && ti_index == -1 ) {
                        gros_free_data_block( disk,
                                              inode->f_block[ DOUBLE_INDRCT ] );
                        inode->f_block[ DOUBLE_INDRCT ] = -1;
                    }
                }
                else if( si_index == ( n_indirects - 1 ) && di_index == -1 ) {
                    gros_free_data_block( disk, inode->f_block[ SINGLE_INDRCT ] );
                    inode->f_block[ SINGLE_INDRCT ] = -1;
                }
            }
            else { // si_index == -1
                inode->f_block[ tmp ] = -1;
            }
        }
        cur_block++;
    }

    // free up the resources we allocated
    if( siblock != NULL ) delete [] siblock;
    if( diblock != NULL ) delete [] diblock;
    if( tiblock != NULL ) delete [] tiblock;

    inode->f_size = size;
    gros_save_inode( disk, inode );

    return 0;
}


int gros_truncate( Disk * disk, const char * path, int size ) {
    return gros_i_truncate( disk,
                            gros_get_inode( disk, gros_namei( disk, path ) ),
                            size );
}


/**
 * Readdir_r takes an inode corresponding to a directory file, a pointer to the
 *  caller's "current" direntry, and returns the next direntry in the out parameter
 *  `result`. If `current` is NULL, then this returns the first direntry into
 *  `result`. If there are no more direntries, then `result` will be NULL.
 *
 * @param Disk      * disk     The disk containing the file system
 * @param Inode     * dir      Directory instance
 * @param DirEntry  * current  Where to start traversing the directory from
 * @param DirEntry ** result   Out parameter for resulting direntry
 *
 * @returns int       status   0 upon success
 */
int gros_readdir_r( Disk * disk, Inode * dir, DirEntry * current,
                    DirEntry ** result ) {
    int         status       = 1;
    int         offset       = 0;
    int         direntrysize = sizeof( DirEntry );
    DirEntry  * cur_de       = new DirEntry();
    DirEntry  * next_de      = new DirEntry();

    if( ! current ) {
        gros_i_read( disk, dir, ( char * ) cur_de, direntrysize, offset );
        * result = cur_de;
        status = 0;
    }

    // read DirEntries until current entry is found
    while( status && gros_i_read( disk, dir, ( char * ) cur_de, direntrysize, offset ) ) {
        offset += direntrysize;
        if( ! strcmp(cur_de->filename, current->filename) ) {
            status = 0;
            if( gros_i_read( disk, dir, ( char * ) next_de, direntrysize, offset ) )
                * result = next_de;
            else * result = NULL;
        }
    }
    return status;
}


DirEntry * gros_readdir( Disk * disk, Inode * dir ) {
    DirEntry * result = NULL;

    if( gros_is_dir( dir->f_acl ) )
        gros_readdir_r( disk, dir, NULL, &result );

    return result;
}


/**
* Copies a file from one directory to another, incrementing the number of links
*
* @param Disk  *  disk     Disk containing the file system
* @param Inode *  from     Inode corresponding to the file to copy
* @param Inode *  todir    Inode corresponding to destination directory
* @param const char * name Desired new name for file
*/
int gros_i_copy( Disk * disk, Inode * from, Inode * todir, const char * filename ) {
    int        status   = 0;
    DirEntry * direntry = new DirEntry();
    direntry->inode_num = from->f_inode_num;
    strcpy( direntry->filename, filename );

    gros_i_write( disk, todir, ( char * ) direntry, sizeof( DirEntry ),
                  todir->f_size );

    from->f_links += 1;
    gros_save_inode( disk, from ) < 0 ? status = -1 : status;

    delete direntry;
    return status;
}


/* @param char*  to     FULL path (from root "/") to the new copied file */
int gros_copy( Disk * disk, const char * from, const char * to ) {
    const char * filename = strrchr( to, '/' ) + 1;
    int          length   = ( int ) ( strlen( to ) - strlen( filename ) );
    char       * dirname  = new char[ length ];

    strncpy( dirname, to, ( size_t ) length );
    dirname[ length - 1 ] = '\0';

    int       from_inode_num = gros_namei( disk, from );
    int       to_dir         = gros_namei( disk, dirname );

    delete [] dirname;
    return gros_i_copy(
            disk,
            gros_get_inode( disk, from_inode_num ),
            gros_get_inode( disk, to_dir ),
            filename
    );
}


int gros_i_stat( Disk * disk, int inode_num, struct stat * stbuf ) {
    short                 ftyp, usr, grp, uni;
    Inode               * inode;
    inode = gros_get_inode( disk, inode_num );
    ftyp  = ( short ) ( ( inode->f_acl >> 9 ) & 0x7 );
    usr   = ( short ) ( ( inode->f_acl >> 6 ) & 0x7 );
    grp   = ( short ) ( ( inode->f_acl >> 3 ) & 0x7 );
    uni   = ( short ) ( inode->f_acl & 0x7 );

    switch ( ftyp ) {
        case 0: // regular file
            stbuf->st_mode = S_IFREG; break;
        case 1: // directory
            stbuf->st_mode = S_IFDIR; break;
        case 2: // device
            stbuf->st_mode = S_IFBLK; break;
        case 3: // symlink
            stbuf->st_mode = S_IFLNK; break;
        default:
            stbuf->st_mode = S_IFREG; break;
    }

    // user
    stbuf->st_mode = ( mode_t ) ( ( usr & 0x4 ) ? stbuf->st_mode | S_IRUSR : stbuf->st_mode );
    stbuf->st_mode = ( mode_t ) ( ( usr & 0x2 ) ? stbuf->st_mode | S_IWUSR : stbuf->st_mode );
    stbuf->st_mode = ( mode_t ) ( ( usr & 0x1 ) ? stbuf->st_mode | S_IXUSR : stbuf->st_mode );
    // group
    stbuf->st_mode = ( mode_t ) ( ( grp & 0x4 ) ? stbuf->st_mode | S_IRGRP : stbuf->st_mode );
    stbuf->st_mode = ( mode_t ) ( ( grp & 0x2 ) ? stbuf->st_mode | S_IWGRP : stbuf->st_mode );
    stbuf->st_mode = ( mode_t ) ( ( grp & 0x1 ) ? stbuf->st_mode | S_IXGRP : stbuf->st_mode );
    // universe
    stbuf->st_mode = ( mode_t ) ( ( uni & 0x4 ) ? stbuf->st_mode | S_IROTH : stbuf->st_mode );
    stbuf->st_mode = ( mode_t ) ( ( uni & 0x2 ) ? stbuf->st_mode | S_IWOTH : stbuf->st_mode );
    stbuf->st_mode = ( mode_t ) ( ( uni & 0x1 ) ? stbuf->st_mode | S_IXOTH : stbuf->st_mode );

    stbuf->st_dev  = 0; // not used
    stbuf->st_rdev = 0; // not used;
    stbuf->st_ino  = inode_num;
    stbuf->st_uid  = ( uid_t ) inode->f_uid;
    stbuf->st_gid  = ( gid_t ) inode->f_gid;

    pdebug << "setting timing stuff" << std::endl;
#if defined(__APPLE__) || defined(__MACH__)
    struct timespec a, m, c;

    a.tv_sec  = inode->f_atime / 1000;
    a.tv_nsec = ( inode->f_atime % 1000 ) * 1000000;
    m.tv_sec  = inode->f_mtime / 1000;
    m.tv_nsec = ( inode->f_mtime % 1000 ) * 1000000;
    c.tv_sec  = inode->f_ctime / 1000;
    c.tv_nsec = ( inode->f_ctime % 1000 ) * 1000000;

    stbuf->st_atimespec = a;
    stbuf->st_mtimespec = m;
    stbuf->st_ctimespec = c;
#else
    stbuf->st_atime = inode->f_atime;
        stbuf->st_mtime = inode->f_mtime;
        stbuf->st_ctime = inode->f_ctime;
#endif

    pdebug << "setting links and size stuff" << std::endl;
    stbuf->st_nlink   = ( nlink_t ) inode->f_links;
    stbuf->st_size    = inode->f_size;
    stbuf->st_blocks  = ( inode->f_size / BLOCK_SIZE ) + 1;
    stbuf->st_blksize = BLOCK_SIZE;

    pdebug << "returning" << std::endl;
    return 0;
}


int gros_i_chmod( Disk * disk, Inode * inode, mode_t mode ) {
    // keep file type in place
    inode->f_acl = ( short ) ( inode->f_acl & (0x7 << 9) );
    // user
    inode->f_acl = ( short ) ( ( mode & S_IRUSR) ? inode->f_acl | (1 << 8) : inode->f_acl );
    inode->f_acl = ( short ) ( ( mode & S_IWUSR) ? inode->f_acl | (1 << 7) : inode->f_acl );
    inode->f_acl = ( short ) ( ( mode & S_IXUSR) ? inode->f_acl | (1 << 6) : inode->f_acl );
    // group
    inode->f_acl = ( short ) ( ( mode & S_IRGRP) ? inode->f_acl | (1 << 5) : inode->f_acl );
    inode->f_acl = ( short ) ( ( mode & S_IWGRP) ? inode->f_acl | (1 << 4) : inode->f_acl );
    inode->f_acl = ( short ) ( ( mode & S_IXGRP) ? inode->f_acl | (1 << 3) : inode->f_acl );
    // universe
    inode->f_acl = ( short ) ( ( mode & S_IROTH) ? inode->f_acl | (1 << 2) : inode->f_acl );
    inode->f_acl = ( short ) ( ( mode & S_IWOTH) ? inode->f_acl | (1 << 1) : inode->f_acl );
    inode->f_acl = ( short ) ( ( mode & S_IXOTH) ? inode->f_acl | (1 << 0) : inode->f_acl );
    return 0;
}
