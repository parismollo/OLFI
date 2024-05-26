

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


/*ioctl functions*/
uint32_t create_block_entry(uint32_t block_number, uint32_t block_size) {
    return (block_size << 20) | (block_number & BLOCK_NUMBER_MASK);
}

uint32_t get_block_number(uint32_t entry) {
    return (entry & BLOCK_NUMBER_MASK);
}

uint32_t get_block_size(uint32_t entry) {
    return (entry & BLOCK_SIZE_MASK) >> 20;
}


/*
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

	if (*ppos >= inode->i_size) {
		return bytes_read;
	}

	bh_index = sb_bread(sb, ci->index_block);
	if (!bh_index)
		return -EIO;
	index = (struct ouichefs_file_index_block *)bh_index->b_data;

	iblock = *ppos / OUICHEFS_BLOCK_SIZE;
	if (index->blocks[iblock] == 0) {
		brelse(bh_index);
		return bytes_read;
	}

	struct buffer_head *bh = sb_bread(sb, get_block_number(index->blocks[iblock]));
	if (!bh) {
		brelse(bh_index);
		return -EIO;
	}

	offset = *ppos % OUICHEFS_BLOCK_SIZE;
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
*/


static int nb_block_read = 0;
static ssize_t ouichefs_read_fragment(struct file *filep, char __user *buf, size_t len, loff_t *ppos)
{	
	//pr_info("Enter in ouichefs_read\n");
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

	/*if (*ppos >= inode->i_size) {
		return bytes_read;
	}*/
	//pr_info("nb_block_read = %d\n", nb_block_read);
	pr_info("inode->i_blocks = %llu\n", inode->i_blocks);
	if ( nb_block_read >= inode->i_blocks - 1) {
		nb_block_read = 0;
		return bytes_read;
	}

	bh_index = sb_bread(sb, ci->index_block);
	if (!bh_index)
		return -EIO;
	index = (struct ouichefs_file_index_block *)bh_index->b_data;

	iblock = *ppos / OUICHEFS_BLOCK_SIZE;
	if (index->blocks[iblock] == 0) {
		brelse(bh_index);
		return bytes_read;
	}
	int bno = index->blocks[iblock];
	struct buffer_head *bh = sb_bread(sb, get_block_number(bno));
	if (!bh) {
		brelse(bh_index);
		return -EIO;
	}
	
	offset = *ppos % OUICHEFS_BLOCK_SIZE;
	uint32_t size = get_block_size(bno);
	int start = -1;
	int end = 0;
	if (size != 0) { 
		for (int i = offset; i < OUICHEFS_BLOCK_SIZE; i++) {
			if (bh->b_data[i] != 0){ 
				if (start == -1){
					start = i;
				}
				end++;
			}else{
				if (start != -1){
					break;
				}
			}
		}
		bytes_to_read = end;
	} else {
		start = offset;
		bytes_to_read = (size_t) OUICHEFS_BLOCK_SIZE;
	}

	bytes_not_read = copy_to_user(buf, bh->b_data + start, bytes_to_read);
	if (bytes_not_read) {
		brelse(bh);
		brelse(bh_index);
		return -EFAULT;
	}

	bytes_read = bytes_to_read - bytes_not_read;
	*ppos += bytes_read;

	if (bytes_read >= size ) {
		nb_block_read++;
		*ppos += OUICHEFS_BLOCK_SIZE - bytes_read;
	}
	
	brelse(bh);
	brelse(bh_index);

	// pr_info("Total bytes read: %ld\n", bytes_read);
	return bytes_read;
}

int clean_block(struct super_block *sb, uint32_t block_entry) 
{
	struct buffer_head *bh = sb_bread(sb, get_block_number(block_entry));
	if (!bh) {
		brelse(bh);
		return -EIO;
	}
	memset(bh->b_data, 0, 4096);
	mark_buffer_dirty(bh);
	sync_dirty_buffer(bh);
	brelse(bh);
	return 0;
}

/*
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

	uint32_t nr_allocs = max(*ppos + (unsigned int) len, inode->i_size) / OUICHEFS_BLOCK_SIZE;
	if (nr_allocs > inode->i_blocks - 1)
		nr_allocs -= inode->i_blocks - 1;
	else
		nr_allocs = 0;
	if (nr_allocs > sbi->nr_free_blocks)
		return -ENOSPC;

	bool app = (filep->f_flags & O_APPEND) != 0;
	if (app) {
		*ppos = inode->i_size;
	}

	bh_index = sb_bread(sb, ci->index_block);
	if (!bh_index)
		return -EIO;
	index = (struct ouichefs_file_index_block *)bh_index->b_data;

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
	
	struct buffer_head *bh = sb_bread(sb, get_block_number(bno));
	if (!bh) {
		brelse(bh_index);
		return -EIO;
	}

	offset = *ppos % OUICHEFS_BLOCK_SIZE;
	remaining = OUICHEFS_BLOCK_SIZE - offset;
	bytes_to_write = min(len, remaining);

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

	if (*ppos > inode->i_size)
		inode->i_size = *ppos;

	uint32_t nr_blocks_old = inode->i_blocks;

	inode->i_blocks = inode->i_size / OUICHEFS_BLOCK_SIZE + 2;
	inode->i_mtime = inode->i_ctime = current_time(inode);
	mark_inode_dirty(inode);

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
}*/

static ssize_t ouichefs_write_fragment(struct file *filep, const char __user *buf, size_t len, loff_t *ppos)
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

	uint32_t nr_allocs = max(*ppos + (unsigned int) len, inode->i_size) / OUICHEFS_BLOCK_SIZE;
	if (nr_allocs > inode->i_blocks - 1)
		nr_allocs -= inode->i_blocks - 1;
	else
		nr_allocs = 0;
	if (nr_allocs > sbi->nr_free_blocks)
		return -ENOSPC;

	bool app = (filep->f_flags & O_APPEND) != 0;
	if (app) {
		
		*ppos = inode->i_size;
	}

	bh_index = sb_bread(sb, ci->index_block);
	if (!bh_index)
		return -EIO;
	index = (struct ouichefs_file_index_block *)bh_index->b_data;
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
	struct buffer_head *bh = sb_bread(sb, get_block_number(bno));
	if (!bh) {
		brelse(bh_index);
		return -EIO;
	}
	offset = *ppos % OUICHEFS_BLOCK_SIZE;
	remaining = OUICHEFS_BLOCK_SIZE - offset;
	bytes_to_write = min(len, remaining);

	size_t number_of_blocks_needed = ((len + offset) / OUICHEFS_BLOCK_SIZE);
	if (number_of_blocks_needed + inode->i_blocks > (OUICHEFS_BLOCK_SIZE >> 2) - 1) {
		brelse(bh);
		brelse(bh_index);
		return -ENOSPC;
	}

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

	uint32_t block_number_fragment = get_block_number(bno);
	uint32_t block_size_fragment = get_block_size(bno);
	block_size_fragment = (block_size_fragment + (uint32_t)bytes_write);
	bno = create_block_entry(block_number_fragment, block_size_fragment);
	index->blocks[iblock] = bno;

	if (*ppos > inode->i_size){
		inode->i_size = *ppos;
	}
	uint32_t nr_blocks_old = inode->i_blocks;
	if (inode->i_blocks == 1 || inode->i_blocks == 0){
		inode->i_blocks = inode->i_size / OUICHEFS_BLOCK_SIZE + 2;
	} else {
		inode->i_blocks += number_of_blocks_needed;
	}
	inode->i_mtime = inode->i_ctime = current_time(inode);
	mark_inode_dirty(inode);

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


static long ouichefs_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    struct inode *inode = file_inode(file);
    struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
    struct ouichefs_file_index_block *file_index;
    struct buffer_head *bh_index;
    struct super_block *sb = inode->i_sb;


	struct ouichefs_ioctl_info *info = kmalloc(sizeof(struct ouichefs_ioctl_info), GFP_KERNEL);
    if (!info) {
        return -ENOMEM;
    }

    memset(info, 0, sizeof(struct ouichefs_ioctl_info));

    if (cmd != OUICHEFS_IOC_GET_INFO) {
		kfree(info);
        return -ENOTTY;
    }

    bh_index = sb_bread(sb, ci->index_block);
    if (!bh_index) {
		kfree(info);
        return -EIO;
    }
    file_index = (struct ouichefs_file_index_block *)bh_index->b_data;

    for (int i = 0; i < (OUICHEFS_BLOCK_SIZE >> 2); ++i) {
        uint32_t entry = file_index->blocks[i];
        if (entry == 0) {
            continue;
        }

        uint32_t block_number = get_block_number(entry);
        uint32_t size = get_block_size(entry);

        info->blocks[info->used_blocks].block_number = block_number;
        info->blocks[info->used_blocks].effective_size = size;
        info->used_blocks++;

        if (size != 0 && size < OUICHEFS_BLOCK_SIZE) {
            info->partially_filled_blocks++;
            info->internal_fragmentation += (OUICHEFS_BLOCK_SIZE - size);
        }
    }
    brelse(bh_index);

    if (copy_to_user((struct ouichefs_ioctl_info *)arg, info, sizeof(struct ouichefs_ioctl_info))) {
		kfree(info);
        return -EFAULT;
    }
	kfree(info);
    return 0;
}


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
		
	bh = sb_bread(sb, get_block_number(current_block));
	if (!bh) {
        return -EIO;
    }
	
	for(loff_t block_pos = 0; block_pos < OUICHEFS_BLOCK_SIZE; block_pos++) {
		if(bh->b_data[block_pos] != 0) {
			if(active) {
				copy_len = block_pos;
			} else {
				full = block_pos;
			}
		}else {
			if(active && copy_len != 0) {
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
	uint32_t current_block;
	for (int i = 0; i < (OUICHEFS_BLOCK_SIZE >> 2); i++) {
		current_block = file_index->blocks[i];
		if (current_block == 0) {
			break;
		}
		int n = apply_contigue(current_block, sb);
		
	}

	int copy_to = 0;
	int copy_from = 0;
	int copied_so_far = 0;
	for (int i = 0; i < (OUICHEFS_BLOCK_SIZE >> 2); i++) {
		uint32_t current_block = file_index->blocks[i];
		if(current_block == 0) {
			break;
		}
		uint32_t size = get_block_size(current_block);
		uint32_t size_left;
		
		if(size == OUICHEFS_BLOCK_SIZE) {
			continue;
		}

		bh = sb_bread(sb, get_block_number(current_block));
		if (!bh) {
			brelse(bh_index);
			return -EIO;
		}
		
		size_left = OUICHEFS_BLOCK_SIZE - size;
		copy_to = i; 
		int j = i + 1;
		for(; j < (OUICHEFS_BLOCK_SIZE >> 2); j++) {
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
			
			bh_bis = sb_bread(sb, get_block_number(from_block));
			if (!bh_bis) {
				brelse(bh_bis);
				return -EIO;
			}
			memcpy(bh->b_data + size, bh_bis->b_data, bytes_to_copy);
			memset(bh_bis->b_data, 0, bytes_to_copy);
			size = size + bytes_to_copy;
			current_block = create_block_entry(get_block_number(current_block), size);
			file_index->blocks[i] = current_block;

			size_from = size_from - bytes_to_copy;
			from_block = create_block_entry(get_block_number(from_block), size_from);
			file_index->blocks[j] = from_block;
			if(bytes_to_copy != size_from) {
				apply_contigue(from_block, sb);
			}

			mark_buffer_dirty(bh_bis);
			sync_dirty_buffer(bh_bis);
			brelse(bh_bis);
		}

		mark_buffer_dirty(bh);
		sync_dirty_buffer(bh);
		brelse(bh);

		copied_so_far += size;
		if(copied_so_far >= inode->i_size) {
			for(int z = j-1; z < inode->i_blocks-1; z++) {
				put_block(OUICHEFS_SB(sb), file_index->blocks[z]);
				file_index->blocks[z] = 0;
			}
			inode->i_blocks = j;
			mark_inode_dirty(inode);
		}
	}
	mark_buffer_dirty(bh_index);
	sync_dirty_buffer(bh_index);
	brelse(bh_index);
	return 0;
}

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