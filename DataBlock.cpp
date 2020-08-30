#include "DataBlock.hpp"
#include "MountedDevice.hpp"

// construct a blank data block
DataBlock::DataBlock()
    : device(nullptr)
    , blockNum(0) {
}

// construct a data block for a given device
DataBlock::DataBlock(MountedDevice* device)
    : device(device)
    , blockNum(-1) {
}

// construct a data block for a given device and block number
DataBlock::DataBlock(MountedDevice* device, int blockNum)
    : device(device)
    , blockNum(blockNum) {
}

// load a data block from a given device
void DataBlock::get(MountedDevice* device, int blockNum) {
    this->device = device;
    this->blockNum = blockNum;
    get();
}

// load a data block from this block's device
void DataBlock::get(int blockNum) { // use when dev has already been set
    this->blockNum = blockNum;
    get();
}

// load this block's data
void DataBlock::get() { // use when dev & blockNum have been set
    if (!device) {
        std::cerr << "DataBlock::get() failed, device not specified\n";
        return;
    }
    if (blockNum <= 0) {
        std::cerr << "DataBlock::get() failed, block number not specified\n";
        return;
    }
    TRACE(3, "reading block %d from disk image file %d\n", blockNum, device->fd);
    lseek(device->fd, (long)blockNum * BLOCK_SIZE, 0);
    read(device->fd, buffer, BLOCK_SIZE);
}

// save a data block to a given device
void DataBlock::put(MountedDevice* device, int blockNum) {
    this->device = device;
    this->blockNum = blockNum;
    put();
}

// save a data block to this block's device
void DataBlock::put(int blockNum) { // use when dev has already been set
    this->blockNum = blockNum;
    put();
}

// save this block's data
void DataBlock::put() { // use when dev & blockNum have been set
    if (!device) {
        std::cerr << "DataBlock::put() failed, device not specified\n";
        return;
    }
    if (blockNum <= 0) {
        std::cerr << "DataBlock::put() failed, block number not specified\n";
        return;
    }
    TRACE(3, "writing block %d to disk image file %d\n", blockNum, device->fd);
    lseek(device->fd, (long)blockNum * BLOCK_SIZE, 0);
    write(device->fd, buffer, BLOCK_SIZE);
}

// determine whether or not a bit is set in a bitmap buffer
bool DataBlock::test_bit(int bit) {
    return buffer[bit / 8] & (1 << (bit % 8));
}

// set a bit in a bitmap buffer
void DataBlock::set_bit(int bit) {
    buffer[bit / 8] |= (char)(1 << (bit % 8));
}

// clear a bit in a bitmap buffer
void DataBlock::clear_bit(int bit) {
    buffer[bit / 8] &= (char)~(1 << (bit % 8));
}
