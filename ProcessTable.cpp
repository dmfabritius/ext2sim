#include "ProcessTable.hpp"
#include "INodeTable.hpp"
#include "FileSystem.hpp"

ProcessTable::ProcessTable() {
    int id = 0;
    for (Process& p : processes) { // initialize processes
        p.pid = id;
        p.uid = id;
        p.next = &processes[id + 1]; // link list
        id++;
    }
    processes[PROCESS_TABLE_SIZE - 1].next = &processes[0]; // circular list
}

// create the first running process
void ProcessTable::create_superuser() {
    TRACE(1, "%s\n", "assigning superuser (pid = 0) as the running process");
    fs.running = &processes[SUPER_USER];
    fs.running->cwd = fs.inodeTable.get(fs.root->device, fs.root->inodeNum);
    fs.running->cwd_path = "/";
}
