#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
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
	int ret;

	while (1) {
		/* accevt_wait */
		ret = syscall(380, dir);
		if (ret != 0)
			return;
		print_motion(child, dir);
	}
}

/*
 * returns number of seconds that
 * passed from the 'start' until present
 */
int run_time(struct timeval start)
{
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
	int mids[3];
	struct timeval start;
	struct acc_motion bothdir;
	struct acc_motion vertical;
	struct acc_motion horizontal;

	if (argc == 2) {
		n = atoi(argv[1]);
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

	/* CREATE MOTIONS
	 * move this code some place else
	 */
	vertical.dlt_x = 10;
	vertical.dlt_y = 10;
	vertical.dlt_z = 100;
	vertical.frq   = 100;

	horizontal.dlt_x = 100;
	horizontal.dlt_y = 100;
	horizontal.dlt_z = 10;
	horizontal.frq   = 100;

	bothdir.dlt_x = 100;
	bothdir.dlt_y = 100;
	bothdir.dlt_z = 100;
	bothdir.frq   = 100;

	/* accevt_create */
	mids[0] = syscall(379, vertical);
	if (mids[0] < 0) {
		perror("accevt_create");
		exit(EXIT_FAILURE);
	}

	mids[1] = syscall(379, horizontal);
	if (mids[1] < 0) {
		perror("accevt_create");
		exit(EXIT_FAILURE);
	}

	mids[2] = syscall(379, bothdir);
	if (mids[2] < 0) {
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
		if (run_time(start) > 60) {
			/* accevt_destroy */
			ret = syscall(382, 0);
			if (ret != 0) {
				perror("accevt_destroy");
				exit(EXIT_FAILURE);
			}

			ret = syscall(382, 1);
			if (ret != 0) {
				perror("accevt_destroy");
				exit(EXIT_FAILURE);
			}

			ret = syscall(382, 2);
			if (ret != 0) {
				perror("accevt_destroy");
				exit(EXIT_FAILURE);
			}
			return 0;
		}
	}

	/* wait for all children */
	while (wait(NULL) > 0)
		;

	return 0;
}
