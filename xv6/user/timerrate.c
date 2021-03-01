#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/date.h"
#include "kernel/defs.h"
// #include "user.h"

/*
	Added for hw1
	TODO
		Figure out how to test and link timerrate func
		Can't run any of this
		Can't make things so can't test
		Logic is all set, current code is in lapic.c but might need to
			be in proc.c can't figure that out yet
		Make fil???
		Still need to add address pointer checks in timerrate()
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