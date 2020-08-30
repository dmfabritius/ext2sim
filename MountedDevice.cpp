#include "FileSystem.hpp"
#include "DataBlock.hpp"

// open a Linux disk image file and initialize this device object
int MountedDevice::mount() {
    TRACE(1, "mounting Linx file %s as %s in simulated file system\n", diskImage.c_str(), mountPath.c_str());
    if ((fd = open(diskImage.c_str(), O_RDWR)) < 0) {
        std::cerr << "mount: cannot open disk image " << diskImage << "\n";
        return FAILURE;
    }
    DataBlock block = DataBlock(this);
    block.get(SUPER_BLOCK);
    SuperBlock* sp = (SuperBlock*)block.buffer;
    if (sp->s_magic != EXT2_SUPER_MAGIC) {
        std::cerr << "mount: " << diskImage << " is not an ext2 filesystem (magic = " << std::hex << sp->s_magic << ")\n";
        return FAILURE;
    }
    TRACE(2, "%s is an EXT2 file system\n", diskImage.c_str());

    ninodes = sp->s_inodes_count;
    nifree = sp->s_free_inodes_count;
    nblocks = sp->s_blocks_count;
    nbfree = sp->s_free_blocks_count;
    TRACE(2, "num inodes = %d, num blocks = %d\n", ninodes, nblocks);

    block.get(GROUP_DESCRIPTOR_0); // read group descriptor block
    GroupDescriptor* gp = (GroupDescriptor*)block.buffer;
    bmap = gp->bg_block_bitmap;
    imap = gp->bg_inode_bitmap;
    inodeStart = gp->bg_inode_table;
    TRACE(2, "block bitmap = %d, inode bitmap = %d, inode table start = %d\n", bmap, imap, inodeStart);

    root = fs.inodeTable.get(this, ROOT_DIR_INODE_NUM); // cache the root of the device
    mountPoint = root; // by default, the device is mounted at its own root

    return SUCCESS;
}

// close the disk image file and mark this device object as free
int MountedDevice::umount() {
    if (fs.inodeTable.device_busy(this)) {
        std::cerr << "umount: cannot unmount, device is busy\n";
        return FAILURE;
    }
    close(fd);
    fd = -1; // mark mount table entry as unused
    mountPoint->deviceRoot = nullptr; // clear pointer to the unmounted device's root inode
    mountPoint->put(); // release the cached inode for the device's mount point
    root->put(); // release the cached inode for the device's root

    return SUCCESS;
}

// allocate a block/inode
int MountedDevice::allocate(BitmapType type) {
    DataBlock block(this);
    const char* types[2] = { "inode", "block" };
    int bitmap = (type == INODE) ? imap : bmap;
    int size = (type == INODE) ? ninodes : nblocks;

    block.get(bitmap);
    for (int i = 0; i < size; i++) {
        if (!block.test_bit(i)) {
            block.set_bit(i);
            block.put(); // write back modified dataBlock
            update_free(type, -1);
            TRACE(2, "allocated %s number %d\n", types[type], i + 1);
            return i + 1;
        }
    }
    std::cerr << "PANIC: failed to allocate new " << types[type] << "\n";
    exit(FAILURE); // terminate the program
}

//deallocate a block/inode
void MountedDevice::deallocate(BitmapType type, int num) {
    DataBlock block(this);
    const char* types[2] = { "inode", "block" };
    int bitmap = (type == INODE) ? imap : bmap;
    int size = (type == INODE) ? ninodes : nblocks;
    if (num >= size) {
        std::cerr << types[type] << " number " << num << " out of range for device " << fd << "\n";
        return;
    }
    block.get(bitmap);
    block.clear_bit(num - 1);
    block.put();
    update_free(type, 1);
    TRACE(2, "deallocated %s number %d\n", types[type], num);
}

// update count of free blocks/inodes
void MountedDevice::update_free(BitmapType type, short change) {
    DataBlock block(this);
    const char* types[2] = { "inodes", "blocks" };
    int count;

    block.get(SUPER_BLOCK);
    SuperBlock* sp = (SuperBlock*)block.buffer;
    if (type == INODE) {
        sp->s_free_inodes_count += change;
        count = nifree = sp->s_free_inodes_count;
    } else {
        sp->s_free_blocks_count += change;
        count = nbfree = sp->s_free_blocks_count;
    }
    block.put();

    block.get(GROUP_DESCRIPTOR_0);
    GroupDescriptor* gp = (GroupDescriptor*)block.buffer;
    if (type == INODE)
        gp->bg_free_inodes_count += change;
    else
        gp->bg_free_blocks_count += change;
    block.put();
    TRACE(2, "changed number of free %s by %d on device %d, count is now %d\n", types[type], change, fd, count);
}
