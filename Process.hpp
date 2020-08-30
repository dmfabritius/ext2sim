#pragma once
#include "OpenFile.hpp"
class CachedINode;

// valid process statuses: FREE, BUSY, READY
enum ProcessStatus {
    FREE,
    BUSY,
    READY
};

// a user process of the file system
class Process {
private:
public:
    int pid; // process ID
    int uid; // user ID
    int gid; // group ID
    Process* next;
    ProcessStatus status = READY;
    CachedINode* cwd; // current working directory
    std::string cwd_path; // cwd as a full absolute path string
    OpenFile* openFiles[PROCESS_FILE_DESCRIPTORS] = { nullptr }; // pointers into the global open file table

    std::string prompt(); // set the command prompt
    void display_open_files(); // display all the open files for this process
    int chdir(const std::string& pathname); // change current working directory
    int pwd(); // print full absolute path name of the current working directory
    int open(const std::string& pathname, OpenMode mode); // open a file and return its file descriptor, or -1 on failure
    int close(int fileDescriptor); // close an open file
    int lseek(int fileDescriptor, int offset); // set the offset of an open file to a given position
    int dup(int fileDescriptor); // duplicate a file descriptor to the next available descriptor
    int dup2(int fileDescriptor, int dupFileDescriptor); // duplicate a source file descriptor to a specific destination
    int read(int fileDescriptor, char* buffer, int numBytes); // read a requested number of bytes from a file
    int write(int fileDescriptor, char* buffer, int numBytes); // write a requested number of bytes to a file
    int cat(const std::string& pathname); // display the contents of a file

    int read_bytes(int fileDescriptor, int numBytes); // used to test/debug the read() method, returns the number of bytes read
    int write_bytes(int fileDescriptor); // used to test/debug the write() method, returns the number of bytes written
};
