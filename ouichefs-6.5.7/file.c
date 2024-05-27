


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
#include <linux/unistd.h>

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


/*Create the block entry based on the 12 bits of the block size
* and the 20 bits of the block number
*/
uint32_t create_block_entry(uint32_t block_number, uint32_t block_size) {
    return (block_size << 20) | (block_number & BLOCK_NUMBER_MASK);
}

/*Return the block number (the 20 last bits) based on block entry*/
uint32_t get_block_number(uint32_t entry) {
    return (entry & BLOCK_NUMBER_MASK);
}

/*Return the block size (the 12 first bits) based on block entry*/
uint32_t get_block_size(uint32_t entry) {
    return (entry & BLOCK_SIZE_MASK) >> 20;
}


/*In summarry this function enables users to retrieve data from a file by copying it into a provided buffer.
* Here's a concisely its functionality:
* - It checks if the current position within the file is valid for reading.
* - It reads the index block of the file to locate the data blocks containing the file's content.
* - It determines the data block corresponding to the current position within the file.
* - It reads data from the determined data block into the user-provided buffer, ensuring not to read beyond the file's end.
* - It updates the position pointer to reflect the number of bytes actually read.
* - It returns the number of bytes read, indicating a successful operation, or an error code if encountered during the process.*/
static ssize_t ouichefs_read(struct file *filep, char __user *buf, size_t len, loff_t *ppos)
{	
	struct inode *inode = filep->f_inode;
	struct super_block *sb = filep->f_inode->i_sb;
	struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
	struct ouichefs_file_index_block *index;
	struct buffer_head *bh_index;
	size_t bytes_to_read;
	size_t bytes_not_read;
	size_t bytes_read = 0;
	sector_t iblock;
	size_t offset;

	/* Nothing more to read */
	if (*ppos >= inode->i_size) {
		return bytes_read;
	}

	/* Read index block from disk */
	bh_index = sb_bread(sb, ci->index_block);
	if (!bh_index)
		return -EIO;
	index = (struct ouichefs_file_index_block *)bh_index->b_data;

	/* Block index calculation*/
	iblock = *ppos / OUICHEFS_BLOCK_SIZE;
	if (index->blocks[iblock] == 0) {
		brelse(bh_index);
		return bytes_read;
	}
	
	/* Read block from disk */
	struct buffer_head *bh = sb_bread(sb, get_block_number(index->blocks[iblock]));
	if (!bh) {
		brelse(bh_index);
		return -EIO;
	}

	/* Position in block calculation */
	offset = *ppos % OUICHEFS_BLOCK_SIZE;

	/* Number of bytes to read */
	size_t tmp = inode->i_size - *ppos;
	bytes_to_read = min((size_t) OUICHEFS_BLOCK_SIZE, tmp);
	bytes_not_read = copy_to_user(buf, bh->b_data + offset, bytes_to_read);
	if (bytes_not_read) {
		brelse(bh);
		brelse(bh_index);
		return -EFAULT;
	}

	bytes_read = bytes_to_read - bytes_not_read;
	*ppos += bytes_read;
	
	brelse(bh);
	brelse(bh_index);

	return bytes_read;
}


/*Read function for a file system implemented in the Linux kernel. 
* It is responsible for reading a fragment of data from a file, with additional 
* logic to handle sparse blocks (blocks that are not fully utilized)
*/


static int nb_block_read = 0; // Global counter to track the number of blocks read
static ssize_t ouichefs_read_fragment(struct file *filep, char __user *buf, size_t len, loff_t *ppos)
{	

	// Get the necessary structures from the file descriptor
	struct inode *inode = filep->f_inode;
	struct super_block *sb = filep->f_inode->i_sb;
	struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
	struct ouichefs_file_index_block *index;
	struct buffer_head *bh_index;
	size_t bytes_to_read;
	size_t bytes_not_read;
	size_t bytes_read = 0;
	sector_t iblock;
	size_t offset;

	// Checks if the number of blocks read exceeds the total number of blocks in the file
	if ( nb_block_read >= inode->i_blocks - 1) {
		nb_block_read = 0;
		return bytes_read;
	}

	// Reads the index block to get information from data blocks
	bh_index = sb_bread(sb, ci->index_block);
	if (!bh_index)
		return -EIO;
	index = (struct ouichefs_file_index_block *)bh_index->b_data;

	// Calculates block index based on current position
	iblock = *ppos / OUICHEFS_BLOCK_SIZE;
	if (index->blocks[iblock] == 0) {
		brelse(bh_index);
		return bytes_read;
	}

	// Retrieves the block number and reads the corresponding data block
	int bno = index->blocks[iblock];
	struct buffer_head *bh = sb_bread(sb, get_block_number(bno));
	if (!bh) {
		brelse(bh_index);
		return -EIO;
	}
	
	// Calculates offset from current position
	offset = *ppos % OUICHEFS_BLOCK_SIZE;
	uint32_t size = get_block_size(bno);
	int start = -1;
	int end = 0;
	// Determines the range of non-zero data to read from the block
	if (size != 0) { 
		for (int i = offset; i < OUICHEFS_BLOCK_SIZE; i++) {
			if (bh->b_data[i] != 0){ 
				if (start == -1){
					start = i; // Start of non-zero data
				}
				end++;
			}else{
				if (start != -1){
					break; // End of non-zero data
				}
			}
		}
		bytes_to_read = end; // Number of bytes to read
	} else {
		start = offset;
		bytes_to_read = (size_t) OUICHEFS_BLOCK_SIZE; // Read the whole block if size is 0
	}

	bytes_not_read = copy_to_user(buf, bh->b_data + start, bytes_to_read);
	if (bytes_not_read) {
		brelse(bh);
		brelse(bh_index);
		return -EFAULT;
	}

	bytes_read = bytes_to_read - bytes_not_read;
	*ppos += bytes_read;

	/* We read the entire block*/
	if (bytes_read >= size ) {
		 nb_block_read++;
		*ppos += OUICHEFS_BLOCK_SIZE - bytes_read;
	}
	
	brelse(bh);
	brelse(bh_index);

 
	return bytes_read;
}


/**
 *Clears a specified block by filling it with zeros.
 * @sb: Pointer to file system superblock.
 * @block_entry: The entry to the block to be cleaned.
 * This function reads the block specified by block_entry, fills it with zeros,
 * marks the buffer as dirty, synchronizes the buffer with the disk, then frees
 * the buffer.
 * Returns 0 on success, a negative error on failure.
 */
static int clean_block(struct super_block *sb, uint32_t block_entry) 
{
	struct buffer_head *bh = sb_bread(sb, get_block_number(block_entry));
	if (!bh) {
		brelse(bh);
		return -EIO;
	}

	// Fills the block with zeros, assuming the block size is 4096 bytes
	memset(bh->b_data, 0, 4096);
	mark_buffer_dirty(bh);
	sync_dirty_buffer(bh);
	brelse(bh);
	return 0;
}

/*
*This function does the basic write operation without optization.
*Here are the steps:
*1- Check Space: Ensures sufficient space is available for writing; if not, returns an error.
*2- Update Position: If in append mode, updates the write position to the end of the file.
*3- Read Index Block: Retrieves the index block of the file.
*4- Locate Data Block: Determines the data block corresponding to the current write position.
*5- Read Data Block: Reads the data block from disk into memory.
*6- Write Data: Copies data from the user buffer to the data block.
*7- Update Metadata: Updates block metadata and file size.
*8- Cleanup: Releases used buffers.
*9- Return: Returns the number of bytes written or an error code.
*/

static ssize_t ouichefs_write(struct file *filep, const char __user *buf, size_t len, loff_t *ppos)
{
	struct inode *inode = filep->f_inode;
	struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
	struct super_block *sb = inode->i_sb;
	struct ouichefs_sb_info *sbi = OUICHEFS_SB(sb);
	struct buffer_head *bh_index;
	struct ouichefs_file_index_block *index;
	size_t bytes_to_write;
	size_t bytes_write = 0;
	size_t bytes_not_write;
	sector_t iblock;
	size_t offset;
	size_t remaining;
	int bno;

	if (*ppos + len > OUICHEFS_MAX_FILESIZE)
		return -ENOSPC;
	
	/* Check if the write can be completed (enough space?) */
	uint32_t nr_allocs = max(*ppos + (unsigned int) len, inode->i_size) / OUICHEFS_BLOCK_SIZE;
	if (nr_allocs > inode->i_blocks - 1)
		nr_allocs -= inode->i_blocks - 1;
	else
		nr_allocs = 0;
	if (nr_allocs > sbi->nr_free_blocks)
		return -ENOSPC;
	

	/* If in append mode, updates the write position to the end of the file. */
	bool app = (filep->f_flags & O_APPEND) != 0;
	if (app) {
		*ppos = inode->i_size;
	}

	/* Read index block from disk */
	bh_index = sb_bread(sb, ci->index_block);
	if (!bh_index)
		return -EIO;
	index = (struct ouichefs_file_index_block *)bh_index->b_data;

	/* Determines the data block corresponding to the current write position */
	iblock = *ppos / OUICHEFS_BLOCK_SIZE;
	if (index->blocks[iblock] == 0) {
		bno = get_free_block(sbi);
		if (!bno) {
			brelse(bh_index);
			return -ENOSPC;
		}
		bno = create_block_entry((uint32_t)bno, (uint32_t)0);
		clean_block(sb, bno);
		index->blocks[iblock] = bno;
		mark_buffer_dirty(bh_index);
		sync_dirty_buffer(bh_index);
	} else {
		bno = index->blocks[iblock];
	}

	/* Reads the data block from disk into memory */
	struct buffer_head *bh = sb_bread(sb, get_block_number(bno));
	if (!bh) {
		brelse(bh_index);
		return -EIO;
	}

	offset = *ppos % OUICHEFS_BLOCK_SIZE;
	remaining = OUICHEFS_BLOCK_SIZE - offset;
	/* Number of bytes to write */
	bytes_to_write = min(len, remaining);

	/* Copies data from the user buffer to the data block */
	bytes_not_write = copy_from_user(bh->b_data + offset, buf, bytes_to_write);
	if (bytes_not_write) {
		brelse(bh);
		brelse(bh_index);
		return -EFAULT;
	}
	mark_buffer_dirty(bh);
	sync_dirty_buffer(bh);

	bytes_write = bytes_to_write - bytes_not_write;
	*ppos += bytes_write;

	uint32_t block_number = get_block_number(bno);
	uint32_t block_size = get_block_size(bno);
	block_size = (block_size + (uint32_t)bytes_write);
	bno = create_block_entry(block_number, block_size);
	index->blocks[iblock] = bno;

	brelse(bh);

	/* Updates block metadata and file size */
	if (*ppos > inode->i_size)
		inode->i_size = *ppos;

	uint32_t nr_blocks_old = inode->i_blocks;

	inode->i_blocks = inode->i_size / OUICHEFS_BLOCK_SIZE + 2;
	inode->i_mtime = inode->i_ctime = current_time(inode);
	mark_inode_dirty(inode);

	/* If file is smaller than before, free unused blocks */
	if (nr_blocks_old > inode->i_blocks) {
		for (int i = inode->i_blocks - 1; i < nr_blocks_old - 1; i++) {
			put_block(OUICHEFS_SB(sb), index->blocks[i]);
			index->blocks[i] = 0;
		}
	}

	mark_buffer_dirty(bh_index);
	sync_dirty_buffer(bh_index);
	brelse(bh_index);
	return bytes_write;
}


/*
* This function does the write operation by inserting blocs from position where one wants to write content.
* Here are the steps:
* This function extends the behavior of writing data to a file in the "ouichefs" filesystem. Here's a summary:
* 1- Space Check: It ensures that there's enough space in the filesystem to accommodate the write operation. If not, it stops and returns an error.
* 2- Position Update: If the file is opened in append mode, it updates the write position to the end of the file.
* 3- Index Block Read: It reads the index block of the file to locate the data blocks that hold the file's content.
* 4- Data Block Location: Based on the current write position, it identifies the specific data block where the data should be written.
* 5- Fragmentation Handling: If the data to be written would cause fragmentation (i.e., if there isn't enough contiguous space in the current block), it rearranges the blocks to make space for the data.
* 6- Data Writing: It copies the data from the user-provided buffer into the data block buffer. If fragmentation occurs, it handles the copying accordingly.
* 7- Metadata Update: After writing the data, it updates the metadata associated with the block to reflect the new size and other relevant information.
* 8- Cleanup: Once the write operation is completed, it releases any buffers or resources used during the process.
* 9- Return: Finally, it returns the number of bytes successfully written to the file, or an error code if any issues occurred during the process.
*/

static ssize_t ouichefs_write_fragment(struct file *filep, const char __user *buf, size_t len, loff_t *ppos)
{	

	// Extract the inode from the file pointer.
	struct inode *inode = filep->f_inode;
	struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
	struct super_block *sb = inode->i_sb;
	struct ouichefs_sb_info *sbi = OUICHEFS_SB(sb);
	// Declare various variables used throughout the function.
	struct buffer_head *bh_index;
	struct ouichefs_file_index_block *index;
	size_t bytes_to_write; 
	size_t bytes_write = 0;
	size_t bytes_not_write;
	sector_t iblock;
	size_t offset;
	size_t remaining;
	int bno;
	
	if (*ppos + len > OUICHEFS_MAX_FILESIZE)
		return -ENOSPC;

	// Calculate the number of blocks needed.
	uint32_t nr_allocs = max(*ppos + (unsigned int) len, inode->i_size) / OUICHEFS_BLOCK_SIZE;
	if (nr_allocs > inode->i_blocks - 1)
		nr_allocs -= inode->i_blocks - 1;
	else
		nr_allocs = 0;
	if (nr_allocs > sbi->nr_free_blocks)
		return -ENOSPC;

	// Check if the file is opened in append mode.
	bool app = (filep->f_flags & O_APPEND) != 0;
	if (app) {
		*ppos = inode->i_size;
	}

	bh_index = sb_bread(sb, ci->index_block);
	if (!bh_index)
		return -EIO;
	index = (struct ouichefs_file_index_block *)bh_index->b_data;
	// Calculate the block index and check if the block needs to be allocated.
	iblock = *ppos / OUICHEFS_BLOCK_SIZE;
	if (index->blocks[iblock] == 0) {
		bno = get_free_block(sbi);
		if (!bno) {
			brelse(bh_index);
			return -ENOSPC;
		}		
		bno = create_block_entry((uint32_t)bno, (uint32_t)0);
		clean_block(sb, bno);
		index->blocks[iblock] = bno;
	} else {
		bno = index->blocks[iblock];
	}
	// Read the block to be written to from disk.
	struct buffer_head *bh = sb_bread(sb, get_block_number(bno));
	if (!bh) {
		brelse(bh_index);
		return -EIO;
	}
	// Calculate the offset within the block and the number of bytes to write.
	offset = *ppos % OUICHEFS_BLOCK_SIZE;
	remaining = OUICHEFS_BLOCK_SIZE - offset;
	bytes_to_write = min(len, remaining);
    // Check if the number of blocks needed exceeds the filesystem's limits.
	size_t number_of_blocks_needed = ((len + offset) / OUICHEFS_BLOCK_SIZE);
	if (number_of_blocks_needed + inode->i_blocks > (OUICHEFS_BLOCK_SIZE >> 2) - 1) {
		brelse(bh);
		brelse(bh_index);
		return -ENOSPC;
	}

	// Loop to check for contiguous blocks to move if needed.
	int cmpt = 0;
	int position_to_copy = -1;
	for (size_t i = offset ; i < OUICHEFS_BLOCK_SIZE; i++) {
		if (bh->b_data[i] != 0){
			if (position_to_copy == -1){
				position_to_copy = i;
				number_of_blocks_needed++;
			}
			cmpt++;
		}else{
			if (position_to_copy != -1){
				break;
			}
		}
	}
	// If a position to copy was found, perform the necessary block allocations and data moves.
	if (position_to_copy != -1){
		for (int j = (int)(inode->i_blocks) - 2; j > (int) iblock; j--) {
			index->blocks[j + number_of_blocks_needed] = index->blocks[j];	
		}
		
		for (int i = iblock + number_of_blocks_needed; i > iblock; i--){
			int bno_bis = get_free_block(sbi);
			if (!bno_bis) {
				brelse(bh);
				brelse(bh_index);
				return -ENOSPC;
			}
			bno_bis = create_block_entry((uint32_t)bno_bis, (uint32_t)0);
			clean_block(sb, bno_bis);
			index->blocks[i] = bno_bis;
		}

		int last_inserted_block = index->blocks[iblock + number_of_blocks_needed];
		struct buffer_head *tmpbh = sb_bread(sb, get_block_number(last_inserted_block));
		if (!tmpbh) {
			brelse(bh_index);
			return -EIO;
		}
		
		char* ptr_to_copy_from = bh->b_data + position_to_copy;
		memcpy(tmpbh->b_data, ptr_to_copy_from, cmpt);
		mark_buffer_dirty(tmpbh);
		sync_dirty_buffer(tmpbh);
		brelse(tmpbh);
		uint32_t block_number = get_block_number(last_inserted_block);
		uint32_t block_size = get_block_size(last_inserted_block);
		block_size = block_size + (uint32_t)cmpt;
		last_inserted_block = create_block_entry(block_number, block_size);
		index->blocks[iblock + number_of_blocks_needed] = last_inserted_block;


		memset(ptr_to_copy_from, 0, cmpt);

		uint32_t block_number_fragment = get_block_number(bno);
		uint32_t block_size_fragment = get_block_size(bno);
		block_size_fragment = block_size_fragment - (uint32_t)cmpt;

		bno = create_block_entry(block_number_fragment, block_size_fragment);
		index->blocks[iblock] = bno;

	}
	 // Copy data from the user buffer to the block
	bytes_not_write = copy_from_user(bh->b_data + offset, buf, bytes_to_write);
	if (bytes_not_write) {
		brelse(bh);
		brelse(bh_index);
		return -EFAULT;
	}
	mark_buffer_dirty(bh);
	sync_dirty_buffer(bh);
	brelse(bh);
	bytes_write = bytes_to_write - bytes_not_write;
	*ppos += bytes_write;

	// Update the block entry with the new size.
	uint32_t block_number_fragment = get_block_number(bno);
	uint32_t block_size_fragment = get_block_size(bno);
	block_size_fragment = (block_size_fragment + (uint32_t)bytes_write);
	bno = create_block_entry(block_number_fragment, block_size_fragment);
	index->blocks[iblock] = bno;

	if (*ppos > inode->i_size) {
		inode->i_size = *ppos;
	}
	
	// Update the number of blocks used by the inode.
	uint32_t nr_blocks_old = inode->i_blocks;
	if (inode->i_blocks == 1 || inode->i_blocks == 0){
		inode->i_blocks = inode->i_size / OUICHEFS_BLOCK_SIZE + 2;
	} else {
		inode->i_blocks += number_of_blocks_needed;
	}
	inode->i_mtime = inode->i_ctime = current_time(inode);
	mark_inode_dirty(inode);


	// Free any old blocks if the number of blocks used has decreased.
	if (nr_blocks_old > inode->i_blocks) {
		for (int i = inode->i_blocks - 1; i < nr_blocks_old - 1; i++) {
			put_block(OUICHEFS_SB(sb), index->blocks[i]);
			index->blocks[i] = 0;
		}
	}
	
	// Mark the index buffer as dirty and sync it to disk.
	mark_buffer_dirty(bh_index);
	sync_dirty_buffer(bh_index);
	brelse(bh_index);
	return bytes_write;
}

/* This function implements an ioctl to handle a command on the file system to check
 * informations regarding input file blocks.
 * Here are its steps:
 * 1- Function Purpose: The `ouichefs_ioctl` function is responsible for handling IOCTL commands in the "ouichefs" filesystem.
 * 2- Initialization: It initializes variables and allocates memory for an `ouichefs_ioctl_info` structure to store block information.
 * 3- Command Validation: It checks if the received command is `OUICHEFS_IOC_GET_INFO`. If not, it returns `-ENOTTY`, indicating that the command is not supported.
 * 4- Reading Index Block: It reads the index block of the file associated with the given inode to retrieve information about the blocks used by the file.
 * 5- Iterating Over Blocks: It iterates through the entries in the index block to extract block numbers and sizes.
 * 6- Collecting Block Information: For each non-zero block entry, it stores the block number and effective size in the `ouichefs_ioctl_info` structure. It also counts partially filled blocks and calculates internal fragmentation if applicable.
 * 7- Copying to User Space: It copies the collected block information from the kernel space to the user space using `copy_to_user`.
 * 8- Cleanup: It frees the allocated memory for the `ouichefs_ioctl_info` structure.
 * 9- Return Value: If the copy to user space is successful, it returns `0` to indicate success. Otherwise, it returns `-EFAULT` if there's a memory copy error or `-EIO` if there's an error reading the index block.
 */
static long ouichefs_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct inode *inode = file_inode(file);
	struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
	struct ouichefs_file_index_block *file_index;
	struct buffer_head *bh_index;
	struct super_block *sb = inode->i_sb;

	/* Allocates memory for an `ouichefs_ioctl_info` structure to store block information */
	struct ouichefs_ioctl_info *info = kmalloc(sizeof(struct ouichefs_ioctl_info), GFP_KERNEL);
	if (!info) {
		return -ENOMEM;
	}

	memset(info, 0, sizeof(struct ouichefs_ioctl_info));

	/* It checks if the received command is `OUICHEFS_IOC_GET_INFO` */
	if (cmd != OUICHEFS_IOC_GET_INFO) {
		kfree(info);
		return -ENOTTY;
	}

	/* Reads the index block */
	bh_index = sb_bread(sb, ci->index_block);
	if (!bh_index) {
		kfree(info);
		return -EIO;
	}
	file_index = (struct ouichefs_file_index_block *)bh_index->b_data;

	/* Iterates through the entries in the index block to extract block numbers and sizes */
	for (int i = 0; i < (OUICHEFS_BLOCK_SIZE >> 2); ++i) {
		uint32_t entry = file_index->blocks[i];
		if (entry == 0) {
			continue;
		}

		uint32_t block_number = get_block_number(entry);
		uint32_t size = get_block_size(entry);

		/* Stores the block number and effective size*/
		info->blocks[info->used_blocks].block_number = block_number;
		info->blocks[info->used_blocks].effective_size = size;
		info->used_blocks++;

		/* Counts partially filled blocks and calculates internal fragmentation if applicable */
		if (size != 0 && size < OUICHEFS_BLOCK_SIZE) {
			info->partially_filled_blocks++;
			info->internal_fragmentation += (OUICHEFS_BLOCK_SIZE - size);
		}
		}
		brelse(bh_index);
	
	/* Copies the collected block information */
	if (copy_to_user((struct ouichefs_ioctl_info *)arg, info, sizeof(struct ouichefs_ioctl_info))) {
		kfree(info);
		return -EFAULT;
	}
	kfree(info);
    return 0;
}

/* Called by ouichefs_ioctl_defragmentation function and helps to move all non zero
 * data to the beginning of the block and zeroing out the remaining space:
 * Following are the steps:
 * 1- Check Block Size: Returns `0` if the block size is zero.
 * 2- Read Block: Loads the block data from the disk.
 * 3- Defragmentation:
 *   - Iterates through the block data.
 *   - Moves non-zero data leftwards to fill gaps.
 *   - Zeros out the space freed by the moved data.
 * 4- Mark and Sync: Marks the buffer as dirty and synchronizes it to disk to save changes.
 * 5- Cleanup: Releases the buffer head.
 * The function returns `0` on success or `-EIO` on read failure.
 */
int apply_contigue(uint32_t current_block, struct super_block *sb)
{

	struct buffer_head *bh;
	loff_t empty = -1;
	loff_t full = -1;
	loff_t copy_len = 0;
	int active = 0;

	if (get_block_size(current_block) == 0) {
		return 0;
	}

	/* Loads the block data from the disk */
	bh = sb_bread(sb, get_block_number(current_block));
	if (!bh) {
        return -EIO;
    }
	
	/*  Iterates through the block data */
	for (loff_t block_pos = 0; block_pos < OUICHEFS_BLOCK_SIZE; block_pos++) {
		if(bh->b_data[block_pos] != 0) {
			if (active) {
				copy_len = block_pos;
			} else {
				full = block_pos;
			}
		} else {
			if (active && copy_len != 0) {
				memcpy(bh->b_data + full  + 1, bh->b_data + empty + 1, copy_len - empty);
				memset(bh->b_data + empty + 1, 0, copy_len - empty);
				full += copy_len - (empty + 1);
				empty = copy_len + 1;
				copy_len = 0;
				active = 0;
			} else {
				active = 1;
				empty = block_pos;
			}
		}
	}

	if (active && empty != OUICHEFS_BLOCK_SIZE - 1) {
		memcpy(bh->b_data + full, bh->b_data + empty + 1, copy_len - empty);
		memset(bh->b_data + empty + 1, 0, copy_len - empty);
	}
	mark_buffer_dirty(bh);
	sync_dirty_buffer(bh);
	brelse(bh);
	return 0;
}

static long ouichefs_ioctl_defragmentation(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct inode *inode = file->f_inode;
    struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
	struct super_block *sb = inode->i_sb;
    struct ouichefs_file_index_block *file_index;
    struct buffer_head *bh_index;
	struct buffer_head *bh;
	struct buffer_head *bh_bis;

	bh_index = sb_bread(sb, ci->index_block);
	if (!bh_index) {
		return -EIO;
	}
	file_index = (struct ouichefs_file_index_block *)bh_index->b_data;

	/* Iterate over the blocks in the index to ensure the blocks are contiguous */
	uint32_t current_block;
	for (int i = 0; i < (OUICHEFS_BLOCK_SIZE >> 2); i++) {
		current_block = file_index->blocks[i];
		if (current_block == 0) {
			break;
		}
		apply_contigue(current_block, sb);
	}
	
	/* Index of the block to copy data to */
	int copy_to = 0;
	/* Index of the block to copy data from */
	int copy_from = 0;
	/* Total bytes copied so far */
	int copied_so_far = 0;

	/* Iterate over the blocks again for defragmentation */
	for (int i = 0; i < (OUICHEFS_BLOCK_SIZE >> 2); i++) {
		uint32_t current_block = file_index->blocks[i];
		if (current_block == 0) {
			break;
		}
		uint32_t size = get_block_size(current_block);
		uint32_t size_left;
		
		/* If the block is already full, skip it */
		if (size == OUICHEFS_BLOCK_SIZE) {
			continue;
		}

		/* Read the current block from disk */
		bh = sb_bread(sb, get_block_number(current_block));
		if (!bh) {
			brelse(bh_index);
			return -EIO;
		}

		/* Calculate the remaining space in the current block */
		size_left = OUICHEFS_BLOCK_SIZE - size;
		copy_to = i;
		
		/* Iterate over the next blocks to merge */
		int j = i + 1;
		for (; j < (OUICHEFS_BLOCK_SIZE >> 2); j++) {
			copy_from = j;
			uint32_t from_block = file_index->blocks[j];

			if (from_block == 0) {
				break;
			}
			if (size == OUICHEFS_BLOCK_SIZE) {
				break;
			}

			uint32_t size_from = get_block_size(from_block);
			size_t bytes_to_copy = min(size_left, size_from);

			/* Read the next block from disk */
			bh_bis = sb_bread(sb, get_block_number(from_block));
			if (!bh_bis) {
				brelse(bh_bis);
				return -EIO;
			}

			/* Copy data from the next block to the current block */
			memcpy(bh->b_data + size, bh_bis->b_data, bytes_to_copy);
			/* Zero out the copied bytes in the next block */
			memset(bh_bis->b_data, 0, bytes_to_copy);

			/* Update the size of the current block */
			size = size + bytes_to_copy;
			/* Update the block entry for the current block */
			current_block = create_block_entry(get_block_number(current_block), size);
			file_index->blocks[i] = current_block;

			/* Update the size of the next block */
			size_from = size_from - bytes_to_copy;
			/* Update the block entry for the next block */
			from_block = create_block_entry(get_block_number(from_block), size_from);
			file_index->blocks[j] = from_block;

			/* Ensure the next block is contiguous if not fully copied */
			if (bytes_to_copy != size_from) {
				apply_contigue(from_block, sb);
			}

			mark_buffer_dirty(bh_bis);
			sync_dirty_buffer(bh_bis);
			brelse(bh_bis);
		}
		mark_buffer_dirty(bh);
		sync_dirty_buffer(bh);
		brelse(bh);

		/* Update the total bytes copied so far */
		copied_so_far += size;

		/* If all the inode data has been copied */
		if (copied_so_far >= inode->i_size) {
			/* Release any remaining blocks */
			for (int z = j-1; z < inode->i_blocks-1; z++) {
				put_block(OUICHEFS_SB(sb), file_index->blocks[z]);
				file_index->blocks[z] = 0;
			}

			/* Update the inode's block count */
			inode->i_blocks = j;
			mark_inode_dirty(inode);
		}
	}
	
	mark_buffer_dirty(bh_index);
	sync_dirty_buffer(bh_index);
	brelse(bh_index);
	return 0;
}

/*
 *This function helps to select the ioctl command to execute
 */
static long my_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case OUICHEFS_IOC_GET_INFO:
		ouichefs_ioctl(file, cmd, arg);
		break;
	case OUICHEFS_IOC_GET_DEFRAG:
		ouichefs_ioctl_defragmentation(file, cmd, arg);
		break;
	default:
		break;
	}
	return 0;
}

const struct file_operations ouichefs_file_ops = {
	.owner = THIS_MODULE,
	.open = ouichefs_open,
	// .read = ouichefs_read,
	// .write = ouichefs_write,
	.llseek = generic_file_llseek,
	.read_iter = generic_file_read_iter,
	.read = ouichefs_read_fragment,
	.write = ouichefs_write_fragment,
	.write_iter = generic_file_write_iter,
	.unlocked_ioctl = my_ioctl
};


/* */

// static int declencher_decalage(loff_t offset, int len, int idx_current_block,  int target_block, struct super_block *sb, struct ouichefs_file_index_block *index) 
// {
// 	int block_preced;
// 	struct buffer_head *bh_preced;
// 	uint32_t block_size_preced;

// 	for(int i = idx_current_block; i >= target_block; i--) {
// 		int block = index->blocks[i];
// 		struct buffer_head *bh = sb_bread(sb, get_block_number(block));
// 		uint32_t block_size = get_block_size(bno);
// 		block_preced = i + 1;
// 		*bh_preced = sb_bread(sb, get_block_number(block_preced));
// 		block_size_preced = get_block_size(block_preced);
// 		if(block_size != 0 || block_size != OUICHEFS_BLOCK_SIZE){
// 			memcpy(bh->b_data + block_size + len, bh->b_data, block_size);
// 		}else if (i == target_block) {
// 			memcpy(bh_preced->b_data, bh->data + offset, len);
// 			return 0;
// 		}else {
// 			memcpy(bh_preced->b_data, bh->data + block_size_preced - len, len);
// 			memcpy(bh->data + block_size_preced - len, bh->data, len);
// 		}
// 	}
// }


// static ssize_t ouichefs_write_plus(struct file *filep, const char __user *buf, size_t len, loff_t *ppos)
// {	
// 	struct inode *inode = filep->f_inode;
// 	struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
// 	struct super_block *sb = inode->i_sb;
// 	struct ouichefs_sb_info *sbi = OUICHEFS_SB(sb);
// 	struct buffer_head *bh_index;
// 	struct ouichefs_file_index_block *index;
// 	size_t bytes_to_write; 
// 	size_t bytes_write = 0;
// 	size_t bytes_not_write;
// 	sector_t iblock;
// 	size_t offset;
// 	size_t remaining;
// 	int bno;
	
// 	if (*ppos + len > OUICHEFS_MAX_FILESIZE)
// 		return -ENOSPC;

// 	uint32_t nr_allocs = max(*ppos + (unsigned int) len, inode->i_size) / OUICHEFS_BLOCK_SIZE;
// 	if (nr_allocs > inode->i_blocks - 1)
// 		nr_allocs -= inode->i_blocks - 1;
// 	else
// 		nr_allocs = 0;
// 	if (nr_allocs > sbi->nr_free_blocks)
// 		return -ENOSPC;

// 	bool app = (filep->f_flags & O_APPEND) != 0;
// 	if (app) {
// 		*ppos = inode->i_size;
// 	}

// 	bh_index = sb_bread(sb, ci->index_block);
// 	if (!bh_index)
// 		return -EIO;
// 	index = (struct ouichefs_file_index_block *)bh_index->b_data;

// 	iblock = *ppos / OUICHEFS_BLOCK_SIZE;
// 	if (index->blocks[iblock] == 0) {
// 		bno = get_free_block(sbi);
// 		if (!bno) {
// 			brelse(bh_index);
// 			return -ENOSPC;
// 		}
// 		bno = create_block_entry((uint32_t)bno, (uint32_t)0);
// 		clean_block(sb, bno);
// 		index->blocks[iblock] = bno;
// 		mark_buffer_dirty(bh_index);
// 		sync_dirty_buffer(bh_index);
// 	} else {
// 		bno = index->blocks[iblock];
// 		// 1. not sure we need to do this (or if we can or should)
// 		apply_contigue(get_block_number(bno),sb);
// 	}
	
// 	struct buffer_head *bh = sb_bread(sb, get_block_number(bno));
// 	if (!bh) {
// 		brelse(bh_index);
// 		return -EIO;
// 	}

// 	offset = *ppos % OUICHEFS_BLOCK_SIZE;
// 	remaining = OUICHEFS_BLOCK_SIZE - offset;
// 	bytes_to_write = min(len, remaining);
// 	if (*(bh->b_data + offset) != 0) {
// 		if (remaining >= len) {
// 			memcpy(bh->b_data + offset + len, bh->b_data, get_block_size(bno)); 
// 		} else {
// 			int current = inode->i_size - 1;
// 			for (; current == iblock; current--) {
// 				uint32_t current_block = index->blocks[current];
// 				uint32_t current_block_size = get_block_size(current_block);
// 				if (OUICHEFS_BLOCK_SIZE - current_block_size >= len) {
// 					declancher_decalage(offset, len, current, iblock, sb, index);
// 				}else {
// 					if(inode->i_blocks - 1 == (OUICHEFS_MAX_FILESIZE >> 2)) {
// 						// pass du espace pour ecriture (echec)
// 					} else {
// 						// allouer nouveau bloc
// 						int bno_bis = get_free_block(sbi);
// 						if (!bno_bis) {
// 							brelse(bh);
// 							brelse(bh_index);
// 							return -ENOSPC;
// 						}
// 						bno_bis = create_block_entry((uint32_t)bno_bis, (uint32_t)0);
// 						clean_block(sb, bno_bis);
// 						index->blocks[inode->i_size - 1] = bno_bis;
// 						declancher_decalage(offset, len, i_size - 1, iblock, sb, index);
// 					}
// 				}
// 			}
// 		}
// 	}
// 	bytes_not_write = copy_from_user(bh->b_data + offset, buf, bytes_to_write);
// 	if (bytes_not_write) {
// 		brelse(bh);
// 		brelse(bh_index);
// 		return -EFAULT;
// 	}
// 	mark_buffer_dirty(bh);
// 	sync_dirty_buffer(bh);

// 	bytes_write = bytes_to_write - bytes_not_write;
// 	*ppos += bytes_write;

// 	uint32_t block_number = get_block_number(bno);
// 	uint32_t block_size = get_block_size(bno);
// 	block_size = (block_size + (uint32_t)bytes_write);
// 	bno = create_block_entry(block_number, block_size);
// 	index->blocks[iblock] = bno;

// 	brelse(bh);

// 	if (*ppos > inode->i_size)
// 		inode->i_size = *ppos;

// 	uint32_t nr_blocks_old = inode->i_blocks;

// 	inode->i_blocks = inode->i_size / OUICHEFS_BLOCK_SIZE + 2;
// 	inode->i_mtime = inode->i_ctime = current_time(inode);
// 	mark_inode_dirty(inode);

// 	if (nr_blocks_old > inode->i_blocks) {
// 		for (int i = inode->i_blocks - 1; i < nr_blocks_old - 1; i++) {
// 			put_block(OUICHEFS_SB(sb), index->blocks[i]);
// 			index->blocks[i] = 0;
// 		}
// 	}
// 	mark_buffer_dirty(bh_index);
// 	sync_dirty_buffer(bh_index);
// 	brelse(bh_index);

// 	return bytes_write;
// }