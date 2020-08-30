#include "FileSystem.hpp"
#include "DataBlock.hpp"
#include "Directory.hpp"
#include "PathComponents.hpp"

// return a cached inode from the inode table for a given device and inode number
CachedINode* INodeTable::get(MountedDevice* device, int inodeNum) {
    for (CachedINode& c : inodes) {
        if (c.refCount && c.device == device && c.inodeNum == inodeNum) {
            c.refCount++;
            TRACE(3, "reference count for cached inode [%d, %d] at address %p is now %d\n", c.device->fd, c.inodeNum, &c, c.refCount);
            return &c;
        }
    }

    for (CachedINode& c : inodes) {
        if (c.refCount == 0) {
            TRACE(3, "allocating cached inode [%d, %d] at address %p\n", device->fd, inodeNum, &c);
            c.refCount = 1;
            c.device = device;
            c.inodeNum = inodeNum;
            c.isDirty = false;
            c.deviceRoot = nullptr;

            // find the desired entry in the device's inode table
            int blockNum = (inodeNum - 1) / INODES_PER_BLOCK + device->inodeStart;
            int entry = (inodeNum - 1) % INODES_PER_BLOCK;
            TRACE(2, "device number=%d, inode number=%d is stored at block number=%d, inode entry=%d\n", device->fd, inodeNum, blockNum, entry);

            DataBlock block(device);
            block.get(blockNum);
            c.inode = block.inodes[entry];
            return &c;
        }
    }
    std::cerr << "PANIC: no more free entries in the cached inodes table\n";
    exit(FAILURE);
}

// return a cached inode from the inode table for a given file or directory
CachedINode* INodeTable::get(const std::string& pathname) {
    CachedINode* file; // the inode of the file or directory for we're looking for
    MountedDevice* device; // the device on which the file is located
    int inodeNum; // the inode number of the file

    if (pathname == "/") {
        fs.root->refCount++;
        return fs.root;
    }

    if (pathname[0] == '/') { // given an absolute pathname, start in the file system's root
        device = fs.root->device;
        inodeNum = fs.root->inodeNum;
    } else { // otherwise, start in the cwd of the file system's running process
        device = fs.running->cwd->device;
        inodeNum = fs.running->cwd->inodeNum;
    }

    file = get(device, inodeNum);
    PathComponents path(pathname);
    for (const std::string& name : path.names) {
        // check to see if we are traversing up through a mount point and need to change devices
        if (name == ".." && file == device->root) {
            file->put();
            file = get(device->mountPoint->device, device->mountPoint->inodeNum);
        }
        inodeNum = file->search(name);
        if (inodeNum == 0) {
            file->put();
            TRACE(1, "name '%s' does not exist\n", name.c_str());
            return nullptr;
        }
        file->put();
        file = get(file->device, inodeNum); // get the next cached INode using the new inode number
        // check to see if we are traversing down through a mount point and need to change devices
        if (file->deviceRoot) {
            file->put();
            file = get(file->deviceRoot->device, file->deviceRoot->inodeNum);
        }
    }
    TRACE(1, "pathname = '%s' is on device %d, inode number %d\n", pathname.c_str(), device->fd, inodeNum);
    return file;
}

// check whether a given device is being used by any of the currently cached inodes
bool INodeTable::device_busy(MountedDevice* device) {
    // one reference to the device's root (which comes from simply having it mounted) is okay; otherwise the device is busy
    for (const CachedINode& c : inodes)
        if (c.refCount != 0 && c.device == device)
            return &c != device->root || c.refCount != 1;
    return false; // this should never be reached since the root of the file system is always cached
}

// display all the currently cached inodes
void INodeTable::display() {
    for (const CachedINode& c : inodes) {
        if (c.refCount)
            printf("reference count for cached inode [%d, %d] at address %p is %d\n", c.device->fd, c.inodeNum, &c, c.refCount);
    }
    std::cout << "root points to address " << fs.root << "\n";
    std::cout << "cwd  points to address " << fs.running->cwd << "\n";
}

// clear the cached inode table, writing back any modified entries
void INodeTable::flush() {
    for (CachedINode& c : inodes)
        if (c.refCount > 0 && c.isDirty) c.put();
}

// list the contents of a directory or display a file's attributes
int INodeTable::ls(const std::string& pathname) {
    CachedINode* file = get(pathname);
    if (!file) {
        std::cerr << "ls: cannot list, " << pathname << " not found\n";
        return FAILURE;
    }
    if (S_ISDIR(file->inode.i_mode)) {
        file->ls_dir();
    } else {
        file->ls_file(pathname);
    }
    file->put(); // free cached inode
    return SUCCESS;
}

// create a new file and return its inode number, or 0 if error
int INodeTable::creat(const std::string& pathname) {
    if (pathname == "") {
        std::cerr << "creat: cannot create file, no name given\n";
        return 0;
    }

    PathComponents path(pathname);
    CachedINode* parent = get(path.parent);
    if (!parent) {
        std::cerr << "creat: cannot make directory, " << path.parent << " does not exist\n";
        return 0;
    }

    if (!S_ISDIR(parent->inode.i_mode)) {
        std::cerr << "creat: cannot create file, " << path.parent << " is not a directory\n";
        parent->put();
        return 0;
    }
    if (parent->search(path.child)) {
        std::cerr << "creat: cannot create file, " << path.child << " already exists in " << path.parent << "\n";
        parent->put();
        return 0;
    }

    int inodeNum = create_file_inode(parent);
    if (inodeNum) {
        parent->make_dir_entry(path.child, inodeNum);
        parent->inode.i_atime = time(0L); // set to current time
        parent->inode.i_ctime = time(0L); // update inode change time
        parent->isDirty = true;
    } else {
        std::cerr << "creat: cannot create file, unable to allocate inode\n";
    }

    parent->put();
    return inodeNum;
}

// make a directory and return its inode number, 0 if error
int INodeTable::mkdir(const std::string& pathname) {
    if (pathname == "") {
        std::cerr << "mkdir: cannot make directory, no name given\n";
        return 0;
    }

    PathComponents path(pathname);
    CachedINode* parent = get(path.parent);
    if (!parent) {
        std::cerr << "mkdir: cannot make directory, " << path.parent << " does not exist\n";
        return 0;
    }
    if (!S_ISDIR(parent->inode.i_mode)) {
        std::cerr << "mkdir: cannot make directory, " << path.parent << " is not a directory\n";
        parent->put();
        return 0;
    }
    if (parent->search(path.child)) {
        std::cerr << "mkdir: cannot make directory, " << path.child << " already exists in " << path.parent << "\n";
        parent->put();
        return 0;
    }

    int inodeNum = make_dir_inode(parent);
    if (inodeNum) {
        parent->make_dir_entry(path.child, inodeNum);
        parent->inode.i_links_count++;
        parent->inode.i_atime = time(0L); // set to current time
        parent->inode.i_ctime = time(0L); // update inode change time
        parent->isDirty = true;
    } else {
        std::cerr << "mkdir: cannot make directory, unable to allocate inode and/or data block\n";
    }

    parent->put();
    return inodeNum;
}

// remove a directory
int INodeTable::rmdir(const std::string& pathname) {
    if (pathname == "") {
        std::cerr << "rmdir: cannot remove directory, no name given\n";
        return FAILURE;
    }
    PathComponents path(pathname);
    if (path.child == ".") {
        std::cerr << "rmdir: cannot remove reference to current directory\n";
        return FAILURE;
    }
    if (path.child == "..") {
        std::cerr << "rmdir: cannot remove reference to parent directory\n";
        return FAILURE;
    }
    if (path.child == "/") {
        std::cerr << "rmdir: cannot remove reference to root directory\n";
        return FAILURE;
    }

    CachedINode* child = get(pathname);
    if (!child) {
        std::cerr << "rmdir: cannot remove " << pathname << ", path not found\n";
        return FAILURE;
    }
    if (!S_ISDIR(child->inode.i_mode)) {
        std::cerr << "rmdir: cannot remove, " << pathname << " is not a directory\n";
        child->put();
        return FAILURE;
    }
    if (child->refCount != 1) {
        std::cerr << "rmdir: cannot remove, " << pathname << " is in use\n";
        child->put();
        return FAILURE;
    }
    if (!child->is_dir_empty()) {
        std::cerr << "rmdir: cannot remove, " << pathname << " is not empty\n";
        child->put();
        return FAILURE;
    }
    // Deallocate all the directory's data blocks and its inode
    for (int i = 0; i < EXT2_NDIR_BLOCKS; i++) {
        if (child->inode.i_block[i] == 0) continue; // unless something is corrupt, this is could be a break
        child->device->deallocate(BLOCK, child->inode.i_block[i]);
    }
    child->device->deallocate(INODE, child->inodeNum);
    child->put();

    // Update parent directory data and attributes

    CachedINode* parent = get(path.parent);
    parent->remove_dir_entry(path.child);
    parent->inode.i_links_count--; // the child directory is no longer pointing back to the parent
    parent->inode.i_atime = time(0L);
    parent->inode.i_mtime = time(0L);
    parent->inode.i_ctime = time(0L); // update inode change time
    parent->isDirty = true;
    parent->put();

    return SUCCESS;
}

// link a new file name to an existing file; create a new reference to the original file's inode
int INodeTable::link(const std::string& srcName, const std::string& dstName, bool isMoving) {

    if (srcName == "") {
        std::cerr << "link: cannot link, no source name given\n";
        return FAILURE;
    }
    if (dstName == "") {
        std::cerr << "link: cannot link, no destination name given\n";
        return FAILURE;
    }
    CachedINode* name = get(dstName);
    if (name) {
        std::cerr << "link: cannot link file, " << dstName << " already exists\n";
        name->put();
        return FAILURE;
    }

    PathComponents dstPath(dstName);
    CachedINode* src = get(srcName);
    CachedINode* dst = get(dstPath.parent);

    if (!src) {
        std::cerr << "link: source file not found\n";
        src->put();
        dst->put();
        return FAILURE;
    }
    if (S_ISDIR(src->inode.i_mode) && !isMoving) {
        std::cerr << "link: cannot link file, " << srcName << " is a directory\n";
        src->put();
        dst->put();
        return FAILURE;
    }
    if (!dst || !S_ISDIR(dst->inode.i_mode)) {
        std::cerr << "link: cannot link file, " << dstPath.parent << " is not a directory\n";
        src->put();
        dst->put();
        return FAILURE;
    }

    if (src->device != dst->device) {
        std::cerr << "link: cannot link, source and destination are on different devices\n";
        src->put();
        dst->put();
        return FAILURE;
    }

    // create the link
    dst->make_dir_entry(dstPath.child, src->inodeNum);
    src->inode.i_links_count++;
    src->inode.i_ctime = static_cast<__u32>(time(0L)); // update inode change time
    src->isDirty = true;
    src->put();
    dst->put();

    return SUCCESS;
}

// remove a reference to an inode; delete the file if the reference count becomes 0
int INodeTable::unlink(const std::string& pathname, bool isMoving) {

    if (pathname == "") {
        std::cerr << "rm/unlink: cannot unlink, no name given\n";
        return -1;
    }

    PathComponents path(pathname);
    CachedINode* file = get(pathname);
    if (!file) {
        std::cerr << "unlink: cannot remove " << pathname << ", file not found\n";
        return -1;
    }
    if (S_ISDIR(file->inode.i_mode) && !isMoving) {
        std::cerr << "unlink: cannot remove, " << pathname << " is a directory\n";
        file->put();
        return -1;
    }
    if (!isMoving && file->refCount > 1) {
        std::cerr << "unlink: cannot remove " << pathname << ", file in use\n";
        return -1;
    }
    if (--file->inode.i_links_count == 0) {
        TRACE(1, "no remaining links, deleting %s\n", pathname.c_str());
        file->truncate(); // deallocate the file's data blocks
        file->device->deallocate(INODE, file->inodeNum);
    }
    file->isDirty = true;
    file->put();

    CachedINode* dir = get(path.parent);
    dir->remove_dir_entry(path.child);
    dir->put();

    return SUCCESS;
}

// create an inode that stores the path to a different file/directory
int INodeTable::symlink(const std::string& srcName, const std::string& dstName) {
    // assume source name has <= 60 chars, inlcuding the ending nullptr byte

    if (srcName == "") {
        std::cerr << "symlink: cannot create symbolic link, no source name given\n";
        return FAILURE;
    }
    if (srcName[0] != '/') {
        std::cerr << "symlink: source name must be an absolute path\n";
        return FAILURE;
    }
    if (dstName == "") {
        std::cerr << "symlink: cannot create symbolic link, no destination name given\n";
        return FAILURE;
    }

    PathComponents dstPath(dstName);
    CachedINode* name = get(dstName);
    if (name) {
        std::cerr << "symlink: cannot create symbolic link, " << dstPath.child << " already exists in " << dstPath.parent << "\n";
        name->put();
        return FAILURE;
    }

    CachedINode* src = get(srcName);
    CachedINode* dst = get(dstPath.parent);

    if (!src) {
        std::cerr << "symlink: source file not found\n";
        src->put();
        dst->put();
        return FAILURE;
    }
    if (!S_ISREG(src->inode.i_mode) && !S_ISDIR(src->inode.i_mode)) {
        std::cerr << "symlink: cannot create symbolic link, " << srcName << " must be a file or directory\n";
        src->put();
        dst->put();
        return FAILURE;
    }
    if (!dst || !S_ISDIR(dst->inode.i_mode)) {
        std::cerr << "symlink: cannot create symbolic link file, " << dstPath.parent << " is not a directory\n";
        src->put();
        dst->put();
        return FAILURE;
    }

    int inodeNum = creat(dstName);
    if (!inodeNum) {
        std::cerr << "symlink: cannot create symbolic link, unable to create file " << dstName << "\n";
        src->put();
        dst->put();
        return FAILURE;
    }

    // create the link
    CachedINode* symlink = get(dst->device, inodeNum);
    symlink->create_symlink_inode(srcName);
    symlink->put();

    src->put();
    dst->put();
    return SUCCESS;
}

// display basic information about a file
int INodeTable::stat(const std::string& pathname) {
    if (pathname == "") {
        std::cerr << "stat: cannot display status, no name given\n";
        return FAILURE;
    }
    CachedINode* file = get(pathname);
    if (!file) {
        std::cerr << "stat: cannot display status, file not found\n";
        return FAILURE;
    }
    file->stat();
    file->put();
    return SUCCESS;
}

// change a file's mode (permissions)
int INodeTable::chmod(const std::string& mode, const std::string& pathname) {
    // convert string to long, inferring base of input from its format, e.g., leading 0x for hex, 0 to octal
    long int modeValue = strtol(mode.c_str(), NULL, 0);
    if (modeValue > 511) {
        std::cerr << "chmod: invalid mode\n";
        return FAILURE;
    }
    if (pathname == "") {
        std::cerr << "chmod: cannot change mode, no name given\n";
        return FAILURE;
    }

    CachedINode* file = get(pathname);
    if (file) {
        file->inode.i_mode &= 0xF000; // clear low-order permission bits
        file->inode.i_mode |= modeValue; // set permission bits
        file->inode.i_ctime = time(0L); // update inode change time
        file->isDirty = true;
        file->put();
    } else {
        std::cerr << "chmod: cannot change mode, file not found\n";
    }
    return SUCCESS;
}

// update the file's access and inode change times
int INodeTable::utime(const std::string& pathname) {
    if (pathname == "") {
        std::cerr << "utime: cannot update time, no name given\n";
        return FAILURE;
    }

    CachedINode* file = get(pathname);
    if (!file) {
        std::cerr << "utime: cannot update time, file not found\n";
        return FAILURE;
    }
    file->inode.i_atime = time(0L); // update access time
    file->inode.i_ctime = time(0L); // update inode change time
    file->isDirty = true;
    file->put();
    return SUCCESS;
}

// copy a file
int INodeTable::cp(const std::string& srcName, const std::string& dstName) {
    char dataBlock[BLOCK_SIZE];
    int numBytes;

    int srcFileDescriptor = fs.running->open(srcName, READ);
    if (srcFileDescriptor == -1) {
        std::cerr << "cp: cannot open the source file for read\n";
        return FAILURE;
    }

    CachedINode* file = get(dstName);
    if (!file) { // if the dest file doesn't exist, create it
        if (!creat(dstName)) {
            std::cerr << "cp: cannot create the destination file\n";
            return FAILURE;
        }
    } else {
        file->put(); // if it does exist, discard the cached inode
    }

    int dstFileDescriptor = fs.running->open(dstName, WRITE);
    if (dstFileDescriptor == -1) {
        std::cerr << "cp: cannot open the destination file for write\n";
        return FAILURE;
    }

    while ((numBytes = fs.running->read(srcFileDescriptor, dataBlock, BLOCK_SIZE))) {
        fs.running->write(dstFileDescriptor, dataBlock, numBytes);
    }

    fs.running->close(srcFileDescriptor);
    fs.running->close(dstFileDescriptor);

    return SUCCESS;
}

// move/rename a file
int INodeTable::mv(const std::string& srcName, const std::string& dstName) {
    if (srcName == "") {
        std::cerr << "mv: cannot move file, no source name given\n";
        return FAILURE;
    }
    if (dstName == "") {
        std::cerr << "mv: cannot move file, no destination name given\n";
        return FAILURE;
    }

    CachedINode* name = get(dstName);
    if (name) { // if file exists, remove it; copy in overwrite mode
        name->put(); // discard cached inode of existing file
        if (unlink(dstName) != SUCCESS) {
            std::cerr << "mv: cannot overwrite existing file\n";
            return FAILURE;
        }
    }

    PathComponents dstPath(dstName);
    CachedINode* src = get(srcName);
    CachedINode* dst = get(dstPath.parent);

    if (!dst || !S_ISDIR(dst->inode.i_mode)) {
        std::cerr << "mv: cannot move file, " << dstPath.parent << " is not a directory\n";
        src->put();
        dst->put();
        return FAILURE;
    }

    if (src->device == dst->device) {
        if (link(srcName, dstName, true) != SUCCESS) { // move to same device
            std::cerr << "mv: cannot move file to same device\n";
            src->put();
            dst->put();
            return FAILURE;
        }
    } else {
        if (cp(srcName, dstName) != SUCCESS) {
            std::cerr << "mv: cannot move file to different device\n";
            src->put();
            dst->put();
            return FAILURE;
        }
    }
    if (unlink(srcName, true) != SUCCESS) {
        std::cerr << "mv: file copied to destination, but cannot remove source file\n";
        src->put();
        dst->put();
        return FAILURE;
    }

    src->put();
    dst->put();
    return SUCCESS;
}

// allocate and initialize an inode for a new file and return its number, or 0 if error
int INodeTable::create_file_inode(CachedINode* parent) {
    int inodeNum = parent->device->allocate(INODE);
    TRACE(1, "inode #%d\n", inodeNum);
    CachedINode* file = get(parent->device, inodeNum);
    file->create_file_inode();
    file->put(); // write new INode to disk
    return inodeNum;
}

// allocate and initialize an inode for a new directory and return its number, or 0 if error
int INodeTable::make_dir_inode(CachedINode* parent) {
    int inodeNum = parent->device->allocate(INODE);
    int blockNum = parent->device->allocate(BLOCK);
    TRACE(1, "inode #%d, block #%d\n", inodeNum, blockNum);

    CachedINode* dir = get(parent->device, inodeNum);
    dir->make_dir_inode(blockNum);
    Directory(dir).init(inodeNum, blockNum, parent->inodeNum);
    dir->put();

    return inodeNum;
}
