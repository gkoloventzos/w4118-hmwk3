#ifndef _ACCEVT_H
#define _ACCEVT_H

#include <linux/slab.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <linux/acceleration.h>

/*Define the noise*/
#define NOISE 10

/*Define the window*/
#define WINDOW 20

/*
 * Define the motion.
 * The motion give the baseline for an EVENT.
 */
struct acc_motion {

	unsigned int dlt_x; /* +/- around X-axis */
	unsigned int dlt_y; /* +/- around Y-axis */
	unsigned int dlt_z; /* +/- around Z-axis */

	unsigned int frq;   /* Number of samples that satisfies:
		sum_each_sample(dlt_x + dlt_y + dlt_z) > NOISE */
};

/* Create an event based on motion.
 * If frq exceeds WINDOW, cap frq at WINDOW.
 * Return an event_id on success and the appropriate error on failure.
 * system call number 379
 */
int accevt_create(struct acc_motion __user *acceleration);

/* Block a process on an event.
 * It takes the event_id as parameter.
 * The event_id requires verification.
 * Return 0 on success and the appropriate error` on failure.
 * system call number 380
 */
int accevt_wait(int event_id);


/* The acc_signal system call
 * takes sensor data from user, stores the data in the kernel,
 * generates a motion calculation, and notify all open events whose
 * baseline is surpassed.  All processes waiting on a given event
 * are unblocked.
 * Return 0 success and the appropriate error on failure.
 * system call number 381
 */
int accevt_signal(struct dev_acceleration __user *acceleration);

/* Destroy an acceleration event using the event_id,
 * Return 0 on success and the appropriate error on failure.
 * system call number 382
 */
int accevt_destroy(int event_id);
#endif
