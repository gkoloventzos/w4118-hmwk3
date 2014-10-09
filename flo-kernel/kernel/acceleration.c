#include <linux/slab.h>
#include <linux/syscalls.h>
#include <asm/uaccess.h>
#include <linux/acceleration.h>
#include <asm-generic/errno-base.h>

/*
 * Define time interval (ms)
 */
#define TIME_INTERVAL  200

struct dev_acceleration dev_acc;

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

	if (current_euid() != 0 && current_uid() != 0)
		return -EACCES;

	rval = copy_from_user(&dev_acc, acceleration,
					sizeof(struct dev_acceleration));
	if (rval < 0)
		return -EFAULT;

	return 0;
}
