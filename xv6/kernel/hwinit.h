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

// This enables all hw edits
#define HW3 false
#define HW4 false
#define HW5 true

#if HW5
#define HW5_pf_handler true // implements page fault handler
#define HW5_userprog false  // implements user test program
#endif

#if HW4
#define HW4_ide2 true // exercise 1; impelments second ide controller
#define HW4_ddn true  // exercise 2; implements disk device nodes
#define HW4_umkfs true	//ex 3: implements user program umkfs
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
#define HW4_debug_df false	// debugging for e2 df user prog
#define HW4_debug_rw false	// debug for e2 ideread/write
#endif // end of debug definitions