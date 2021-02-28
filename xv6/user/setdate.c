#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/date.h"
#include "user.h"
#include "kernel/lapic.c"

/*
  Added for hw1
  Obtains user info to set the date and time of the system
  Returns 0 on success and -1 otherwise
  TODO
    Check validation ranges
    Check leap years
    Test and debug
    Figure out how to connect to console
    MOVED TO proc.c
*/

int main(int argc, char* argv[]) {
	// run user input checks
	if(argc <= 6)
		printf("%s\n", "usage: setdate [year] [day] [month] [hour] [min] [sec]");

	struct rtcdate tmp;
	tmp.year = *argv[1];
	tmp.day = *argv[2];
	tmp.month = *argv[3];
	tmp.hour = *argv[4];
	tmp.minute = *argv[5];
	tmp.second = *argv[6];

	if(setdate(&tmp))
		printf("%s\n", "setdate failed");
	exit();
}