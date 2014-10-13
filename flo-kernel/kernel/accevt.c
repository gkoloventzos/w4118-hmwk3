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
static DEFINE_SPINLOCK(acc_evt_lock);

//static DEFINE_SPINLOCK(mvmt_evt_lock);
/*
 * Search in motions_list to find and return the motion with event_id.
 * Lock must be free before entering in this function.
 */
static
struct motion_event *event_search(int event_id, struct list_head *head)
{
	struct list_head *position;
	struct motion_event *event;

	spin_lock(&motions_list_lock);
	list_for_each(position, head) {
		event = list_entry(position, struct motion_event, list);
		if (event->event_id == event_id)
			return event;
	}
	spin_unlock(&motions_list_lock);
	return NULL;
}

/*
 * Traverse the motions_list adn returns the number of events.
 * Returns -1 if the events already exists.
 * Lock sould be acquired before calling this function.
 */
static int motion_exists(struct list_head *head, struct acc_motion *new)
{
	struct list_head *pos;
	struct motion_event *old;
	int num_events = 0;

	list_for_each(pos, head) {
		old = list_entry(pos, struct motion_event, list);
		if (old->motion.dlt_x == new->dlt_x && 
		    old->motion.dlt_y == new->dlt_y &&
		    old->motion.dlt_z == new->dlt_z && 
		    old->motion.frq == new->frq)
			return -1;
		++num_events;
	}
	return num_events;
}

int sys_accevt_create(struct acc_motion __user *acceleration)
{
	int num_events = 0;
	struct motion_event *new_event;
	unsigned int correct_frq;
	int errno;

	correct_frq = MIN(acceleration->frq, WINDOW);
	new_event = kmalloc(sizeof(struct motion_event), GFP_KERNEL);
	if (new_event == NULL) {
		errno = -ENOMEM;
		goto error;
	}

	spin_lock(&motions_list_lock);
	if (!list_empty(&motions_list)) {
		num_events = motion_exists(&motions_list, acceleration);
	}
	if (num_events == -1) {
		errno = num_events;
		goto exists;
	}
	new_event->event_id = ++num_events;
	new_event->motion = *acceleration;
	new_event->happened = 0;
	init_waitqueue_head(&(new_event->my_queue));
	//LIST_HEAD_INIT(new_event->list);
	list_add(&(new_event->list), &motions_list);
	spin_unlock(&motions_list_lock);
	return num_events;

exists:
	spin_unlock(&motions_list_lock);
	kfree(new_event);
error:
	return errno;

}

int sys_accevt_wait(int event_id)
{
	struct motion_event *evt;
	evt = event_search(event_id, &motions_list);
	if (evt == NULL)
		return -ENODATA; //NOT CORRECT ERROR
	return 0;
	/**/
}


/*
 * Helper to substract to movements
 */
static int match_motion(struct dev_acceleration first,
			struct dev_acceleration last,
			struct acc_motion motion)
{
	if ( abs(last.x - first.x) +
	     abs(last.y - first.y) +
	     abs(last.z - first.z) > NOISE ) {
		if ( abs(last.x - first.x) >= motion.dlt_x &&
		     abs(last.y - first.y) >= motion.dlt_y &&
		     abs(last.z - first.z) >= motion.dlt_z) {
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
int motion_fullfilled(struct kfifo *sensor_events, struct acc_motion motion)
{
	struct dev_acceleration cur_acc, prv_acc;
	unsigned int kfifo_size;
	unsigned int events;
	int match, i;

	kfifo_size = kfifo_len(sensor_events);
	events = kfifo_size / sizeof(struct dev_acceleration);
	if (!events) {
		 //kfifo_out_peek(sensor_events, &cur_acc, sizeof(cur_acc), 0);
		 //memcpy(&prv_acc, &cur_acc, sizeof(cur_acc));
	}
	match = 0;
	for (i = 1; i < events; i++) {
		//kfifo_out_peek(sensor_events, &cur_acc, sizeof(cur_acc), i);
		/*sub(&prv_acc, &cur_acc);*/
		//match += match_motion(prv_acc, cur_acc, motion);
		//memcpy(&prv_acc, &cur_acc, sizeof(cur_acc));
	}
	if ( match > motion.frq )
		return 1;
	return 0;
}

int sys_accevt_signal(struct dev_acceleration __user *acceleration)
{

	int rval;
	int errno;
	struct acc_motion my_motion;
	struct acceleration_event *acc_evt;
	/*static int should_init = 1;*/
	static int events = 0;

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

	spin_lock(&acc_evt_lock);
	if (events == WINDOW + 1) {
		printk(KERN_ERR "kfifo: dell\n");
		list_del(acceleration_events.next);
	} else {
		events++;
		list_add_tail(&(acc_evt->list), &acceleration_events);
	}
	printk(KERN_ERR "kfifo: %d %d\n", acc_evt->dev_acc.x, acc_evt->dev_acc.y);

	list_for_each_entry(acc_evt, &acceleration_events, list)
		printk(KERN_ERR "kfifo: %d %d\n", acc_evt->dev_acc.x, acc_evt->dev_acc.y);
	
	printk(KERN_ERR "kfifo: our motion\n");
	my_motion.dlt_x = 10;
	my_motion.dlt_y = 10;
	my_motion.dlt_z = 10;
	my_motion.frq = 2;
	printk(KERN_ERR "kfifo: THIS IS THE END\n");
//	if (motion_fullfilled(sens_evt, my_motion))
//		printk(KERN_ERR "DETECTED MOTION FULLFILLED\n");
	spin_unlock(&acc_evt_lock);	
	kfree(acc_evt);
	return 0;

	printk(KERN_ERR "ERRRRRRRRRRRRRRRRROR\n");
error_free_mem_unlock:
	spin_unlock(&acc_evt_lock);
error_free_mem:
	kfree(acc_evt);
error:
	return errno;

}


int sys_accevt_destroy(int event_id)
{
	struct motion_event *event;
	event = event_search(event_id, &motions_list);
	if (event != NULL) {
		//remove queue?
		list_del(&(event->list));
	}
	return -ENODATA; //NOT CORRECT ERROR
}
