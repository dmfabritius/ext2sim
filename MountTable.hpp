#pragma once
#include "MountedDevice.hpp"
class CachedINode;

// a fixed-size table of devices mounted into the file system
class MountTable {
private:
    MountedDevice devices[MOUNT_TABLE_SIZE];

public:
    CachedINode* mount(const std::string& diskImage, const std::string& mountPath); // mount a device into the file system simulation
    int umount(const std::string& mountPath); // unmount a device from the file system simulation
    void display(); // show a list of all mounted devices
};
