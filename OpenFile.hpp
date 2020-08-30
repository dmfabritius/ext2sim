#pragma once
#include "main.hpp"
class CachedINode;

// valid open file modes: READ, WRITE, READWRITE, APPEND
enum OpenMode {
    READ,
    WRITE,
    READWRITE,
    APPEND
};

// an open file shared across all the file system's processes
class OpenFile {
private:
    const std::vector<std::string> modes { "READ", "WRITE", "READWRITE", "APPEND" };

public:
    int refCount = 0; // number of times this open file (available simulate-wide) is being used by various Processes
    int offset; // the current byte position within the file where reading/writing will occur
    CachedINode* cachedINode; // the file's inode
    OpenMode mode;

    OpenFile* open(CachedINode* cachedINode, OpenMode mode); // initialize this open file object and return a pointer to it
    std::string mode_str() const; // return a string representation of the open file mode
};

std::ostream& operator<<(std::ostream& stream, const OpenFile& openFile); // send an OpenFile object to an output stream
