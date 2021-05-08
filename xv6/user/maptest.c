#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"

#include "kernel/fs.h"
#include "kernel/spinlock.h"
#include "kernel/sleeplock.h"
#include "kernel/file.h"
#include "kernel/proc.h"

#include "kernel/hwinit.h"
#include "user.h"


/*
  Added for hw5
  
  Shows the functionality of mmap and munmap (syscalls 28 and 29)
*/

int main(int argc, char* argv[]) {

	#if !HW5_userprog
	printf(1, "%s\n", "This function has been turned off due to a flag in kernel/hwinit.h");
	exit();
	#endif

	#if HW5_userprog
	// 
	#endif
}