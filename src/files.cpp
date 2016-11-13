/**
 * files.cpp
 */

#include "files.hpp"


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
    root[0].has_next = 1;
    root[0].filename = ".";

    root[1].inode_num = root_i->f_inode_num;
    root[1].has_next = 0;
    root[1].filename = ".";

    i_write(disk, root_i, root, 2 * sizeof(DirEntry), 0);
}
