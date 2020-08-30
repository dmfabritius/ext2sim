#include "FileSystem.hpp"

// the main user input loop for the simulation
void FileSystem::start(const std::string& diskImage) {
    TRACE(1, "%s\n", "initializing file sysem simulation");
    root = mountTable.mount(diskImage, "/");
    processTable.create_superuser();

    std::cout << "Enter menu or help to see a summary of available commands\n";
    std::string line, word;
    std::vector<std::string> input;
    while (true) {
        input.clear();
        std::cout << "\n"
                  << running->prompt() << "$ ";
        std::getline(std::cin, line);
        std::istringstream stream(line);
        while (std::getline(stream, word, ' '))
            input.push_back(word); // add each word (separated by a space) to an array
        for (int i = input.size(); i < 3; i++)
            input.push_back(""); // it's easier if we always have 3 inputs
        execute(input);

        if (TRACE_LEVEL >= 2) inodeTable.display();
    }
}

// display the available commands for the file system
void FileSystem::menu() {
    std::cout << "EXT2 File System Simulator Project\n\n"
                 "help   menu    cache  minodes  quit   exit\n"
                 "ls     cd      pwd    mkdir    creat  rmdir  rd\n"
                 "link   unlink  rm     symlink  stat   chmod  utime  touch\n"
                 "pfd    open    close  lseek    dup    dup2\n"
                 "read   cat     write  cp       mv\n"
                 "mount  umount\n";
}

// terminate the file system simulation
void FileSystem::quit() {
    inodeTable.flush();
    exit(SUCCESS);
}

// run a file system command
void FileSystem::execute(const std::vector<std::string>& input) {
    std::string command(input[0]);
    std::string param1(input[1]);
    std::string param2(input[2]);

    // some command arguments are numbers, so make those variants
    int num1 = atoi(param1.c_str());
    int num2 = atoi(param2.c_str());
    TRACE(1, "cmd='%s', param1='%s' (numeric %d), param2='%s' (numeric %d)\n",
        command.c_str(), param1.c_str(), num1, param2.c_str(), num2);

    if (command == "quit" || command == "exit")
        quit();
    else if (command == "menu" || command == "help")
        menu();
    else if (command == "cache" || command == "minodes")
        inodeTable.display();
    else if (command == "pwd")
        running->pwd();
    else if (command == "cd")
        running->chdir(param1);
    else if (command == "cd..")
        running->chdir("..");
    else if (command == "ls" || command == "dir")
        inodeTable.ls(param1);
    else if (command == "mkdir" || command == "md")
        inodeTable.mkdir(param1);
    else if (command == "creat")
        inodeTable.creat(param1);
    else if (command == "rmdir" || command == "rd")
        inodeTable.rmdir(param1);
    else if (command == "link")
        inodeTable.link(param1, param2);
    else if (command == "unlink" || command == "rm")
        inodeTable.unlink(param1);
    else if (command == "symlink")
        inodeTable.symlink(param1, param2);
    else if (command == "stat")
        inodeTable.stat(param1);
    else if (command == "chmod")
        inodeTable.chmod(param1, param2);
    else if (command == "utime" || command == "touch")
        inodeTable.utime(param1);
    else if (command == "pfd")
        running->display_open_files();
    else if (command == "open")
        running->open(param1, (OpenMode)num2);
    else if (command == "close")
        running->close(num1);
    else if (command == "lseek")
        running->lseek(num1, num2);
    else if (command == "dup")
        running->dup(num1);
    else if (command == "dup2")
        running->dup2(num1, num2);
    else if (command == "read")
        running->read_bytes(num1, num2);
    else if (command == "cat")
        running->cat(param1);
    else if (command == "write")
        running->write_bytes(num1);
    else if (command == "cp")
        inodeTable.cp(param1, param2);
    else if (command == "mv")
        inodeTable.mv(param1, param2);
    else if (command == "mount")
        mountTable.mount(param1, param2);
    else if (command == "umount")
        mountTable.umount(param1);
    else
        std::cerr << "* invalid command\n";
}
