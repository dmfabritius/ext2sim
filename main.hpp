#pragma once
#include <ext2fs/ext2_fs.h> // sudo apt-get install e2fslibs-dev
#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <vector>
#include <ctime>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <libgen.h>
#include <sys/stat.h>

// define some friendlier type names
typedef struct ext2_super_block SuperBlock;
typedef struct ext2_group_desc GroupDescriptor;
typedef struct ext2_inode INode;
typedef struct ext2_dir_entry_2 DirectoryEntry;

#define SUPER_BLOCK 1
#define GROUP_DESCRIPTOR_0 2

// define the scalability of our simulation by specifying the table sizes
#define PROCESS_TABLE_SIZE 2
#define INODE_TABLE_SIZE 64
#define MOUNT_TABLE_SIZE 4
#define OPEN_FILES_TABLE_SIZE 32
#define PROCESS_FILE_DESCRIPTORS 16

#define STRING_SIZE 256
#define BLOCK_SIZE 1024
#define BLOCKNUMS_PER_BLOCK (BLOCK_SIZE / (int)sizeof(int))
#define INODES_PER_BLOCK (BLOCK_SIZE / (int)sizeof(INode))
#define ROOT_DIR_INODE_NUM 2
#define PARENT_DIR_ENTRY_OFFSET 12 // byte offset location of parent directory entry

#define DIR_FILE_MODE 0040755 // DIR type and rwxr-xr-x permissions
#define REG_FILE_MODE 0100644 // REG type and rw-r--r-- permissions
#define LNK_FILE_MODE 0120777 // LNK type and rwxrwxrwx permissions

#define SUPER_USER 0
#define SUCCESS 0
#define FAILURE 1

// https://stackoverflow.com/questions/1644868/define-macro-for-debug-printing-in-c
#ifndef TRACE_LEVEL // define via gcc argument -D'TRACE_LEVEL=n'
#define TRACE_LEVEL 0 // 0=None, 1=Low, 2=Medium, 3=High
#endif
#define TRACE(level, fmt, ...)                                        \
    do {                                                              \
        if (TRACE_LEVEL >= level) fprintf(stderr, "%s:%d:%s(): " fmt, \
            __FILE__, __LINE__, __func__, __VA_ARGS__);               \
    } while (0)
