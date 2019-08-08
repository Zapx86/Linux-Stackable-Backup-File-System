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

#include "bkpfs.h"

/*
 * The inode cache is used with alloc_inode for both our inode info and the
 * vfs inode.
 */
static struct kmem_cache *bkpfs_inode_cachep;

/* final actions when unmounting a file system */
static void bkpfs_put_super(struct super_block *sb)
{
	struct bkpfs_sb_info *spd;
	struct super_block *s;

	spd = BKPFS_SB(sb);
	if (!spd)
		return;

	/* decrement lower super references */
	s = bkpfs_lower_super(sb);
	bkpfs_set_lower_super(sb, NULL);
	atomic_dec(&s->s_active);

	kfree(spd);
	sb->s_fs_info = NULL;
}

static int bkpfs_statfs(struct dentry *dentry, struct kstatfs *buf)
{
	int err;
	struct path lower_path;

	bkpfs_get_lower_path(dentry, &lower_path);
	err = vfs_statfs(&lower_path, buf);
	bkpfs_put_lower_path(dentry, &lower_path);

	/* set return buf to our f/s to avoid confusing user-level utils */
	buf->f_type = BKPFS_SUPER_MAGIC;

	return err;
}

/*
 * @flags: numeric mount options
 * @options: mount options string
 */
static int bkpfs_remount_fs(struct super_block *sb, int *flags, char *options)
{
	int err = 0;

	/*
	 * The VFS will take care of "ro" and "rw" flags among others.  We
	 * can safely accept a few flags (RDONLY, MANDLOCK), and honor
	 * SILENT, but anything else left over is an error.
	 */
	if ((*flags & ~(MS_RDONLY | MS_MANDLOCK | MS_SILENT)) != 0) {
		printk(KERN_ERR
		       "bkpfs: remount flags 0x%x unsupported\n", *flags);
		err = -EINVAL;
	}

	return err;
}

/*
 * Called by iput() when the inode reference count reached zero
 * and the inode is not hashed anywhere.  Used to clear anything
 * that needs to be, before the inode is completely destroyed and put
 * on the inode free list.
 */
static void bkpfs_evict_inode(struct inode *inode)
{
	struct inode *lower_inode;

	truncate_inode_pages(&inode->i_data, 0);
	clear_inode(inode);
	/*
	 * Decrement a reference to a lower_inode, which was incremented
	 * by our read_inode when it was created initially.
	 */
	lower_inode = bkpfs_lower_inode(inode);
	bkpfs_set_lower_inode(inode, NULL);
	iput(lower_inode);
}

static struct inode *bkpfs_alloc_inode(struct super_block *sb)
{
	struct bkpfs_inode_info *i;

	i = kmem_cache_alloc(bkpfs_inode_cachep, GFP_KERNEL);
	if (!i)
		return NULL;

	/* memset everything up to the inode to 0 */
	memset(i, 0, offsetof(struct bkpfs_inode_info, vfs_inode));

        atomic64_set(&i->vfs_inode.i_version, 1);
	return &i->vfs_inode;
}

static void bkpfs_destroy_inode(struct inode *inode)
{
	kmem_cache_free(bkpfs_inode_cachep, BKPFS_I(inode));
}

/* bkpfs inode cache constructor */
static void init_once(void *obj)
{
	struct bkpfs_inode_info *i = obj;
	
	inode_init_once(&i->vfs_inode);
}

int bkpfs_init_inode_cache(void)
{
	int err = 0;

	bkpfs_inode_cachep =
		kmem_cache_create("bkpfs_inode_cache",
				  sizeof(struct bkpfs_inode_info), 0,
				  SLAB_RECLAIM_ACCOUNT, init_once);
	if (!bkpfs_inode_cachep)
		err = -ENOMEM;
	return err;
}

/* bkpfs inode cache destructor */
void bkpfs_destroy_inode_cache(void)
{
	if (bkpfs_inode_cachep)
		kmem_cache_destroy(bkpfs_inode_cachep);
}

/*
 * Used only in nfs, to kill any pending RPC tasks, so that subsequent
 * code can actually succeed and won't leave tasks that need handling.
 */
static void bkpfs_umount_begin(struct super_block *sb)
{
	struct super_block *lower_sb;

	lower_sb = bkpfs_lower_super(sb);
	if (lower_sb && lower_sb->s_op && lower_sb->s_op->umount_begin)
		lower_sb->s_op->umount_begin(lower_sb);
}

const struct super_operations bkpfs_sops = {
	.put_super	= bkpfs_put_super,
	.statfs		= bkpfs_statfs,
	.remount_fs	= bkpfs_remount_fs,
	.evict_inode	= bkpfs_evict_inode,
	.umount_begin	= bkpfs_umount_begin,
	.alloc_inode	= bkpfs_alloc_inode,
	.destroy_inode	= bkpfs_destroy_inode,
	.drop_inode	= generic_delete_inode,
};

/* NFS support */

static struct inode *bkpfs_nfs_get_inode(struct super_block *sb, u64 ino,
					  u32 generation)
{
	struct super_block *lower_sb;
	struct inode *inode;
	struct inode *lower_inode;

	lower_sb = bkpfs_lower_super(sb);
	lower_inode = ilookup(lower_sb, ino);
	inode = bkpfs_iget(sb, lower_inode);
	return inode;
}

static struct dentry *bkpfs_fh_to_dentry(struct super_block *sb,
					  struct fid *fid, int fh_len,
					  int fh_type)
{
	return generic_fh_to_dentry(sb, fid, fh_len, fh_type,
				    bkpfs_nfs_get_inode);
}

static struct dentry *bkpfs_fh_to_parent(struct super_block *sb,
					  struct fid *fid, int fh_len,
					  int fh_type)
{
	return generic_fh_to_parent(sb, fid, fh_len, fh_type,
				    bkpfs_nfs_get_inode);
}

/*
 * all other funcs are default as defined in exportfs/expfs.c
 */

const struct export_operations bkpfs_export_ops = {
	.fh_to_dentry	   = bkpfs_fh_to_dentry,
	.fh_to_parent	   = bkpfs_fh_to_parent
};
