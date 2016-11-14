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
  // stub
  return -1;
}

int namei(Disk * disk, const char* path) {
	//stub
	return -1;
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
	char *file = strrchr(path, '/'); //get filename this way
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
