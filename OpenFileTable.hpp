#pragma once
#include "OpenFile.hpp"

// a fixed-size table of the file system's open files
class OpenFileTable {
private:
    OpenFile openFiles[OPEN_FILES_TABLE_SIZE];

public:
    OpenFile* get(CachedINode* inode); // get an open file by its inode
    OpenFile* open(CachedINode* inode, OpenMode mode); // return a matching entry in the open file table, or initialize a new entry
};
