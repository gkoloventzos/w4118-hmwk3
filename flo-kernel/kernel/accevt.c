/*
 * kernel/accevt.c
 * Copyright (C) 2014 V. Atlidakis, G. Koloventzos, A. Papancea
 *
 * COMS W4118 implementation of syscalls for accelerometer events,
 * i.e., motions identification.
 *
 */
#include <linux/accevt.h>
#include <asm-generic/errno-base.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/acceleration.h>
#include <linux/kfifo.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <asm/atomic.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))


/*
 * An acceleration event polled by the sensor.
 */
struct acceleration_event {
	struct dev_acceleration dev_acc;
	struct list_head list;
};

/*
 * Create a list of acceleration events and
 * protect it with a spinlock.
 */
static LIST_HEAD(acceleration_events);
static DEFINE_SPINLOCK(acceleration_events_lock);

/*
 * A motion event for which processes may wait
 */
struct motion_event {
	unsigned int event_id;
	struct acc_motion motion;
	wait_queue_head_t waiting_procs;
	struct mutex waiting_procs_lock;
	bool happened;
	int waiting_procs_cnt;
	struct list_head list;
};

/*
 * Create a list of motion events and protect
 * it with a mutex.
 */
LIST_HEAD(motion_events);
static DEFINE_SPINLOCK(motion_events_lock);


/*
 * Helper function to search and fetch a specific
 * motion event from motion events list.
 *
 * @motions: The head of the motions' list
 * @id:      The id of the event
 *
 * NOTE: The caller MUST hold the motion_events_lock
 */
static inline
struct motion_event *get_motion_event_id(struct list_head *motions, int id)
{
	struct motion_event *mtn;

	if (id <= 0)
		return NULL;
	if (list_empty(motions))
		return NULL;

	list_for_each_entry_reverse(mtn, motions, list)
		if (mtn->event_id == id)
			return mtn;
	return NULL;
}

/* Create an event based on motion.
 * If frq exceeds WINDOW, cap frq at WINDOW.
 * Return an event_id on success and the appropriate error on failure.
 * system call number 379
 */
int sys_accevt_create(struct acc_motion __user *acceleration)
{
	struct motion_event *new_event;
	static int num_events;
	unsigned int correct_frq;
	int errno;
	int rval;

	correct_frq = MIN(acceleration->frq, WINDOW);
	new_event = kmalloc(sizeof(struct motion_event), GFP_KERNEL);
	if (new_event == NULL) {
		errno = -ENOMEM;
		goto error;
	}
	rval = copy_from_user(&(new_event->motion),
			      acceleration,
			      sizeof(struct acc_motion));
	if (rval < 0) {
		errno = -EFAULT;
		goto error;
	}
	/* create the motion event and grab the lock
	 * when trying to insert it.
	 */
	init_waitqueue_head(&(new_event->waiting_procs));
	new_event->happened = false;
	mutex_init(&new_event->waiting_procs_lock);
	new_event->waiting_procs_cnt = 0;
	spin_lock(&motion_events_lock);
	new_event->event_id = ++num_events;
	list_add(&(new_event->list), &motion_events);
	spin_unlock(&motion_events_lock);
	return num_events;

error:
	return errno;

}

/* Block a process on an event.
 * It takes the event_id as parameter. The event_id requires verification.
 * Return 0 on success and the appropriate error on failure.
 * system call number 380
 */
int sys_accevt_wait(int event_id)
{
	int errno;
	struct motion_event *evt;

	spin_lock(&motion_events_lock);
	evt = get_motion_event_id(&motion_events, event_id);
	if (evt == NULL) {
		errno = -ENODATA;
		goto error_unlock;
	}
	++evt->waiting_procs_cnt;
	spin_unlock(&motion_events_lock);


	wait_event_interruptible(evt->waiting_procs, evt->happened);
	/*
	 * the last notified proc assures noone else
	 * will be woken up for this event.
	 */
	mutex_lock(&evt->waiting_procs_lock);
	if (!--evt->waiting_procs_cnt)
		evt->happened = false;
	mutex_unlock(&evt->waiting_procs_lock);
	return 0;

error_unlock:
	spin_unlock(&motion_events_lock);
	return errno;
}

/*
 * Helper fuction which checks if two consequtive acceleration
 * events fullfil the requirements of a specific motion.
 *
 * @first:  The first acceleration event
 * @last:   The second acceleraetion event
 * @motion: The motion to check
 */
static int matching_acc(struct dev_acceleration first,
			struct dev_acceleration last,
			struct acc_motion motion)
{
	if (abs(last.x - first.x) +
	    abs(last.y - first.y) +
	    abs(last.z - first.z) > NOISE) {
		if (abs(last.x - first.x) >= motion.dlt_x &&
		    abs(last.y - first.y) >= motion.dlt_y &&
		    abs(last.z - first.z) >= motion.dlt_z) {
			return 1;
		}
	}
	return 0;
}

/*
 * Helper function that checks if a specific motion is
 * fulfilled by the acceleration events currently
 * buffered into the kernel
 *
 * @acceleration_events: The list of the acceleration
 *                       events currently buffered in
 *                       the kernel.
 * @motion:              The motion to check for
 *
 * NOTE that the caller MUST hold acceleration_events_lock
 */
static
int check_for_motion(struct list_head *acceleration_events,
		  struct acc_motion motion)
{
	struct acceleration_event *cur_acc_evt, *prv_acc_evt;
	int match = 0;
	int iter = 0;

	if (list_empty(acceleration_events))
		return 0;
	if (list_empty(acceleration_events->next))
		return 0;

	prv_acc_evt = list_first_entry(acceleration_events,
				       struct acceleration_event,
				       list);
	list_for_each_entry(cur_acc_evt, acceleration_events, list) {
		if (!iter) {
			iter++;
			prv_acc_evt = cur_acc_evt;
			continue;
		}
		match += matching_acc(prv_acc_evt->dev_acc,
				      cur_acc_evt->dev_acc,
				      motion);
		prv_acc_evt = cur_acc_evt;
		if (match >= motion.frq)
			return 1;
	}
	return 0;
}

/* The acc_signal system call
 * takes sensor data from user, stores the data in the kernel,
 * generates a motion calculation, and notify all open events whose
 * baseline is surpassed.  All processes waiting on a given event
 * are unblocked.
 * Return 0 success and the appropriate error on failure.
 * system call number 381
 */
int sys_accevt_signal(struct dev_acceleration __user *acceleration)
{
	int rval;
	int errno;
	static int events;
	struct motion_event *mtn;
	struct acceleration_event *acc_evt;
	struct acceleration_event *trash;

	acc_evt = kmalloc(sizeof(struct acceleration_event), GFP_KERNEL);
	if (!acc_evt) {
		errno = -ENOMEM;
		goto error;
	}
	rval = copy_from_user(&(acc_evt->dev_acc),
			      acceleration,
			      sizeof(struct dev_acceleration));
	if (rval < 0) {
		errno = -EFAULT;
		goto error_free_mem;
	}

	spin_lock(&acceleration_events_lock);
	if (events == WINDOW + 1) {
		trash = list_first_entry(&acceleration_events,
				   struct acceleration_event,
				   list);
		list_del(&trash->list);
		kfree(trash);
	} else {
		events++;
	}
	list_add_tail(&(acc_evt->list), &acceleration_events);
	spin_lock(&motion_events_lock);
	list_for_each_entry(mtn, &motion_events, list) {
		if (check_for_motion(&acceleration_events, mtn->motion)) {
			mtn->happened = true;
			wake_up_interruptible(&(mtn->waiting_procs));
		}
	}
	spin_unlock(&motion_events_lock);
	spin_unlock(&acceleration_events_lock);
	return 0;

error_free_mem:
	kfree(acc_evt);
error:
	return errno;
}

/* Destroy an acceleration event using the event_id,
 * Return 0 on success and the appropriate error on failure.
 * system call number 382
 */
int sys_accevt_destroy(int event_id)
{
	int errno;
	struct motion_event *evt;

	spin_lock(&motion_events_lock);
	evt = get_motion_event_id(&motion_events, event_id);
	if (evt == NULL) {
		errno =  -ENODATA;
		goto error_unlock;
	}
	list_del(&(evt->list));
	spin_unlock(&motion_events_lock);

	mutex_lock(&evt->waiting_procs_lock);
	evt->happened = true;
	/* Is anybody sleeping? */
	if (evt->waiting_procs_cnt)
		wake_up_interruptible(&(evt->waiting_procs));
	else
		goto exit;
	while (1) {
		mutex_unlock(&evt->waiting_procs_lock);
		mutex_lock(&evt->waiting_procs_lock);
		if (!evt->waiting_procs_cnt)
			break;
	}
exit:
	mutex_unlock(&evt->waiting_procs_lock);
	kfree(evt);
	return 0;

error_unlock:
	spin_unlock(&motion_events_lock);
	return errno;
}
