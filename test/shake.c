#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <syscall.h>
#include "acceleration.h"

#define VERTICAL	0
#define HORIZONTAL	1
#define BOTHDIR		2

#define accevt_create	379
#define accevt_wait	380
#define accevt_destroy	382

static void print_motion(int child, int dir)
{
	if (dir == VERTICAL)
		printf("%d detected a vertical shake\n", child);
	else if (dir == HORIZONTAL)
		printf("%d detected a horizontal shake\n", child);
	else if (dir == BOTHDIR)
		printf("%d detected a shake\n", child);
	else
		printf("something went wrong...%d\n", dir);
}

/*
 * listens to specific 'dir' shake motion
 * for the given child
 */
static void listen_to(int child, int dir)
{
	int ret;

	while (1) {
		ret = syscall(accevt_wait, dir);
		if (ret != 0)
			return;
		print_motion(child, dir);
	}
}

/*
 * returns number of seconds that
 * passed from the 'start' until present
 */
static int run_time(struct timeval start)
{
	struct timeval end;
	int ret;

	ret = gettimeofday(&end, 0);
	if (ret != 0) {
		perror("gettimeofday");
		exit(EXIT_FAILURE);
	}

	return (end.tv_sec-start.tv_sec) + (end.tv_usec-start.tv_usec)/1000000;
}

int main(int argc, char **argv)
{
	int i;
	int n;
	int ret;
	pid_t pid;
	int mids[3];
	struct timeval start;
	struct acc_motion bothdir;
	struct acc_motion vertical;
	struct acc_motion horizontal;

	if (argc == 2) {
		n = atoi(argv[1]);
	} else {
		printf("Usage: %s <num_childred>\n", *argv);
		exit(EXIT_FAILURE);
	}

	ret = gettimeofday(&start, 0);
	if (ret != 0) {
		perror("gettimeofday");
		exit(EXIT_FAILURE);
	}

	/* CREATE MOTIONS
	 * move this code some place else
	 */
	vertical.dlt_x = 1;
	vertical.dlt_y = 1;
	vertical.dlt_z = 10;
	vertical.frq   = 10;

	horizontal.dlt_x = 10;
	horizontal.dlt_y = 10;
	horizontal.dlt_z = 1;
	horizontal.frq   = 10;

	bothdir.dlt_x = 10;
	bothdir.dlt_y = 10;
	bothdir.dlt_z = 10;
	bothdir.frq   = 10;

	mids[VERTICAL] = syscall(accevt_create, &vertical);
	if (mids[VERTICAL] < 0) {
		perror("accevt_create");
		exit(EXIT_FAILURE);
	}

	mids[HORIZONTAL] = syscall(accevt_create, &horizontal);
	if (mids[HORIZONTAL] < 0) {
		perror("accevt_create");
		exit(EXIT_FAILURE);
	}

	mids[BOTHDIR] = syscall(accevt_create, &bothdir);
	if (mids[BOTHDIR] < 0) {
		perror("accevt_create");
		exit(EXIT_FAILURE);
	}

	for (i = 0; i < n; i++) {
		pid = fork();
		if (pid < 0) {
			perror("fork");
			exit(EXIT_FAILURE);
		} else if (!pid) {
			listen_to(i, mids[i % 3]);
			exit(EXIT_SUCCESS);
		}
	}

	while (1) {
		if (run_time(start) <= 60)
			continue;

		ret = syscall(accevt_destroy, 0);
		if (ret != 0) {
			perror("accevt_destroy");
			exit(EXIT_FAILURE);
		}

		ret = syscall(accevt_destroy, 1);
		if (ret != 0) {
			perror("accevt_destroy");
			exit(EXIT_FAILURE);
		}

		ret = syscall(accevt_destroy, 2);
		if (ret != 0) {
			perror("accevt_destroy");
			exit(EXIT_FAILURE);
		}
		return 0;
	}

	/* wait for all children */
	while (wait(NULL) > 0)
		;

	return 0;
}
