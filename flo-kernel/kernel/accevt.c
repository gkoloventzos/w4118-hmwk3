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

#define MIN(a, b) ((a) < (b) ? (a) : (b))
static DEFINE_SPINLOCK(events_list_lock);

static 
struct motion_event *event_search(int event_id, struct list_head *head)
{
	struct list_head *position;
	struct motion_event *event;

	spin_lock(&events_list_lock);
	list_for_each(position, &events_list) {
		event = list_entry(position, struct motion_event, list);
		if (event->event_id == event_id) {
			return event;
		}
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

int sys_accevt_signal(struct dev_acceleration __user *acceleration)
{

	struct dev_acceleration tmp_accel;
	if (copy_from_user(&tmp_accel, acceleration, sizeof(tmp_accel)))
		return -EFAULT;
	return 0;

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
