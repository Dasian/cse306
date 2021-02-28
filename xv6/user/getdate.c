#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/date.h"
#include "user.h"
#include "kernel/lapic.c"

/* 
  Added for hw1
  Retrieves the current date and time and writes it in the appropriate addr
  Returns 0 on success and -1 otherwise
  TODO 
  	Test and debug this function
  	Add the proper includes
    Change year to 21 century?
  	Figure out how to connect to console
    Write in proc.c??
*/

int main(int argc, char* argv[]) {
	struct rtcdate tmp;
	if(getdate(&tmp)) 
		printf("%s\n", "getdate failed");
	printf("Date: %i/%i/%i Time: %i %i:%i:%i",tmp.month, tmp.day, tmp.year, tmp.hour, tmp.min, tmp.sec);
	exit();
}