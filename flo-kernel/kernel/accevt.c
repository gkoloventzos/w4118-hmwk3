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


int sys_accevt_create(struct acc_motion __user *acceleration)
{

}

int sys_accevt_wait(int event_id)
{

}

int sys_accevt_signal(struct dev_acceleration __user *acceleration)
{

}

int sys_accevt_destroy(int event_id)
{

}
