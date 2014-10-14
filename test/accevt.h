#ifndef _ACCELERATION_H
#define _ACCELERATION_H

struct acc_motion {
	unsigned int dlt_x; /* +/- around X-axis */
	unsigned int dlt_y; /* +/- around Y-axis */
	unsigned int dlt_z; /* +/- around Z-axis */
	unsigned int frq;
};

#endif
