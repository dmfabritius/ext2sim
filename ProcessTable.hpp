#pragma once
#include "Process.hpp"

// a fixed-size table of the file system's processes
class ProcessTable {
private:
    Process processes[PROCESS_TABLE_SIZE];

public:
    ProcessTable();
    void create_superuser(); // create the first running process
};
