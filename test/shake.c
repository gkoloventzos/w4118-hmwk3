#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/types.h>
#include "acceleration.h"

#define VERTICAL	0
#define HORIZONTAL	1
#define BOTHDIR		2

void print_motion(int child, int dir)
{
	if (dir == VERTICAL)
		printf("%d detected a vertical shake\n", child);
	else if (dir == HORIZONTAL)
		printf("%d detected a horizontal shake\n", child);
	else if (dir == BOTHDIR)
		printf("%d detected a shake\n", child);
	else
		printf("something went wrong...\n");
}

/*
 * listens to specific 'dir' shake motion
 * for the given child
 */
void listen_to(int child, int dir)
{
	struct acc_motion motion;
	int ret;

	if (dir == VERTICAL) {
		motion.dlt_x = 10;
		motion.dlt_y = 10;
		motion.dlt_z = 100;
		motion.frq   = 100;
	} else if (dir == HORIZONTAL) {
		motion.dlt_x = 100;
		motion.dlt_y = 100;
		motion.dlt_z = 10;
		motion.frq   = 100;
	} else if (dir == BOTHDIR) {
		motion.dlt_x = 100;
		motion.dlt_y = 100;
		motion.dlt_z = 100;
		motion.frq   = 100;
	}

	/* accevt_create */
	syscall(379, motion);

	while (1) {
		/* accevt_wait */
		ret = syscall(380, dir);
		if (ret != 0)
			return;
		else
			print_motion(child, dir);
	}
}

/*
 * returns number of seconds that
 * passed from the 'start' until present
 */
int run_time(struct timeval start) {
	struct timeval end;
	int ret;

	ret = gettimeofday(&end, 0);
	if (ret != 0) {
		perror("gettimeofday");
		exit(EXIT_FAILURE);
	}

	return (end.tv_sec-start.tv_sec)*1000000 + end.tv_usec-start.tv_usec;
}

int main(int argc, char **argv)
{
	int i;
	int n;
	int ret;
	pid_t pid;
	int status;
	struct timeval start;
	struct dev_acceleration acceleration;

	if (argc == 2) {
		n = (int) argv[1];
	} else {
		printf("shake.c: invalid number of arguments\n");
		printf("usage:\n");
		printf("\tshake num_childred\n");
		exit(EXIT_FAILURE);
	}

	ret = gettimeofday(&start, 0);
	if (ret != 0) {
		perror("gettimeofday");
		exit(EXIT_FAILURE);
	}

	for (i = 0; i < n; i++) {
		pid = fork();

		if (pid < 0) {
			perror("fork");
			exit(EXIT_FAILURE);
		} else if (!pid) {
			listen_to(i, i % 3);
			exit(EXIT_SUCCESS);
		}
	}

	while (1) {
		if (run_time(start) > 60) {
			/* accevt_destroy */
			syscall(382, 0);
			syscall(382, 1);
			syscall(382, 2);

			return 0;
		}

		acceleration.x = 0; /* TODO: get acc x from device */
		acceleration.y = 0; /* TODO: get acc y from device */
		acceleration.z = 0; /* TODO: get acc z from device */

		/* accevt_signal */
		syscall(381, acceleration);
	}

	return 0;
}
