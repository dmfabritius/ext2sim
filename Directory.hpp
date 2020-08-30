#pragma once
#include "DataBlock.hpp"
#include "DirEntry.hpp"
class CachedINode;

// an iterable container of directory entries
class Directory {
public:
    INode* dirINode; // the directory's inode
    DataBlock block; // a block of directory data
    int index; // index into the inode.i_block[] array
    DirEntry* current; // the current entry

    Directory(CachedINode* cachedInode); // initialize Directory object
    ~Directory(); // free the memory for the current entry object
    void init(int inodeNum, int blockNum, int parentINodeNum); // initialize a new directory with the default directory entries
    DirEntry* next(); // advance to the next entry
    void createEntry(const std::string& name, int inodeNum, int blockNum); // create a new directory entry in a new data block
    void appendEntry(const std::string& name, int inodeNum, int actualLength); // append a new directory entry at the end of an existing data block
    void removeEntry(); // remove an entry from somewhere within a directory data block

    class Iter { // iterator for the entries in this directory
    private:
        Directory* directory;
        DirEntry* entry;

    public:
        Iter(Directory* directory, DirEntry* entry); // constructor
        Iter& operator++(); // advance to next entry
        bool operator!=(Iter rhs) const; // test for inequality
        DirEntry& operator*(); // get a reference to the current entry
    };
    Iter begin(); // create an iterator representing the beginning of the Directory entries
    Iter end(); // create an interator representing the end of the Directory entries
};
