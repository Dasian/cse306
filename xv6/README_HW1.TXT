**CSE 306 HW1**
David Espiritu & Rocktem Bhowmik

For the purpose of maintaining an organized structure, we will describe our process in separate sections based on the exercises provided. This should allow you to quickly look back to the README should you find any points of interest as you analyze the code. Before proceeding with this README, please ensure that your Makefile in the user folder has the following added to UPROGS:

_getdate\
_setdate\
_timerrate\

The code cannot be tested if this is not fulfilled.

> EXERCISE 1	
Files of Interest: user/getdate.c, user/user.h, user/usys.S, kernel/proc.c, kernel/syscall.h, kernel/syscall.c
Process: We initially made sure to have getdate added to the following: user/user.h, user/usys.S, kernal/syscall.h, and kernal/syscall.h. We then created the getdate.c file in the user folder to contain the main function that will call getdate in kernal/proc.c after given user input. By creating a pointer to a space in memory of size struct rtcdate, we called getdate using the pointer as the argument for the function. We checked to see if the pointer was valid by comparing the size of the pointer to the size of the rtcdate struct and also took advantage of the argptr function using sys_fstat as a point of reference to understand how the function worked. After that we simply called cmostime with the pointer to the rtcdate struct we made earlier as the input which essentially just placed values into the struct based on UTC. As we successfully reached cmostime, we return 0, go back to getdate.c, and print out the output which should present the currently set time. 

> EXERCISE 2
Files of Interest: user/setdate.c, user/user.h, user/usys.S, kernel/proc.c, kernel/syscall.h, kernel/syscall.c
Process: We initially made sure to have setdate added to the following: user/user.h, user/usys.S, kernal/syscall.h, and kernal/syscall.h. We then created the setdate.c file in the user folder to contain the main function that will call setdate in kernal/proc.c after given user input. The main function for setdate takes in the year, day, month, hour, minutes, and seconds in that order. We take these values, place them into a rtcdate struct and send its pointer to the setdate function in proc.c. We perform the same validations done for getdate and then go on to define the fields for the purpose of further validation of input based on date ranges as well as the occurrence of a leap year. Accounting for leap years was simple, we merely tested to see if it was a multiple of four and if that was the case we updated the existing value of our isleap variable to 1 to add said variable to index 1 of our max_day array. Index 1 specifically holds the value of the maximum days present in February of which should increase by 1 during a leap year. We follow this up by checking the ranges of the seconds, minutes, hours, months, and days that the user had input. Then thereafter check to see if the day is greater than the maximum day possible for the month it is in (which should account for leap years since we updated the max_day array of which this if statement checks). We then encoded the base 10 values from the user input to bcd and wrote the values to the registers. Making sure there are no interrupts during the process, we eventually are able to return 0 which should, at the end of it all, have set the date based on the user input. During any checks if the conditions are not met, the function will return a -1 and the console will claim that setdate failed. 

> EXERCISE 3
Files of Interest: user/timerrate.c, kernel/lapic.c, user/user.h, user/usys.S, kernel/syscall.h, kernel/syscall.c
Process: We initially made sure to have timerrate added to the following: user/user.h, user/usys.S, kernal/syscall.h, and kernal/syscall.h. We then created the timerrate.c file in the user folder to contain the main function that will call timerrate in kernal/lapic.c after given user input. The main function in timerrate.c takes the target hz as a user input and applies the pointer to that value into the timerrate_cmdline function. We then set the target_ticks to be the user inputted value and then call the timerrate function written in lapic.c. After initializing some variables to use later in the function within timerrate, we eventually check to see if the input value is within the range of [1,1000] ticks and perform the same test to check if the user space is proper. If either check is failed, we return -1 and account for the failed case. We then move into a while loop wherein we get the current ticks per second and place it into a variable called current_ticks. We get the current ticks by getting the initial time using cmostime and once we get the initial time we wait one second wherein we get the number of ticks since the computer started, then wait an additional second to get the number of ticks once again. Thereafter we subtract both of the recorded values to obtain the number of the current ticks. Once we do so, we get the ratio of the current ticks versus target ticks, stored as a double. If the ratio is in between the range of 0.9 and 1.1, we step, i.e., do a linear search where we check to see if the current ticks are 5% larger or smaller. Once we do that, we either increment or decrement the countdown by a variable increment that is set at 100000 that is multiplied by the number of ticks away that the current number is from the target. With the number, depending on the condition, we either add or subtract it from the countdown. When we encounter a tick within 5% of the target, we increment the number of successes (which we must reach 5 to exit the loop). And in the case the ratio is not in range, we have the new countdown equal to the old countdown times the ratio. This repeats until the loop ends at which point we record both the new and old target values and print out the results of the function.

> How to Compile
To run any of the functions made in the exercises mentioned above, make sure to use either Bochs or QEMU. We used QEMU exclusively for most of this assignment so that is what we would suggest. Open a terminal in the top level directory of the xv6 folder and start QEMU:

make qemu-nox

For the sake of tests involving Exercise 3 please do the following instead:

make CPUS=1 qemu-nox

You can then do the following tests:

Input: 
getdate
Output: 
	Date: /* Current date based on UTC should appear */ Time: /*Current UTC Time*/

Input:
	setdate 2000 29 02 12 59 59
	getdate
Output:
	Date: 2/29/2000 Time: \*Time a few seconds after 12:59:59*\

Input: 
	setdate 2001 29 02 12 59 59
Output:
	setdate failed

Input:
	setdate 2019 31 12 23 59 59
	getdate
Output:
	Date: 1/1/2020 Time: \*Time a few seconds into 2020*\

Input:
	setdate 1
Output:
	usage: setdate [year] [day] [month] [hour] [min] [sec]

Input:
	setdate this is a test for strings
Output: 
	setdate failed

Input:
	timerrate -1
Output:
	Incorrect range [1-1000] Return Value: -1

// The following outputs may appear differently for you, in these cases you’ll see sample outputs of what appears on one of our computers.

Input:
	Timerrate 256
Output:
	Current tps: 100 Target tps: 256 Old LAPIC: 10000000 New LAPIC: 3906250
	Old Target: 100
	New Target: 256
	Return Value: 0

Input:
	Timerrate 8
Output:
	Current tps: 256 Target tps: 8 Old LAPIC: 3906250 New LAPIC: 125000000
	Old Target: 256
	New Target: 8
	Return Value: 0

