// SPDX-License-Identifier: GPL-2.0
/*
 * ouiche_fs - a simple educational filesystem for Linux
 *
 * Copyright (C) 2018 Redha Gouicem <redha.gouicem@lip6.fr>
 */

#define pr_fmt(fmt) "%s:%s: " fmt, KBUILD_MODNAME, __func__

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/mpage.h>

#include "ouichefs.h"
#include "bitmap.h"

/*
 * Map the buffer_head passed in argument with the iblock-th block of the file
 * represented by inode. If the requested block is not allocated and create is
 * true, allocate a new block on disk and map it.
 */
static int ouichefs_file_get_block(struct inode *inode, sector_t iblock,
				   struct buffer_head *bh_result, int create)
{
	struct super_block *sb = inode->i_sb;
	struct ouichefs_sb_info *sbi = OUICHEFS_SB(sb);
	struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
	struct ouichefs_file_index_block *index;
	struct buffer_head *bh_index;
	int ret = 0, bno;

	/* If block number exceeds filesize, fail */
	if (iblock >= OUICHEFS_BLOCK_SIZE >> 2)
		return -EFBIG;

	/* Read index block from disk */
	bh_index = sb_bread(sb, ci->index_block);
	if (!bh_index)
		return -EIO;
	index = (struct ouichefs_file_index_block *)bh_index->b_data;

	/*
	 * Check if iblock is already allocated. If not and create is true,
	 * allocate it. Else, get the physical block number.
	 */
	if (index->blocks[iblock] == 0) {
		if (!create) {
			ret = 0;
			goto brelse_index;
		}
		bno = get_free_block(sbi);
		if (!bno) {
			ret = -ENOSPC;
			goto brelse_index;
		}
		index->blocks[iblock] = bno;
	} else {
		bno = index->blocks[iblock];
	}

	/* Map the physical block to the given buffer_head */
	map_bh(bh_result, sb, bno);

brelse_index:
	brelse(bh_index);

	return ret;
}

/*
 * Called by the page cache to read a page from the physical disk and map it in
 * memory.
 */
static void ouichefs_readahead(struct readahead_control *rac)
{
	mpage_readahead(rac, ouichefs_file_get_block);
}

/*
 * Called by the page cache to write a dirty page to the physical disk (when
 * sync is called or when memory is needed).
 */
static int ouichefs_writepage(struct page *page, struct writeback_control *wbc)
{
	return block_write_full_page(page, ouichefs_file_get_block, wbc);
}

/*
 * Called by the VFS when a write() syscall occurs on file before writing the
 * data in the page cache. This functions checks if the write will be able to
 * complete and allocates the necessary blocks through block_write_begin().
 */
static int ouichefs_write_begin(struct file *file,
				struct address_space *mapping, loff_t pos,
				unsigned int len, struct page **pagep,
				void **fsdata)
{
	struct ouichefs_sb_info *sbi = OUICHEFS_SB(file->f_inode->i_sb);
	int err;
	uint32_t nr_allocs = 0;

	/* Check if the write can be completed (enough space?) */
	if (pos + len > OUICHEFS_MAX_FILESIZE)
		return -ENOSPC;
	nr_allocs = max(pos + len, file->f_inode->i_size) / OUICHEFS_BLOCK_SIZE;
	if (nr_allocs > file->f_inode->i_blocks - 1)
		nr_allocs -= file->f_inode->i_blocks - 1;
	else
		nr_allocs = 0;
	if (nr_allocs > sbi->nr_free_blocks)
		return -ENOSPC;

	/* prepare the write */
	err = block_write_begin(mapping, pos, len, pagep,
				ouichefs_file_get_block);
	/* if this failed, reclaim newly allocated blocks */
	if (err < 0) {
		pr_err("%s:%d: newly allocated blocks reclaim not implemented yet\n",
		       __func__, __LINE__);
	}
	return err;
}

/*
 * Called by the VFS after writing data from a write() syscall to the page
 * cache. This functions updates inode metadata and truncates the file if
 * necessary.
 */
static int ouichefs_write_end(struct file *file, struct address_space *mapping,
			      loff_t pos, unsigned int len, unsigned int copied,
			      struct page *page, void *fsdata)
{
	int ret;
	struct inode *inode = file->f_inode;
	struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
	struct super_block *sb = inode->i_sb;

	/* Complete the write() */
	ret = generic_write_end(file, mapping, pos, len, copied, page, fsdata);
	if (ret < len) {
		pr_err("%s:%d: wrote less than asked... what do I do? nothing for now...\n",
		       __func__, __LINE__);
	} else {
		uint32_t nr_blocks_old = inode->i_blocks;

		/* Update inode metadata */
		inode->i_blocks = inode->i_size / OUICHEFS_BLOCK_SIZE + 2;
		inode->i_mtime = inode->i_ctime = current_time(inode);
		mark_inode_dirty(inode);

		/* If file is smaller than before, free unused blocks */
		if (nr_blocks_old > inode->i_blocks) {
			int i;
			struct buffer_head *bh_index;
			struct ouichefs_file_index_block *index;

			/* Free unused blocks from page cache */
			truncate_pagecache(inode, inode->i_size);

			/* Read index block to remove unused blocks */
			bh_index = sb_bread(sb, ci->index_block);
			if (!bh_index) {
				pr_err("failed truncating '%s'. we just lost %llu blocks\n",
				       file->f_path.dentry->d_name.name,
				       nr_blocks_old - inode->i_blocks);
				goto end;
			}
			index = (struct ouichefs_file_index_block *)
					bh_index->b_data;

			for (i = inode->i_blocks - 1; i < nr_blocks_old - 1;
			     i++) {
				put_block(OUICHEFS_SB(sb), index->blocks[i]);
				index->blocks[i] = 0;
			}
			mark_buffer_dirty(bh_index);
			brelse(bh_index);
		}
	}
end:
	return ret;
}

const struct address_space_operations ouichefs_aops = {
	.readahead = ouichefs_readahead,
	.writepage = ouichefs_writepage,
	.write_begin = ouichefs_write_begin,
	.write_end = ouichefs_write_end
};

static int ouichefs_open(struct inode *inode, struct file *file) {
	bool wronly = (file->f_flags & O_WRONLY) != 0;
	bool rdwr = (file->f_flags & O_RDWR) != 0;
	bool trunc = (file->f_flags & O_TRUNC) != 0;

	if ((wronly || rdwr) && trunc && (inode->i_size != 0)) {
		struct super_block *sb = inode->i_sb;
		struct ouichefs_sb_info *sbi = OUICHEFS_SB(sb);
		struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
		struct ouichefs_file_index_block *index;
		struct buffer_head *bh_index;
		sector_t iblock;

		/* Read index block from disk */
		bh_index = sb_bread(sb, ci->index_block);
		if (!bh_index)
			return -EIO;
		index = (struct ouichefs_file_index_block *)bh_index->b_data;

		for (iblock = 0; index->blocks[iblock] != 0; iblock++) {
			put_block(sbi, index->blocks[iblock]);
			index->blocks[iblock] = 0;
		}
		inode->i_size = 0;
		inode->i_blocks = 0;

		brelse(bh_index);
	}
	
	return 0;
}

/*
- function returns a ssize_t (signed size type) to indicate number of bytes read or an error
- file is a pointer to a file (file being read)
- data  is user space buffer
- len number of bytes to read
- pos pointer to the file offset

(1) Check if current file position is greater then or equal to the file size (i_size). We return 0 because there are nothing left to read.
(2) get super block (metadata about filesystem); iblock is the index of current block containing the file position. To get the index of the block we do current offset
divided by the block size of ouichefs. We then retrieve the inode (custom inode) for ouichefs.
(3) Check if block index is within a valid range. 
(4) sb bread reads the block containing the file index block from disk. The index block maps file offsets to actual data blocks. Converts raw data from disk into structured format ouichefs_file_index_block
(5) retrieves the block number corresponding to the current block index, this number will point to the actual data block on disk
(6) read the actual data block from disk
(7) intially the buffer will point to the beggining of the data block, but we need to adjust for the right offset, that will find the correct offset within that block
(8) it reads the entire buffer or len so that it does not attempt to read beyonf the avaialable data in the block
(9) transfer data from kernel space to the user space buffer, and calcualates how many bytes were copied from what was expected
(10) update offset based on how much was copied and ensure file pointer reflects new position
(11) release index block and data block
*/
static ssize_t ouichefs_read(struct file *file, char __user *data, size_t len, loff_t *offset)
{


	printk(KERN_INFO "ouiches_read(): file_name = %s, len = %zu, current_offset = %lld\n", file_dentry(file)->d_name.name, len, (long long)*offset);
	printk(KERN_INFO "ouiches_read(): started\n");

	// (1)
	if (*offset >= file->f_inode->i_size) {
		printk(KERN_INFO "ouiches_read(): offset goes beyond file size\n");
		return 0;
	}

	unsigned long bytes_to_copy = 0;
	unsigned long bytes_copied = 0;

	// (2) 
	struct super_block *super_blk = file->f_inode->i_sb;
	sector_t block_index = *offset / OUICHEFS_BLOCK_SIZE;
	printk(KERN_INFO "ouiches_read(): block_index = %llu\n", (unsigned long long)block_index);
	struct ouichefs_file_index_block *file_index;
	struct buffer_head *buffer_head_index;
	struct ouichefs_inode_info *inode_info = OUICHEFS_INODE(file->f_inode);

	// (3)
	if (block_index >= OUICHEFS_BLOCK_SIZE >> 2)
		return -EFBIG;

	// (4)
	buffer_head_index = sb_bread(super_blk, inode_info->index_block);
	if (!buffer_head_index)
		return -EIO;
	file_index = (struct ouichefs_file_index_block *)buffer_head_index->b_data;

	// (5)
	int block_number = file_index->blocks[block_index];
	printk(KERN_INFO "ouiches_read(): block_number = %d\n", block_number);

	if (block_number == 0) {
		brelse(buffer_head_index);
		return -EIO;
	}

	// (6)
	struct buffer_head *buffer_head_data = sb_bread(super_blk, block_number);

	if (!buffer_head_data) {
		brelse(buffer_head_index);
		return -EIO;
	}

	// (7)
	char *data_buffer = buffer_head_data->b_data;
	data_buffer += *offset % OUICHEFS_BLOCK_SIZE;

	// (8)
	if (buffer_head_data->b_size < len)
		bytes_to_copy = buffer_head_data->b_size;
	else
		bytes_to_copy = len;
	printk(KERN_INFO "ouiches_read(): bytes_to_copy = %lu\n", bytes_to_copy);

	// (9)
	bytes_copied = bytes_to_copy - copy_to_user(data, data_buffer, bytes_to_copy);
	printk(KERN_INFO"ouiches_read(): bytes_copied = %lu\n", bytes_copied);

	// (10)
	*offset += bytes_copied;
	file->f_pos = *offset;
	printk(KERN_INFO "ouiches_read(): new offset = %lld\n", (long long)*offset);

	// (11)
	brelse(buffer_head_data);
	brelse(buffer_head_index);
	printk(KERN_INFO "ouiches_read(): finished\n");
	return bytes_copied;
}

static ssize_t ouichefs_write(struct file *file, const char __user *data, size_t len, loff_t *pos) {
    struct inode *inode = file->f_inode;
    struct super_block *sb = inode->i_sb;
    struct ouichefs_inode_info *inode_info = OUICHEFS_INODE(inode);
    struct ouichefs_file_index_block *file_index;
    struct buffer_head *index_bh, *data_bh;
    char *data_buffer;
    size_t bytes_to_write, bytes_written = 0;
    sector_t logical_block;
    int block_number;

    printk(KERN_INFO "ouichefs_write: Starting write operation\n");
    printk(KERN_INFO "ouichefs_write: Position: %lld, Length: %zu\n", *pos, len);

    if (file->f_flags & O_APPEND) {
        *pos = inode->i_size;
        printk(KERN_INFO "ouichefs_write: Append mode, new position: %lld\n", *pos);
    }

    if (*pos >= OUICHEFS_MAX_FILESIZE) {
        printk(KERN_ERR "ouichefs_write: Position exceeds max file size\n");
        return -EFBIG;
    }

    while (len > 0) {
        // Calculate the logical block number to write to
        logical_block = *pos / OUICHEFS_BLOCK_SIZE;
        printk(KERN_INFO "ouichefs_write: Writing to logical block: %lu\n", (unsigned long)logical_block);

        // Read the file's index block
        index_bh = sb_bread(sb, inode_info->index_block);
        if (!index_bh) {
            printk(KERN_ERR "ouichefs_write: Failed to read index block\n");
            return -EIO;
        }
        file_index = (struct ouichefs_file_index_block *)index_bh->b_data;

        // Check if the block is already allocated
        if (file_index->blocks[logical_block] == 0) {
            // Allocate a new block
            block_number = get_free_block(OUICHEFS_SB(sb));
            if (!block_number) {
                brelse(index_bh);
                printk(KERN_ERR "ouichefs_write: No space left to allocate new block\n");
                return -ENOSPC;
            }
            file_index->blocks[logical_block] = block_number;
            mark_buffer_dirty(index_bh);
            sync_dirty_buffer(index_bh);
            printk(KERN_INFO "ouichefs_write: Allocated new block number: %d\n", block_number);
        } else {
            block_number = file_index->blocks[logical_block];
            printk(KERN_INFO "ouichefs_write: Using existing block number: %d\n", block_number);
        }

        // Read or initialize the data block
        data_bh = sb_bread(sb, block_number);
        if (!data_bh) {
            brelse(index_bh);
            printk(KERN_ERR "ouichefs_write: Failed to read data block\n");
            return -EIO;
        }
        data_buffer = data_bh->b_data;

        // Calculate the amount of data to write in this block
        bytes_to_write = min(len, (size_t)(OUICHEFS_BLOCK_SIZE - (*pos % OUICHEFS_BLOCK_SIZE)));
        printk(KERN_INFO "ouichefs_write: Bytes to write in this iteration: %zu\n", bytes_to_write);

        // Copy data from userspace to the data block
        if (copy_from_user(data_buffer + (*pos % OUICHEFS_BLOCK_SIZE), data, bytes_to_write)) {
            brelse(data_bh);
            brelse(index_bh);
            printk(KERN_ERR "ouichefs_write: Failed to copy data from user space\n");
            return -EFAULT;
        }

        mark_buffer_dirty(data_bh);
        sync_dirty_buffer(data_bh);
        brelse(data_bh);
        brelse(index_bh);

        // Update pointers and counters
        *pos += bytes_to_write;
        data += bytes_to_write;
        len -= bytes_to_write;
        bytes_written += bytes_to_write;
        printk(KERN_INFO "ouichefs_write: Updated position: %lld, Bytes written so far: %zu\n", *pos, bytes_written);

        // Update the file size if necessary
        if (*pos > inode->i_size) {
            inode->i_size = *pos;
            mark_inode_dirty(inode);
            printk(KERN_INFO "ouichefs_write: Updated file size: %lld\n", inode->i_size);
        }
    }

    printk(KERN_INFO "ouichefs_write: Write operation completed, total bytes written: %zu\n", bytes_written);
    return bytes_written;
}


<<<<<<< HEAD
static ssize_t ouichefs_write(struct file *file, const char __user *data, size_t len, loff_t *pos)
{
	printk(KERN_INFO "BEGIN CUSTOM WRITE!$$$$$\n");
	printk(KERN_INFO "write: au voir\n");
	printk(KERN_INFO "pos: %lld\n", *pos);
	printk(KERN_INFO "len: %zu\n", len);
	printk("file->f_pos: %lld\n", file->f_pos);

	struct inode *inode = file->f_inode;
	struct super_block *sb = inode->i_sb;
	struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
	struct ouichefs_file_index_block *index;
	struct buffer_head *bh_index, *bh;
	char *buffer;
	size_t to_be_written, written = 0;
	sector_t iblock;
	int bno;

	if (file->f_flags & O_APPEND) {
		*pos = inode->i_size;
	}

	if (*pos >= OUICHEFS_MAX_FILESIZE)
		return -EFBIG;

	while (len > 0) {
		// Calculer le bloc logique où écrire
		iblock = *pos / OUICHEFS_BLOCK_SIZE;

		// Lire l'index du fichier
		bh_index = sb_bread(sb, ci->index_block);
		if (!bh_index)
			return -EIO;
		index = (struct ouichefs_file_index_block *)bh_index->b_data;

		// Vérifier si le bloc est déjà alloué
		if (index->blocks[iblock] == 0) {
			// Allouer un nouveau bloc
			bno = get_free_block(OUICHEFS_SB(sb));
			if (!bno) {
				brelse(bh_index);
				return -ENOSPC;
			}
			index->blocks[iblock] = bno;
			mark_buffer_dirty(bh_index);
			sync_dirty_buffer(bh_index);
		} else {
			bno = index->blocks[iblock];
		}

		// Lire ou initialiser le bloc de données
		bh = sb_bread(sb, bno);
		if (!bh) {
			brelse(bh_index);
			return -EIO;
		}
		buffer = bh->b_data;

		// Calculer la quantité de données à écrire dans ce bloc
		to_be_written = min(len, (size_t)(OUICHEFS_BLOCK_SIZE - (*pos % OUICHEFS_BLOCK_SIZE)));

		// Copier les données de l'utilisateur dans le bloc de données
		if (copy_from_user(buffer + (*pos % OUICHEFS_BLOCK_SIZE), data, to_be_written)) {
			brelse(bh);
			brelse(bh_index);
			return -EFAULT;
		}

		mark_buffer_dirty(bh);
		sync_dirty_buffer(bh);
		brelse(bh);
		brelse(bh_index);

		*pos += to_be_written;
		data += to_be_written;
		len -= to_be_written;
		written += to_be_written;

		// Mettre à jour la taille du fichier si nécessaire
		if (*pos > inode->i_size) {
			inode->i_size = *pos;
			mark_inode_dirty(inode);
		}
	}
	printk(KERN_INFO "END CUSTOM WRITE!$$$$$\n");

	return written;
}


// static ssize_t ouichefs_write(struct file *file, const char __user *data, size_t len, loff_t *pos)
// {
// 	printk(KERN_INFO "BEGIN CUSTOM WRITE!$$$$$\n");
// 	printk(KERN_INFO "write: au voir\n");
// 	printk(KERN_INFO "pos: %lld\n", *pos);
// 	printk(KERN_INFO "len: %zu\n", len);
// 	printk("file->f_pos: %lld\n", file->f_pos);

// 	struct inode *inode = file->f_inode;
// 	struct super_block *sb = inode->i_sb;
// 	struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
// 	struct ouichefs_file_index_block *index;
// 	struct buffer_head *bh_index, *bh;
// 	char *buffer;
// 	size_t to_be_written, written = 0;
// 	sector_t iblock;
// 	int bno;

// 	if (file->f_flags & O_APPEND) {
// 		*pos = inode->i_size;
// 	}

// 	if (*pos >= OUICHEFS_MAX_FILESIZE)
// 		return -EFBIG;

// 	while (len > 0) {
// 		// Calculer le bloc logique où écrire
// 		iblock = *pos / OUICHEFS_BLOCK_SIZE;

// 		// Lire l'index du fichier
// 		bh_index = sb_bread(sb, ci->index_block);
// 		if (!bh_index)
// 			return -EIO;
// 		index = (struct ouichefs_file_index_block *)bh_index->b_data;

// 		// Vérifier si le bloc est déjà alloué
// 		if (index->blocks[iblock] == 0) {
// 			// Allouer un nouveau bloc
// 			bno = get_free_block(OUICHEFS_SB(sb));
// 			if (!bno) {
// 				brelse(bh_index);
// 				return -ENOSPC;
// 			}
// 			index->blocks[iblock] = bno;
// 			mark_buffer_dirty(bh_index);
// 			sync_dirty_buffer(bh_index);
// 		} else {
// 			bno = index->blocks[iblock];
// 		}

// 		// Lire ou initialiser le bloc de données
// 		bh = sb_bread(sb, bno);
// 		if (!bh) {
// 			brelse(bh_index);
// 			return -EIO;
// 		}
// 		buffer = bh->b_data;

// 		// Calculer la quantité de données à écrire dans ce bloc
// 		to_be_written = min(len, (size_t)(OUICHEFS_BLOCK_SIZE - (*pos % OUICHEFS_BLOCK_SIZE)));

// 		// Copier les données de l'utilisateur dans le bloc de données
// 		if (copy_from_user(buffer + (*pos % OUICHEFS_BLOCK_SIZE), data, to_be_written)) {
// 			brelse(bh);
// 			brelse(bh_index);
// 			return -EFAULT;
// 		}

// 		mark_buffer_dirty(bh);
// 		sync_dirty_buffer(bh);
// 		brelse(bh);
// 		brelse(bh_index);

// 		*pos += to_be_written;
// 		data += to_be_written;
// 		len -= to_be_written;
// 		written += to_be_written;

// 		// Mettre à jour la taille du fichier si nécessaire
// 		if (*pos > inode->i_size) {
// 			inode->i_size = *pos;
// 			mark_inode_dirty(inode);
// 		}
// 	}
// 	printk(KERN_INFO "END CUSTOM WRITE!$$$$$\n");

// 	return written;
// }


=======
>>>>>>> 71633bc (Removing duplicated write function)
const struct file_operations ouichefs_file_ops = {
	.owner = THIS_MODULE,
	.open = ouichefs_open,
	.llseek = generic_file_llseek,
	.read_iter = generic_file_read_iter,
	.read = ouichefs_read,
	.write = ouichefs_write,
	.write_iter = generic_file_write_iter
};
