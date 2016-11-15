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
    root_i->f_links = 1;

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
    int cur_si = -1, cur_di = -1; /* ids of blocks last read into siblock and diblock */
    int si = -1, di = -1; /* id of siblock and diblock that we need */
    int *siblock, *diblock, *tiblock; /* buffers to store indirects */
    Superblock *superblock; /* reference to a superblock */

    file_size = inode->f_size;
    // by default, the double indirect block we read from is the one given in the inode
    // this will change if we are in the triple indirect block
    di = inode->f_block[13];
    // by default, the single indirect block we read from is the one given in the inode
    // this will change if we are in the double indirect block
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
    return -1; // stub
}

/* @param char*  path     FULL path (from root "/") to the file */
int write(Disk * disk, const char* path, char *buf, int size, int offset) {
   int inode_num = namei(disk, path);
   Inode *inode = get_inode(disk, inode_num);
   return i_write(disk, inode, buf, size, offset);
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
	DirEntry *direntry = new DirEntry();
	direntry->inode_num = new_file_inode->f_inode_num;
	strcpy(direntry->filename, filename);
	i_write(disk, inode, (char *)direntry, sizeof(DirEntry), inode->f_size);
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
	return i_mknod(disk, inode, file);
}
