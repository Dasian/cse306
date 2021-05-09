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

	// All of these require multi process execution
	// By completing these it is implied that the same
	// operations work on a single proc execution

	// Show COW execution:
	// Show contents of table of proc 1 (parent)
	// Fork and show contents of table for proc 2 (child)
	// These should be the same
	// Have proc 2 (child) write
	// Show the contents of table for both procs
	// They should be different
	
	// Show private files:
	// Both load the same file
	// proc 1 writes to the file
	// Both print file
	// Should be different

	// Show shared memory:
	// Allocate shared anon mem
	// Have proc 1 write
	// Have proc 2 read
	// Should show data written by proc 1

	// Show private memory:
	// Both allocate private memory
	// Both write to it
	// Both read it
	// Should be different
	#endif
}