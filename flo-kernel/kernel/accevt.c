/*
 * kernel/accevt.c
 * Copyright (C) 2014 V. Atlidakis, G. Koloventzos, A. Papancea
 *
 * COMS W4118 implementation of syscalls for accelerometer
 */
#include <linux/accevt.h>
#include <asm-generic/errno-base.h> 

int accevt_create(struct acc_motion __user *acceleration){

}

int accevt_wait(int event_id){
}

int accevt_signal(struct dev_acceleration __user * acceleration){
}

int accevt_destroy(int event_id){
}
