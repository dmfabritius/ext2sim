#include "OpenFile.hpp"
#include "CachedINode.hpp"
#include "MountedDevice.hpp"

// initialize this open file object and return a pointer to it
OpenFile* OpenFile::open(CachedINode* cachedINode, OpenMode mode) {
    refCount = 1;
    this->mode = mode;
    this->cachedINode = cachedINode;
    offset = (mode == APPEND) ? cachedINode->inode.i_size : 0;
    if (mode == WRITE) cachedINode->truncate();
    return this;
}

// return a string representation of the open file mode
std::string OpenFile::mode_str() const {
    return modes[mode];
}

// send an OpenFile object to an output stream; formatted print of its member variables
std::ostream& operator<<(std::ostream& stream, const OpenFile& openFile) {
    // this is definitely more cumbersome and confusing than just using printf()
    if (openFile.cachedINode) stream << std::setw(7) << std::left << openFile.mode_str()
                                     << std::setw(6) << openFile.offset
                                     << " [" << std::setw(3) << openFile.cachedINode->device->fd << ", "
                                     << std::setw(3) << openFile.cachedINode->inodeNum << ']';
    return stream;
}
