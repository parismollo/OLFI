# OuicheFS Light Insertion File System
This project implements new features on top of the existing OuicheFS file system, transforming it into a fast insertion file system for Linux 6.5.7. The goal is to optimize data insertion in the middle of files by allowing more flexible block filling.
Key Features:

- Reimplementation of read and write functions
- Modified data structure for efficient partial block management
- Optimized write function for faster data insertion
- Adapted read function to handle non-contiguous data blocks
- File defragmentation functionality
- Custom ioctl commands for file system analysis

This project explores the trade-offs between write performance and read performance in file systems, providing hands-on experience with Linux kernel module development and file system internals.
