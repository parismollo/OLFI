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


 436  sudo dd if=/dev/loop0 bs=4096 skip=BLOCK_NUMBER count=1 | hexdump -C
  437  sudo dd if=/dev/loop0 bs=4096 skip=436 count=1 | hexdump -C
  438  sudo dd if=/dev/loop0 bs=4096 skip=435 count=1 | hexdump -
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


[   61.253842] ouichefs:ouichefs_write: ppos: 0
[   61.254298] ouichefs:ouichefs_write: i_blocks before allocation = 1
[   61.258997] ouichefs:ouichefs_write: i_blocks after allocation = 1
[   61.259615] ouichefs:ouichefs_write: offset in the beginning: 0
[   61.260093] ouichefs:ouichefs_write: cmpt: 0
[   61.260340] ouichefs:ouichefs_write: position_to_copy: -1
[   61.260886] ouichefs:ouichefs_write: number_of_blocks_needed: 0
[   61.264241] ouichefs:ouichefs_write: bytes_write = 7
[   61.264843] ouichefs:ouichefs_write: block number: 261 has block_size_fragment = 0 + 7
[   61.265169] ouichefs:ouichefs_write: fragmented block_size after writin = 7
[   61.265633] ouichefs:ouichefs_write: before check if inode->i_blocks == 0 we have: 1 blocks
[   61.266057] ouichefs:ouichefs_write: inode->i_blocks is 2
[   61.271301] ouichefs:ouichefs_write: ppos: 3
[   61.272496] ouichefs:ouichefs_write: offset in the beginning: 3
[   61.272779] ouichefs:ouichefs_write: found data that will need to be defragmented at b_data[3]: 100!
[   61.273212] ouichefs:ouichefs_write: found data that will need to be defragmented at b_data[4]: 101!
[   61.273806] ouichefs:ouichefs_write: found data that will need to be defragmented at b_data[5]: 102!
[   61.274157] ouichefs:ouichefs_write: found data that will need to be defragmented at b_data[6]: 103!
[   61.274623] ouichefs:ouichefs_write: cmpt: 4
[   61.274776] ouichefs:ouichefs_write: position_to_copy: 3
[   61.274948] ouichefs:ouichefs_write: number_of_blocks_needed: 1
[   61.279197] ouichefs:ouichefs_write: ptr_to_copy_from: 100
[   61.281185] ouichefs:ouichefs_write: new block number: 262; new block_size = 4
[   61.281938] ouichefs:ouichefs_write: casted cmpt is 4
[   61.282190] ouichefs:ouichefs_write: fragmented block_size before writing = 3
[   61.284560] ouichefs:ouichefs_write: bytes_write = 5
[   61.284818] ouichefs:ouichefs_write: block number: 261 has block_size_fragment = 3 + 5
[   61.285087] ouichefs:ouichefs_write: fragmented block_size after writin = 8
[   61.285512] ouichefs:ouichefs_write: before check if inode->i_blocks == 0 we have: 2 blocks
[   61.285864] ouichefs:ouichefs_write: inode->i_blocks is 3




-----
read et write 1.3
[root@pnl new_version]# ./benchmark 5
 ------------------------------------------------- 
| Benchmark test for ext4: performance read/write |
 ------------------------------------------------- 
    Average Read Time: 0.000034 seconds
    Average Write Time: 0.000177 seconds

 -----------------------------------------------------
| Benchmark test for ouichefs: performance read/write |
 -----------------------------------------------------
    Average Read Time: 0.000029 seconds
    Average Write Time: 0.000288 seconds

 --------------------------- 
| Check read/write for ext4 |
 --------------------------- 
    Check write/read ok

 ------------------------------- 
| Check read/write for ouichefs |
 ------------------------------- 
    Check write/read ok

----

read et write fragmented
| Check read/write for ouichefs |
[root@pnl new_version]# ./benchmark 30
 ------------------------------------------------- 
| Benchmark test for ext4: performance read/write |
 ------------------------------------------------- 
    Average Read Time: 0.000025 seconds
    Average Write Time: 0.000127 seconds

 -----------------------------------------------------
| Benchmark test for ouichefs: performance read/write |
 -----------------------------------------------------
    Average Read Time: 0.000034 seconds
    Average Write Time: 0.000298 seconds

 --------------------------- 
| Check read/write for ext4 |
 --------------------------- 
    Check write/read ok

 ------------------------------- 
| Check read/write for ouichefs |
 ------------------------------- 
    Check write/read failed

[root@pnl new_version]# 




write 1.3cat
[root@pnl new_version]# ./benchmark_plus 
Time taken to write 10MB: 0.000867 seconds
[root@pnl new_version]# 


----
Used blocks: 3
Partially filled blocks: 3
Internal fragmentation: 7284 bytes
Block 290: 8 bytes
Block 293: 4093 bytes
Block 292: 903 bytes



full = 0;

--xxx------
-----------
-------xxxxx
