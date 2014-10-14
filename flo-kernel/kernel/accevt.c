/*
 * kernel/accevt.c
 * Copyright (C) 2014 V. Atlidakis, G. Koloventzos, A. Papancea
 *
 * COMS W4118 implementation of syscalls for accelerometer
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


static DEFINE_SPINLOCK(motions_list_lock);


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
	bool happend;
	atomic_t processes;
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
struct motion_event *get_n_motion_event(struct list_head *motions, int n)
{
	struct motion_event *mtn;

	if (n <= 0)
		return NULL;
	if (list_empty(motions))
		return NULL;

	list_for_each_entry_reverse(mtn, motions, list) {
		if (!--n)
			return mtn;
	}
	return NULL;
}



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
	//new_event->happend = false;
	//mutex_init(new_event->mutex);
	new_event->processes = ATOMIC_INIT(0);
	spin_lock(&motions_list_lock);
	new_event->event_id = ++num_events;
	list_add(&(new_event->list), &motion_events);
	spin_unlock(&motions_list_lock);
	printk(KERN_ERR "Created ecvent eith id:%d\n", num_events);
	return num_events;

error:
	return errno;

}

int sys_accevt_wait(int event_id)
{
	struct motion_event *evt;

	spin_lock(&motion_events_lock);
	evt = get_n_motion_event(&motion_events, event_id);
	spin_unlock(&motion_events_lock);
	if (evt == NULL) {
		printk(KERN_ERR "WAIT ERR\n");
		return -ENODATA; //NOT CORRECT ERROR
	}
	//atomic_inc(&evt->processes);
	wait_event_interruptible(evt->waiting_procs, evt->happend);
	evt->happend = false;
	//printk(KERN_ERR "WAIT\n");
	return 0;
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
printk(KERN_ERR "MATCHING motion: %d %d %d, %d %d %d NOISE:%d\n",  first.x, first.y, first.z, last.x, last.y, last.z, NOISE);
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
		printk(KERN_ERR "MATCH = %d\n", match);
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

int sys_accevt_signal(struct dev_acceleration __user *acceleration)
{
	int rval;
	int errno;
	static int events;
	struct motion_event *mtn;
	struct acceleration_event *acc_evt;

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
	if (events == WINDOW + 1)
		list_del(acceleration_events.next);
	else
		events++;
	list_add_tail(&(acc_evt->list), &acceleration_events);

	printk(KERN_ERR "REGISTERING :%d %d %d\n", acceleration->x, acceleration->y, acceleration->z);
//	printk(KERN_ERR "CHECKING MOTIONS\n");
	spin_lock(&motion_events_lock);
	list_for_each_entry(mtn, &motion_events, list) {
//		printk(KERN_ERR "mtn :%d %d %d\n", mtn->motion.dlt_x, mtn->motion.dlt_y, mtn->motion.dlt_z);
		if (check_for_motion(&acceleration_events, mtn->motion)) {
			printk(KERN_ERR "DETECTED MOVEMENT\n");
			mtn->happend = true;
			wake_up_interruptible(&(mtn->waiting_procs));
			atomic_set(&mtn->processes, 0);
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


int sys_accevt_destroy(int event_id)
{
	struct motion_event *evt;

	evt = get_n_motion_event(&motion_events, event_id);
	if (evt != NULL) {
		//remove queue?
		if (atomic_read(&evt->processes)) {
			evt->happend = true;
			wake_up_interruptible(&(evt->waiting_procs));
			printk(KERN_ERR "Waking everything up\n");
		}
		list_del(&(evt->list));
		kfree(evt);
	}
	return -ENODATA; //NOT CORRECT ERROR
}
