#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"

/*
	Added for hw1
	TODO
		Figure out how to count ticks
		Figure out how to change the target and countdown var
		Figure out how to connect that to this func
*/
int timerrate(int *hz) {
	return -1;
}

int main(int argc, char* argv[]) {
	if(argc < 2)
		printf("%s\n", "Usage: timerrate [hz]");
	timerrate(argv[1]);
	exit();
}