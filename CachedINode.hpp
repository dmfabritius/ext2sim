#pragma once
#include "main.hpp"
class MountedDevice;

// a file's inode cached in memory
class CachedINode {
public:
    INode inode; // EXT2 inode structure
    MountedDevice* device; // device on which the inode is located
    int inodeNum; // inode number
    int refCount = 0; // number of times this inode is currently being used by the simulation program
    bool isDirty = false; // does this cached data need to be written to the disk?
    CachedINode* deviceRoot = nullptr; // root inode of the device mounted at this point

    std::string fullpath(); // find the full absolute path of this diretory
    std::string linkname(); // for symbolic link files, returns the absolute pathname that it links to
    std::string mode(); // returns this file's mode as a string, e.g., 0644 is -rw-r--r--
    std::string search(int targetINodeNum); // search this directory for a given inode number and return its name
    int search(const std::string& targetName); // search this directory for a given name and return its inode number
    int logical2physical(int logicalBlockNum); // convert a logical block number for this file into an actual block number
    int allocate_block(); // get a new data block number and update the inode i_block[] structure
    bool is_dir_empty(); // checks if this directory contains no file entries
    void ls_dir(); // list the contents of this directory
    void ls_file(const std::string& filename); // list the attributes of this file
    void stat(); // print basic info about this file
    void create_file_inode(); // initialize the inode structure for this new file
    void create_symlink_inode(const std::string& srcName); // modify a regular file inode to make it a symbolic link
    void make_dir_inode(int blockNum); // initialize the inode structure for this new directory
    void make_dir_entry(const std::string& name, int inodeNum); // add an entry to this directory for a new file/sub-directory
    void remove_dir_entry(const std::string& name); // delete an entry from this directory
    void truncate(); // erases a file; deallocates all its blocks, clears i_block[], and sets size to 0
    void put(); // decrement reference count; if no longer in use and it was modified, write back cached inode data to its device

private:
    int allocate_indirect(MountedDevice* device, int* indirectBlockNum); // attempt to allocate a new data block in an indirect block
    void truncate_indirect(MountedDevice* device, int indirectBlockNum); // deallocate all the data blocks listed in an indirect block
};
