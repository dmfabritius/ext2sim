#pragma once
class CachedINode;

// valid device bitmaps: INODE, BLOCK
enum BitmapType {
    INODE,
    BLOCK
};

// a Linux disk image used as a file system device
class MountedDevice {
public:
    std::string diskImage; // name of Linux disk image file
    int fd = -1; // Linux file descriptor of mounted disk image file; a -1 indicates this device object is free/unused/available
    int nblocks; // the total number of blocks
    int nbfree; // number of free blocks
    int ninodes; // the total number of inodes
    int nifree; // number of free inodes
    int bmap; // the block number where the free/used blocks bitmap is stored
    int imap; // the block number where the free/used indoes bitmap is stored
    int inodeStart; // the block number where the inodes table starts
    CachedINode* root; // a cached copy of this device's root inode
    CachedINode* mountPoint; // a cached copy of the inode in the primary file system where this device is mounted
    std::string mountPath; // absolute pathname of where the device is mounted in the simulated file system

    int mount(); // open a Linux disk image file and initialize this device object
    int umount(); // close the disk image file and mark this device object as free
    int allocate(BitmapType type); // allocate a block/inode
    void deallocate(BitmapType type, int num); //deallocate a block/inode
    void update_free(BitmapType type, short change); // update count of free blocks/inodes
};
