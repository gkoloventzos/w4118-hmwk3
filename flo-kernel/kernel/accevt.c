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
	struct list_head list;
};

LIST_HEAD(motion_events);
static DEFINE_SPINLOCK(motion_events_lock);


static inline
struct motion_event* get_n_motion_event(struct list_head *motions, int n)
{
	struct motion_event *mtn;

	if (n <= 0)
		return NULL;
	spin_lock(&motion_events_lock);
	if(list_empty(motions)) {
		spin_unlock(&motion_events_lock);
		return NULL;
	}
	list_for_each_entry(mtn, motions, list) {
		if (!--n) {
			spin_unlock(&motion_events_lock);
			return mtn;
		}
	}
	spin_unlock(&motion_events_lock);
	return NULL;
}



int sys_accevt_create(struct acc_motion __user *acceleration)
{
	struct motion_event *new_event;
	static int num_events = 0;
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
	if (rval < 0){
		errno = -EFAULT;
		goto error;
	}
	init_waitqueue_head(&(new_event->waiting_procs));
	spin_lock(&motions_list_lock);
	new_event->event_id = ++num_events;
	list_add(&(new_event->list), &motion_events);
	spin_unlock(&motions_list_lock);
	printk( KERN_ERR "Created ecvent eith id:%d\n", num_events);
	return num_events;

error:
	return errno;

}

int sys_accevt_wait(int event_id)
{
	struct motion_event *evt;

	evt = get_n_motion_event(&motion_events, event_id);
	if (evt == NULL)
		return -ENODATA; //NOT CORRECT ERROR
	return -1;
	return 0;
	/**/
}


/*
 * Helper to substract to movements
 */
static int matching_acc(struct dev_acceleration first,
			struct dev_acceleration last,
			struct acc_motion motion)
{

	printk(KERN_ERR "MATCHIN_ACC INVOKED\n");
	if ( abs(last.x - first.x) +
	     abs(last.y - first.y) +
	     abs(last.z - first.z) > NOISE ) {
		printk(KERN_ERR "EXCEEDS NOISE\n");
		if ( abs(last.x - first.x) >= motion.dlt_x &&
		     abs(last.y - first.y) >= motion.dlt_y &&
		     abs(last.z - first.z) >= motion.dlt_z) {
			printk(KERN_ERR "MATCHING motion: %d %d %d, %d %d %d NOISE:%d\n",  first.x, first.y, first.z, last.x, last.y,last.z, NOISE);
			return 1;
		}
	}
	return 0;
}

/*
 * Helper function that checks if a specific motion is satisfied
 * from the movement data currently buffered into the kernel
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

	prv_acc_evt = list_first_entry(acceleration_events, struct acceleration_event, list);
	match = 0;
	iter = 0;
	list_for_each_entry(cur_acc_evt, acceleration_events, list) {
		printk(KERN_ERR "checnking acc_evtn\n");
		if (!iter) {
			iter++;
			prv_acc_evt = cur_acc_evt;
			//, sizeof(struct acceleration_event));
			continue;
		}
		match += matching_acc(prv_acc_evt->dev_acc, cur_acc_evt->dev_acc, motion);
		prv_acc_evt = cur_acc_evt;
		//, sizeof(struct acceleration_event));
	}
	printk(KERN_ERR "---\n");
	if (match >= motion.frq)
		return 1;
	return 0;
}

int sys_accevt_signal(struct dev_acceleration __user *acceleration)
{
	int rval;
	int errno;
	static int events = 0;
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
	printk(KERN_ERR "ADD acc_event: %d %d\n", events, WINDOW);

	struct motion_event *e2 = get_n_motion_event(&motion_events, 1);
	if (e2)
		printk(KERN_ERR "%d %d %d %d", e2->motion.dlt_x,  e2->motion.dlt_y, e2->motion.dlt_z, e2->motion.frq);

	e2 = get_n_motion_event(&motion_events, 2);
	if (e2)
		printk(KERN_ERR "%d %d %d %d", e2->motion.dlt_x,  e2->motion.dlt_y, e2->motion.dlt_z, e2->motion.frq);
	printk(KERN_ERR "CHECKING MOTIONS\n");

	list_for_each_entry(mtn, &motion_events, list) {
		printk(KERN_ERR "mtn :%d %d %d\n", mtn->motion.dlt_x, mtn->motion.dlt_y, mtn->motion.dlt_z);
		if (check_for_motion(&acceleration_events, mtn->motion))
			printk(KERN_ERR "DETECTED MOTION FULLFILLED\n");
	}
	printk(KERN_ERR "ALL MOTIONS CHECKED\n");
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
		list_del(&(evt->list));
		kfree(evt);
	}
	return -ENODATA; //NOT CORRECT ERROR
}


