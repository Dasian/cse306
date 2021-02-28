#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/date.h"
#include "user.h"
// #include "kernel/defs.h"

/* 
  Added for hw1
  Retrieves the current date and time and writes it in the appropriate addr
  Returns 0 on success and -1 otherwise
*/

int main(int argc, char* argv[]) {
	struct rtcdate *tmp = malloc(sizeof(struct rtcdate));
	if(getdate(tmp)) 
		printf(1, "%s\n", "getdate failed");
	printf(1, "Date: %d/%d/%d Time:  %d:%d:%d\n",tmp -> month, tmp -> day, tmp -> year, tmp -> hour, tmp -> minute, tmp -> second);
	free(tmp);
	exit();
}