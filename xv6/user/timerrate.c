#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"

/*
	Added for hw1
*/
int timerrate(int *hz) {
	return -1;
}

int main(int argc, char* argv[]) {
	timerrate(argv[1]);
	exit();
}