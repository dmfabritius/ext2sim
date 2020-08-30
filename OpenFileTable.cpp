#include "OpenFileTable.hpp"

// get an open file by its inode
OpenFile* OpenFileTable::get(CachedINode* inode) {
    for (OpenFile& f : openFiles) {
        if (f.refCount != 0 && f.cachedINode == inode)
            return &f;
    }
    return nullptr;
}

// return a matching entry in the open file table, or initialize a new entry if needed
OpenFile* OpenFileTable::open(CachedINode* inode, OpenMode mode) {
    OpenFile* openFile = get(inode);
    if (openFile) {
        // file is already open; check for incompatible mode (only multiple read operations are okay)
        if (openFile->mode != READ) {
            std::cerr << "open: cannot open, file is already open for write\n";
            return nullptr;
        } else if (mode != READ) {
            std::cerr << "open: cannot open for write, file is already open\n";
            return nullptr;
        } else {
            openFile->refCount++; // mode is compatible, increment the reference count
        }
    } else {
        // file is not already open; allocate entry in the open file table
        for (OpenFile& f : openFiles) {
            if (f.refCount == 0) {
                f.open(inode, mode);
                openFile = &f;
                break;
            }
        }
        if (!openFile) {
            std::cerr << "open: cannot open, the global open file table is full\n";
            return nullptr;
        }
    }
    return openFile; // return address of the entry in the global open file table
}
