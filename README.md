# Containerized File Systems

## Overview

This is a file system in user space which allows Linux kernel to allocate files to containers based on demand. A new process can be assigned with one of the containers. The file system orchestrates access to files.

### Kernel Compilation
```shell
cd kernel_module
sudo make clean
sudo make
sudo make install
cd ..
```

### User Space Library Compilation
```shell
cd library
sudo make clean
sudo make
sudo make install
cd ..
```

### File System Compilation
```shell
cd src
./configure
make clean
make
./src/fcfuse /dev/fcontainer {data_location} {mount_point}
```
**{data_location}** is a directory stores your data for each container and  **{mount_point}** is an empty directory serve as the mount point