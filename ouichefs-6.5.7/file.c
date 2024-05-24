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


static ssize_t ouichefs_read(struct file *filep, char __user *buf, size_t len, loff_t *ppos)
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
	//uint32_t size = get_block_size(index->blocks[iblock]);
	size_t tmp = inode->i_size - *ppos;
	bytes_to_read = min((size_t) OUICHEFS_BLOCK_SIZE, tmp);
	//bytes_to_read = (size_t) size;
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

	//pr_info("Total bytes read: %ld\n", bytes_read);
	return bytes_read;
}

static ssize_t ouichefs_write(struct file *filep, const char __user *buf, size_t len, loff_t *ppos)
{	
	//pr_info("Enter in ouichefs_write\n");
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

	if (filep->f_flags && O_APPEND) {
		*ppos = inode->i_size;
	}

	bh_index = sb_bread(sb, ci->index_block);
	if (!bh_index)
		return -EIO;
	index = (struct ouichefs_file_index_block *)bh_index->b_data;

	iblock = *ppos / OUICHEFS_BLOCK_SIZE;
	if (index->blocks[iblock] == 0) {
		bno = get_free_block(sbi);
		pr_info("if()Before BNO: %u\n", bno);
		bno = create_block_entry((uint32_t)bno, (uint32_t)0);
		pr_info("if()After BNO: %u\n", bno);
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
	// here something can go wrong
	struct buffer_head *bh = sb_bread(sb, get_block_number(bno));
	if (!bh) {
		brelse(bh_index);
		return -EIO;
	}
	

	offset = *ppos % OUICHEFS_BLOCK_SIZE;
	remaining = OUICHEFS_BLOCK_SIZE - offset;
	bytes_to_write = min(len, remaining);

	//check if we have enough blocks ==> number_of_blocks_needed
	size_t number_of_blocks_needed = ((len + offset) / OUICHEFS_BLOCK_SIZE);
	if (number_of_blocks_needed + inode->i_blocks > (OUICHEFS_BLOCK_SIZE >> 2) - 1) {
		brelse(bh);
		brelse(bh_index);
		return -ENOSPC;
	}

	pr_info("number_of_blocks_needed: %ld\n", number_of_blocks_needed); // 1
	int cmpt = 0;
	int position_to_copy = -1;
	for (size_t i = offset ; i < OUICHEFS_BLOCK_SIZE; i++) {
		pr_info("b_data[i] %d \n", bh->b_data[i]);
		if (bh->b_data[i] != 0){
			
			pr_info("here\n");
			if (position_to_copy == -1){
				position_to_copy = i;
			}
			cmpt++;
		}else{
			if (position_to_copy != -1){
				break;
			}
		}
	} 
	pr_info("cmpt: %d\n", cmpt); // 5
	pr_info("position_to_copy: %d\n", position_to_copy); // 3
	if (position_to_copy != -1){
		number_of_blocks_needed++; // only do this if position_to_copy
		for (int j = inode->i_blocks - 2; j > iblock; j--) { // -ici
			pr_info("xx\n");
			pr_info("j: %d and iblock: %llu\n", j, iblock);
			index->blocks[j + number_of_blocks_needed] = index->blocks[j];
			for(int k=0; k < 100000000000; k++) {}	
		}
		
		for (int i = iblock + number_of_blocks_needed; i > iblock; i--){
			bno = get_free_block(sbi);
			if (!bno) {
				brelse(bh);
				brelse(bh_index);
				return -ENOSPC;
			}
			bno = create_block_entry((uint32_t)bno, (uint32_t)0);
			pr_info("i = %d\n", i);
			index->blocks[i] = bno;
		}

		int last_inserted_block = index->blocks[iblock + number_of_blocks_needed];
		struct buffer_head *tmpbh = sb_bread(sb, get_block_number(last_inserted_block));
		if (!bh) {
			brelse(bh_index);
			return -EIO;
		}
		
		char* ptr_to_copy_from = bh->b_data + position_to_copy;
		
		pr_info("pointer to ci = %s\n", ptr_to_copy_from);// degf
		memcpy(tmpbh->b_data, ptr_to_copy_from, cmpt);
		mark_buffer_dirty(tmpbh);
		sync_dirty_buffer(tmpbh);
		pr_info("data block 2: %d", tmpbh->b_data[0]);
		pr_info("%d", tmpbh->b_data[1]);
		pr_info("%d", tmpbh->b_data[2]);
		pr_info("%d \n", tmpbh->b_data[3]);
		uint32_t block_number = get_block_number(last_inserted_block);
		uint32_t block_size = get_block_size(last_inserted_block);
		block_size = block_size + (uint32_t)cmpt;
		pr_info("block size %d\n", block_size);
		index->blocks[iblock + number_of_blocks_needed] = create_block_entry(block_number, block_size);
		memset(ptr_to_copy_from, 0, cmpt);
		//mark_buffer_dirty(bh);
		//sync_dirty_buffer(bh);
		pr_info("data block 1 at offset 3: %d\n", bh->b_data[3]);
		block_number = get_block_number(index->blocks[iblock]);
		block_size = get_block_size(index->blocks[iblock]);
		block_size = block_size - (uint32_t)cmpt;
		index->blocks[iblock] = create_block_entry(block_number, block_size);
	}
	
	bytes_not_write = copy_from_user(bh->b_data + offset, buf, bytes_to_write);
	if (bytes_not_write) {
		brelse(bh);
		brelse(bh_index);
		return -EFAULT;
	}
	mark_buffer_dirty(bh);
	sync_dirty_buffer(bh);
	pr_info("bytes_not_write: %ld\n", bytes_not_write);
	bytes_write = bytes_to_write - bytes_not_write;
	*ppos += bytes_write;

	brelse(bh);

	uint32_t block_number = get_block_number(bno);
	uint32_t block_size = get_block_size(bno);
	block_size = (block_size + (uint32_t)bytes_write);
	pr_info("BLOCK NUMBER: %u BLOCK SIZE: %u\n", block_number, block_size);
	index->blocks[iblock] = create_block_entry(block_number, block_size);

	if (*ppos > inode->i_size){
		inode->i_size = *ppos;
	}

	uint32_t nr_blocks_old = inode->i_blocks;

	inode->i_blocks = inode->i_size / OUICHEFS_BLOCK_SIZE + 2;
	pr_info("bytes_not_write: ==> inode->i_blocks is %lld\n", inode->i_blocks);
	inode->i_mtime = inode->i_ctime = current_time(inode);
	mark_inode_dirty(inode);

	if (nr_blocks_old > inode->i_blocks) {
		pr_info("in case we come here!\n");
		for (int i = inode->i_blocks - 1; i < nr_blocks_old - 1; i++) {
			put_block(OUICHEFS_SB(sb), index->blocks[i]); // maybe here get_block_number
			index->blocks[i] = 0;
		}
	}
	mark_buffer_dirty(bh_index);
	sync_dirty_buffer(bh_index);
	brelse(bh_index);
	pr_info("new writes implem STEP 78\n");

	//pr_info("Total bytes write: %ld\n", bytes_write);
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

const struct file_operations ouichefs_file_ops = {
	.owner = THIS_MODULE,
	.open = ouichefs_open,
	.read = ouichefs_read,	
	.write = ouichefs_write,
	.llseek = generic_file_llseek,
	.read_iter = generic_file_read_iter,
	.read = ouichefs_read,
	.write = ouichefs_write,
	.write_iter = generic_file_write_iter,
	.unlocked_ioctl = ouichefs_ioctl,
};