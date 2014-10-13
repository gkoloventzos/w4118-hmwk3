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

static LIST_HEAD(acc_evt);
static DEFINE_SPINLOCK(acc_evt_lock);

#define window (WINDOW % 2 ? WINDOW + 1 : WINDOW)

static DECLARE_KFIFO(sens_evt, int, 10);
INIT_KFIFO(sens_evt);
static DEFINE_SPINLOCK(sens_evt_lock);

INIT_KFIFO(mvmt_evt);
//static DEFINE_SPINLOCK(mvmt_evt_lock);

static
struct motion_event *event_search(int event_id, struct list_head *head)
{
	struct list_head *position;
	struct motion_event *event;

	spin_lock(&eventslist_lock);
	list_for_each(position, &events_list) {
		event = list_entry(position, struct motion_event, list);
		if (event->event_id == event_id)
			return event;
	}
	spin_unlock(&events_list_lock);
	return NULL;
}

int sys_accevt_create(struct acc_motion __user *acceleration)
{
	int num_events = 0;
	struct motion_event *new_event;
	struct list_head *position;
	unsigned int correct_frq;
	correct_frq = MIN(acceleration->frq, WINDOW);
	new_event = kmalloc(sizeof(struct motion_event), GFP_KERNEL);
	if (new_event == NULL)
		return -ENOMEM;
	spin_lock(&events_list_lock);
	if (!list_empty(&events_list)) {
		list_for_each(position, &events_list) {
			++num_events;
		}
	}
	new_event->event_id = ++num_events;
	new_event->motion = *acceleration;
	new_event->happened = 0;
	init_waitqueue_head(&(new_event->my_queue));
	//LIST_HEAD_INIT(new_event->list);
	list_add(&(new_event->list), &events_list);
	spin_unlock(&events_list_lock);
	return num_events;
}

int sys_accevt_wait(int event_id)
{
	struct motion_event *evt;
	evt = event_search(event_id, &events_list);
	if (evt == NULL)
		return -ENODATA; //NOT CORRECT ERROR
	return 0;
}


/*
 * Helper to substract to movements
 */
static int match_motion(struct dev_acceleration first,
			struct dev_acceleration last,
			struct acc_motion motion)
{
	if ( abs(last.x - first.x) +
	     abs(last->y - first->y) +
	     abs(last->z - first->z) > NOISE ) {
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
	int match;

	kfifo_size = kfifo_len(sensor_events);
	events = kfifo_size / sizeof(struct dev_acceleration));
	if (!events) {
		 kfifo_out_peek(sensor_events, &cur_acc, sizeof(cur_acc), 0);
		 memcpy(&prv_acc, &cur_acc, sizeof(cur_acc));
	}
	match = 0;
	for (i = 1; i < events; i++) {
		kfifo_out_peek(sensor_events, &cur_acc, sizeof(cur_acc), i)
		sub(&prv_acc, &cur_acc);
		match += match_motion(prv_acc, cur_acc, motion);
		memcpy(&prv_acc, &cur_acc, sizeof(cur_acc));
	}
	if ( match > motion.frq )
		return 1;
	return 0;
}


int sys_accevt_signal(struct dev_acceleration __user *acceleration)
{

	int errno;
	unsigned int rbytes;
	struct dev_acceleration tmp_accel, trash;

	if (copy_from_user(&tmp_accel, acceleration, sizeof(tmp_accel)))
		return -EFAULT;

	spin_lock(acc_evt_lock);
	if (kfifo_is_full(sens_evt)) {
		rbytes = kfifo_out(&sens_evt, &thrash, sizeof(tmp_accel));
		if (rbytes!= sizeof(tmp_accel)) {
			errno = -ENOMEM;
			goto unlock_error;
		}
	}
	bytes_written = kfifo_in(&sens_evt, &tmp_accel, sizeof(tmp_accel));
	if (rbytes != sizeof(tmp_accel)){
		errno = -ENOMEM;
		goto unlock_error;
	}
/*	spin_lock(&eventslist_lock);
	list_for_each(position, &events_list) {
		event = list_entry(position, struct motion_event, list);
*/	
	struct acc_motion my_motion;
	my_motion.dlt_x = 10;
	my_motion.dlt_y = 10;
	my_motion.dlt_z = 10;
	my_motion.frq = 2;

	if (motion_fullfilled(sens_evt, event))
		print_err("DETECTED MOTION FULLFILLED\n");
/*	}
	spin_unlock(acc_evt_lock);
*/
	
	return 0;
unlock_error:
	spin_unlock(sensevents_lock);
	return errno;

}

int sys_accevt_destroy(int event_id)
{
	struct motion_event *event;
	spin_lock(&events_list_lock);
	event = event_search(event_id, &events_list);
	if (event != NULL) {
		//remove queue?
		list_del(&(event->list));
	}
	spin_unlock(&events_list_lock);
	return -ENODATA; //NOT CORRECT ERROR
}
