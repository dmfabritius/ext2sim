#include "Process.hpp"
#include "DataBlock.hpp"
#include "FileSystem.hpp"

// set the command prompt
std::string Process::prompt() {
    std::ostringstream stream;
    stream << '(' << cwd->device->fd << ' ' << cwd->inodeNum << "):" << cwd_path;
    return stream.str();
}

// display all the open files and their modes for the current process
void Process::display_open_files() {
    bool found = false;

    std::cout << "fd mode   offset [dev, inode]\n"
                 "-- ------ ------ ------------\n";
    int fd = 0;
    for (OpenFile* f : openFiles) {
        if (f) {
            std::cout << std::setw(2) << std::left << fd << ' ' << *f << '\n';
            found = true;
        }
        fd++;
    }
    if (!found) std::cout << "no open files\n";
}

// change current working directory
int Process::chdir(const std::string& pathname) {
    if (pathname == "") {
        if (cwd != fs.root) { // change to root if not there already
            cwd->put();
            cwd = fs.inodeTable.get(fs.root->device, fs.root->inodeNum);
            cwd_path = "/";
        }
        return SUCCESS;
    }

    CachedINode* dir = fs.inodeTable.get(pathname);
    if (!dir) {
        std::cerr << "cd: cannot change directory, " << pathname << " not found\n";
        return FAILURE;
    }
    if (!S_ISDIR(dir->inode.i_mode)) {
        std::cerr << "cd: cannot change directory, " << pathname << " is not a directory\n";
        dir->put();
        return FAILURE;
    }
    cwd->put();
    cwd = dir;
    cwd_path = dir->fullpath();
    pwd();
    return SUCCESS;
}

// print full absolute path name of the current working directory
int Process::pwd() {
    std::cout << cwd_path << "\n";
    return SUCCESS;
}

// open a file and return its file descriptor, or -1 on failure
int Process::open(const std::string& pathname, OpenMode mode) {
    int fileDescriptor;
    for (fileDescriptor = 0; fileDescriptor < PROCESS_FILE_DESCRIPTORS; fileDescriptor++) {
        if (openFiles[fileDescriptor] == nullptr) break; // found free slot for new file descriptor
    }
    if (fileDescriptor == PROCESS_FILE_DESCRIPTORS) {
        std::cerr << "open: cannot open file, the process's open file table is full\n";
        return -1;
    }
    if (pathname == "") {
        std::cerr << "open: cannot open file, no name given\n";
        return -1;
    }
    if (mode < READ || mode > APPEND) {
        std::cerr << "open: cannot open file, invalid mode\n"
                     "0=READ, 1=WRITE, 2=READ/WRITE, 3=APPEND\n";
        return -1;
    }

    CachedINode* file = fs.inodeTable.get(pathname);
    if (!file) {
        std::cerr << "open: cannot open " << pathname << ", file not found\n";
        return -1;
    }
    if (!S_ISREG(file->inode.i_mode)) {
        std::cerr << "open: cannot open, " << pathname << " is not a regular file\n";
        file->put();
        return -1;
    }
    openFiles[fileDescriptor] = fs.openFileTable.open(file, mode);
    if (openFiles[fileDescriptor] == nullptr) {
        file->put(); // cannot open file, release cached inode
        return -1;
    }

    file->isDirty = true;
    file->inode.i_atime = time(0L); // access time
    if (mode != READ) file->inode.i_mtime = time(0L); // modified time

    TRACE(1, "file opened in mode %d and assigned file descriptor %d\n", mode, fileDescriptor);
    return fileDescriptor;
}

// close an open file
int Process::close(int fileDescriptor) {
    if (fileDescriptor < 0 || fileDescriptor >= PROCESS_FILE_DESCRIPTORS) {
        std::cerr << "close: cannot close file, invalid file descriptor\n";
        return FAILURE;
    }
    if (!openFiles[fileDescriptor]) {
        std::cerr << "close: cannot close file, file descriptor not in use\n";
        return FAILURE;
    }

    // decrement the reference count and see if it's no longer in use
    if (--openFiles[fileDescriptor]->refCount == 0) {
        openFiles[fileDescriptor]->cachedINode->put(); // release the cached inode
        openFiles[fileDescriptor]->cachedINode = nullptr; // clear the reference to the inode
    }
    openFiles[fileDescriptor] = nullptr; // release the file descriptor for the current process
    return SUCCESS;
}

// set the offset of an open file to a given position
int Process::lseek(int fileDescriptor, int offset) {
    if (fileDescriptor < 0 || fileDescriptor >= PROCESS_FILE_DESCRIPTORS) {
        std::cerr << "lseek: cannot seek file, invalid file descriptor\n";
        return -1;
    }
    if (!openFiles[fileDescriptor]) {
        std::cerr << "lseek: cannot seek file, file descriptor not in use\n";
        return -1;
    }
    int origOffset = openFiles[fileDescriptor]->offset;
    if (offset < 0 || offset > (int)openFiles[fileDescriptor]->cachedINode->inode.i_size) {
        std::cerr << "lseek: cannot seek to " << offset << ", out of range\n";
    } else {
        openFiles[fileDescriptor]->offset = offset;
    }

    return origOffset;
}

// duplicate a file descriptor to the next available descriptor
int Process::dup(int fileDescriptor) {
    unsigned int dupFileDescriptor;
    for (dupFileDescriptor = 0; dupFileDescriptor < PROCESS_FILE_DESCRIPTORS; dupFileDescriptor++) {
        if (openFiles[dupFileDescriptor] == nullptr) break; // found free slot for new file descriptor
    }
    if (dupFileDescriptor == PROCESS_FILE_DESCRIPTORS) {
        std::cerr << "dup: cannot duplicate, the process's open file table is full\n";
        return -1;
    }
    if (fileDescriptor < 0 || fileDescriptor >= PROCESS_FILE_DESCRIPTORS) {
        std::cerr << "dup: cannot duplicate, invalid file descriptor\n";
        return -1;
    }
    if (!openFiles[fileDescriptor]) {
        std::cerr << "dup: cannot duplicate, file descriptor not in use\n";
        return -1;
    }

    openFiles[dupFileDescriptor] = openFiles[fileDescriptor];
    openFiles[fileDescriptor]->refCount++;
    return dupFileDescriptor;
}

// duplicate a source file descriptor to a specific destination descriptor; close destination file if it's open
int Process::dup2(int fileDescriptor, int dupFileDescriptor) {
    if (fileDescriptor < 0 || fileDescriptor >= PROCESS_FILE_DESCRIPTORS) {
        std::cerr << "dup2: cannot duplicate, invalid source file descriptor\n";
        return -1;
    }
    if (!openFiles[fileDescriptor]) {
        std::cerr << "dup2: cannot duplicate, file descriptor not in use\n";
        return -1;
    }
    if (dupFileDescriptor == fileDescriptor || dupFileDescriptor < 0 || dupFileDescriptor >= PROCESS_FILE_DESCRIPTORS) {
        std::cerr << "dup2: cannot duplicate, invalid destination file descriptor\n";
        return -1;
    }

    // if the destination file descriptor is open, close it
    if (openFiles[dupFileDescriptor]) close(dupFileDescriptor);

    openFiles[dupFileDescriptor] = openFiles[fileDescriptor];
    openFiles[fileDescriptor]->refCount++;
    return dupFileDescriptor;
}

// read a requested number of bytes from a file into a buffer; return the actual number of bytes read
int Process::read(int fileDescriptor, char* buffer, int numBytes) {
    char* dst = buffer;
    int startByte; // starting byte offset in the current data block at which to start reading
    int remainingBytes; // number of bytes remaining in the current data block
    int actualBytes; // actual number of bytes read (vs. numBytes requested)
    int logicalBlockNum, actualBlockNum;

    OpenFile* file = openFiles[fileDescriptor];
    if (file->mode != READ && file->mode != READWRITE) {
        std::cerr << "read: cannot read file, file is not opened for read or read/write\n";
        return -1;
    }

    CachedINode* cachedINode = file->cachedINode;
    INode* inode = &cachedINode->inode;
    DataBlock block(cachedINode->device);

    logicalBlockNum = file->offset / BLOCK_SIZE;
    startByte = file->offset % BLOCK_SIZE;
    if (numBytes > (int)inode->i_size - file->offset) {
        numBytes = (int)inode->i_size - file->offset; // don't attempt to read beyond the size of the file
    }
    actualBytes = numBytes;

    while (numBytes) {
        actualBlockNum = cachedINode->logical2physical(logicalBlockNum);
        remainingBytes = BLOCK_SIZE - startByte;
        block.get(actualBlockNum);
        if (numBytes <= remainingBytes) {
            memcpy(dst, &block.buffer[startByte], numBytes);
            file->offset += numBytes;
            numBytes = 0;
        } else {
            memcpy(dst, &block.buffer[startByte], remainingBytes);
            file->offset += remainingBytes;
            numBytes -= remainingBytes;
            dst += remainingBytes;
            startByte = 0;
            logicalBlockNum++;
        }
    }
    inode->i_atime = time(0L); // update file accessed time
    cachedINode->isDirty = true;
    return actualBytes;
}

// used to test/debug the read() method, returns the number of bytes read
int Process::read_bytes(int fileDescriptor, int numBytes) {
    if (fileDescriptor < 0 || fileDescriptor >= PROCESS_FILE_DESCRIPTORS) {
        std::cerr << "read: cannot read file, invalid file descriptor\n";
        return -1;
    }
    if (!openFiles[fileDescriptor]) {
        std::cerr << "read: cannot read file, file descriptor not in use\n";
        return -1;
    }
    char* bytes = new char[numBytes];
    int actualBytes = read(fileDescriptor, bytes, numBytes);
    delete bytes;
    std::cout << "read: " << actualBytes << " bytes read from file\n";
    return actualBytes;
}

// write a requested number of bytes to a file from a buffer; return the actual number of bytes written
int Process::write(int fileDescriptor, char* buffer, int numBytes) {
    char* src = buffer;
    int actualBytes = numBytes; // assume we won't fail to write all the bytes given, i.e., the device has enough free space available
    int logicalBlockNum, actualBlockNum, startByte, remainingBytes;

    OpenFile* file = openFiles[fileDescriptor];
    if (file->mode == READ) {
        std::cerr << "write: cannot write to file, file is opened for read only\n";
        return -1;
    }

    CachedINode* cachedINode = file->cachedINode;
    INode* inode = &cachedINode->inode;
    DataBlock block(cachedINode->device);

    logicalBlockNum = file->offset / BLOCK_SIZE;
    startByte = file->offset % BLOCK_SIZE;
    actualBlockNum = cachedINode->logical2physical(logicalBlockNum);
    if (actualBlockNum == 0)
        actualBlockNum = cachedINode->allocate_block(); // could be new file or could be starting a new block
    else
        block.get(actualBlockNum); // start with existing data

    while (numBytes) {
        remainingBytes = BLOCK_SIZE - startByte;
        if (numBytes <= remainingBytes) {
            memcpy(&block.buffer[startByte], src, numBytes);
            file->offset += numBytes;
            numBytes = 0;
        } else {
            memcpy(&block.buffer[startByte], src, remainingBytes);
            file->offset += remainingBytes;
            numBytes -= remainingBytes;
            src += remainingBytes;
            startByte = 0;
        }
        block.put(actualBlockNum);
        if (numBytes) { // if there's more to write, add another data block to the file
            actualBlockNum = cachedINode->allocate_block();
        }
    }
    inode->i_size += actualBytes;
    inode->i_atime = time(0L); // update file accessed time
    inode->i_ctime = time(0L); // update inode change time
    inode->i_mtime = time(0L); // update file modified time
    cachedINode->isDirty = true;
    return actualBytes;
}

// used to test/debug the write() method, returns the number of bytes written
int Process::write_bytes(int fileDescriptor) {
    char buffer[STRING_SIZE];
    int numBytes, actualBytes;

    if (fileDescriptor < 0 || fileDescriptor >= PROCESS_FILE_DESCRIPTORS) {
        std::cerr << "write: cannot write file, invalid file descriptor\n";
        return -1;
    }
    if (!openFiles[fileDescriptor]) {
        std::cerr << "write: cannot write file, file descriptor not in use\n";
        return -1;
    }
    if (openFiles[fileDescriptor]->mode == READ) {
        std::cerr << "write: cannot write file, file open for read only\n";
        return -1;
    }
    std::cout << "String to write: ";
    fgets(buffer, STRING_SIZE, stdin);
    numBytes = (int)strlen(buffer);
    if (numBytes == 1) { // a single character is just the newline, so it's an empty string
        std::cerr << "write: nothing to write\n";
        return -1;
    }
    actualBytes = write(fileDescriptor, buffer, numBytes);
    std::cout << "write: " << actualBytes << " bytes written to file";
    return actualBytes;
}

// display the contents of a file
int Process::cat(const std::string& pathname) {
    char dataBlock[BLOCK_SIZE + 1];
    int numBytes;

    int fileDescriptor = open(pathname, READ);
    if (fileDescriptor == -1) {
        std::cerr << "cat: cannot open the file\n";
        return -1;
    }

    while ((numBytes = read(fileDescriptor, dataBlock, BLOCK_SIZE))) {
        dataBlock[numBytes] = '\0';
        printf("%s", dataBlock);
    }

    close(fileDescriptor);
    return SUCCESS;
}
