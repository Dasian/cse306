#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/date.h"
#include "user.h"

/*
  Added for hw1
  Obtains user info to set the date and time of the system
  Returns 0 on success and -1 otherwise
  TODO
    none
*/

int main(int argc, char* argv[]) {
	// run user input checks
	if(argc <= 6)
		printf(1, "%s\n", "usage: setdate [year] [day] [month] [hour] [min] [sec]");
	else{
		struct rtcdate tmp;
		tmp.year = atoi(argv[1]);
		tmp.day = atoi(argv[2]);
		tmp.month = atoi(argv[3]);
		tmp.hour = atoi(argv[4]);
		tmp.minute = atoi(argv[5]);
		tmp.second = atoi(argv[6]);

		if(setdate(&tmp))
			printf(1, "%s\n", "setdate failed");
	}
	exit();
}