#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include "acceleration.h"

int main(int argc, char **argv)
{
	int result = syscall(378, NULL, NULL);

	printf("%d\n", result);

	return 0;
}

