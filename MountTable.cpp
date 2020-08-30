#include "FileSystem.hpp"

// mount a device into the file system simulation
CachedINode* MountTable::mount(const std::string& diskImage, const std::string& mountPath) {
    CachedINode* mnt = nullptr;

    if (diskImage == "") {
        display();
        return nullptr;
    }
    if (mountPath == "") {
        std::cerr << "mount: cannot mount, no mount point given\n";
        return nullptr;
    }
    if (mountPath[0] != '/') {
        std::cerr << "mount: cannot mount, specify absolute path for mount point\n";
        return nullptr;
    }
    for (const MountedDevice& d : devices) {
        if (d.fd != -1 && (d.diskImage == diskImage || d.mountPath == mountPath)) {
            std::cerr << "mount: disk image " << d.diskImage << " is already mounted at " << d.mountPath << "\n";
            return nullptr;
        }
    }

    for (MountedDevice& d : devices) {
        if (d.fd == -1) { // look for a file descriptor of -1, indicating a free entry
            if (mountPath != "/") { // skip checks for root of file system
                mnt = fs.inodeTable.get(mountPath);
                if (!mnt) {
                    std::cerr << "mount: cannot mount, " << mountPath << " not found\n";
                    return nullptr;
                }
                if (!S_ISDIR(mnt->inode.i_mode)) {
                    std::cerr << "mount: cannot mount, " << mountPath << " is not a directory\n";
                    mnt->put();
                    return nullptr;
                }
                if (mnt->refCount > 1) {
                    std::cerr << "mount: cannot mount, " << mountPath << " is in use\n";
                    mnt->put();
                    return nullptr;
                }
            }

            d.diskImage = diskImage;
            d.mountPath = mountPath;
            if (d.mount() != SUCCESS) {
                if (mountPath == "/") exit(FAILURE); // terminate simulation if the root device can't be mounted
                mnt->put();
                return nullptr; // when attempting to mount other devices, return failure
            }

            if (mountPath != "/") { // skip when mounting root of the file system
                d.mountPoint = mnt; // the inode in the primary file system where the device is mounted
                mnt->deviceRoot = d.root; // the root inode of the mounted device
            }

            return d.root;
        }
    }
    std::cerr << "mount: cannot mount, global mount table is full\n";
    return nullptr;
}

// unmount a device from the file system simulation
int MountTable::umount(const std::string& mountPath) {
    if (mountPath == "") {
        std::cerr << "umount: cannot unmount, no mount point given\n";
        return FAILURE;
    }
    for (MountedDevice& d : devices) {
        if (d.fd != -1 && d.mountPath == mountPath)
            return d.umount();
    }
    std::cerr << "umount: cannot unmount, invalid mount point\n";
    return FAILURE;
}

// show a list of all mounted devices
void MountTable::display() {
    std::cout << "Dev Disk image name Mount point Num blk Free blk Num ino Free ino\n"
                 "--- --------------- ----------- ------- -------- ------- --------\n";
    for (const MountedDevice& d : devices) {
        if (d.fd == -1) continue; // skip unused entries
        printf("%-3d %-15s %-11s %-7d %-8d %-7d %-8d\n",
            d.fd, d.diskImage.c_str(), d.mountPath.c_str(), d.nblocks, d.nbfree, d.ninodes, d.nifree);
    }
}
