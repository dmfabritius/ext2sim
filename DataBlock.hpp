#pragma once
#include "main.hpp"
class MountedDevice;

// a block of data from a given device
class DataBlock {
public:
    MountedDevice* device;
    int blockNum;
    union {
        char buffer[BLOCK_SIZE] = { 0 }; // used for raw data of various types/structures
        int nums[BLOCKNUMS_PER_BLOCK]; // used for a block of integers
        INode inodes[INODES_PER_BLOCK]; // used for a block of inodes
    };

    DataBlock(); // construct a blank data block
    DataBlock(MountedDevice* device); // construct a data block for a given device
    DataBlock(MountedDevice* device, int blockNum); // construct a data block for a given device and block number

    void get(MountedDevice* device, int blockNum); // load a data block from a given device
    void get(int blockNum); // load a data block from this block's device
    void get(); // load this block's data
    void put(MountedDevice* device, int blockNum); // save a data block to a given device
    void put(int blockNum); // save a data block to this block's device
    void put(); // save this block's data

    bool test_bit(int bit); // determine whether or not a bit is set in a bitmap buffer
    void set_bit(int bit); // set a bit in a bitmap buffer
    void clear_bit(int bit); // clear a bit in a bitmap buffer
};