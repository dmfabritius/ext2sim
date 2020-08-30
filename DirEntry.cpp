#include "DirEntry.hpp"
#include "DataBlock.hpp"

// initialize DirEntry object
DirEntry::DirEntry(DataBlock* block)
    : block(block)
    , entry(block->buffer)
    , prevEntry(nullptr) {
    update();
}

// advance to the first entry in a newly loaded data block
void DirEntry::nextBlock() {
    entry = block->buffer;
    prevEntry = nullptr;
    update();
}

// advance to the next entry in the current data block
void DirEntry::nextEntry() {
    entry += dirEntry->rec_len; // advance to the next entry
    prevEntry = dirEntry;
    update();
}

// update all the directory entry fields for the current location in the data block
void DirEntry::update() {
    dirEntry = (DirectoryEntry*)entry; // get the current directory entry
    inodeNum = dirEntry->inode;
    name = std::string(dirEntry->name, dirEntry->name_len);
    length = dirEntry->rec_len;
    idealLength = 4 * ((8 + (int)name.length() + 3) / 4);
    isLast = (entry + length == block->buffer + BLOCK_SIZE);
    TRACE(3, "directory entry: %s (inode %d, rec_len %d)\n", name.c_str(), inodeNum, length);
}

// remove entry from the middle of the current data block
void DirEntry::remove() {
    int removedEntrySize = dirEntry->rec_len;
    int copySize = block->buffer + BLOCK_SIZE - entry - removedEntrySize;
    memcpy(entry, entry + dirEntry->rec_len, copySize); // move everything up, overwriting the deleted entry

    // advance to the last directory entry record
    while ((entry + dirEntry->rec_len + removedEntrySize) != (block->buffer + BLOCK_SIZE)) {
        entry += dirEntry->rec_len;
        dirEntry = (DirectoryEntry*)entry;
    }
    dirEntry->rec_len += removedEntrySize; // adjust the size of the last entry
}
