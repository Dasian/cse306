#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/date.h"
#include "kernel/defs.h"
// #include "user.h"

/*
	Added for hw1
	TODO
		Figure out how to test and link timerrate func
		func is found in proc.c and sys_timerrate is found in syscall.c
  		Create a cmdline function that
  		returns the prev target ticks value
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
	int ret = timerrate_cmdline((int*) argv[1]);
	printf(1, "Old target: %d New target: %d Return Value: %d",prev_target, curr_target, ret);
	exit();
}