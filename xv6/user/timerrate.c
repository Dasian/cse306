#include "kernel/types.h"
#include "kernel/stat.h"
#include "defs.h"
#include "user.h"

/*
	Added for hw1
	TODO
		Figure out how to test and link timerrate func
		func is found in proc.c and sys_timerrate is found in syscall.c
*/

int main(int argc, char* argv[]) {
	if(argc < 2) {
		printf(1, "%s\n", "Usage: timerrate [hz]");
		exit();
	}
	int prev=timerrate(argv[1])
	printf(1, "Old target: %d New target: %d Return Value: %d",prev, *argv[1], prev);
	exit();
}