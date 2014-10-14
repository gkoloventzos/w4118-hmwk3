#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <syscall.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/types.h>
#include "accevt.h"

#define VERTICAL	0
#define HORIZONTAL	1
#define BOTHDIR		2

#define accevt_create	379
#define accevt_wait	380
#define accevt_destroy	382

static void print_motion(int dir)
{
	if (dir == VERTICAL)
		printf("%ld detected a vertical shake\n", (long) getpid());
	else if (dir == HORIZONTAL)
		printf("%ld detected a horizontal shake\n", (long) getpid());
	else if (dir == BOTHDIR)
		printf("%ld detected a shake\n", (long) getpid());
	else
		printf("something went wrong...%d\n", dir);
}

/*
 * listens to specific 'dir' shake motion
 * for the given child
 */
static void listen_to(int event_id, int dir)
{
	int ret;

	while (1) {
		printf("GOING TO WAIIT\n");
		ret = syscall(accevt_wait, event_id);
		printf("WOKEN UP\n");
		if (ret != 0)
			return;
		print_motion(dir);
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
	int err;
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
	vertical.dlt_z = 50;
	vertical.frq   = 20;

	horizontal.dlt_x = 30;
	horizontal.dlt_y = 1;
	horizontal.dlt_z = 1;
	horizontal.frq   = 20;

	bothdir.dlt_x = 10;
	bothdir.dlt_y = 10;
	bothdir.dlt_z = 30;
	bothdir.frq   = 20;
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
			listen_to(mids[i % 3], i % 3);
			exit(EXIT_SUCCESS);
		}
	}

	err = 0;
	while (1) {
		if (run_time(start) <= 60)
			continue;
		ret = syscall(accevt_destroy, 0);
		if (ret != 0) {
			err = ret;
			perror("accevt_destroy");
		}
		ret = syscall(accevt_destroy, 1);
		if (ret != 0) {
			err = ret;
			perror("accevt_destroy");
		}

		ret = syscall(accevt_destroy, 2);
		if (ret != 0) {
			err = ret;
			perror("accevt_destroy");
		}
		if (err)
			return err;
	}

	/* wait for all children */
	while ((ret = wait(NULL)) == -1 && errno == EINTR)
		;

	return 0;
}
