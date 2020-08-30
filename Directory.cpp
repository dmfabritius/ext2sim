#include "Directory.hpp"
#include "CachedINode.hpp"
#include "MountedDevice.hpp"

// initialize Directory object; load the first block and populate the first entry's info
Directory::Directory(CachedINode* cachedINode)
    : dirINode(&cachedINode->inode)
    , block(DataBlock(cachedINode->device))
    , index(0) {

    // if non-directory, set current entry to null
    if (S_ISDIR(dirINode->i_mode)) {
        block.get(dirINode->i_block[index]);
        current = new DirEntry(&block);
    } else {
        current = nullptr;
    }
}

// free the memory for the current entry object
Directory::~Directory() {
    if (current) delete current;
}

// initialize a new directory with the default directory entries
void Directory::init(int inodeNum, int blockNum, int parentINodeNum) {
    DirectoryEntry* dirEntry = (DirectoryEntry*)block.buffer;
    dirEntry->inode = inodeNum; // this directory's own inode number
    dirEntry->rec_len = 12;
    dirEntry->name_len = 1;
    dirEntry->name[0] = '.';
    dirEntry = (DirectoryEntry*)&block.buffer[PARENT_DIR_ENTRY_OFFSET];
    dirEntry->inode = parentINodeNum; // this directory's parent's inode number
    dirEntry->rec_len = BLOCK_SIZE - 12; // this is the last entry, so the size is the rest of the block
    dirEntry->name_len = 2;
    dirEntry->name[0] = '.';
    dirEntry->name[1] = '.';
    block.put(blockNum);
}

// get a directory entry, either from the current block or after loading the next block
DirEntry* Directory::next() {
    if (!current)
        return nullptr;
    if (!current->isLast) {
        current->nextEntry();
    } else {
        ++index;
        if (index == EXT2_NDIR_BLOCKS || !dirINode->i_block[index]) {
            // stop after direct blocks or when a block number of 0 is found
            return nullptr;
        }
        block.get(dirINode->i_block[index]);
        current->nextBlock();
    }
    return current;
}

// create a new directory entry in a new data block
void Directory::createEntry(const std::string& name, int inodeNum, int blockNum) {
    bzero(block.buffer, BLOCK_SIZE); // fill the buffer with zeros
    DirectoryEntry* dirEntry = (DirectoryEntry*)block.buffer;
    dirEntry->inode = inodeNum;
    dirEntry->rec_len = BLOCK_SIZE;
    dirEntry->name_len = name.length();
    strcpy(dirEntry->name, name.c_str());
    block.put(blockNum); // save the new block

    dirINode->i_size += BLOCK_SIZE;
    dirINode->i_block[index] = blockNum;
}

// insert a new directory entry at the end of an existing data block
void Directory::appendEntry(const std::string& name, int inodeNum, int rec_len) {
    current->dirEntry->rec_len = current->idealLength; // fix the size of the last dir entry's record length
    current->entry += current->dirEntry->rec_len; // advance to the next entry

    DirectoryEntry* dirEntry = (DirectoryEntry*)current->entry;
    dirEntry->inode = inodeNum;
    dirEntry->rec_len = rec_len;
    dirEntry->name_len = name.length();
    strcpy(dirEntry->name, name.c_str());
    block.put(); // save the updated block
}

// remove an entry from somewhere within a directory data block
void Directory::removeEntry() {
    if (current->dirEntry->rec_len == BLOCK_SIZE) {
        // FIRST and ONLY entry; throw away entire data block
        block.device->deallocate(BLOCK, dirINode->i_block[index]);
        dirINode->i_size -= BLOCK_SIZE;
        // if there are any non-zero data blocks after this one, scoot them up;
        // and because we're assuming no indirect blocks, we can "cheat" and
        // assume there is a zero after all the direct block numbers
        for (int j = index; j < EXT2_NDIR_BLOCKS; j++)
            dirINode->i_block[j] = dirINode->i_block[j + 1];
    } else if (current->isLast) {
        // LAST entry (preceded by other entries, but not followed by any)
        // simply adjust the length of the next-to-last record, indicating it's now the last
        current->prevEntry->rec_len += current->dirEntry->rec_len;
        block.put();
    } else {
        // MIDDLE entry (followed by other entries)
        current->remove();
        block.put();
    }
}

// create an iterator representing the beginning of the Directory entries
Directory::Iter Directory::begin() { return Iter(this, current); }

// create an interator representing the end of the Directory entries
Directory::Iter Directory::end() { return Iter(this, nullptr); }

// iterator for the entries in a directory
Directory::Iter::Iter(Directory* directory, DirEntry* entry)
    : directory(directory)
    , entry(entry) {}
Directory::Iter& Directory::Iter::operator++() {
    entry = directory->next();
    return *this;
}
bool Directory::Iter::operator!=(Iter rhs) const {
    return entry != rhs.entry;
}
DirEntry& Directory::Iter::operator*() {
    return *entry;
}
