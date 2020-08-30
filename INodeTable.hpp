#pragma once
#include "CachedINode.hpp"
class MountedDevice;

// a fixed-size table of cached inodes
class INodeTable {
private:
    CachedINode inodes[INODE_TABLE_SIZE]; // table of inodes cached in memory

public:
    CachedINode* get(MountedDevice* device, int inodeNum); // return a cached inode from the table for a given device and inode number
    CachedINode* get(const std::string& pathname); // return a cached inode from the table for a given file or directory
    bool device_busy(MountedDevice* device); // check whether a given device is being used by any of the currently cached inodes
    void display(); // display all the currently cached inodes
    void flush(); // clear the cached inode table, writing back any modified entries

    int ls(const std::string& pathname); // list the contents of a directory or display a file's attributes
    int creat(const std::string& pathname); // create a new file and return its inode number, or 0 if error
    int mkdir(const std::string& pathname); // make a directory and return its inode number, 0 if error
    int rmdir(const std::string& pathname); // remove a directory
    int link(const std::string& srcName, const std::string& dstName, bool isMoving = false); // link a new file name to an existing file
    int unlink(const std::string& pathname, bool isMoving = false); // remove a reference to an inode; delete the file
    int symlink(const std::string& srcName, const std::string& dstName); // create an inode that stores the path to a different file/directory
    int stat(const std::string& pathname); // display basic information about a file
    int chmod(const std::string& mode, const std::string& pathname); // change a file's mode (permissions)
    int utime(const std::string& pathname); // update the file's access and inode change times
    int cp(const std::string& srcName, const std::string& dstName); // copy a file
    int mv(const std::string& srcName, const std::string& dstName); // move/rename a file

private:
    int create_file_inode(CachedINode* parent); // allocate and initialize an inode for a new file
    int make_dir_inode(CachedINode* parent); // allocate and initialize an inode for a new directory
};
