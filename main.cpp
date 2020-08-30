/*
    Partial simulation of the ext2 file system
 */
#include "FileSystem.hpp"
FileSystem fs;

int main(int argc, char* argv[]) {
    std::string diskImage("disk0");
    if (argc == 2) diskImage = argv[1];

    fs.start(diskImage);
}
