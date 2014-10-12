/*
 * accelerationd.c
 * A user space daemon that read accelerometer sensor
 * information and registers them into the kernel
 *
 * Copyright (C) 2014 V. Atlidakis, G. Koloventzos, A. Papancea
 *
 * COMS W4118 implementation of user-space daemon
 */
#define _GNU_SOURCE
#include <bionic/errno.h> /* Google does things a little different...*/
#include <fcntl.h>
#include <math.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <hardware/hardware.h>
#include <hardware/sensors.h> /* <-- This is a good place to look! */
#include "../flo-kernel/include/linux/akm8975.h"
#include "acceleration.h"

/* from sensors.c */
#define ID_ACCELERATION   (0)
#define ID_MAGNETIC_FIELD (1)
#define ID_ORIENTATION	  (2)
#define ID_TEMPERATURE	  (3)

#define SENSORS_ACCELERATION   (1<<ID_ACCELERATION)
#define SENSORS_MAGNETIC_FIELD (1<<ID_MAGNETIC_FIELD)
#define SENSORS_ORIENTATION    (1<<ID_ORIENTATION)
#define SENSORS_TEMPERATURE    (1<<ID_TEMPERATURE)


#define __NR_set_acceleration 378


/* set to 1 for a bit of debug output */
#ifdef _DEBUG
	#define LOGFILE "/data/misc/accelerometer.log"
	#define DBG(fmt, ...) fprintf(fp, fmt, ## __VA_ARGS__)
	static FILE *fp;
#else
	#define DBG(fmt, ...)
#endif


static int  stop;

static int effective_sensor;

static int open_sensors(struct sensors_module_t **hw_module,
			struct sensors_poll_device_t **poll_device);
static void enumerate_sensors(const struct sensors_module_t *sensors);

static void stop_me(int signal)
{
	DBG("Terminated by signal: %d\n", signal);
	stop = 1;
}

static int poll_sensor_data(struct sensors_poll_device_t *sensors_device)
{
	int i;
	int err;
	const size_t numEventMax = 16;
	const size_t minBufferSize = numEventMax;
	struct dev_acceleration acceleration;
	sensors_event_t buffer[minBufferSize];

	ssize_t count = sensors_device->poll(sensors_device,
					     buffer,
					     minBufferSize);
	for (i = 0; i < count; ++i) {
		if (buffer[i].sensor != effective_sensor)
			continue;
		/* At this point we should have valid data*/
		/* Scale it and pass it to kernel*/
		acceleration.x = (int)(buffer[i].acceleration.x*100);
		acceleration.y = (int)(buffer[i].acceleration.y*100);
		acceleration.z = (int)(buffer[i].acceleration.z*100);
		DBG("Acceleration: x= %d y= %d z= %d\n",
		    acceleration.x, acceleration.y, acceleration.z);
		err = syscall(378, &acceleration);
		if (err)
			goto error;
	}
	err = 0;
error:
	return err;
}

/*
 * entry point:
 * daemon implementation
 */
int main(int argc, char **argv)
{
	int rval;
	struct sigaction act;
	struct sensors_module_t *sensors_module;
	struct sensors_poll_device_t *sensors_device = NULL;

	rval = daemon(0, 0);
	if (rval < 0) {
		perror("daemon");
		goto exit_failure;
	}

	effective_sensor = -1;
	sensors_module = NULL;
	sensors_device = NULL;
	if (open_sensors(&sensors_module,
			 &sensors_device) < 0) {
		perror("open_sensors");
		goto exit_failure;
	}
	enumerate_sensors(sensors_module);

	memset(&act, 0, sizeof(act));
	act.sa_handler = &stop_me;
	act.sa_flags = 0;
	if (sigaction(SIGINT, &act, NULL) == -1 ||
	    sigaction(SIGQUIT, &act, NULL) == -1 ||
	    sigaction(SIGTERM, &act, NULL) == -1) {
		perror("sigaction");
		goto exit_failure;
	}

#ifdef _DEBUG
	fp = fopen(LOGFILE, "w+");
	if (fp == NULL) {
		perror("fopen");
		goto exit_failure;
	}
#endif
	stop = 0;
	while (!stop) {
		rval = poll_sensor_data(sensors_device);
		if (rval < 0) {
			perror("poll_sensor_data");
			goto exit_failure;
		}
		usleep((useconds_t) 1000 * TIME_INTERVAL);
	}
#ifdef _DEBUG
	fclose(fp);
#endif
	if (sensors_close(sensors_device) < 0) {
		perror("Failed to close sensor device");
		goto exit_failure;
	}
	return EXIT_SUCCESS;

exit_failure:
	return EXIT_FAILURE;
}

/*                DO NOT MODIFY BELOW THIS LINE                    */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static int open_sensors(struct sensors_module_t **mSensorModule,
			struct sensors_poll_device_t **mSensorDevice)
{

	int err = hw_get_module(SENSORS_HARDWARE_MODULE_ID,
				     (hw_module_t const **)mSensorModule);

	if (err) {
		printf("couldn't load %s module (%s)",
			SENSORS_HARDWARE_MODULE_ID, strerror(-err));
	}

	if (!*mSensorModule)
		return -1;

	err = sensors_open(&((*mSensorModule)->common), mSensorDevice);

	if (err) {
		printf("couldn't open device for module %s (%s)",
			SENSORS_HARDWARE_MODULE_ID, strerror(-err));
	}

	if (!*mSensorDevice)
		return -1;

	const struct sensor_t *list;
	ssize_t count = (*mSensorModule)->get_sensors_list(*mSensorModule,
							   &list);
	size_t i;
	for (i = 0 ; i < (size_t) count ; i++)
		(*mSensorDevice)->activate(*mSensorDevice, list[i].handle, 1);

	return 0;
}

static void enumerate_sensors(const struct sensors_module_t *sensors)
{
	int nr, s;
	const struct sensor_t *slist = NULL;
	if (!sensors)
		printf("going to fail\n");

	nr = sensors->get_sensors_list((struct sensors_module_t *)sensors,
					&slist);
	if (nr < 1 || slist == NULL) {
		printf("no sensors!\n");
		return;
	}

	for (s = 0; s < nr; s++) {
		printf("%s (%s) v%d\n\tHandle:%d, type:%d, max:%0.2f, "\
			"resolution: %0.2f\n", slist[s].name, slist[s].vendor,
			slist[s].version, slist[s].handle, slist[s].type,
			slist[s].maxRange, slist[s].resolution);

		/* Awful hack to make it work on emulator */
		if (slist[s].type == 1 && slist[s].handle == 0)
			effective_sensor = 0; /*the sensor ID*/
	}
}
