#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/date.h"
#include "user.h"

/*
	Added for hw1
	TODO
		none
*/

// maybe make these static? test later..
int curr_target = -1;
int prev_target = -1;

int timerrate_cmdline(int *hz) {
	int target_ticks = *hz;
	// setting previous tick target (global var)
	if(prev_target == -1)
  		prev_target = 100; // perhaps have a conditional for bochs
  	else
    	prev_target = curr_target;
  	curr_target = target_ticks;

  	return timerrate(hz);
}

int main(int argc, char* argv[]) {
	if(argc < 2) {
		printf(1, "%s\n", "Usage: timerrate [hz]");
		exit();
	}
	int hz = atoi(argv[1]);
	int ret = timerrate_cmdline(&hz);
	printf(1, "Old target: %d \nNew target: %d \nReturn Value: %d\n",prev_target, curr_target, ret);
	exit();
}