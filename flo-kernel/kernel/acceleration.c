/*
 * flo-kernel/kernel/acceleration.c
 * A system call that sets the
 * current device acceleration.
 *
 * Copyright (C) 2014 V. Atlidakis, G. Koloventzos, A. Papancea
 *
 * COMS W4118 implementation of syscall set_acceleration
 */
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <asm/uaccess.h>
#include <linux/acceleration.h>
#include <asm-generic/errno-base.h>


static DEFINE_SPINLOCK(acc_lock);
static struct dev_acceleration dev_acc;


/*
 * Set current device acceleration in the kernel.
 * The parameter acceleration is the pointer to the address
 * where the sensor data is stored in user space.  Follow system call
 * convention to return 0 on success and the appropriate error value
 * on failure.
 * syscall number 378
 */
int sys_set_acceleration(struct dev_acceleration __user *acceleration)
{
	int rval;
	struct dev_acceleration *dvacc;

	if (current_euid() != 0 && current_uid() != 0)
		return -EACCES;

	rval = copy_from_user(dvacc, acceleration,
					sizeof(struct dev_acceleration));
	if (rval < 0)
		return -EFAULT;

	spin_lock(&acc_lock);
	memcpy(&dev_acc, dvacc, sizeof(struct dev_acceleration));
	spin_unlock(&acc_lock);

	return 0;
}
