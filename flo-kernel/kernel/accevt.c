/*
 * kernel/accevt.c
 * Copyright (C) 2014 V. Atlidakis, G. Koloventzos, A. Papancea
 *
 * COMS W4118 implementation of syscalls for accelerometer
 */
#include <linux/accevt.h>
#include <asm-generic/errno-base.h>
#include <linux/slab.h>
#include <linux/acceleration.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))
static DEFINE_SPINLOCK(events_list_lock);
static LIST_HEAD(events_list);

static struct list_head *event_search(int event_id, struct list_head *head)
{
	struct list_head *position;
	spin_lock(&eventlist_lock);
	list_for_each(position, &events_list) {
		if (position.event_id == event_id) {
			return position;
		}
	}
	spin_unlock(&eventlist_lock);
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
	spin_lock(&eventlist_lock);
	if (!list_empty(&events_list)) {
		list_for_each(position, &events_list) {
			++num_events;
		}
	}
	new_event->event_id = ++num_events;
	new_event->motion.dlt_x = acceleration->dlt_x;
	new_event->motion.dlt_y = acceleration->dlt_y;
	new_event->motion.dlt_z = acceleration->dlt_z;
	new_event->motion.frq = correct_frq;
	new_event->motion.happened = 0;
	init_waitqueue_head(&(new_event->motion.my_queue));
	LIST_HEAD_INIT(new_event->list);
	list_add(new_event->list, events_list);
	spin_unlock(&eventlist_lock);
	return num_events;
}

int sys_accevt_wait(int event_id)
{
	struct list_head *position;
	position = event_search(position, events_list);
	if (position == NULL)
		return -ENODATA; //NOT CORRECT ERROR

}

int sys_accevt_signal(struct dev_acceleration __user *acceleration)
{

}

int sys_accevt_destroy(int event_id)
{
	struct list_head *position;
	int found;
	spin_lock(&eventlist_lock);
	position = event_search(event_id, events_list);
	if (position != NULL) {
		//remove queue?
		list_del(position);
	}
	spin_unlock(&eventlist_lock);
	return -ENODATA; //NOT CORRECT ERROR
}
