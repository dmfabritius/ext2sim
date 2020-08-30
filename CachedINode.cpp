#include "CachedINode.hpp"
#include "DataBlock.hpp"
#include "Directory.hpp"
#include "FileSystem.hpp"

// find the full absolute path of this diretory
std::string CachedINode::fullpath() {
    CachedINode* dir = this; // start at this inode
    CachedINode* parent;
    DataBlock block;
    DirectoryEntry* parentDirEntry;
    std::string fullpath, name;

    if (dir == fs.root) // handle the special case of being at the root of the file system
        return "/";

    dir->refCount++; // increment the reference count of this cached inode
    while (dir != fs.root) {
        if (dir == dir->device->root) { // if we reach the root of a mounted device, switch to the mount point
            dir->put();
            dir = fs.inodeTable.get(device->mountPoint->device, device->mountPoint->inodeNum);
        }
        block.get(dir->device, dir->inode.i_block[0]);
        parentDirEntry = (DirectoryEntry*)&block.buffer[PARENT_DIR_ENTRY_OFFSET];
        parent = fs.inodeTable.get(dir->device, parentDirEntry->inode);
        fullpath = "/" + parent->search(dir->inodeNum) + fullpath;
        dir->put();
        dir = parent;
    }
    dir->put();
    return fullpath;
}

// for symbolic link files, returns the absolute pathname that it links to
std::string CachedINode::linkname() {
    if (S_ISLNK(inode.i_mode)) {
        return (char*)inode.i_block;
    }
    return "";
}

// returns this file's mode as a string, e.g., 0644 is -rw-r--r--
std::string CachedINode::mode() {
    std::stringstream stream;
    std::string permissions("xwrxwrxwr");

    if (S_ISREG(inode.i_mode)) // regular file
        stream << '-';
    else if (S_ISDIR(inode.i_mode)) // directory
        stream << 'd';
    else if (S_ISLNK(inode.i_mode)) // symbolic link
        stream << 'l';
    for (int i = 8; i >= 0; i--) {
        if (inode.i_mode & (1 << i))
            stream << permissions[i]; // print r, w, x
        else
            stream << '-';
    }
    return stream.str();
}

// search this directory for a given inode number and return its name (empty string if not found)
std::string CachedINode::search(int inodeNum) {
    for (const auto& entry : Directory(this)) {
        if (entry.inodeNum == inodeNum)
            return entry.name;
    }
    return ""; // inode number not found, return empty string
}

// search this directory for a given name and return its inode number (0 if not found)
int CachedINode::search(const std::string& targetName) {
    for (const auto& entry : Directory(this)) {
        if (entry.name == targetName)
            return entry.inodeNum;
    }
    return 0; // target not found, return 0 for inode number
}

// convert a logical block number for this file into an actual block number on its device
int CachedINode::logical2physical(int logicalBlockNum) {
    DataBlock indirectBlock(device);
    DataBlock doubleBlock(device);

    if (logicalBlockNum < EXT2_NDIR_BLOCKS)
        // direct block numbers
        return inode.i_block[logicalBlockNum];
    else if (logicalBlockNum < EXT2_NDIR_BLOCKS + BLOCKNUMS_PER_BLOCK) {
        //  indirect block numbers
        if (!inode.i_block[EXT2_IND_BLOCK])
            return 0; // this file has no indirect blocks
        indirectBlock.get(inode.i_block[EXT2_IND_BLOCK]);
        return indirectBlock.nums[logicalBlockNum - EXT2_NDIR_BLOCKS];
    } else {
        //  double-indirect block numbers
        if (!inode.i_block[EXT2_DIND_BLOCK])
            return 0; // this file has no double-indirect blocks
        doubleBlock.get(inode.i_block[EXT2_DIND_BLOCK]);
        int i = (logicalBlockNum - EXT2_NDIR_BLOCKS - BLOCKNUMS_PER_BLOCK) / BLOCKNUMS_PER_BLOCK;
        indirectBlock.get(doubleBlock.nums[i]);
        int j = (logicalBlockNum - EXT2_NDIR_BLOCKS - BLOCKNUMS_PER_BLOCK) % BLOCKNUMS_PER_BLOCK;
        return indirectBlock.nums[j];
    }
}

// get a new data block number and update the inode i_block[] structure
int CachedINode::allocate_block() {
    DataBlock doubleBlock(device);
    int blockNum;

    inode.i_ctime = time(0L); // update inode change time
    isDirty = true;

    // look for an available direct block entry; if so, return allocated block number
    for (int i = 0; i < EXT2_NDIR_BLOCKS; i++) {
        if (inode.i_block[i] == 0) {
            // found an empty direct block entry
            inode.i_block[i] = device->allocate(BLOCK);
            return inode.i_block[i];
        }
    }

    // look for an available indirect block entry; if so, return allocated block number
    blockNum = allocate_indirect(device, (int*)&inode.i_block[EXT2_IND_BLOCK]);
    if (blockNum != 0)
        return blockNum;

    // look for an available double-indirect block entry (we will not use triple-indirect blocks)
    // if the double-indirect block doesn't exist, create it
    if (inode.i_block[EXT2_DIND_BLOCK] == 0) {
        inode.i_block[EXT2_DIND_BLOCK] = device->allocate(BLOCK);
        blockNum = allocate_indirect(device, &doubleBlock.nums[0]); // use the first double-indirect block entry
        doubleBlock.put(inode.i_block[EXT2_DIND_BLOCK]);
        return blockNum;
    }

    // get the existing double-indirect block
    doubleBlock.get(inode.i_block[EXT2_DIND_BLOCK]);
    // look for an available double-indirect block entry
    for (int i = 0; i < BLOCKNUMS_PER_BLOCK; i++) {
        blockNum = allocate_indirect(device, &doubleBlock.nums[i]);
        // since we may have added a new indirect block number to the double-indirect data block, write it back
        doubleBlock.put();
        if (blockNum != 0)
            return blockNum;
    }

    return 0; // if the file is too big to fit inside of double-indirect blocks, fail
}

// checks if this directory contains no file entries
bool CachedINode::is_dir_empty() {
    if (!S_ISDIR(inode.i_mode))
        return false; // if non-directory, default to false

    if (inode.i_links_count > 2)
        return false; // not empty if there are more links than just . and ..

    DataBlock block(device);
    block.get(inode.i_block[0]);
    DirectoryEntry* dirEntry = (DirectoryEntry*)&block.buffer[PARENT_DIR_ENTRY_OFFSET]; // look at entry for parent directory

    // is empty if the record length for .. is the rest of the block
    return dirEntry->rec_len == (BLOCK_SIZE - PARENT_DIR_ENTRY_OFFSET);
}

// list the contents of this directory
void CachedINode::ls_dir() {
    CachedINode* file; // cached inode used for the entries in this directory
    for (const auto& entry : Directory(this)) {
        file = fs.inodeTable.get(device, entry.inodeNum);
        file->ls_file(entry.name);
        file->put();
    }
}

// list the attributes of this file
void CachedINode::ls_file(const std::string& name) {
    std::stringstream stream;
    stream << mode();
    stream << std::setw(5) << inode.i_links_count; // link count
    stream << std::setw(5) << inode.i_gid; // gid
    stream << std::setw(5) << inode.i_uid; // uid
    stream << std::setw(8) << inode.i_size; // file size
    time_t timer = inode.i_ctime;
    std::string fileTime(ctime(&timer)); // convert time value into a string
    stream << " " << fileTime.substr(0, fileTime.size() - 1); // remove the newline character
    stream << " " << name;
    if (linkname() != "")
        stream << " -> " << linkname();
    std::cout << stream.str() << "\n";
}

// print basic info about this file
void CachedINode::stat() {
    time_t timer = inode.i_ctime;
    std::string fileTime(ctime(&timer)); // convert time value into a string

    // following example of simulator.bin sample project file
    printf("dev : %3d  ino: %3d  size : %d\n", device->fd, inodeNum, inode.i_size);
    printf("uid : %3d  gid: %3d  links: %d\n", inode.i_uid, inode.i_gid, inode.i_links_count);
    printf("mode: %s, 0x%x, 0%o\n", mode().c_str(), inode.i_mode, inode.i_mode);
    printf("time: %s\n", fileTime.substr(0, fileTime.size() - 1).c_str());
}

// initialize the inode structure for this new file
void CachedINode::create_file_inode() {
    bzero(&inode, sizeof(INode));
    inode.i_mode = REG_FILE_MODE;
    inode.i_uid = fs.running->uid; // owner user ID
    inode.i_gid = fs.running->gid; // group ID
    inode.i_size = 0; // empty file, no data blocks
    inode.i_links_count = 1; // links count=1 because of parent directory
    inode.i_atime = time(0L); // set access to current time
    inode.i_ctime = time(0L); // set inode change to current time
    inode.i_mtime = time(0L); // set modification to current time
    isDirty = true;
}

// modify a regular file inode to make it a symbolic link
void CachedINode::create_symlink_inode(const std::string& srcName) {
    inode.i_mode = LNK_FILE_MODE;
    // hijack the i_block array area to store the full absolute path name of source file
    strcpy((char*)inode.i_block, srcName.c_str());
    // hijack the i_size field store the length of the source file's name
    inode.i_size = static_cast<__u32>(srcName.length());
    inode.i_ctime = static_cast<__u32>(time(0L)); // update inode change time
    isDirty = true;
}

// initialize the inode structure for this new directory
void CachedINode::make_dir_inode(int blockNum) {
    bzero(&inode, sizeof(INode));
    inode.i_mode = DIR_FILE_MODE;
    inode.i_uid = fs.running->uid; // owner user ID
    inode.i_gid = fs.running->gid; // group ID
    inode.i_size = BLOCK_SIZE; // directories start with 1 data block to store the . and .. entries
    inode.i_links_count = 2; // Links count=2 because of . and ..
    inode.i_atime = time(0L); // set access to current time
    inode.i_ctime = time(0L); // set inode change to current time
    inode.i_mtime = time(0L); // set modification to current time
    inode.i_blocks = 2; // number of 512-bytes blocks reserved to contain the data of this inode
    inode.i_block[0] = blockNum; // new DIR has one data block
    isDirty = true;
}

// add an entry to this directory for a new file/sub-directory
void CachedINode::make_dir_entry(const std::string& name, int inodeNum) {
    int idealLength = 4 * ((8 + name.length() + 3) / 4); // the new entry's ideal length
    int remaining; // the actual length for the new entry will be all the remaining space in the block

    auto dir = Directory(this);
    for (const auto& entry : dir) {
        if (entry.isLast) {
            remaining = entry.length - entry.idealLength;
            if (idealLength <= remaining) {
                dir.appendEntry(name, inodeNum, remaining);
                return;
            }
        }
    }

    // no space in existing data blocks, so create a new one
    int blockNum = device->allocate(BLOCK);
    dir.createEntry(name, inodeNum, blockNum);
    isDirty = true;
}

// delete an entry from this directory
void CachedINode::remove_dir_entry(const std::string& name) {
    auto dir = Directory(this);
    for (const auto& entry : dir) {
        if (entry.name == name) {
            dir.removeEntry();
            inode.i_ctime = time(0L); // update inode change time
            isDirty = true; // we modified this cached inode, so mark it as dirty
            return;
        }
    }
}

// erases a file; deallocates all its blocks, clears i_block[], and sets size to 0
void CachedINode::truncate() {
    if (S_ISLNK(inode.i_mode))
        return; // symbolic links have no data blocks to deallocate
    DataBlock block(device);
    for (int i = 0; i < EXT2_NDIR_BLOCKS; i++) {
        if (inode.i_block[i] == 0)
            break;
        device->deallocate(BLOCK, inode.i_block[i]);
    }
    if (inode.i_block[EXT2_IND_BLOCK] != 0)
        truncate_indirect(device, inode.i_block[EXT2_IND_BLOCK]);
    if (inode.i_block[EXT2_DIND_BLOCK]) {
        block.get(inode.i_block[EXT2_DIND_BLOCK]);
        for (int i = 0; i < BLOCKNUMS_PER_BLOCK; i++) {
            if (!block.nums[i])
                break;
            truncate_indirect(device, block.nums[i]);
        }
    }
    bzero(inode.i_block, EXT2_N_BLOCKS * sizeof(int)); // erase all the block numbers
    inode.i_atime = time(0L); // update file accessed time
    inode.i_ctime = time(0L); // update inode change time
    inode.i_mtime = time(0L); // update file modified time
    inode.i_size = 0;
    isDirty = true;
}

// decrement reference count; if no longer in use and it was modified, write back cached inode data to its device
void CachedINode::put() {
    refCount--;
    TRACE(3, "reference count for cached inode [%d, %d] at address %p is now %d\n", device->fd, inodeNum, this, refCount);

    if (refCount > 0)
        return;
    if (!isDirty)
        return;
    isDirty = false; // clear isDirty flag

    TRACE(1, "writing back dev=%d, ino=%d\n", device->fd, inodeNum);
    int blockNum = ((inodeNum - 1) / INODES_PER_BLOCK) + device->inodeStart;
    int entry = (inodeNum - 1) % INODES_PER_BLOCK;
    DataBlock block(device);
    block.get(blockNum);
    block.inodes[entry] = inode; // replace device data with the data from memory
    block.put();
}

// attempt to allocate a new data block and save its block number in an indirect block
int CachedINode::allocate_indirect(MountedDevice* device, int* indirectBlockNum) {
    DataBlock block(device);

    // if the indirect block doesn't already exist (its block number is 0), create a new one
    // and populate the indrector block number so it is modified in the calling function
    if (*indirectBlockNum == 0) {
        *indirectBlockNum = device->allocate(BLOCK);
        block.nums[0] = device->allocate(BLOCK); // assign the first indirect block entry
        block.put(*indirectBlockNum);
        return block.nums[0];
    }

    block.get(*indirectBlockNum); // get the existing indirect block
    for (int i = 0; i < BLOCKNUMS_PER_BLOCK; i++) {
        if (block.nums[i] == 0) {
            block.nums[i] = device->allocate(BLOCK); // assign the next available block entry
            block.put();
            return block.nums[i];
        }
    }

    return 0; // could not allocate an entry in this indirect block
}

// deallocate all the data blocks listed in an indirect block
void CachedINode::truncate_indirect(MountedDevice* device, int indirectBlockNum) {
    DataBlock block(device);
    block.get(indirectBlockNum);
    for (int i = 0; i < BLOCKNUMS_PER_BLOCK; i++) {
        if (!block.nums[i])
            break;
        device->deallocate(BLOCK, block.nums[i]);
    }
}
