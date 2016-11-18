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
void mkroot( Disk * disk ) {
    Inode *  root_i;
    DirEntry root[ 2 ];

    // get the zero-th inode to store the root dir in
    root_i          = new_inode( disk ); // should be inode 0
    // !! root_i->f_inode_num == 0 !!
    root_i->f_acl   = 0x1ff; // 01 111 111 111
    root_i->f_links = 2;

    root[ 0 ].inode_num = root_i->f_inode_num;
    strcpy( root[ 0 ].filename, "." );

    root[ 1 ].inode_num = root_i->f_inode_num;
    strcpy( root[ 1 ].filename, ".." );

    i_write( disk, root_i, ( char * ) root, 2 * sizeof( DirEntry ), 0 );
    save_inode( disk, root_i );
    delete root_i;
}


// TODO note: linux implementation returns 0 upon success, not exactly sure how it works
/**
 * Returns the inode number of the file corresponding to the given path
 *
 * @param Disk * disk  Disk containing the file system
 * @param char * path  Path to the file, starting from root "/"
 */
int namei( Disk * disk, const char * path ) {
    char     * filename;
    Inode    * dir;
    DirEntry * direntry;

    // start at root
    dir      = get_inode( disk, 0 );
    // parse name from path
    filename = strdup( path );
    filename = strtok( filename, "/" );

    // travese directory for filename in path
    while( ( direntry = readdir( disk, dir ) ) && filename ) {
        if( ! strcmp( direntry -> filename, filename ) ) {
            dir      = get_inode( disk, direntry -> inode_num );
            filename = strtok( NULL, "/" );
        }
    }
    free( filename );
    return dir -> f_inode_num;
}


/**
 * Reads `size` bytes at `offset` offset bytes from file corresponding
 *  to given Inode on the given disk into given buffer
 *
 * @param Disk*  disk     Disk containing the file system
 * @param Inode* inode    Inode corresponding to the file to read
 * @param char*  buf      Buffer to read into (must be allocated to at
 *                          least `size` bytes)
 * @param int    size     Number of bytes to read
 * @param int    offset   Offset into the file to start reading from
 */
int i_read( Disk * disk, Inode * inode, char * buf, int size, int offset ) {
    char         data[ BLOCK_SIZE ]; /* buffer to read file contents into */
    int          block_size;         /* fs block size */
    int          file_size;          /* file size, i.e. inode->f_size */
    int          n_indirects;        /* how many ints can fit in a block */
    int          n_indirects_sq;     /* n_indirects * n_indirects */
    int          cur_block;          /* current block (relative to file) to read */
    int          block_to_read;      /* current block (relative to fs) to read */
    int          bytes_to_read;      /* bytes to read from cur_block */
    int          bytes_read = 0;     /* number of bytes already read into buf */
    int          cur_si     = -1;    /* id of blocks last read into siblock */
    int          cur_di     = -1;    /* id of blocks last read into diblock */
    int          si         = -1;    /* id of siblock that we need */
    int          di         = -1;    /* id of diblock that we need */
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

    // if we don't have to read, don't read. ¯\_(ツ)_/¯
    if( size <= 0 || offset >= file_size )
        return 0;

    // get the superblock so we can get the data we need about the file system
    read_block( disk, 0, data );
    superblock      = ( Superblock * ) data;
    block_size      = superblock->fs_block_size;
    // the number of indirects a block can have
    n_indirects     = block_size / sizeof( int );
    n_indirects_sq  = n_indirects * n_indirects;
    // this is the file's n-th block that we will read
    cur_block       = offset / block_size;

    // while we have more bytes in the file to read and have not read the
    // requested amount of bytes
    while( ( offset + bytes_read ) <= file_size && bytes_read <= size ) {
        // for this block, we either finish reading or read the entire block
        bytes_to_read = std::min( size - bytes_read, block_size );
        // tmp var to store index into indirect blocks if necessary
        block_to_read = cur_block;

        // in a triple indirect block
        if( block_to_read >= ( n_indirects_sq + SINGLE_INDRCT ) ) {
            // if we haven't fetched the triple indirect block yet, do so now
            if( tiblock == NULL ) {
                tiblock = new int[ n_indirects ];
                read_block( disk,
                            inode->f_block[ TRIPLE_INDRCT ],
                            ( char * ) tiblock );
            }

            // subtracting n^2+n+12 to obviate lower layers of indirection
            int pos =
                    ( block_to_read - ( n_indirects_sq
                                        + n_indirects
                                        + SINGLE_INDRCT ) )
                    / n_indirects_sq;
            // get the double indirect block that contains the block we need to read
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
                read_block( disk, di, ( char * ) diblock );
            }
            // subtracting n+12 to obviate lower layers of indirection
            int pos = ( block_to_read - ( n_indirects + SINGLE_INDRCT ) ) /
                      n_indirects;
            // get the single indirect block that contains the block we need to read
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
                read_block( disk, si, ( char * ) siblock );
            }
            // relative index into single indirects
            block_to_read = siblock[ block_to_read - SINGLE_INDRCT ];
        }

        // in a direct block
        if( cur_block < SINGLE_INDRCT ) {
            block_to_read = inode->f_block[ block_to_read ];
        }

        read_block( disk, block_to_read, data );
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

int read( Disk * disk, const char * path, char * buf, int size, int offset ) {
    return i_read( disk, get_inode( disk, namei( disk, path ) ), buf, size, offset );
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
int i_write( Disk * disk, Inode * inode, char * buf, int size, int offset ) {
    char         data[ BLOCK_SIZE ];   /* buffer to read/write file contents into/from */
    int          block_size;           /* copy of the fs block size */
    int          file_size;            /* copy of the file size, i.e. inode->f_size */
    int          n_indirects;          /* how many ints can fit in a block */
    int          n_indirects_sq;       /* n_indirects * n_indirects */
    int          cur_block;            /* the current block (relative to file) to write */
    int          block_to_write;       /* the current block (relative to fs) to write */
    int          bytes_to_write;       /* bytes to write into cur_block */
    int          bytes_written = 0;    /* number of bytes already written from buf */
    int          cur_si        = -1;
    int          cur_di        = -1;   /* ids of blocks last read into siblock & diblock */
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
    // by default, the double indirect block we write to is the one given in the
    // inode. this will change if we are in the triple indirect block
    di = inode->f_block[ DOUBLE_INDRCT ];
    // by default, the single indirect block we write to is the one given in the
    // inode. this will change if we are in the double indirect block
    si = inode->f_block[ SINGLE_INDRCT ];

    // if we don't have to write, don't write. ¯\_(ツ)_/¯
    if( size <= 0 || offset >= file_size )
        return 0;

    // get the superblock so we can get the data we need about the file system
    read_block( disk, 0, data );
    superblock     = ( Superblock * ) data;
    block_size     = superblock->fs_block_size;

    // the number of indirects a block can have
    n_indirects    = block_size / sizeof( int );
    n_indirects_sq = n_indirects * n_indirects;

    // this is the file's n-th block that we will write to
    cur_block      = offset / block_size;

    // while we have more bytes to write
    while( bytes_written <= size ) {
        // for this block, we either finish writing or write an entire block
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
                i_ensure_size(
                        disk, inode,
                        ( n_indirects_sq + n_indirects + SINGLE_INDRCT ) *
                        block_size
                );
                // if there is no triple indirect block yet, we need to allocate one
                if( inode->f_block[ TRIPLE_INDRCT ] == -1 ) {
                    inode->f_block[ TRIPLE_INDRCT ] = allocate_data_block(
                            disk );
                    save_inode( disk, inode );
                }
                // read the triple indirect block into tiblock
                read_block( disk, inode->f_block[ TRIPLE_INDRCT ],
                            ( char * ) tiblock );
            }

            // subtracting n^2+n+12 to obviate lower layers of indirection
            ti_index = ( block_to_write
                         - ( n_indirects_sq + n_indirects + SINGLE_INDRCT ) )
                       / n_indirects_sq;
            // get the double indirect block that contains the block we need to write to
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
                i_ensure_size( disk, inode,
                               ( n_indirects + SINGLE_INDRCT ) * block_size );
                // allocate a data block and save the inode
                inode->f_block[ DOUBLE_INDRCT ] = allocate_data_block( disk );
                di = inode->f_block[ DOUBLE_INDRCT ];
                save_inode( disk, inode );
            }
            else if( ti_index != -1 && di == -1 ) {
                // if we're in a double indirect block from a triple indirect, but it
                // hasn't been allocated yet
                // make sure all blocks before <this> double indirect block
                // are filled/allocated
                i_ensure_size( disk, inode,
                        // number of data blocks passed in triple indirects plus
                        // all inode double indirects plus
                        // all inode single indirects plus 12 (direct)
                        ( ti_index * n_indirects_sq + n_indirects_sq
                          + n_indirects + SINGLE_INDRCT ) * block_size );
                // allocate a double indirect block and save the tiblock
                tiblock[ ti_index ] = allocate_data_block( disk );
                di = tiblock[ ti_index ];
                write_block( disk, inode->f_block[ TRIPLE_INDRCT ],
                             ( char * ) tiblock );
            }

            diblock = diblock == NULL ? ( new int[ n_indirects ] ) : diblock;
            if( cur_di != di ) {
                cur_di = di;
                read_block( disk, di, ( char * ) diblock );
            }
            // subtracting n+12 to obviate lower layers of indirection
            di_index = ( block_to_write - ( n_indirects + SINGLE_INDRCT ) )
                       / n_indirects;
            // get the single indirect block that contains the block we need to read
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
                i_ensure_size( disk, inode, SINGLE_INDRCT * block_size );
                // allocate a data block and save the inode
                inode->f_block[ SINGLE_INDRCT ] = allocate_data_block( disk );
                si = inode->f_block[ SINGLE_INDRCT ];
                save_inode( disk, inode );
            } else if( ti_index == -1 && di_index != -1 && si == -1 ) {
                // if we're in a single indirect block from the inode's double indirect,
                // but the single indirect block hasn't been allocated yet
                // make sure all blocks before <this> single indirect block
                // are filled/allocated
                i_ensure_size(
                        disk, inode,
                        // number of data blocks passed in double indirects plus
                        // all inode single indirects plus 12 (direct)
                        ( di_index * n_indirects + n_indirects +
                          SINGLE_INDRCT ) * block_size
                );
                // allocate a single indirect block and save the diblock
                diblock[ di_index ] = allocate_data_block( disk );
                si = diblock[ di_index ];
                write_block( disk, inode->f_block[ DOUBLE_INDRCT ],
                             ( char * ) diblock );
            } else if( ti_index != -1 && di_index != -1 && si == -1 ) {
                // if we're in a single indirect block from a double indirect (itself from
                // a triple indirect), but it hasn't been allocated yet
                // make sure all blocks before <this> single indirect block
                // are filled/allocated
                i_ensure_size(
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
                diblock[ di_index ] = allocate_data_block( disk );
                si = diblock[ di_index ];
                write_block( disk, di, ( char * ) diblock );
            }
            siblock = siblock == NULL ? ( new int[ n_indirects ] ) : siblock;
            // if we dont' already have the single indirects loaded into memory, load it
            if( cur_si != si ) {
                cur_si = si;
                read_block( disk, si, ( char * ) siblock );
            }
            // relative index into single indirects
            block_to_write = siblock[ block_to_write - SINGLE_INDRCT ];
            si_index = block_to_write - SINGLE_INDRCT;
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
        i_ensure_size(
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
            siblock[ si_index ] = allocate_data_block( disk );
            block_to_write = siblock[ si_index ];
            write_block( disk, si, ( char * ) siblock );
        }

        // in a direct block
        if( cur_block < SINGLE_INDRCT ) {
            block_to_write = inode->f_block[ cur_block ];
            if( inode->f_block[ cur_block ] == -1 ) {
                inode->f_block[ cur_block ] = allocate_data_block( disk );
                block_to_write = inode->f_block[ cur_block ];
            }
        }

        // if we are writing an entire block, we don't need to read, since we're
        // overwriting it. Otherwise, we need to save what we're not writing over
        if( bytes_to_write < block_size )
            read_block( disk, block_to_write, data );

        std::memcpy( data, buf + bytes_written, bytes_to_write );
        write_block( disk, block_to_write, data );
        bytes_written += bytes_to_write;

        if( bytes_to_write < block_size ) {
            inode->f_size = std::max(
                    // if we didn't write to the end of the file
                    inode->f_size,
                    // if we wrote past the end of the file
                    inode->f_size - ( inode->f_size % block_size ) +
                    bytes_to_write
            );
            save_inode( disk, inode );
        }

        cur_block++;
    }

    // free up the resources we allocated
    if( siblock != NULL ) delete [] siblock;
    if( diblock != NULL ) delete [] diblock;
    if( tiblock != NULL ) delete [] tiblock;

    return bytes_written;
}

int write( Disk * disk, const char * path, char * buf, int size, int offset ) {
    return i_write( disk, get_inode( disk, namei( disk, path ) ), buf, size, offset );
}


// TODO how to find end of file if potentially zero filled ?
/**
 * Ensures that a file is at least `size` bytes long. If it is already
 *  `size` bytes, nothing happens. Otherwise, the file is allocated
 *  data blocks as necessary and zero-filled to be `size` bytes long.
 *
 * @param Disk*  disk     Disk containing the file system
 * @param Inode* inode    Inode corresponding to the file to resize
 * @param int    size     Desired file size
 */
int i_ensure_size( Disk * disk, Inode * inode, int size ) {
    return -1; // stub
}

int ensure_size( Disk * disk, char * path, int size ) {
    return i_ensure_size( disk, get_inode( disk, namei( disk, path ) ), size );
}


/**
 * Creates a file from free inode, adds entry to parent directory
 *
 * @param  Disk  *  disk      Disk containing the file system
 * @param  Inode *  inode     Inode of parent directory
 * @param  char  *  filename  Name of new file
 * @return int                Inode number of new file
 */
int i_mknod( Disk * disk, Inode * inode, const char * filename ) {
    int        file_num;
    DirEntry * direntry = new DirEntry();
    Inode    * new_file = new_inode( disk );

    new_file->f_links   = 1;
    direntry->inode_num = new_file->f_inode_num;
    strcpy( direntry->filename, filename );

    i_write( disk, inode, ( char * ) direntry, sizeof( DirEntry ),
             inode->f_size );
    save_inode( disk, new_file );
    file_num = new_file->f_inode_num;

    delete new_file;
    delete direntry;
    return file_num;
}


int mknod( Disk * disk, const char * path ) {
    char        * new_path = new char[ strlen( path ) + 1 ];
    const char  * file     = strrchr( path, '/' ); // get filename this way

    // invalid path
    if( ! file )
        return 0;

    file += 1; // skip over delimiter

    strcpy( new_path, path );
    // TODO what is this index ?
    int index             = ( int ) ( file - path );
    new_path[ index - 1 ] = '\0'; // cuts off filename
    Inode * inode         = get_inode( disk, namei( disk, new_path ) );

    delete [] new_path;
    return i_mknod( disk, inode, file );
}


/**
 * Creates a directory with two entries (. and ..)
 *
 * @param Disk  * disk     Disk containing the file system
 * @param Inode * inode    Inode of directory in which to place new directory
 * @param char  * dirname  Name of new directory
 */
int i_mkdir( Disk * disk, Inode * inode, const char * dirname ) {
    int        dir_num;
    DirEntry   entries[ 2 ];
    DirEntry * direntry = new DirEntry();
    Inode    * new_dir  = new_inode( disk );

    direntry->inode_num = new_dir->f_inode_num;
    strcpy( direntry->filename, dirname );

    entries[ 0 ].inode_num = new_dir->f_inode_num;
    strcpy( entries[ 0 ].filename, "." );
    entries[ 1 ].inode_num = inode->f_inode_num;
    strcpy( entries[ 1 ].filename, ".." );

    new_dir->f_links = 2;

    // add new direntry to current directory
    i_write( disk, inode, ( char * ) direntry, sizeof( DirEntry ),
             inode->f_size );

    // add first entries to new directory
    i_write( disk, new_dir, ( char * ) entries, 2 * sizeof( DirEntry ),
             0 );

    // save directories back to disk
    save_inode( disk, inode );
    save_inode( disk, new_dir );
    dir_num = direntry->inode_num;

    delete new_dir;
    delete direntry;
    return dir_num;
}


int mkdir( Disk * disk, const char * path ) {
    char       * new_path = new char[ strlen( path ) + 1 ];
    const char * dir      = strrchr( path, '/' ); // get filename

    // invalid path
    if( ! dir )
        return 0;

    dir += 1;  // skip over delimiter

    strcpy( new_path, path );
    // add null terminator to end of filename
//    TODO not sure what index is dir - path
    new_path[ ( int ) ( dir - path ) - 1 ] = '\0';
    Inode * inode = get_inode( disk, namei( disk, new_path ) );

    delete [] new_path;
    return i_mkdir( disk, inode, dir );
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
int i_rmdir( Disk * disk, Inode * inode, const char * dirname ) {
    // TODO STUB
    return 0;
}


// TODO check for correct filename passed into fn
//   TODO ** what if not path with a '/' ? **
int rmdir( Disk * disk, const char * path ) {
    char * dirname = strrchr( path, '/' ) + 1;
    return i_rmdir( disk, get_inode( disk, namei( disk, path ) ), dirname );
}


/**
* Removes a file or directory. If the file is a directory, calls rmdir.
*
* @param Disk  *  disk       Disk containing the file system
* @param Inode *  inode      Inode of directory containing file to delete
* @param char  *  filename   Name of file to delete
*/
int i_unlink( Disk * disk, Inode * inode, const char * filename ) {
    // TODO STUB
    return 0;
}


// TODO check for correct filename
int unlink( Disk * disk, const char * path ) {
    char * filename = strrchr( path, '/' ) + 1;
    return i_unlink( disk, get_inode( disk, namei( disk, path ) ), filename );
}


/**
* Renames a file or directory
*
* @param Disk  *  disk       Disk containing the file system
* @param Inode *  inode      Inode of directory containing file to rename
* @param char  *  oldname    Name of file to rename
* @param char  *  oldname    New name for file
*/
int i_rename( Disk * disk, Inode * inode,
              const char * oldname, const char * newname ) {
    // TODO STUB
    return 0;
}


// TODO check for correct filename
int rename( Disk * disk, const char * path, const char * newname ) {
    char * oldname = strrchr( path, '/' ) + 1;
    return i_rename( disk, get_inode( disk, namei( disk, path ) ),
                     oldname, newname );
}


/**
* Truncates or extends the file to a specified length
*
* @param Disk  *  disk     Disk containing the file system
* @param Inode *  inode    Inode corresponding to the file to resize
* @param int      size     Desired file size
*/
int i_truncate( Disk * disk, Inode * inode, int size ) {
    // TODO STUB
    return 0;
}


int truncate( Disk * disk, const char * path, int size ) {
    return i_truncate( disk, get_inode( disk, namei( disk, path ) ), size );
}


// TODO linux implementation takes in directory stream, otherwise not sure
//    how to know where at in directory and which entry to return next
DirEntry * readdir( Disk * disk, Inode * dir ) {
    if( is_dir( dir->f_acl ) ) {
        // TODO STUB
    }
    return NULL;
}
