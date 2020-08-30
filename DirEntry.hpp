#pragma once
#include "main.hpp"
class DataBlock;
class CachedINode;

// an entry in a Directory
class DirEntry {
public:
    DataBlock* block; // a block of directory data
    char* entry; // pointer into the directory data block buffer
    DirectoryEntry* prevEntry; // the previous entry
    DirectoryEntry* dirEntry; // used to map the Directory Entry structure onto the raw data in the data block
    std::string name; // current directory entry's filename
    int inodeNum = 0; // current directory entry's inode number
    int length; // the actual length of this entry
    int idealLength; // the "correct" length for this entry; the last entry in a block has a different actual length
    bool isLast; // is this the last entry in the current data block?

    DirEntry(DataBlock* block); // initialize DirEntry object
    void nextBlock(); // advance to the first entry in a newly loaded data block
    void nextEntry(); // advance to the next entry in the current data block
    void update(); // update all the directory entry fields for the current location in the data block
    void remove(); // remove entry from the middle of the current data block
};
