/**
 * files.cpp
 */

#include "files.hpp"
#include <cstring>


/**
 * Creates the primordial directory for the file system (i.e. root "/")
 *  The root of the file system will always be inode #0.
 *
 *  @param Disk* disk   The disk containing the file system
 */
void mkroot( Disk * disk ) {
    Inode *root_i;
    DirEntry root[2];

    // get the zero-th inode to store the root dir in
    root_i = new_inode(disk); // should be inode 0
    // !! root_i->f_inode_num == 0 !!
    root_i->f_acl = 0x1ff; // 01 111 111 111
    root_i->f_links = 2;

    root[0].inode_num = root_i->f_inode_num;
    strcpy(root[0].filename, ".");

    root[1].inode_num = root_i->f_inode_num;
    strcpy(root[1].filename, "..");

    i_write(disk, root_i, (char *)root, 2 * sizeof(DirEntry), 0);
    save_inode(disk, root_i);
    delete root_i;
}


/**
 * Returns the inode number of the file corresponding to the given path
 *
 * @param Disk* disk  Disk containing the file system
 * @param char* path  Path to the file, starting from root "/"
 */
int namei(Disk * disk, const char* path) {
  return -1;
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
int i_read(Disk * disk, Inode *inode, char *buf, int size, int offset) {
    int block_size; /* copy of the fs block size */
    int file_size; /* copy of the file size, i.e. inode->f_size */
    int n_indirects; /* how many ints can fit in a block */
    int n_indirects_sq; /* n_indirects * n_indirects */
    int cur_block; /* the current block (relative to file) to read */
    int block_to_read; /* the current block (relative to fs) to read */
    int bytes_to_read; /* bytes to read from cur_block */
    int bytes_read = 0; /* number of bytes already read into buf */
    char data[BLOCK_SIZE]; /* buffer to read file contents into */
    int cur_si = -1, cur_di = -1; /* ids of blocks last read into siblock & diblock */
    int si = -1, di = -1; /* id of siblock and diblock that we need */
    int *siblock, *diblock, *tiblock; /* buffers to store indirects */
    Superblock *superblock; /* reference to a superblock */

    file_size = inode->f_size;
    // by default, the double indirect block we read from is the one given in
    // the inode. this will change if we are in the triple indirect block
    di = inode->f_block[13];
    // by default, the single indirect block we read from is the one given in
    // the inode. this will change if we are in the double indirect block
    si = inode->f_block[12];

    // if we don't have to read, don't read. ¯\_(ツ)_/¯
    if (size <= 0 || offset >= file_size) {
      return 0;
    }

    // get the superblock so we can get the data we need about the file system
    read_block( disk, 0, data );
    superblock = (Superblock *) data;
    block_size = superblock->fs_block_size;
    // the number of indirects a block can have
    n_indirects = block_size / sizeof(int);
    n_indirects_sq = n_indirects * n_indirects;
    // this is the file's n-th block that we will read
    cur_block = offset / block_size;

    // while we have more bytes in the file to read and have not read the
    // requested amount of bytes
    while ((offset + bytes_read) <= file_size && bytes_read <= size) {
      // for this block, we either finish reading or read the entire block
      bytes_to_read = std::min(size - bytes_read, block_size);
      // tmp var to store index into indirect blocks if necessary
      block_to_read = cur_block;

      // in a triple indirect block
      if (block_to_read >= (n_indirects_sq + 12)) {
        // if we haven't fetched the triple indirect block yet, do so now
        if (tiblock == NULL) {
          tiblock = new int[n_indirects];
          read_block(disk, inode->f_block[14], (char*) tiblock);
        }

        // subtracting n^2+n+12 to obviate lower layers of indirection
        int pos = (block_to_read - (n_indirects_sq + n_indirects + 12)) / n_indirects_sq;
        // get the double indirect block that contains the block we need to read
        di = tiblock[pos];
        // since we're moving onto double indirect addressing, subtract all
        // triple indirect related index information
        block_to_read -= pos*n_indirects_sq; /* still >= n+12 */
      }

      // in a double indirect block
      if (block_to_read >= (n_indirects + 12)) {
        diblock = diblock == NULL ? new int[n_indirects] : diblock;
        if (cur_di != di) {
          cur_di = di;
          read_block(disk, di, (char*) diblock);
        }
        // subtracting n+12 to obviate lower layers of indirection
        int pos = (block_to_read - (n_indirects + 12)) / n_indirects;
        // get the single indirect block that contains the block we need to read
        si = diblock[pos];
        // since we're moving onto single indirect addressing, subtract all
        // double indirect related index information
        block_to_read -= pos*n_indirects; /* still >= 12 */
      }

      // in a single indirect block
      if (block_to_read >= 12) {
        siblock = siblock == NULL ? new int[n_indirects] : siblock;
        // if we dont' already have the single indirects loaded into memory, load it
        if (cur_si != si) {
          cur_si = si;
          read_block(disk, si, (char*) siblock);
        }
        // relative index into single indirects
        block_to_read = siblock[block_to_read - 12];
      }

      // in a direct block
      if (cur_block < 12) {
        block_to_read = inode->f_block[block_to_read];
      }

      read_block( disk, block_to_read, data );
      std::memcpy(buf + bytes_read, data, bytes_to_read);
      bytes_read += bytes_to_read;

      cur_block++;
    }

    // free up the resources we allocated
    if (siblock != NULL) delete [] siblock;
    if (diblock != NULL) delete [] diblock;
    if (tiblock != NULL) delete [] tiblock;

    return bytes_read;
}
/* @param char*  path     FULL path (from root "/") to the file */
int read(Disk * disk, const char* path, char *buf, int size, int offset) {
   int inode_num = namei(disk, path);
   Inode *inode = get_inode(disk, inode_num);
   return i_read(disk, inode, buf, size, offset);
}


/**
 * Writes `size` bytes (at `offset` bytes from 0) into file
 *  corresponding to given Inode on the given disk from given buffer
 *
 * @param Disk*  disk     Disk containing the file system
 * @param Inode* inode    Inode corresponding to the file to write to
 * @param char*  buf      Buffer to write to file (must be at least
 *                           `size` bytes)
 * @param int    size     Number of bytes to write
 * @param int    offset   Offset into the file to start writing to
 */
int i_write(Disk * disk, Inode *inode, char *buf, int size, int offset) {
    int block_size; /* copy of the fs block size */
    int file_size; /* copy of the file size, i.e. inode->f_size */
    int n_indirects; /* how many ints can fit in a block */
    int n_indirects_sq; /* n_indirects * n_indirects */
    int cur_block; /* the current block (relative to file) to write */
    int block_to_write; /* the current block (relative to fs) to write */
    int bytes_to_write; /* bytes to write into cur_block */
    int bytes_written = 0; /* number of bytes already written from buf */
    char data[BLOCK_SIZE]; /* buffer to read/write file contents into/from */
    int cur_si = -1, cur_di = -1; /* ids of blocks last read into siblock & diblock */
    int si = -1, di = -1; /* id of siblock and diblock that we need */
    int *siblock, *diblock, *tiblock; /* buffers to store indirects */
    int si_index = -1; /* index into siblock for data */
    int di_index = -1; /* index into diblock for siblock */
    int ti_index = -1; /* index into tiblock for diblock */
    Superblock *superblock; /* reference to a superblock */

    file_size = inode->f_size;
    // by default, the double indirect block we write to is the one given in the
    // inode. this will change if we are in the triple indirect block
    di = inode->f_block[13];
    // by default, the single indirect block we write to is the one given in the
    // inode. this will change if we are in the double indirect block
    si = inode->f_block[12];

    // if we don't have to write, don't write. ¯\_(ツ)_/¯
    if (size <= 0 || offset >= file_size) {
      return 0;
    }

    // get the superblock so we can get the data we need about the file system
    read_block( disk, 0, data );
    superblock = (Superblock *) data;
    block_size = superblock->fs_block_size;
    // the number of indirects a block can have
    n_indirects = block_size / sizeof(int);
    n_indirects_sq = n_indirects * n_indirects;
    // this is the file's n-th block that we will write to
    cur_block = offset / block_size;

    // while we have more bytes to write
    while (bytes_written <= size) {
      // for this block, we either finish writing or write an entire block
      bytes_to_write = std::min(size - bytes_written, block_size);
      // tmp var to store index into indirect blocks if necessary
      block_to_write = cur_block;

      // in a triple indirect block
      if (block_to_write >= (n_indirects_sq + 12)) {
        // if we haven't fetched the triple indirect block yet, do so now
        if (tiblock == NULL) {
          tiblock = new int[n_indirects];
          // make sure all blocks before triple indirect block
          // are filled/allocated
          i_ensure_size(
            disk, inode,
            (n_indirects_sq + n_indirects + 12)*block_size
          );
          // if there is no triple indirect block yet, we need to allocate one
          if (inode->f_block[14] == -1) {
            inode->f_block[14] = allocate_data_block(disk);
            save_inode(disk, inode);
          }
          // read the triple indirect block into tiblock
          read_block(disk, inode->f_block[14], (char*) tiblock);
        }

        // subtracting n^2+n+12 to obviate lower layers of indirection
        ti_index = (block_to_write - (n_indirects_sq + n_indirects + 12)) / n_indirects_sq;
        // get the double indirect block that contains the block we need to write to
        di = tiblock[ti_index];
        // since we're moving onto double indirect addressing, subtract all
        // triple indirect related index information
        block_to_write -= ti_index*n_indirects_sq; /* still >= n+12 */
      }

      // in a double indirect block
      if (block_to_write >= (n_indirects + 12)) {
        // if we're in the inode's double indirect block and it hasn't been allocated
        if (ti_index == -1 && di == -1) {
          // make sure all blocks before double indirect block
          // are filled/allocated
          i_ensure_size(
            disk, inode,
            (n_indirects + 12)*block_size
          );
          // allocate a data block and save the inode
          inode->f_block[13] = allocate_data_block(disk);
          di = inode->f_block[13];
          save_inode(disk, inode);
        } else if (ti_index != -1 && di == -1) {
        // if we're in a double indirect block from a triple indirect, but it
        // hasn't been allocated yet
          // make sure all blocks before <this> double indirect block
          // are filled/allocated
          i_ensure_size(
            disk, inode,
            // number of data blocks passed in triple indirects plus
            // all inode double indirects plus
            // all inode single indirects plus 12 (direct)
            (ti_index*n_indirects_sq + n_indirects_sq + n_indirects + 12)*block_size
          );
          // allocate a double indirect block and save the tiblock
          tiblock[ti_index] = allocate_data_block(disk);
          di = tiblock[ti_index];
          write_block(disk, inode->f_block[14], (char*) tiblock);
        }

        diblock = diblock == NULL ? new int[n_indirects] : diblock;
        if (cur_di != di) {
          cur_di = di;
          read_block(disk, di, (char*) diblock);
        }
        // subtracting n+12 to obviate lower layers of indirection
        di_index = (block_to_write - (n_indirects + 12)) / n_indirects;
        // get the single indirect block that contains the block we need to read
        si = diblock[di_index];
        // since we're moving onto single indirect addressing, subtract all
        // double indirect related index information
        block_to_write -= di_index*n_indirects; /* still >= 12 */
      }

      // in a single indirect block
      if (block_to_write >= 12) {
        // if we're in the inode's single indirect block and it hasn't been allocated
        if (ti_index == -1 && di_index == -1 && si == -1) {
          // make sure all blocks before single indirect block are filled/allocated
          i_ensure_size(
            disk, inode,
            12*block_size
          );
          // allocate a data block and save the inode
          inode->f_block[12] = allocate_data_block(disk);
          si = inode->f_block[12];
          save_inode(disk, inode);
        } else if (ti_index == -1 && di_index != -1 && si == -1) {
        // if we're in a single indirect block from the inode's double indirect,
        // but the single indirect block hasn't been allocated yet
          // make sure all blocks before <this> single indirect block
          // are filled/allocated
          i_ensure_size(
            disk, inode,
            // number of data blocks passed in double indirects plus
            // all inode single indirects plus 12 (direct)
            (di_index*n_indirects + n_indirects + 12)*block_size
          );
          // allocate a single indirect block and save the diblock
          diblock[di_index] = allocate_data_block(disk);
          si = diblock[di_index];
          write_block(disk, inode->f_block[13], (char*) diblock);
        } else if (ti_index != -1 && di_index != -1 && si == -1) {
        // if we're in a single indirect block from a double indirect (itself from
        // a triple indirect), but it hasn't been allocated yet
          // make sure all blocks before <this> single indirect block
          // are filled/allocated
          i_ensure_size(
            disk, inode,
            ( // number of data blocks passed in triple indirects plus
              ti_index*n_indirects_sq +
              // number of data blocks passed in current double indirect plus
              di_index*n_indirects +
              // all inode double indirects plus
              n_indirects_sq +
              // all inode single indirects plus 12 (direct)
              n_indirects + 12
            )*block_size
          );
          // allocate a single indirect block and save the diblock
          diblock[di_index] = allocate_data_block(disk);
          si = diblock[di_index];
          write_block(disk, di, (char*) diblock);
        }
        siblock = siblock == NULL ? new int[n_indirects] : siblock;
        // if we dont' already have the single indirects loaded into memory, load it
        if (cur_si != si) {
          cur_si = si;
          read_block(disk, si, (char*) siblock);
        }
        // relative index into single indirects
        block_to_write = siblock[block_to_write - 12];
        si_index = block_to_write - 12;
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
          std::max(ti_index,0)*n_indirects_sq +
          // number of data blocks passed in current double indirect plus
          std::max(di_index,0)*n_indirects +
          // number of data blocks passed in current single indirect plus
          std::max(si_index,0) +
          // all inode double indirects plus
          std::max(inode->f_block[13],0)*n_indirects_sq +
          // all inode single indirects plus
          std::max(inode->f_block[12],0)*n_indirects +
          // direct blocks
          std::min(cur_block, 12)
        )*block_size
      );

      // we're in a single indirect block and the data block hasn't been allocated
      if (si_index != -1 && block_to_write == -1) {
        siblock[si_index] = allocate_data_block(disk);
        block_to_write = siblock[si_index];
        write_block(disk, si, (char*) siblock);
      }

      // in a direct block
      if (cur_block < 12) {
        block_to_write = inode->f_block[cur_block];
        if (inode->f_block[cur_block] == -1) {
          inode->f_block[cur_block] = allocate_data_block(disk);
          block_to_write = inode->f_block[cur_block];
        }
      }

      // if we are writing an entire block, we don't need to read, since we're
      // overwriting it. Otherwise, we need to save what we're not writing over
      if (bytes_to_write < block_size) {
        read_block( disk, block_to_write, data );
      }
      std::memcpy(data, buf + bytes_written, bytes_to_write);
      write_block( disk, block_to_write, data );
      bytes_written += bytes_to_write;

      if (bytes_to_write < block_size) {
        inode->f_size = std::max(
          // if we didn't write to the end of the file
          inode->f_size,
          // if we wrote past the end of the file
          inode->f_size - (inode->f_size % block_size) + bytes_to_write
        );
        save_inode(disk, inode);
      }

      cur_block++;
    }

    // free up the resources we allocated
    if (siblock != NULL) delete [] siblock;
    if (diblock != NULL) delete [] diblock;
    if (tiblock != NULL) delete [] tiblock;

    return bytes_written;
}

/* @param char*  path     FULL path (from root "/") to the file */
int write(Disk * disk, const char* path, char *buf, int size, int offset) {
   int inode_num = namei(disk, path);
   Inode *inode = get_inode(disk, inode_num);
   return i_write(disk, inode, buf, size, offset);
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
int i_ensure_size(Disk * disk, Inode *inode, int size) {
  return -1; // stub
}
/* @param char*  path     FULL path (from root "/") to the file */
int ensure_size(Disk * disk, const char* path, int size) {
   int inode_num = namei(disk, path);
   Inode *inode = get_inode(disk, inode_num);
   return i_ensure_size(disk, inode, size);
}


/**
 * Creates a file
 *
 * @param Disk*  disk     Disk containing the file system
 * @param Inode* inode    Inode of directory in which to place new file
 * @param char*  filename  Name of new file
 */
int i_mknod(Disk *disk, Inode *inode, const char* filename) {
	Inode *new_file_inode = new_inode(disk);
	new_file_inode->f_links = 1;
	
	DirEntry *direntry = new DirEntry();
	direntry->inode_num = new_file_inode->f_inode_num;
	strcpy(direntry->filename, filename);
	
	i_write(disk, inode, (char *)direntry, sizeof(DirEntry), inode->f_size);
	save_inode(disk, new_file_inode);
	delete new_file_inode;
	return direntry->inode_num;
}
/* @param char*  path     FULL path (from root "/") to place the new file */
int mknod(Disk * disk, const char* path) {
	const char *file = strrchr(path, '/'); //get filename this way
	if (file) { //not sure what to do otherwise
		file = file + 1; //skip over delimiter
	}
	char *new_path = new char[strlen(path) + 1];
	strcpy(new_path, path);
	int index = (int)(file - path);
	new_path[index - 1] = '\0'; // cuts off filename
	int inode_num = namei(disk, new_path);
	Inode *inode = get_inode(disk, inode_num);
	delete[] new_path;
	return i_mknod(disk, inode, file);
}

/**
 * Creates a directory with two entries (. and ..)
 *
 * @param Disk*  disk     Disk containing the file system
 * @param Inode* inode    Inode of directory in which to place new directory
 * @param char*  dirname  Name of new directory
 */
int i_mkdir(Disk *disk, Inode *inode, const char* dirname) {
	Inode *new_dir_inode = new_inode(disk);
	new_dir_inode->f_links = 2;
	
	DirEntry entries[2];
	entries[0].inode_num = new_dir_inode->f_inode_num;
    strcpy(entries[0].filename, ".");
    entries[1].inode_num = inode->f_inode_num;
    strcpy(entries[1].filename, "..");
    
	DirEntry *direntry = new DirEntry();
	direntry->inode_num = new_dir_inode->f_inode_num;
	strcpy(direntry->filename, dirname);
	i_write(disk, inode, (char *)direntry, sizeof(DirEntry), inode->f_size);
	
    i_write(disk, new_dir_inode, (char *)entries, 2 * sizeof(DirEntry), 0);
    save_inode(disk, new_dir_inode);
    delete new_dir_inode;
    
	return direntry->inode_num;
}
/* @param char*  path     FULL path (from root "/") to place the new directory */
int mkdir(Disk * disk, const char* path) {
	const char *dir = strrchr(path, '/'); //get filename this way
	if (dir) { //not sure what to do otherwise
		dir = dir + 1; //skip over delimiter
	}
	char *new_path = new char[strlen(path) + 1];
	strcpy(new_path, path);
	int index = (int)(dir - path);
	new_path[index - 1] = '\0'; // cuts off filename
	int inode_num = namei(disk, new_path);
	Inode *inode = get_inode(disk, inode_num);
	delete[] new_path;
	return i_mkdir(disk, inode, dir);
}
