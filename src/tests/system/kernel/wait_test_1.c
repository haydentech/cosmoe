/*
 * Copyright 2007, Jérôme Duval. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>


/*!
	wait()/waitpid() should return -1 and set errno to ECHILD, since there
	are no children to wait for.
*/
int
main()
{
	int childStatus;
	pid_t pid = wait(&childStatus);
	printf("wait() returned %d (%s)\n", pid, strerror(errno));

	pid = waitpid(-1, &childStatus, 0);
	printf("waitpid(-1, ...) returned %d (%s)\n", pid, strerror(errno));

	pid = waitpid(0, &childStatus, 0);
	printf("waitpid(0, ...) returned %d (%s)\n", pid, strerror(errno));

	pid = waitpid(getpgrp(), &childStatus, 0);
	printf("waitpid(%d, ...) returned %d (%s)\n", getpgrp(), pid, strerror(errno));

	return 0;
}

