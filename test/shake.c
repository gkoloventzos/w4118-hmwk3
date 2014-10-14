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
#define HORIZONTALX	1
#define HORIZONTALY	2
#define BOTHDIR		3

#define accevt_create	379
#define accevt_wait		380
#define accevt_destroy	382

/*
 * give a motion 'dir' print
 * that the current process detected
 * the given motion
 */
static void print_motion(int dir)
{
	if (dir == VERTICAL)
		printf("%ld detected a vertical shake\n", (long) getpid());
	else if (dir == HORIZONTALX || dir == HORIZONTALY)
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
		ret = syscall(accevt_wait, event_id);
		/* if wait fails (i.e. nothing to wait for), return */
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
	int mids[4];
	struct timeval start;
	struct acc_motion bothdir;
	struct acc_motion vertical;
	struct acc_motion horizontal_x;
	struct acc_motion horizontal_y;

	err = 0;
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

	/*
	 * define custom motions for vertical,
	 * horizontal x & y and both directions
	 */
	vertical.dlt_x = 1;
	vertical.dlt_y = 1;
	vertical.dlt_z = 50;
	vertical.frq   = 15;

	horizontal_x.dlt_x = 30;
	horizontal_x.dlt_y = 1;
	horizontal_x.dlt_z = 1;
	horizontal_x.frq   = 15;

	horizontal_y.dlt_x = 1;
	horizontal_y.dlt_y = 30;
	horizontal_y.dlt_z = 1;
	horizontal_y.frq   = 15;

	bothdir.dlt_x = 10;
	bothdir.dlt_y = 10;
	bothdir.dlt_z = 30;
	bothdir.frq   = 15;

	/* create the motions defined above, in the kernel */
	mids[VERTICAL] = syscall(accevt_create, &vertical);
	if (mids[VERTICAL] < 0) {
		perror("accevt_create");
		exit(EXIT_FAILURE);
	}

	mids[HORIZONTALX] = syscall(accevt_create, &horizontal_x);
	if (mids[HORIZONTALX] < 0) {
		perror("accevt_create");
		exit(EXIT_FAILURE);
	}

	mids[HORIZONTALY] = syscall(accevt_create, &horizontal_y);
	if (mids[HORIZONTALY] < 0) {
		perror("accevt_create");
		exit(EXIT_FAILURE);
	}

	mids[BOTHDIR] = syscall(accevt_create, &bothdir);
	if (mids[BOTHDIR] < 0) {
		perror("accevt_create");
		exit(EXIT_FAILURE);
	}

	/*
	 * fork n children, where the i-th child
	 * listens for the i % 4-th motion event
	 */
	for (i = 0; i < n; i++) {
		pid = fork();
		if (pid < 0) {
			perror("fork");
			exit(EXIT_FAILURE);
		} else if (!pid) {
			listen_to(mids[i % 4], i % 4);
			exit(EXIT_SUCCESS);
		}
	}

	err = 0;
	while (1) {
		/* loop and do nothing for 60 seconds */
		if (run_time(start) <= 60)
			continue;
		/* start children cleanup, by destroying each motion event */
		ret = syscall(accevt_destroy, mids[VERTICAL]);
		if (ret != 0) {
			err = ret;
			perror("accevt_destroy: VERTICAL");
		}
		ret = syscall(accevt_destroy, mids[HORIZONTALX]);
		if (ret != 0) {
			err = ret;
			perror("accevt_destroy: HORIZONTALX");
		}
		ret = syscall(accevt_destroy, mids[HORIZONTALY]);
		if (ret != 0) {
			err = ret;
			perror("accevt_destroy: HORIZONTALY");
		}
		ret = syscall(accevt_destroy, mids[BOTHDIR]);
		if (ret != 0) {
			err = ret;
			perror("accevt_destroy: BOTHDIR");
		}
		break;
	}
	/* wait for all children */
	while (wait(NULL) > 0)
		;

	return err;
}
