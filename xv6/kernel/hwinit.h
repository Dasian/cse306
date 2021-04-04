/*
	This file contains C macros that can be used to change xv6
		according to the hw specifications
	Since I can mess up some things in the hw, commenting out
		a HW defintion (HW3 for example) will run xv6 without
		any hw3 edits (only run a single shell instead of 3)
*/

// general debug statements
#define DEBUG 0

// This enables all hw3 edits
#define HW3 

#ifdef HW3

// Commenting out one macro removes all of an exercise's code
// #define HW3_multiprocessing // This is for exercise 1; multiple shells
#define HW3_scheduler 		// This is for exercise 2; cpu scheduling

#ifdef HW3_multiprocessing

#define HW3_COM2 1	// adds com2 generalization code to uart.c
#define HW3_init  1	// multi shell generation in init.c

#endif // end of HW3_multiprocessing conditionals

#ifdef HW3_scheduler



#endif // end of HW3_scheduler conditionals

#endif // end of HW3 definitions