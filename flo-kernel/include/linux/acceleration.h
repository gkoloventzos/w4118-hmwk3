#ifndef _ACCELERATION_H
#define _ACCELERATION_H

/*
 * Set current device acceleration in the kernel.
 * The parameter <b>acceleration</b> is the pointer to the address
 * where the sensor data is stored in user space.  Follow system call
 * convention to return 0 on success and the appropriate error value
 * on failure.
 * syscall number 378
 */
int set_acceleration(struct dev_acceleration __user *acceleration);


/*
 * The guide on how to get an instance of the default acceleration
 * sensor is available ...
 */

/*
 * The data structure to be used for passing accelerometer data to the
 * kernel and storing the data in the kernel.
 */
struct dev_acceleration {
	int x; /* acceleration along X-axis */
	int y; /* acceleration along Y-axis */
	int z; /* acceleration along Z-axis */
};

#endif
