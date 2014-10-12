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

static DEFINE_SPINLOCK(events_list_lock);
static LIST_HEAD(events_list);

static int event_search(int event_id, struct list_head *head)
{
	struct list_head *position;
	int find = 0;
	spin_lock(&eventlist_lock);
	list_for_each(position, &events_list) {
		if (position.event_id == event_id) {
			find = 1;
			break;
		}
	}
	spin_unlock(&eventlist_lock);
	return find;
}

int sys_accevt_create(struct acc_motion __user *acceleration)
{
	int num_events = 0;
	struct motion_event *new_event;
	struct list_head *position;
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
	list_add(new_event, events_list);
	spin_unlock(&eventlist_lock);
	return num_events;
}

int sys_accevt_wait(int event_id)
{
	struct list_head *position;
	int found;
	found = event_search(event_id, events_list);
	if (!found)
		return -ENOMEM;
}

int sys_accevt_signal(struct dev_acceleration __user *acceleration)
{

}

int sys_accevt_destroy(int event_id)
{

}
