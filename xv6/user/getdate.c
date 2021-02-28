// Driver for getdate()
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"

/* Added for hw1
  Retrieves the current date and time and writes it in the appropriate addr
  Returns 0 on success and -1 otherwise
  TODO test and debug this function
*/
int getdate(struct rtcdate *r) {
  
  // checks if pointer is valid
  if(sizeof(*r) != sizeof(struct rtcdate))
    return -1;
  if(argptr(1, (void*) &r, sizeof(struct rtcdate)) != 0)
    return -1;
  if(sys_fstat() == -1)
    return -1;

  // retrieves date and time and writes it into r
  cmostime(r);
  return 0;

}

int main(int argc, char* argv[]) {
	exit();
}