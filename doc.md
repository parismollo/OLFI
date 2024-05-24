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

df /mnt/ouichefs/test



[root@pnl usr]# sudo dd if=/dev/loop0 bs=4096 skip=436
[  440.951586] audit: type=1101 audit(1716577034.423:48): pid=299 uid=0 auid=0 ses=2 subj=kernel msg='op=PAM:account'
[  440.959662] audit: type=1110 audit(1716577034.429:49): pid=299 uid=0 auid=0 ses=2 subj=kernel msg='op=PAM:setcred'
00000000  64 65 66 67 00 00 00 00  00 00 00 00 00 00 00 00  |defg............|
00000010  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
1+0 records in
1+0 records out
4096 bytes (4.1 kB, 4.0 KiB) copied, 0.00546013 s, 750 kB/s
*
00001000
[root@pnl usr]# [  440.980933] audit: type=1105 audit(1716577034.430:50): pid=299 uid=0 auid=0 ses=2 subj=kernel msg'
[  440.986617] audit: type=1106 audit(1716577034.447:51): pid=299 uid=0 auid=0 ses=2 subj=kernel msg='op=PAM:session'
[  440.991881] audit: type=1104 audit(1716577034.447:52): pid=299 uid=0 auid=0 ses=2 subj=kernel msg='op=PAM:setcred'
sudo dd if=/dev/loop0 bs=4096 skip=435 count=1 | hexdump -C
[  454.247896] audit: type=1101 audit(1716577047.719:53): pid=302 uid=0 auid=0 ses=2 subj=kernel msg='op=PAM:account'
[  454.256273] audit: type=1110 audit(1716577047.726:54): pid=302 uid=0 auid=0 ses=2 subj=kernel msg='op=PAM:setcred'
00000000  61 62 63 73 75 69 74 65  00 00 00 00 00 00 00 00  |abcsuite........|
00000010  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|[  454.270042] audit: type=1105 audit('

1+0 records in
1+0 records out
4096 bytes (4.1 kB, 4.0 KiB) copied, 0.0124472 s, 329 kB/s
*
00001000
[root@pnl usr]# [  454.284493] audit: type=1106 audit(1716577047.751:56): pid=302 uid=0 auid=0 ses=2 subj=kernel msg'
[  454.290936] audit: type=1104 audit(1716577047.752:57): pid=302 uid=0 auid=0 ses=2 subj=kernel msg='op=PAM:setcred'



- regler size du block  -> ioctl 
- regler i_block
- regler read (effective size plutot) et implementer