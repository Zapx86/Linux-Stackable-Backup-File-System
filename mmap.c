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

static int bkpfs_fault(struct vm_fault *vmf)
{
	int err;
        struct vm_area_struct *vma = vmf->vma;
	struct file *file, *lower_file;
	const struct vm_operations_struct *lower_vm_ops;
	struct vm_area_struct lower_vma;

	memcpy(&lower_vma, vma, sizeof(struct vm_area_struct));
	file = lower_vma.vm_file;
	lower_vm_ops = BKPFS_F(file)->lower_vm_ops;
	BUG_ON(!lower_vm_ops);

	lower_file = bkpfs_lower_file(file);
	/*
	 * XXX: vm_ops->fault may be called in parallel.  Because we have to
	 * resort to temporarily changing the vma->vm_file to point to the
	 * lower file, a concurrent invocation of bkpfs_fault could see a
	 * different value.  In this workaround, we keep a different copy of
	 * the vma structure in our stack, so we never expose a different
	 * value of the vma->vm_file called to us, even temporarily.  A
	 * better fix would be to change the calling semantics of ->fault to
	 * take an explicit file pointer.
	 */
	lower_vma.vm_file = lower_file;
	vmf->vma = &lower_vma; /* override vma temporarily */
	err = lower_vm_ops->fault(vmf);
	vmf->vma = vma; /* restore vma*/
	return err;
}

static int bkpfs_page_mkwrite(struct vm_fault *vmf)
{
	int err = 0;
        struct vm_area_struct *vma = vmf->vma;
	struct file *file, *lower_file;
	const struct vm_operations_struct *lower_vm_ops;
	struct vm_area_struct lower_vma;

	memcpy(&lower_vma, vma, sizeof(struct vm_area_struct));
	file = lower_vma.vm_file;
	lower_vm_ops = BKPFS_F(file)->lower_vm_ops;
	BUG_ON(!lower_vm_ops);
	if (!lower_vm_ops->page_mkwrite)
		goto out;

	lower_file = bkpfs_lower_file(file);
	/*
	 * XXX: vm_ops->page_mkwrite may be called in parallel.
	 * Because we have to resort to temporarily changing the
	 * vma->vm_file to point to the lower file, a concurrent
	 * invocation of bkpfs_page_mkwrite could see a different
	 * value.  In this workaround, we keep a different copy of the
	 * vma structure in our stack, so we never expose a different
	 * value of the vma->vm_file called to us, even temporarily.
	 * A better fix would be to change the calling semantics of
	 * ->page_mkwrite to take an explicit file pointer.
	 */
	lower_vma.vm_file = lower_file;
	vmf->vma = &lower_vma; /* override vma temporarily */
	err = lower_vm_ops->page_mkwrite(vmf);
	vmf->vma = vma; /* restore vma */
out:
	return err;
}

static ssize_t bkpfs_direct_IO(struct kiocb *iocb, struct iov_iter *iter)
{
	/*
	 * This function should never be called directly.  We need it
	 * to exist, to get past a check in open_check_o_direct(),
	 * which is called from do_last().
	 */
	return -EINVAL;
}

const struct address_space_operations bkpfs_aops = {
	.direct_IO = bkpfs_direct_IO,
};

const struct vm_operations_struct bkpfs_vm_ops = {
	.fault		= bkpfs_fault,
	.page_mkwrite	= bkpfs_page_mkwrite,
};
