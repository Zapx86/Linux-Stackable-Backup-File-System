/*
 * Copyright (c) 1998-2017 Erez Zadok
 * Copyright (c) 2009	   Shrikar Archak
 * Copyright (c) 2003-2017 Stony Brook University
 * Copyright (c) 2003-2017 The Research Foundation of SUNY
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _BKPFS_H_
#define _BKPFS_H_

#include <linux/dcache.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <linux/seq_file.h>
#include <linux/statfs.h>
#include <linux/fs_stack.h>
#include <linux/magic.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/xattr.h>
#include <linux/exportfs.h>

/* the file system name */
#define BKPFS_NAME "bkpfs"

/* bkpfs root inode number */
#define BKPFS_ROOT_INO     1

/* useful for tracking code reachability */
#define UDBG printk(KERN_DEFAULT "DBG:%s:%s:%d\n", __FILE__, __func__, __LINE__)

/* operations vectors defined in specific files */
extern const struct file_operations bkpfs_main_fops;
extern const struct file_operations bkpfs_dir_fops;
extern const struct inode_operations bkpfs_main_iops;
extern const struct inode_operations bkpfs_dir_iops;
extern const struct inode_operations bkpfs_symlink_iops;
extern const struct super_operations bkpfs_sops;
extern const struct dentry_operations bkpfs_dops;
extern const struct address_space_operations bkpfs_aops, bkpfs_dummy_aops;
extern const struct vm_operations_struct bkpfs_vm_ops;
extern const struct export_operations bkpfs_export_ops;
extern const struct xattr_handler *bkpfs_xattr_handlers[];

extern int bkpfs_init_inode_cache(void);
extern void bkpfs_destroy_inode_cache(void);
extern int bkpfs_init_dentry_cache(void);
extern void bkpfs_destroy_dentry_cache(void);
extern int new_dentry_private_data(struct dentry *dentry);
extern void free_dentry_private_data(struct dentry *dentry);
extern struct dentry *bkpfs_lookup(struct inode *dir, struct dentry *dentry,
				    unsigned int flags);
extern struct inode *bkpfs_iget(struct super_block *sb,
				 struct inode *lower_inode);
extern int bkpfs_interpose(struct dentry *dentry, struct super_block *sb,
			    struct path *lower_path);

/* file private data */
struct bkpfs_file_info {
	struct file *lower_file;
	const struct vm_operations_struct *lower_vm_ops;
};

/* bkpfs inode data in memory */
struct bkpfs_inode_info {
	struct inode *lower_inode;
	struct inode vfs_inode;
};

/* bkpfs dentry data in memory */
struct bkpfs_dentry_info {
	spinlock_t lock;	/* protects lower_path */
	struct path lower_path;
};

/* bkpfs super-block data in memory */
struct bkpfs_sb_info {
	struct super_block *lower_sb;
};

/*
 * inode to private data
 *
 * Since we use containers and the struct inode is _inside_ the
 * bkpfs_inode_info structure, BKPFS_I will always (given a non-NULL
 * inode pointer), return a valid non-NULL pointer.
 */
static inline struct bkpfs_inode_info *BKPFS_I(const struct inode *inode)
{	
	return container_of(inode, struct bkpfs_inode_info, vfs_inode);
}

/* dentry to private data */
#define BKPFS_D(dent) ((struct bkpfs_dentry_info *)(dent)->d_fsdata)

/* superblock to private data */
#define BKPFS_SB(super) ((struct bkpfs_sb_info *)(super)->s_fs_info)

/* file to private Data */
#define BKPFS_F(file) ((struct bkpfs_file_info *)((file)->private_data))

/* file to lower file */
static inline struct file *bkpfs_lower_file(const struct file *f)
{
	return BKPFS_F(f)->lower_file;
}

static inline void bkpfs_set_lower_file(struct file *f, struct file *val)
{	
	BKPFS_F(f)->lower_file = val;	
}

/* inode to lower inode. */
static inline struct inode *bkpfs_lower_inode(const struct inode *i)
{	
	return BKPFS_I(i)->lower_inode;
}

static inline void bkpfs_set_lower_inode(struct inode *i, struct inode *val)
{	
	BKPFS_I(i)->lower_inode = val;
}

/* superblock to lower superblock */
static inline struct super_block *bkpfs_lower_super(
	const struct super_block *sb)
{	
	return BKPFS_SB(sb)->lower_sb;
}

static inline void bkpfs_set_lower_super(struct super_block *sb,
					  struct super_block *val)
{	
	BKPFS_SB(sb)->lower_sb = val;
}

/* path based (dentry/mnt) macros */
static inline void pathcpy(struct path *dst, const struct path *src)
{	
	dst->dentry = src->dentry;
	dst->mnt = src->mnt;
}
/* Returns struct path.  Caller must path_put it. */
static inline void bkpfs_get_lower_path(const struct dentry *dent,
					 struct path *lower_path)
{	
	spin_lock(&BKPFS_D(dent)->lock);
	pathcpy(lower_path, &BKPFS_D(dent)->lower_path);
	path_get(lower_path);
	spin_unlock(&BKPFS_D(dent)->lock);
	return;
}
static inline void bkpfs_put_lower_path(const struct dentry *dent,
					 struct path *lower_path)
{	
	path_put(lower_path);
	return;
}
static inline void bkpfs_set_lower_path(const struct dentry *dent,
					 struct path *lower_path)
{	
	spin_lock(&BKPFS_D(dent)->lock);
	pathcpy(&BKPFS_D(dent)->lower_path, lower_path);
	spin_unlock(&BKPFS_D(dent)->lock);
	return;
}
static inline void bkpfs_reset_lower_path(const struct dentry *dent)
{
	
	spin_lock(&BKPFS_D(dent)->lock);
	BKPFS_D(dent)->lower_path.dentry = NULL;
	BKPFS_D(dent)->lower_path.mnt = NULL;
	spin_unlock(&BKPFS_D(dent)->lock);
	return;
}
static inline void bkpfs_put_reset_lower_path(const struct dentry *dent)
{
	struct path lower_path;
	
	spin_lock(&BKPFS_D(dent)->lock);
	pathcpy(&lower_path, &BKPFS_D(dent)->lower_path);
	BKPFS_D(dent)->lower_path.dentry = NULL;
	BKPFS_D(dent)->lower_path.mnt = NULL;
	spin_unlock(&BKPFS_D(dent)->lock);
	path_put(&lower_path);
	return;
}

/* locking helpers */
static inline struct dentry *lock_parent(struct dentry *dentry)
{
	struct dentry *dir = dget_parent(dentry);
		
	inode_lock_nested(d_inode(dir), I_MUTEX_PARENT);
	return dir;
}

static inline void unlock_dir(struct dentry *dir)
{
	inode_unlock(d_inode(dir));
	dput(dir);
}
#endif	/* not _BKPFS_H_ */
