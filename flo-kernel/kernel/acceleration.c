#include <linux/slab.h>
#include <linux/acceleration.h>

/*
 * Define time interval (ms)
 */

#define TIME_INTERVAL  200

/*
 * Set current device acceleration in the kernel.
 * The parameter acceleration is the pointer to the address
 * where the sensor data is stored in user space.  Follow system call
 * convention to return 0 on success and the appropriate error value
 * on failure.
 * syscall number 378
 */

int sys_set_acceleration(struct dev_acceleration __user * acceleration)
{
	return 0;
}
