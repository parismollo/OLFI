# 1.1 Installation
1. run linux vm
2. load module ouichefs.ko (make KERNELDIR=) + make in mkfs
3. mount partition
   1. dd if=/dev/zero of=test.img bs=1M count=50
      1. ./mkfs.ouichefs test.img
      2. mount -o loop -t ouichefs /share/ouichefs_partition.img /mnt/ouichefs 

# 1.2 Benchmarks

# 1.3 Standard read - write
## Read
* **standard read with page cache** - application that needs to read data from a file, will call the read system call. It will pass a file descriptor, buffer to store data and the number of bytes. 
  * VFS provides a generic interface for filesystem operations, it will dispatch the read request to the specific filesystem by invoking the .read_iter function pointer (or .read) of the file operations structure associated with the file descriptor. 
  * check cache (cache hit or miss)
  * if miss, read data from the disk. it schedule a read request to fetch the data into the page cache
  * once data is loaded into page cache, data is coped from page cache to user space buffer specified in the read system call. (copt_to_user())
  * update file offset
  * return value
* **standard read without cache**
  * need to write `ouichefs_read(struct file *file, char __user *buf, size_t len, loff_t *ppos)` bypassing the page cache and accessing the disk directly.
  * need to write `ouichefs_read_iter(struct kiocb *iocb, struct iov_iter *iter)`

## Write

## Questions
- why /mtn/ouichefs est toujours apres éteint la machine?
- how this still works?
```bash
[root@pnl ~]# rmmod ouichefs 
[  138.423171] ouichefs:ouichefs_exit: module unloaded
[root@pnl ~]# cat /mnt/ouichefs/cool.txt 
hello world
[root@pnl ~]# 
```
- To make it call our own function we need everytime to mount, why?
- how it works this command: mount -o loop -t ouichefs /share/ouichefs_partition.img /mnt/ouichefs