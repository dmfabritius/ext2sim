#pragma once
#include "ProcessTable.hpp"
#include "OpenFileTable.hpp"
#include "MountTable.hpp"
#include "INodeTable.hpp"

// the global variables and utility functions of the file system simulation
class FileSystem {
public:
    ProcessTable processTable; // all processes using the file system
    Process* running; // the currently running process
    OpenFileTable openFileTable; // all files opened across the file system
    MountTable mountTable; // all devices mounted by the file system
    INodeTable inodeTable; // all inodes being used by the file system
    CachedINode* root; // the root of the file system

    void start(const std::string& diskImage); // the main user input loop for the simulation
    void menu(); // display the available commands for the file system
    void execute(const std::vector<std::string>& input); // run a file system command
    void quit(); // terminate the file system simulation
};

extern FileSystem fs; // globally defined in main.cpp
