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


struct acceleration_event {
	struct dev_acceleration dev_acc;
	struct list_head list;
};
static LIST_HEAD(acceleration_events);
static DEFINE_SPINLOCK(acceleration_events_lock);

/*
 * Definition of event list
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
LIST_HEAD(motion_events);
static DEFINE_SPINLOCK(motion_events_lock);

/*
 * Helper function....
 *
 * NOTE: The caller SHOULD hold the motion_events_lock
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
	printk(KERN_ERR "inside create\n");
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
	init_waitqueue_head(&(new_event->waiting_procs));
	new_event->happened = false;
	mutex_init(&new_event->waiting_procs_lock);
	new_event->waiting_procs_cnt = 0;
	spin_lock(&motion_events_lock);
	new_event->event_id = ++num_events;
	list_add(&(new_event->list), &motion_events);
	spin_unlock(&motion_events_lock);
	printk(KERN_ERR "Created ecvent eith id:%d\n", num_events);
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
//	spin_unlock(&motion_events_lock);
	if (evt == NULL) {
		printk(KERN_ERR "WAIT ERR\n");
		errno = -ENODATA; //NOT CORRECT ERROR
		goto error_unlock;
	}
//	mutex_lock(&evt->waiting_procs_lock);
	++evt->waiting_procs_cnt;
	spin_unlock(&motion_events_lock);
//	mutex_unlock(&evt->waiting_procs_lock);


	wait_event_interruptible(evt->waiting_procs, evt->happened);
	mutex_lock(&evt->waiting_procs_lock);
	if(!--evt->waiting_procs_cnt)
		evt->happened = false;
	mutex_unlock(&evt->waiting_procs_lock);
	return 0;

error_unlock:
	spin_unlock(&motion_events_lock);
	return errno;
}


/*
 * Helper to substract to movements
 */
static int matching_acc(struct dev_acceleration first,
			struct dev_acceleration last,
			struct acc_motion motion)
{

//	printk(KERN_ERR "MATCHIN_ACC INVOKED\n");
	if (abs(last.x - first.x) +
	    abs(last.y - first.y) +
	    abs(last.z - first.z) > NOISE) {
//		printk(KERN_ERR "EXCEEDS NOISE\n");
		if (abs(last.x - first.x) >= motion.dlt_x &&
		    abs(last.y - first.y) >= motion.dlt_y &&
		    abs(last.z - first.z) >= motion.dlt_z) {
//printk(KERN_ERR "MATCHING motion: %d %d %d, %d %d %d NOISE:%d\n",  first.x, first.y, first.z, last.x, last.y, last.z, NOISE);
			return 1;
		}
	}
	return 0;
}

/*
 * Helper function that checks if a specific motion is satisfied
 * from the movement data currently buffered into the kernel
 *
 * NOTE that the caller MUST hold acceleration_events_lock
 *
 */
static
int check_for_motion(struct list_head *acceleration_events,
		  struct acc_motion motion)
{
	struct acceleration_event *cur_acc_evt, *prv_acc_evt;
	int match;
	int iter;

	if (list_empty(acceleration_events))
		return 0;
	if (list_empty(acceleration_events->next))
		return 0;

	prv_acc_evt = list_first_entry(acceleration_events,
				       struct acceleration_event,
				       list);
	match = 0;
	iter = 0;
	list_for_each_entry(cur_acc_evt, acceleration_events, list) {
		if (!iter) {
			iter++;
			prv_acc_evt = cur_acc_evt;
			continue;
		}
//		printk(KERN_ERR "MATCH = %d\n", match);
		match += matching_acc(prv_acc_evt->dev_acc,
				      cur_acc_evt->dev_acc,
				      motion);
		prv_acc_evt = cur_acc_evt;
		if (match >= motion.frq) {
//			printk(KERN_ERR "MATCHEDDDDDDDDDDDDDDDDDDDDDDDDDDDD\n");
			return 1;
		}
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
//		printk(KERN_ERR "DELETED :%d %d %d\n", trash->dev_acc.x, trash->dev_acc.y, trash->dev_acc.z);
		kfree(trash);
	} else {
		events++;
	}
	list_add_tail(&(acc_evt->list), &acceleration_events);
//	printk(KERN_ERR "REGISTERED :%d %d %d\n", acceleration->x, acceleration->y, acceleration->z);
//	printk(KERN_ERR "CHECKING MOTIONS\n");
	spin_lock(&motion_events_lock);
	list_for_each_entry(mtn, &motion_events, list) {
//		printk(KERN_ERR "mtn :%d %d %d\n", mtn->motion.dlt_x, mtn->motion.dlt_y, mtn->motion.dlt_z);
		if (check_for_motion(&acceleration_events, mtn->motion)) {
//			printk(KERN_ERR "DETECTED MOVEMENT\n");
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

	printk(KERN_ERR "DESIIIYYYYYYYYYYYYYYYYYYYYYYYYYYYYy:%d \n", event_id);
	spin_lock(&motion_events_lock);
	evt = get_motion_event_id(&motion_events, event_id);
	if (evt == NULL) {
		printk(KERN_ERR "DESYYYYYYYYYYYYYYYYYy: ERROR\n");
		errno =  -ENODATA;
		goto error_unlock;
	}
	list_del(&(evt->list));
	spin_unlock(&motion_events_lock);

	printk(KERN_ERR "DESYYYYYYYYYYYYYYYYYy: NO ERROR\n");
	mutex_lock(&evt->waiting_procs_lock);
	evt->happened = true;
	/* Is anybody sleeping? */
	if (evt->waiting_procs_cnt) {
		wake_up_interruptible(&(evt->waiting_procs));
	} else {
		goto exit;
	}
	while (1) {
		mutex_unlock(&evt->waiting_procs_lock);
		mutex_lock(&evt->waiting_procs_lock);
		printk(KERN_ERR "DESYYYYYYYYYYYYYYYYYy:%d\n", evt->waiting_procs_cnt);
		if(!evt->waiting_procs_cnt)
			break;
	}
exit:
	mutex_unlock(&evt->waiting_procs_lock);
	printk(KERN_ERR "BEFORE KFREE\n");
	kfree(evt);
	return 0;

error_unlock:
	spin_unlock(&motion_events_lock);
	return errno;
}
