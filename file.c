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
#include </usr/src/hw2-sjeevan/include/linux/custom_ioctl.h>

bool BACKUP_FLAG = false;

/**
 * bkp_getxattr - gets the matching attribute value
 * @lower_dentry: lower dentry of the file
 * @name: attribute name
 * @buffer: the buffer to place matching value
 * @size: size of buffer
 */
static int
bkp_getxattr(struct dentry *lower_dentry, const char *name,
void *buffer, size_t size)
{
        int err;
        if (!(d_inode(lower_dentry)->i_opflags & IOP_XATTR)) {
                err = -EOPNOTSUPP;
                goto out;
        } 
	err = vfs_getxattr(lower_dentry, name, buffer, size);
out:
        return err;
}

/**
 * bkp_setxattr - sets the attributes needed
 * @lower_dentry: lower dentry of the file
 * @size: size of buffer
 * @init_flag: indicates if initialisation
 */
static int
bkp_setxattr(struct dentry *lower_dentry, size_t size,
int flags, const bool init_flag)
{
        int err;
	int min_version = 1, max_version = 4;
	int cur_version = 0, num_version = 0; 
 	
	if (!(d_inode(lower_dentry)->i_opflags & IOP_XATTR)) {
                err = -EOPNOTSUPP;
                goto out;
	}
	if (init_flag) {
		err = vfs_setxattr(lower_dentry, "user.max_version", &max_version, size, flags);
		if (err)
			goto out;
		err = vfs_setxattr(lower_dentry, "user.min_version", &min_version, size, flags);
		if (err)
			goto out;
		err = vfs_setxattr(lower_dentry, "user.cur_version", &cur_version, size, flags);
		if (err)
			goto out;
		err = vfs_setxattr(lower_dentry, "user.num_version", &num_version, size, flags);
		if (err)
			goto out;
	}
out:	
	return err;
}

/**
 * init_qstr - initialises struct qstr (needed to create dentry)
 * @filename: name of the file
 * @lower_dir_dentry: dentry of directory of lower FS
 */
static struct 
qstr init_qstr(const char *filename,
struct dentry *lower_dir_dentry)
{
	struct qstr new_qstr;
	
	new_qstr.name = filename;
        new_qstr.len  = strlen(filename);
        new_qstr.hash = full_name_hash(lower_dir_dentry, new_qstr.name, new_qstr.len);

	return new_qstr;
}	

/**
 * create_dentry - creates a new dentry 
 * @lower_parent_path: lower path of its parent
 * @this: initialised quick string
 * @lower_path: lower path of the file
 */
static struct dentry
*create_dentry(struct path lower_parent_path, struct qstr *this,
struct path *lower_path)
{
	struct dentry *lower_dentry;
	struct dentry *lower_dir_dentry = lower_parent_path.dentry;
	struct vfsmount *lower_dir_mnt = lower_parent_path.mnt;
	
	/* create a new negative dentry for the lower file system */
	lower_dentry = d_alloc(lower_dir_dentry, this);
	d_add(lower_dentry, NULL);

	/* initialise the lower sturct lower path of bkpfs dentry */
	lower_path->dentry = lower_dentry;
        lower_path->mnt = mntget(lower_dir_mnt);
     
	return lower_dentry; 
}

/**
 * create_inode - creates a new inode for given dentry 
 * @lower_parent_dentry: lower dentry of its parent
 * @lower_dentry: the file's lower dentry
 * @mode: the file permissions
 */
static int 
create_inode(struct dentry *lower_dentry,
struct dentry *lower_parent_dentry,
umode_t mode, bool want_excl)
{
	int err = 0;
	
	lower_parent_dentry = lock_parent(lower_dentry);
        err = vfs_create(d_inode(lower_parent_dentry), lower_dentry, mode, want_excl);		
        unlock_dir(lower_parent_dentry);
	
	return err;
}

static int bkpfs_restore(struct file *file, int version_num)
{
	int err = 0, get_xattr, vfs_err = 0;
	struct path lower_backup_path;
	struct dentry *parent;
	struct file *lower_bkp_file, *main_file;
	char bkp_fname[256], *main_fname;
	loff_t pos_in = 0, pos_out = 0;
	int min_buffer = 0, cur_buffer = 0;
	parent = dget_parent((bkpfs_lower_file(file))->f_path.dentry);
	
	if (version_num == -2) {
		get_xattr = bkp_getxattr(bkpfs_lower_file(file)->f_path.dentry, "user.min_version",
					(void *) &min_buffer, sizeof (int));
		version_num = min_buffer;
	} else if (version_num == -1) {
		get_xattr = bkp_getxattr(bkpfs_lower_file(file)->f_path.dentry, "user.cur_version",
				(void *) &cur_buffer, sizeof (int));
		version_num = cur_buffer;
	}
	if (version_num < 1) {
		err = -ENOENT;
		goto out;
	}
	main_fname = (char *) file->f_path.dentry->d_name.name;
	sprintf(bkp_fname, ".backup.%s.%d", main_fname, version_num);
	err = vfs_path_lookup(parent, bkpfs_lower_file(file)->f_path.mnt,
				bkp_fname, 0, &lower_backup_path);
	if (err) {
		goto out;
	}
	
	lower_bkp_file = dentry_open(&lower_backup_path,
					O_RDWR, current_cred());	
	if (IS_ERR(lower_bkp_file)) {
                err = PTR_ERR(lower_bkp_file);
		goto out;
	}
	
	main_file = dentry_open(&(bkpfs_lower_file(file)->f_path), O_WRONLY, current_cred());
	err = vfs_truncate(&main_file->f_path, 0);
	if (err) {
		goto out;
	}
	else {	
		vfs_err = vfs_copy_file_range(
			lower_bkp_file,
			pos_in,
			main_file,
			pos_out, 
			lower_bkp_file->f_inode->i_size, 0
		);
		if (vfs_err < lower_bkp_file->f_inode->i_size) {
			err = -EIO;
			goto out;
		}
	}
out:	
	//fput(lower_bkp_file);
	//fput(main_file);
	dput(parent);
	return err;
}

static int 
bkp_unlink(struct file *file, struct dentry *lower_dir,
struct dentry *lower_del_dentry, int version_num)
{
	int i, err, get_xattr;
	struct dentry *lower_dir_dentry, *parent, *lower_dentry;
	char *max_version = "user.max_version", *min_version = "user.min_version";
	char *num_version = "user.num_version", *cur_version = "user.cur_version";
	int max_buffer = 0, min_buffer = 0, cur_buffer = 0, num_buffer = 0;
	struct vfsmount *lower_dir_mnt;
	struct path lower_parent_path, del_lower_path;		
	char delete_filename[256];
	struct file *lower_file;	
	
	parent = dget_parent(file->f_path.dentry);
	bkpfs_get_lower_path(parent, &lower_parent_path);
	lower_dir = lower_parent_path.dentry;
	lower_dir_mnt = lower_parent_path.mnt;
	
	dget(lower_del_dentry);
	lower_dir_dentry = lock_parent(lower_del_dentry);
	err = vfs_unlink(lower_dir->d_inode, lower_del_dentry, NULL);
	if (err == -EBUSY && lower_del_dentry->d_flags & DCACHE_NFSFS_RENAMED) 
		err = 0;
	unlock_dir(lower_dir_dentry);
	dput(lower_del_dentry);
	if (err)
		goto out;
	
	lower_file = bkpfs_lower_file(file);
	lower_dentry = lower_file->f_path.dentry;
	get_xattr = bkp_getxattr(lower_dentry, max_version, (void *) &max_buffer, sizeof (int));
	if ((get_xattr < 0) || (get_xattr == -ENODATA)) {
		err = get_xattr;
		goto out;
	}
	get_xattr = bkp_getxattr(lower_dentry, min_version, (void *) &min_buffer, sizeof (int));
	if ((get_xattr < 0) || (get_xattr == -ENODATA)) {
		err = get_xattr;
		goto out;
	}
	get_xattr = bkp_getxattr(lower_dentry, cur_version, (void *) &cur_buffer, sizeof (int));
	if ((get_xattr < 0) || (get_xattr == -ENODATA)) {
		err = get_xattr;
		goto out;
	}
	get_xattr = bkp_getxattr(lower_dentry, num_version, (void *) &num_buffer, sizeof (int));
	if ((get_xattr < 0) || (get_xattr == -ENODATA)) {
		err = get_xattr;
		goto out;
	}
	if (version_num == min_buffer) {
		if (cur_buffer == min_buffer) {
			num_buffer = 0;
			min_buffer = 1;
			cur_buffer = 0;
			max_buffer = 4;
		} else {
			for (i = min_buffer + 1; i <= cur_buffer; i++) {
				sprintf(delete_filename, ".backup.%s.%d", file->f_path.dentry->d_name.name, i);
			        err = vfs_path_lookup(lower_dir_dentry, lower_dir_mnt, delete_filename, 0, &del_lower_path);
				if (!err) {		
					min_buffer = i;
					num_buffer = num_buffer - 1;
					break;
				}  
			}
		}		
	} else if (version_num == max_buffer) {
		if (num_buffer == 1) {
			cur_buffer = 0;
			num_buffer = 0;
			min_buffer = 1;
			max_buffer = 4;
		} else {
			num_buffer = num_buffer -1;
			for (i = cur_buffer -1; i >= min_buffer; i--) {
				sprintf(delete_filename, ".backup.%s.%d", file->f_path.dentry->d_name.name, i);
				err = vfs_path_lookup(lower_dir_dentry, lower_dir_mnt, delete_filename, 0, &del_lower_path);
				if (!err) {		
					cur_buffer = i;
					max_buffer = i;
					break;
				}  
			}
		}
	} else 
		num_buffer = num_buffer - 1; 
	
	err = vfs_setxattr(lower_dentry, "user.max_version", (const void *) &max_buffer, sizeof (int), XATTR_REPLACE);
	if (err)
		goto out;
	err = vfs_setxattr(lower_dentry, "user.min_version", (const void *) &min_buffer, sizeof (int), XATTR_REPLACE);
	if (err)
		goto out;
	err = vfs_setxattr(lower_dentry, "user.cur_version", (const void *) &cur_buffer, sizeof (int), XATTR_REPLACE);
	if (err)
		goto out;
	err = vfs_setxattr(lower_dentry, "user.num_version", (const void *) &num_buffer, sizeof (int), XATTR_REPLACE);
	if (err)
		goto out;
	printk("After Unlink - Min: %d, Max: %d, Cur: %d, Num: %d\n", min_buffer, max_buffer, cur_buffer, num_buffer);
out:
	dput(parent);
	bkpfs_put_lower_path(parent, &lower_parent_path);
	
	return err;
}

static struct file *bkpfs_backup(struct file *file)
{	
	int i, err = 0, get_xattr;
	int max_buffer = 0, min_buffer = 0, cur_buffer = 0, num_buffer = 0;
	struct dentry *lower_dir_dentry = NULL;
	struct qstr new_qstr;
	struct dentry *lower_dentry, *orig_lowerdentry, *parent, *lower_parent_dentry = NULL;
	struct path lower_path, lower_parent_path, del_lower_path;
	struct vfsmount *lower_dir_mnt;	
	struct file *lower_file = NULL, *orig_lowerfile = NULL;	
	char *max_version = "user.max_version", *min_version = "user.min_version";
	char *num_version = "user.num_version", *cur_version = "user.cur_version";
	char current_filename[30], delete_filename[30]; 	
	bool num_flag = false;
	int delete_version;

	/* get the parents dentry */
	parent = dget_parent(file->f_path.dentry);

        /* 
 	 * gets the lower fs-specific parent struct path!
 	 * The series of function calls made are below:
 	 * spin_lock(&BKPFS_D(dent)->lock): grab the lock of bkpfs dentry.	 
 	 * pathcpy(lower_path, &BKPFS_D(dent)->lower_path): copy struct path of bkpfs dentry.
 	 * path_get(lower_path): increase reference count of mount, and lower fs specific dentry.
 	 * spin_unlock(&BKPFS_D(dent)->lock): release the lock on bkpfs dentry.
 	 */
	
	/* The lower path of bkpfs dentry has a link to lower fs dentry.
 	 * struct path {
 	 * 	struct vfsmount *mnt;
 	 * 	struct dentry *dentry; // lower fs dentry
 	 * } __randomize_layout;
 	 */
	bkpfs_get_lower_path(parent, &lower_parent_path);
	lower_dir_dentry = lower_parent_path.dentry;
	lower_dir_mnt = lower_parent_path.mnt;
	
	orig_lowerfile = bkpfs_lower_file(file);
	orig_lowerdentry = orig_lowerfile->f_path.dentry;
	
	get_xattr = bkp_getxattr(orig_lowerdentry, max_version, (void *) &max_buffer, sizeof (int));
	
	if ((get_xattr < 0) || (get_xattr == -ENODATA)) 
		bkp_setxattr(orig_lowerdentry, sizeof (int), XATTR_CREATE, true);
	get_xattr = bkp_getxattr(orig_lowerdentry, num_version, (void *) &num_buffer, sizeof (int));
	if ((get_xattr < 0) || (get_xattr == -ENODATA)) {
		err = get_xattr;
		goto out;
	}
	get_xattr = bkp_getxattr(orig_lowerdentry, cur_version, (void *) &cur_buffer, sizeof (int));
	if ((get_xattr < 0) || (get_xattr == -ENODATA)) {
		err = get_xattr;
		goto out;
	}

	get_xattr = bkp_getxattr(orig_lowerdentry, min_version, (void *) &min_buffer, sizeof (int));
	if ((get_xattr < 0) || (get_xattr == -ENODATA)) {
		err = get_xattr;
		goto out;
	}
	if (num_buffer == 4) {
		
		num_flag = true;
		get_xattr = bkp_getxattr(orig_lowerdentry, max_version, (void *) &max_buffer, sizeof (int));
		if ((get_xattr < 0) || (get_xattr == -ENODATA)) {
			err = get_xattr;
			goto out;
		}
		get_xattr = bkp_getxattr(orig_lowerdentry, min_version, (void *) &min_buffer, sizeof (int));
		if ((get_xattr < 0) || (get_xattr == -ENODATA)) {
			err = get_xattr;
			goto out;
		}
		max_buffer = max_buffer + 1; 
		delete_version = min_buffer;
		
		for (i = min_buffer + 1; i < max_buffer; i++) {
			sprintf(delete_filename, "backup.%s.%d", file->f_path.dentry->d_name.name, i);			
			err = vfs_path_lookup(lower_dir_dentry, lower_dir_mnt, delete_filename, 0, &del_lower_path);
			if (!err) {
				min_buffer = i;
				err = vfs_setxattr(orig_lowerdentry, "user.min_version", (const void *) &min_buffer, sizeof (int), XATTR_REPLACE);
				if (err)
					goto out;
				break;
			}  
		}
		
		num_buffer = num_buffer - 1;
		err = vfs_setxattr(orig_lowerdentry, "user.num_version", (const void *) &num_buffer, sizeof (int), XATTR_REPLACE);
		if (err)
			goto out;
		err = vfs_setxattr(orig_lowerdentry, "user.max_version", (const void *) &max_buffer, sizeof (int), XATTR_REPLACE);
		if (err)
			goto out;
		sprintf(delete_filename, ".backup.%s.%d", file->f_path.dentry->d_name.name, (delete_version));
		err = vfs_path_lookup(lower_dir_dentry, lower_dir_mnt, delete_filename, 0, &del_lower_path);
		
		if (!err) {
			bkp_unlink(file, lower_dir_dentry, del_lower_path.dentry, delete_version);
		} else 
			goto out;
	}
	
	cur_buffer = cur_buffer + 1;
	sprintf(current_filename, ".backup.%s.%d", file->f_path.dentry->d_name.name, cur_buffer);
	err = vfs_setxattr(orig_lowerdentry, "user.cur_version", (const void *) &cur_buffer, sizeof (int), XATTR_REPLACE);
	if (err)
		goto out;
	
	/* initialize the quick string structure */
  	new_qstr = init_qstr(current_filename, lower_dir_dentry);	
	
	/* decrease reference count of parent */
	
	lower_dentry = create_dentry(lower_parent_path, &new_qstr, &lower_path);	
	err = create_inode(lower_dentry, lower_parent_dentry, 33188, true);
	if (err) {
		goto out;
	}
	
	num_buffer = num_buffer + 1;
	err = vfs_setxattr(orig_lowerdentry, "user.num_version", (const void *) &num_buffer, sizeof (int), XATTR_REPLACE);
	if (err)
		goto out;
	
	printk("After Backup - Min: %d, Max: %d, Cur: %d, Num: %d\n", min_buffer, max_buffer, cur_buffer, num_buffer);
	/* open file for writing */
	lower_file = dentry_open(&lower_path, file->f_flags, current_cred());
	if (IS_ERR(lower_file)) {
		err = PTR_ERR(lower_file);
		goto out;
	}
out:
	dput(parent);
	bkpfs_put_lower_path(parent, &lower_parent_path);

	return lower_file;
}

static ssize_t bkpfs_read(struct file *file, char __user *buf,
			   size_t count, loff_t *ppos)
{
	int err;
	struct file *lower_file;
	struct dentry *dentry = file->f_path.dentry;

	
	lower_file = bkpfs_lower_file(file);
	err = vfs_read(lower_file, buf, count, ppos);
	/* update our inode atime upon a successful lower read */
	if (err >= 0)
		fsstack_copy_attr_atime(d_inode(dentry),
					file_inode(lower_file));

	return err;
}

static ssize_t bkpfs_write(struct file *file, const char __user *buf,
			    size_t count, loff_t *ppos)
{
	int err;
	struct file *lower_file;
	struct dentry *dentry = file->f_path.dentry;

	lower_file = bkpfs_lower_file(file);
	err = vfs_write(lower_file, buf, count, ppos);
	
	/* update our inode times+sizes upon a successful lower write */
	if (err >= 0) {
		BACKUP_FLAG = true;
		fsstack_copy_inode_size(d_inode(dentry),
					file_inode(lower_file));
		fsstack_copy_attr_times(d_inode(dentry),
					file_inode(lower_file));
	}

	return err;
}

struct bkpfs_getdents_callback {
        struct dir_context ctx;
        struct dir_context *caller;
        struct super_block *sb;
        int filldir_called;
        int entries_written;
};

/* Inspired by generic filldir in fs/readdir.c */
static int
bkpfs_filldir(struct dir_context *ctx, const char *lower_name,
	int lower_namelen, loff_t offset, u64 ino, unsigned int d_type)
{
        struct bkpfs_getdents_callback *buf =
                container_of(ctx, struct bkpfs_getdents_callback, ctx);
        char *name;
        int rc;

	name = (char *) kmalloc(strlen(lower_name), GFP_KERNEL);
        
	buf->filldir_called++;
	strcpy(name, lower_name);
	rc = strncmp(lower_name, ".backup.", 8);
        
	if (rc != 0) {
        	buf->caller->pos = buf->ctx.pos;
        	rc = !dir_emit(buf->caller, name, lower_namelen, ino, d_type);
        	if (!rc)
                	buf->entries_written++;
	}
	kfree(name);
        return rc;
}

/*
 * bkpfs_readdir
 * @file: The bkpfs directory file
 * @ctx: The actor to feed the entries to
 */
static int bkpfs_readdir(struct file *file, struct dir_context *ctx)
{
        int err;
        struct file *lower_file = NULL;
        struct dentry *dentry = file->f_path.dentry;
        struct bkpfs_getdents_callback buf = {
                .ctx.actor = bkpfs_filldir,
                .caller = ctx,
                .sb = d_inode(dentry)->i_sb,
        };
	
        lower_file = bkpfs_lower_file(file);
        err = iterate_dir(lower_file, &buf.ctx);
        ctx->pos = buf.ctx.pos;
        if (err < 0)
                goto out;
        if (buf.filldir_called && !buf.entries_written)
                goto out;
        if (err >= 0)
                fsstack_copy_attr_atime(file->f_inode,
                                        file_inode(lower_file));
out:
        return err;
}

/**
 * bkpfs_list - list all versions of existing backups for a file
 * @file: struct file of the main file
 * @flag: list -N (newest), -O(oldest), -A (all)
 */
static int 
bkpfs_list(struct file *file,
const int flag, char *list_string)
{	
	struct dentry *lower_dentry;
	struct file *lower_file;
	char cur_filename[256 + 8], *filename;
	int i, get_xattr, cur_buffer = 0, min_buffer = 0, err = 0;
	char snum[100];

	struct dentry *lower_dir_dentry, *parent;
	struct path lower_path, lower_parent_path;
	struct vfsmount *lower_dir_mnt;

	lower_file = bkpfs_lower_file(file);
	lower_dentry = lower_file->f_path.dentry;

	filename = (char *) lower_dentry->d_name.name;
	if (flag == -1 || flag == 0) {
		get_xattr = bkp_getxattr(lower_dentry, "user.cur_version",
					(void *) &cur_buffer, sizeof (int));
		if ((get_xattr < 0) || (get_xattr == -ENODATA)) {
			err = get_xattr;
			goto out;
		}
		if (flag == -1)
			min_buffer = cur_buffer;
	} 
	if (flag == 1 || flag == 0) {
		get_xattr = bkp_getxattr(lower_dentry, "user.min_version",
					(void *) &min_buffer, sizeof (int));
		if ((get_xattr < 0) || (get_xattr == -ENODATA)) {
			err = get_xattr;
			goto out;
		}
		if (flag  == 1)
			cur_buffer = min_buffer;
	}
	parent = dget_parent(file->f_path.dentry);
	bkpfs_get_lower_path(parent, &lower_parent_path);
	lower_dir_dentry = lower_parent_path.dentry;
	lower_dir_mnt = lower_parent_path.mnt;
	if (min_buffer > cur_buffer) {
		err = -ENOENT;
		goto out;
		
	}
	for (i = min_buffer; i <= cur_buffer; i++) {
		sprintf(cur_filename, ".backup.%s.%d", lower_dentry->d_name.name, i);			
		err = vfs_path_lookup(lower_dir_dentry, lower_dir_mnt, cur_filename, 0, &lower_path);
		if (!err) {
			sprintf(snum,"%d", i);
			strcat(list_string, ":");
			strcat(list_string, snum);
		} else 
			goto out;
	}
	
out:
	dput(parent);
	bkpfs_put_lower_path(parent, &lower_parent_path);
	return err;
}

/**
 * bkpfs_view - view the content of a specific backup file
 * @file: struct file of main file
 * @rw_buffer: a malloced buffer used in vfs_read
 * @readsize: number of bytes to read from the file/iteration
 */
static int 
bkpfs_view(struct file *file, int operation_flag,
char *rw_buffer, unsigned int readsize)
{
	int ret = 0;
	mm_segment_t oldFs;
	char view_filename[256];
	struct vfsmount *lower_dir_mnt;
	struct dentry *lower_dir_dentry, *parent;
	struct path lower_path, lower_parent_path;
	struct file *lower_file;
	int get_xattr, min_buffer = 0, cur_buffer = 1;

	parent = dget_parent(file->f_path.dentry);
	bkpfs_get_lower_path(parent, &lower_parent_path);
	lower_dir_dentry = lower_parent_path.dentry;
	lower_dir_mnt = lower_parent_path.mnt;

	if (operation_flag == -2) {
		get_xattr = bkp_getxattr(bkpfs_lower_file(file)->f_path.dentry, "user.min_version",
					(void *) &min_buffer, sizeof (int));
		operation_flag = min_buffer;
	} else if (operation_flag == -1) {
		get_xattr = bkp_getxattr(bkpfs_lower_file(file)->f_path.dentry, "user.cur_version",
				(void *) &cur_buffer, sizeof (int));
		operation_flag = cur_buffer;
	}
	sprintf(view_filename, ".backup.%s.%d", file->f_path.dentry->d_name.name, operation_flag);
	if (operation_flag < 1) {
		ret = -ENOENT;
		goto out;
	}
	ret = vfs_path_lookup(lower_dir_dentry, lower_dir_mnt, view_filename, 0, &lower_path);	
	if (!ret) {
		lower_file = dentry_open(&lower_path, file->f_flags, current_cred());
		oldFs = get_fs();
		set_fs(KERNEL_DS);
		if(!vfs_read(lower_file, rw_buffer, readsize, &(file->f_pos)))
			ret = -EFAULT;
		set_fs(oldFs);
	}
out:
	return ret;
}
static int 
bkpfs_delete(struct file *file, const int flag)
{
	struct dentry *lower_dentry, *lower_dir_dentry, *parent;
        struct file *lower_file;
	struct path lower_path, lower_parent_path;
        char cur_filename[30];
        int err = 0, i, get_xattr, cur_buffer = 0, min_buffer = 0;
	struct vfsmount *lower_dir_mnt;

	
	lower_file = bkpfs_lower_file(file);
	lower_dentry = lower_file->f_path.dentry;
	
	if (flag == -1 || flag == 0) {
		
		get_xattr = bkp_getxattr(lower_dentry, "user.cur_version",
					(void *) &cur_buffer, sizeof (int));
		
		printk("Cur: %d\n", cur_buffer);
		if (flag == -1)
			min_buffer = cur_buffer;
		
		if ((get_xattr < 0) || (get_xattr == -ENODATA)) {
			err = get_xattr;
			goto out;
		}
		
	} 
	if (flag == -2 || flag == 0) {
		
		get_xattr = bkp_getxattr(lower_dentry, "user.min_version",
					(void *) &min_buffer, sizeof (int));
		
		if (flag == -2)
			cur_buffer = min_buffer;
		
		if ((get_xattr < 0) || (get_xattr == -ENODATA)) {
			err = get_xattr;
			goto out;
		}
		
	} 
	else if (flag > 0) {
		cur_buffer = flag;
		min_buffer = flag;
	}
	
	/* find the backup file */
	parent = dget_parent(file->f_path.dentry);
	bkpfs_get_lower_path(parent, &lower_parent_path);
	lower_dir_dentry = lower_parent_path.dentry;
	lower_dir_mnt = lower_parent_path.mnt;
	for (i = min_buffer; i <= cur_buffer; i++) {
		sprintf(cur_filename, ".backup.%s.%d", lower_dentry->d_name.name, i);			
		err = vfs_path_lookup(lower_dir_dentry, lower_dir_mnt, cur_filename, 0, &lower_path);
		if (!err) {
			bkp_unlink(file, lower_dentry->d_parent, lower_path.dentry, i);
		} else 
			goto out;

	}
	
out:
	dput(parent);
	bkpfs_put_lower_path(parent, &lower_parent_path);
	
	return err;
}

/**
 * check_operation - checks operation to perform (list, view, restore, delete)
 * @file: struct file of the main file
 * @operation: to indicate list, view, restore, or delete
 * @operation_flag: to indicate A (all), O (oldest), N (newest), number (nth)
 */
static long 
check_operation(struct file *file, unsigned int operation, void *user_args)
{
	int err = 0, readsize = 4096;
	char *rw_buffer = NULL;
	char list_string[256];
	int operation_flag;
	operationInfo *file_para;

	file_para = (operationInfo *) kmalloc(sizeof(operationInfo), GFP_KERNEL);			
	if (copy_from_user((void *) file_para, (void *) user_args, (sizeof(operationInfo)))) {
		err = -EFAULT;
		goto out;
	}
	operation_flag = file_para->operation_flag;
	printk("operation: %d\n", operation_flag);
	if (operation == LIST_VERSION) {
		if (bkpfs_list(file, operation_flag, list_string)) {
			err = -EINVAL;
			goto out;
		}
		if (copy_to_user((void *)((operationInfo *) user_args)->buffer, list_string, 256 * sizeof(char))) {	
			err = -EFAULT;
			goto out;
		}
	} else if (operation == VIEW_VERSION) {
		rw_buffer = (char *) kmalloc((sizeof(char) * readsize), GFP_KERNEL);
		readsize = file_para->readsize;
		if (readsize >= 4096)
			readsize = 4096;
		printk("readsize: %d\n", readsize);
		if (bkpfs_view(file, operation_flag, rw_buffer, readsize)) {	
			err = -EINVAL;
			goto view_out;
		}
		if (copy_to_user((void *)((operationInfo *) user_args)->buffer, rw_buffer, readsize)) {	
			err = -EFAULT;
			goto view_out;
		}
		printk("%s\n", rw_buffer);
	} else if (operation == DELETE_VERSION) {
		err = bkpfs_delete(file, operation_flag);
		if (err) 
			goto out;
	} else if (operation == RESTORE_VERSION) {
		err = bkpfs_restore(file, operation_flag);
		if (err)
			goto out;
	}
	else {
		err = -EINVAL;
		goto out;
	}
view_out:
	if (rw_buffer)
		kfree(rw_buffer);
out:	
	kfree(file_para);
	return err;
}

static long 
bkpfs_unlocked_ioctl(struct file *file,
unsigned int cmd, unsigned long arg)
{
	int err = 0;
	
	/* added (3 lines): pass the args to check_operation */
	err = check_operation(file, cmd, (void *) arg);
	return err;
}

#ifdef CONFIG_COMPAT
static long bkpfs_compat_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	long err = -ENOTTY;
	struct file *lower_file;

	
	lower_file = bkpfs_lower_file(file);

	/* XXX: use vfs_ioctl if/when VFS exports it */
	if (!lower_file || !lower_file->f_op)
		goto out;
	if (lower_file->f_op->compat_ioctl)
		err = lower_file->f_op->compat_ioctl(lower_file, cmd, arg);
out:
	return err;
}
#endif

static int bkpfs_mmap(struct file *file, struct vm_area_struct *vma)
{
	int err = 0;
	bool willwrite;
	struct file *lower_file;
	const struct vm_operations_struct *saved_vm_ops = NULL;

	
	/* this might be deferred to mmap's writepage */
	willwrite = ((vma->vm_flags | VM_SHARED | VM_WRITE) == vma->vm_flags);

	/*
	 * File systems which do not implement ->writepage may use
	 * generic_file_readonly_mmap as their ->mmap op.  If you call
	 * generic_file_readonly_mmap with VM_WRITE, you'd get an -EINVAL.
	 * But we cannot call the lower ->mmap op, so we can't tell that
	 * writeable mappings won't work.  Therefore, our only choice is to
	 * check if the lower file system supports the ->writepage, and if
	 * not, return EINVAL (the same error that
	 * generic_file_readonly_mmap returns in that case).
	 */
	lower_file = bkpfs_lower_file(file);
	if (willwrite && !lower_file->f_mapping->a_ops->writepage) {
		err = -EINVAL;
		printk(KERN_ERR "bkpfs: lower file system does not "
		       "support writeable mmap\n");
		goto out;
	}

	/*
	 * find and save lower vm_ops.
	 *
	 * XXX: the VFS should have a cleaner way of finding the lower vm_ops
	 */
	if (!BKPFS_F(file)->lower_vm_ops) {
		err = lower_file->f_op->mmap(lower_file, vma);
		if (err) {
			printk(KERN_ERR "bkpfs: lower mmap failed %d\n", err);
			goto out;
		}
		saved_vm_ops = vma->vm_ops; /* save: came from lower ->mmap */
	}

	/*
	 * Next 3 lines are all I need from generic_file_mmap.  I definitely
	 * don't want its test for ->readpage which returns -ENOEXEC.
	 */
	file_accessed(file);
	vma->vm_ops = &bkpfs_vm_ops;

	file->f_mapping->a_ops = &bkpfs_aops; /* set our aops */
	if (!BKPFS_F(file)->lower_vm_ops) /* save for our ->fault */
		BKPFS_F(file)->lower_vm_ops = saved_vm_ops;

out:
	return err;
}
static int bkpfs_open(struct inode *inode, struct file *file)
{
	int err = 0;
	struct file *lower_file = NULL;
	struct path lower_path;

	/* don't open unhashed/deleted files */
	if (d_unhashed(file->f_path.dentry)) {
		err = -ENOENT;
		goto out_err;
	}
		
	file->private_data =
		kzalloc(sizeof(struct bkpfs_file_info), GFP_KERNEL);
	if (!BKPFS_F(file)) {
		err = -ENOMEM;
		goto out_err;
	}
	
	/* open lower object and link bkpfs's file struct to lower's */
	bkpfs_get_lower_path(file->f_path.dentry, &lower_path);
	lower_file = dentry_open(&lower_path, file->f_flags, current_cred());
	
	path_put(&lower_path);
	if (IS_ERR(lower_file)) {
		err = PTR_ERR(lower_file);
		lower_file = bkpfs_lower_file(file);
		if (lower_file) {
			bkpfs_set_lower_file(file, NULL);
			fput(lower_file); /* fput calls dput for lower_dentry */
		}
	} else {
		bkpfs_set_lower_file(file, lower_file);
	}
	
	if (err)
		kfree(BKPFS_F(file));
	else
		fsstack_copy_attr_all(inode, bkpfs_lower_inode(inode));	
out_err:
	return err;
}

static int bkpfs_flush(struct file *file, fl_owner_t id)
{
	int err = 0;
	struct file *lower_file = NULL;

	
	lower_file = bkpfs_lower_file(file);
	if (lower_file && lower_file->f_op && lower_file->f_op->flush) {
		filemap_write_and_wait(file->f_mapping);
		err = lower_file->f_op->flush(lower_file, id);
	}

	return err;
}

/* release all lower object references & free the file info structure */
static int bkpfs_file_release(struct inode *inode, struct file *file)
{
	struct file *lower_file, *backup_file = NULL;
	int err = 0;
	fmode_t inFile_mode;
	loff_t pos_in = 0, pos_out = 0;

	lower_file = bkpfs_lower_file(file);

	if (BACKUP_FLAG == true) {
		BACKUP_FLAG = false;
		backup_file = bkpfs_backup(file);
		inFile_mode = lower_file->f_mode;
		lower_file->f_mode = FMODE_READ;
		err = vfs_copy_file_range(lower_file, pos_in, backup_file, pos_out, file->f_inode->i_size, 0);
		if (err < 0)
			goto out;
		lower_file->f_mode = inFile_mode;
		fput(backup_file);
	}
	if (lower_file) {
		bkpfs_set_lower_file(file, NULL);
		fput(lower_file);
	}
	kfree(BKPFS_F(file));
out:
	return 0;
}

static int bkpfs_fsync(struct file *file, loff_t start, loff_t end,
			int datasync)
{
	int err;
	struct file *lower_file;
	struct path lower_path;
	struct dentry *dentry = file->f_path.dentry;

	
	err = __generic_file_fsync(file, start, end, datasync);
	if (err)
		goto out;
	lower_file = bkpfs_lower_file(file);
	bkpfs_get_lower_path(dentry, &lower_path);
	err = vfs_fsync_range(lower_file, start, end, datasync);
	bkpfs_put_lower_path(dentry, &lower_path);
out:
	return err;
}

static int bkpfs_fasync(int fd, struct file *file, int flag)
{
	int err = 0;
	struct file *lower_file = NULL;

	
	lower_file = bkpfs_lower_file(file);
	if (lower_file->f_op && lower_file->f_op->fasync)
		err = lower_file->f_op->fasync(fd, lower_file, flag);

	return err;
}

/*
 * Bkpfs cannot use generic_file_llseek as ->llseek, because it would
 * only set the offset of the upper file.  So we have to implement our
 * own method to set both the upper and lower file offsets
 * consistently.
 */
static loff_t bkpfs_file_llseek(struct file *file, loff_t offset, int whence)
{
	int err;
	struct file *lower_file;

	
	err = generic_file_llseek(file, offset, whence);
	if (err < 0)
		goto out;

	lower_file = bkpfs_lower_file(file);
	err = generic_file_llseek(lower_file, offset, whence);

out:
	return err;
}

/*
 * Bkpfs read_iter, redirect modified iocb to lower read_iter
 */
ssize_t
bkpfs_read_iter(struct kiocb *iocb, struct iov_iter *iter)
{
	int err;
	struct file *file = iocb->ki_filp, *lower_file;

	
	lower_file = bkpfs_lower_file(file);
	if (!lower_file->f_op->read_iter) {
		err = -EINVAL;
		goto out;
	}

	get_file(lower_file); /* prevent lower_file from being released */
	iocb->ki_filp = lower_file;
	err = lower_file->f_op->read_iter(iocb, iter);
	iocb->ki_filp = file;
	fput(lower_file);
	/* update upper inode atime as needed */
	if (err >= 0 || err == -EIOCBQUEUED)
		fsstack_copy_attr_atime(d_inode(file->f_path.dentry),
					file_inode(lower_file));
out:
	return err;
}

/*
 * Bkpfs write_iter, redirect modified iocb to lower write_iter
 */
ssize_t
bkpfs_write_iter(struct kiocb *iocb, struct iov_iter *iter)
{
	int err;
	struct file *file = iocb->ki_filp, *lower_file;

	
	lower_file = bkpfs_lower_file(file);
	if (!lower_file->f_op->write_iter) {
		err = -EINVAL;
		goto out;
	}

	get_file(lower_file); /* prevent lower_file from being released */
	iocb->ki_filp = lower_file;
	err = lower_file->f_op->write_iter(iocb, iter);
	iocb->ki_filp = file;
	fput(lower_file);
	/* update upper inode times/sizes as needed */
	if (err >= 0 || err == -EIOCBQUEUED) {
		fsstack_copy_inode_size(d_inode(file->f_path.dentry),
					file_inode(lower_file));
		fsstack_copy_attr_times(d_inode(file->f_path.dentry),
					file_inode(lower_file));
	}
out:
	return err;
}

const struct file_operations bkpfs_main_fops = {
	.llseek		= generic_file_llseek,
	.read		= bkpfs_read,
	.write		= bkpfs_write,
	.unlocked_ioctl	= bkpfs_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= bkpfs_compat_ioctl,
#endif
	.mmap		= bkpfs_mmap,
	.open		= bkpfs_open,
	.flush		= bkpfs_flush,
	.release	= bkpfs_file_release,
	.fsync		= bkpfs_fsync,
	.fasync		= bkpfs_fasync,
	.read_iter	= bkpfs_read_iter,
	.write_iter	= bkpfs_write_iter,
};

/* trimmed directory options */
const struct file_operations bkpfs_dir_fops = {
	.llseek		= bkpfs_file_llseek,
	.read		= generic_read_dir,
	.iterate	= bkpfs_readdir,
	.unlocked_ioctl	= bkpfs_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= bkpfs_compat_ioctl,
#endif
	.open		= bkpfs_open,
	.release	= bkpfs_file_release,
	.flush		= bkpfs_flush,
	.fsync		= bkpfs_fsync,
	.fasync		= bkpfs_fasync,
};

