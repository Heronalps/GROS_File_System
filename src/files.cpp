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
    Inode *  root_i;
    DirEntry root[ 2 ];

    // get the zero-th inode to store the root dir in
    root_i          = gros_new_inode( disk ); // should be inode 0
    // !! root_i->f_inode_num == 0 !!
    root_i->f_acl   = 0x1ff; // 01 111 111 111
    root_i->f_links = 2;

    root[ 0 ].inode_num = root_i->f_inode_num;
    strcpy( root[ 0 ].filename, "." );

    root[ 1 ].inode_num = root_i->f_inode_num;
    strcpy( root[ 1 ].filename, ".." );

    gros_i_write( disk, root_i, ( char * ) root, 2 * sizeof( DirEntry ), 0 );
    gros_save_inode( disk, root_i );
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

    // start at root
    dir      = gros_get_inode( disk, 0 );
    // parse name from path
    filename = strdup( path );
    filename = strtok( filename, "/" );

    // travese directory for filename in path
    while( ! gros_readdir_r( disk, dir, direntry, &direntry ) && filename ) {
        if( ! strcmp( direntry -> filename, filename ) ) {
            dir      = gros_get_inode( disk, direntry->inode_num );
            filename = strtok( NULL, "/" );
        }
    }
    free( filename );
    if( ! dir -> f_inode_num )
        return -1;
    return dir -> f_inode_num;
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
    int        * siblock;            /* buffer to store indirects */
    int        * diblock;            /* buffer to store indirects */
    int        * tiblock;            /* buffer to store indirects */
    Superblock * superblock;         /* reference to a superblock */

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
    gros_read_block( disk, 0, data );
    superblock      = ( Superblock * ) data;
    block_size      = superblock->fs_block_size;
    // the number of indirects a block can have
    n_indirects     = block_size / sizeof( int );
    n_indirects_sq  = n_indirects * n_indirects;
    // this is the file's n-th block that we will gros_read
    cur_block       = offset / block_size;

    // while we have more bytes in the file to read and have not gros_read the
    // requested amount of bytes
    while( ( offset + bytes_read ) <= file_size && bytes_read <= size ) {
        // for this block, we either finish reading or gros_read the entire block
        bytes_to_read = std::min( size - bytes_read, block_size );
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
        std::memcpy( buf + bytes_read, data, bytes_to_read );
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
    int        * siblock;
    int        * diblock;
    int        * tiblock;              /* buffers to store indirects */
    Superblock * superblock;           /* reference to a superblock */

    file_size = inode->f_size;
    // by default, the double indirect block we gros_write to is the one given in the
    // inode. this will change if we are in the triple indirect block
    di = inode->f_block[ DOUBLE_INDRCT ];
    // by default, the single indirect block we gros_write to is the one given in the
    // inode. this will change if we are in the double indirect block
    si = inode->f_block[ SINGLE_INDRCT ];

    // if we don't have to write, don't gros_write. ¯\_(ツ)_/¯
    if( size <= 0 || offset >= file_size )
        return 0;

    // get the superblock so we can get the data we need about the file system
    gros_read_block( disk, 0, data );
    superblock     = ( Superblock * ) data;
    block_size     = superblock->fs_block_size;

    // the number of indirects a block can have
    n_indirects    = block_size / sizeof( int );
    n_indirects_sq = n_indirects * n_indirects;

    // this is the file's n-th block that we will gros_write to
    cur_block      = offset / block_size;

    // while we have more bytes to gros_write
    while( bytes_written <= size ) {
        // for this block, we either finish writing or gros_write an entire block
        bytes_to_write = std::min( size - bytes_written, block_size );
        // tmp var to store index into indirect blocks if necessary
        block_to_write = cur_block;

        // in a triple indirect block
        if( block_to_write >= ( n_indirects_sq + SINGLE_INDRCT ) ) {
            // if we haven't fetched the triple indirect block yet, do so now
            if( tiblock == NULL ) {
                tiblock = new int[n_indirects];
                // make sure all blocks before triple indirect block
                // are filled/allocated
                gros_i_ensure_size(
                        disk, inode,
                        ( n_indirects_sq + n_indirects + SINGLE_INDRCT ) *
                        block_size
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
                gros_i_ensure_size( disk, inode,
                                    ( n_indirects + SINGLE_INDRCT ) *
                                    block_size );
                // allocate a data block and save the inode
                inode->f_block[ DOUBLE_INDRCT ] = gros_allocate_data_block(
                        disk );
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
                                      + n_indirects + SINGLE_INDRCT ) *
                                    block_size );
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
        gros_i_ensure_size(
                disk, inode,
                ( // number of data blocks passed in triple indirects plus
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
                        std::min( cur_block, SINGLE_INDRCT )
                ) * block_size
        );

        // we're in a single indirect block and the data block hasn't been allocated
        if( si_index != -1 && block_to_write == -1 ) {
            siblock[ si_index ] = gros_allocate_data_block( disk );
            block_to_write = siblock[ si_index ];
            gros_write_block( disk, si, ( char * ) siblock );
        }

        // in a direct block
        if( cur_block < SINGLE_INDRCT ) {
            block_to_write = inode->f_block[ cur_block ];
            if( inode->f_block[ cur_block ] == -1 ) {
                inode->f_block[ cur_block ] = gros_allocate_data_block( disk );
                block_to_write = inode->f_block[ cur_block ];
            }
        }

        // if we are writing an entire block, we don't need to gros_read, since we're
        // overwriting it. Otherwise, we need to save what we're not writing over
        if( bytes_to_write < block_size )
            gros_read_block( disk, block_to_write, data );

        std::memcpy( data, buf + bytes_written, bytes_to_write );
        gros_write_block( disk, block_to_write, data );
        bytes_written += bytes_to_write;

        if( bytes_to_write < block_size ) {
            inode->f_size = std::max(
                    // if we didn't gros_write to the end of the file
                    inode->f_size,
                    // if we wrote past the end of the file
                    inode->f_size - ( inode->f_size % block_size ) +
                    bytes_to_write
            );
            gros_save_inode( disk, inode );
        }

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
int gros_i_mknod( Disk * disk, Inode * inode, const char * filename ) {
    int        file_num;
    DirEntry * direntry = new DirEntry();
    Inode    * new_file = gros_new_inode( disk );

    new_file->f_links   = 1;
    direntry->inode_num = new_file->f_inode_num;
    strcpy( direntry->filename, filename );

    gros_i_write( disk, inode, ( char * ) direntry, sizeof( DirEntry ),
                  inode->f_size );
    gros_save_inode( disk, new_file );
    file_num = new_file->f_inode_num;

    delete new_file;
    delete direntry;
    return file_num;
}


int gros_mknod( Disk * disk, const char * path ) {
    char        * new_path = new char[ strlen( path ) + 1 ];
    const char  * file     = strrchr( path, '/' ); // get filename this way

    // invalid path
    if( ! file )
        return 0;

    file += 1; // skip over delimiter

    strcpy( new_path, path );
    int index             = ( int ) ( file - path );
    new_path[ index - 1 ] = '\0'; // cuts off filename
    Inode * inode         = gros_get_inode( disk, gros_namei( disk, new_path ) );

    delete [] new_path;
    return gros_i_mknod( disk, inode, file );
}


/**
 * Creates a directory with two entries (. and ..)
 *
 * @param Disk  * disk     Disk containing the file system
 * @param Inode * inode    Inode of directory in which to place new directory
 * @param char  * dirname  Name of new directory
 */
int gros_i_mkdir( Disk * disk, Inode * inode, const char * dirname ) {
    int        dir_num;
    DirEntry   entries[ 2 ];
    DirEntry * direntry = new DirEntry();
    Inode    * new_dir  = gros_new_inode( disk );

    direntry->inode_num = new_dir->f_inode_num;
    strcpy( direntry->filename, dirname );

    entries[ 0 ].inode_num = new_dir->f_inode_num;
    strcpy( entries[ 0 ].filename, "." );
    entries[ 1 ].inode_num = inode->f_inode_num;
    strcpy( entries[ 1 ].filename, ".." );

    new_dir->f_links = 2;
    inode->f_links  += 1;

    // add new direntry to current directory
    gros_i_write( disk, inode, ( char * ) direntry, sizeof( DirEntry ),
                  inode->f_size );

    // add first entries to new directory
    gros_i_write( disk, new_dir, ( char * ) entries, 2 * sizeof( DirEntry ), 0 );

    // save directories back to disk
    gros_save_inode( disk, inode );
    gros_save_inode( disk, new_dir );
    dir_num = direntry->inode_num;

    delete new_dir;
    delete direntry;
    return dir_num;
}


int gros_mkdir( Disk * disk, const char * path ) {
    char       * new_path = new char[ strlen( path ) + 1 ];
    const char * dir      = strrchr( path, '/' ); // get filename

    // invalid path
    if( ! dir )
        return 0;

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
* @param Disk  *  disk     Disk containing the file system
* @param Inode *  inode    Inode of directory containing directory to delete
* @param char  *  dirname  Name of directory to delete
*/
int gros_i_rmdir( Disk * disk, Inode * inode, Inode * dir_inode ) {
    int i;
    DirEntry *result;
    int found = 0;
    Inode * child_inode;

    gros_readdir_r( disk, dir_inode, NULL, &result);
    gros_readdir_r( disk, dir_inode, result, &result);
    gros_readdir_r( disk, dir_inode, result, &result);
    while (result) {
    	if (gros_is_dir(gros_get_inode(disk,result -> inode_num) -> f_acl)) {
    		child_inode = gros_get_inode(disk, result -> inode_num);
    		gros_i_rmdir( disk, dir_inode, child_inode);
    	} else {
    		gros_i_unlink( disk, dir_inode, result -> filename );
    	}
    	gros_readdir_r( disk, dir_inode, result, &result);
    }

    char buf[ sizeof(DirEntry) ];
    int direntry_counter = 0;

    gros_readdir_r( disk, inode, NULL, &result);
    while (result && !found) {
    	if (result -> inode_num == dir_inode -> f_inode_num) {
    		gros_i_read( disk, inode, buf, sizeof(DirEntry), inode -> f_size - sizeof(DirEntry) );
    		gros_i_write( disk, inode, buf, sizeof(DirEntry), direntry_counter * sizeof(DirEntry) );
    		found = 1;
    	}
    	direntry_counter++;
    	gros_readdir_r( disk, inode, result, &result);
    }
    gros_i_truncate( disk, inode, inode -> f_size - sizeof(DirEntry) );
    gros_free_inode( disk, dir_inode );
    return 0;
}


// TODO check for correct filename passed into fn
//   TODO ** what if not path with a '/' ? **
int gros_rmdir( Disk * disk, const char * path ) {
    const char * dirname = strrchr( path, '/' ) + 1;
    int length = strlen(path) - strlen(dirname);
    char       * new_path = new char[ length ];

    strncpy(new_path, path, length);
    new_path[ length ] = '\0';
    int inode_num = gros_namei( disk, new_path );
    delete[] new_path;
    return gros_i_rmdir( disk, gros_get_inode( disk, inode_num ), gros_get_inode( disk, gros_namei( disk, path ) ) );
}


/**
* Removes a file or directory. If the file is a directory, calls rmdir.
*
* @param Disk  *  disk       Disk containing the file system
* @param Inode *  inode      Inode of directory containing file to delete
* @param char  *  filename   Name of file to delete
*/
int gros_i_unlink( Disk * disk, Inode * inode, const char * filename ) {
    char buf[ sizeof(DirEntry) ];
	Inode * child_inode;
	int direntry_counter = 0;
	int found = 0;

    DirEntry *result;
    gros_readdir_r( disk, inode, NULL, &result);
    while (result && !found) {
    	if (strcmp(result -> filename, filename) == 0) {
    		child_inode = gros_get_inode(disk, result -> inode_num);
    		child_inode -> f_links--;
    		if (child_inode -> f_links == 0) {
    			gros_free_inode( disk, child_inode);
    		}
    		gros_i_read( disk, inode, buf, sizeof(DirEntry), inode -> f_size - sizeof(DirEntry) );
    		gros_i_write( disk, inode, buf, sizeof(DirEntry), direntry_counter * sizeof(DirEntry) );
    		found = 1;
    	}
    	direntry_counter++;
    	gros_readdir_r( disk, inode, result, &result);
    }
    gros_i_truncate( disk, inode, inode -> f_size - sizeof(DirEntry) );
    return 0;
}


// TODO check for correct filename
int gros_unlink( Disk * disk, const char * path ) {
    const char * filename = strrchr( path, '/' ) + 1;
    int length = strlen(path) - strlen(filename);
    char       * new_path = new char[ length ];

    strncpy(new_path, path, length);
    new_path[ length ] = '\0';
    int inode_num = gros_namei( disk, new_path );
    delete[] new_path;
    return gros_i_unlink( disk, gros_get_inode( disk, inode_num ), filename );

}

// TODO check for correct filename
int gros_frename( Disk * disk, const char * from, const char * to ) {
    gros_copy(disk, from, to);
    return gros_unlink(disk, from);
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
    int        * siblock;            /* buffer to store indirects */
    int        * diblock;            /* buffer to store indirects */
    int        * tiblock;            /* buffer to store indirects */
    Superblock * superblock;         /* reference to a superblock */

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
    if( inode->f_size == size )
        return 0;
    offset = size;

    // get the superblock so we can get the data we need about the file system
    gros_read_block( disk, 0, data );
    superblock      = ( Superblock * ) data;
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
        if (block_to_free == -1) {
          done = 1;
        } else if (last_of_file == 1) { // if this block contains the new end of file
          bytes_to_dealloc = block_size - (inode->f_size % block_size);
          gros_read_block( disk, block_to_free, data );
          // set zeros from the new end of the file to the end of the block
          std::memset(data + (inode->f_size % block_size), 0, bytes_to_dealloc);
          // save the block back
          gros_write_block(disk, block_to_free, data);
          last_of_file = 0;
        } else { // just free this block
          gros_free_data_block(disk, block_to_free);

          if (si_index != -1) {
            siblock[si_index] = -1;
            gros_write_block(disk, si, (char*) siblock);
            if (si_index == (n_indirects-1) && di_index != -1) {
              gros_free_data_block(disk, diblock[di_index]);
              diblock[di_index] = -1;
              gros_write_block(disk, di, (char*) diblock);
              if (di_index == (n_indirects-1) && ti_index != -1) {
                gros_free_data_block(disk, diblock[di_index]);
                tiblock[ti_index] = -1;
                gros_write_block(disk, inode->f_block[ TRIPLE_INDRCT ], (char*) tiblock);
                if (ti_index == (n_indirects-1)) {
                  gros_free_data_block(disk, inode->f_block[ TRIPLE_INDRCT ]);;
                  inode->f_block[ TRIPLE_INDRCT ] = -1;
                }
              } else if (di_index == (n_indirects-1) && ti_index == -1) {
                gros_free_data_block(disk, inode->f_block[ DOUBLE_INDRCT ]);;
                inode->f_block[ DOUBLE_INDRCT ] = -1;
              }
            } else if (si_index == (n_indirects-1) && di_index == -1) {
              gros_free_data_block(disk, inode->f_block[ SINGLE_INDRCT ]);;
              inode->f_block[ SINGLE_INDRCT ] = -1;
            }
          } else { // si_index == -1
            inode->f_block[ tmp ] = -1;
          }
        }

        cur_block++;
    }

    // free up the resources we allocated
    if( siblock != NULL ) delete[] siblock;
    if( diblock != NULL ) delete[] diblock;
    if( tiblock != NULL ) delete[] tiblock;

    inode->f_size = size;
        gros_save_inode( disk, inode );

    return 0;
}


// TODO check for correct filename
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
    int         direntrysize = sizeof( DirEntry );
    char        data[ direntrysize ];
    char        next[ direntrysize ];
    int         status       = 1;
    int         offset       = 0;

    if( ! current ) {
        gros_i_read( disk, dir, data, direntrysize, offset );
        * result = ( DirEntry * ) data;
        status = 0;
    }

    // read DirEntries until current entry is found
    while( status && gros_i_read( disk, dir, data, direntrysize, offset ) ) {
        offset += direntrysize;
        if( ( DirEntry * ) data == current ) {
            status = 0;
            if( gros_i_read( disk, dir, next, direntrysize, offset ) )
                * result = ( DirEntry * ) next;
            else * result = NULL;
        }
    }
    return status;
}


DirEntry * gros_readdir( Disk * disk, Inode * dir ) {
    DirEntry * result = NULL;

    if( gros_is_dir( dir->f_acl ) )
        gros_readdir_r( disk, dir, result, &result );

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
    DirEntry * direntry = new DirEntry();

    direntry->inode_num = from->f_inode_num;
    strcpy( direntry->filename, filename );

    gros_i_write(disk, todir, (char*) direntry, sizeof( DirEntry ), todir->f_size);

    from->f_links += 1;
    gros_save_inode( disk, from );

    delete direntry;
    return from->f_inode_num;
}

/* @param char*  to     FULL path (from root "/") to the new copied file */
int gros_copy( Disk * disk, const char * from, const char * to ) {
    const char * filename = strrchr( from, '/' ) + 1;
    int length = strlen(from) - strlen(filename);
    char       * dirname = new char[ length ];

    strncpy(dirname, from, length);
    dirname[ length ] = '\0';

    int from_inode_num = gros_namei(disk, from);
    int to_dir = gros_namei( disk, dirname );
    delete [] dirname;
    return gros_i_copy(
        disk,
        gros_get_inode(disk, from_inode_num),
        gros_get_inode(disk, to_dir),
        filename
    );
}
