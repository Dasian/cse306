/*
	This file contains C macros that can be used to change xv6
		according to the hw specifications
	Since I can mess up some things in the hw, setting a HW 
		defintion (HW3 for example) to false will run xv6
		without any hw3 edits. Each exercise and parts within
		the exercise are also specified.
*/

#define true 1
#define false 0

// enables all debug statements
#define DEBUG false

// This enables all hw3 edits
#define HW3 false
#define HW4 true

#if HW4
#define HW4_ide2 true // exercise 1; impelments second ide controller
// HW4_ddn can't be true at the same time as HW3 init since they overlap
#define HW4_ddn true  // exercise 2; implements disk device nodes
#endif

#if HW3
#define HW3_multiprocessing true // This is for exercise 1; multiple shells
#define HW3_scheduler 		true // This is for exercise 2; cpu scheduling
// Specific sections in exercise 1
#if HW3_multiprocessing
#define HW3_COM2 true	// adds com2 generalization code to uart.c
#define HW3_init true	// multi shell generation in init.c
#endif // end of HW3_multiprocessing conditionals
// Specific sections in exercise 2
#if HW3_scheduler
#define HW3_cpu_util true // adds cpu measurements and printout
#endif // end of HW3_scheduler definitons
#endif // end of HW3 definitions


#if DEBUG
#define HW3_debug false		 // debugging for exercise 1
#define HW3_debug_sched false // debugging for exercise 2
#endif // end of debug definitions