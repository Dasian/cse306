#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/date.h"
#include "user.h"
#include "kernel/lapic.c"

/* 
  Added for hw1
  Returns x in bcd format
  ex 12 -> 0001 0010
*/
uint tobcd(uint x) {
  uint conv = 0, i=0;
  while(x > 0){
    conv += (x%10 << 4*i);
    x /= 10;
    i++;
  }
  return conv;
}

/*
  Added for hw1
  Obtains user info to set the date and time of the system
  Returns 0 on success and -1 otherwise
  TODO
    Check validation ranges
    Check leap years
    Figure out the proper register numbers to write into
    Test and debug
*/
int setdate(struct rtcdate *r) {
  // validate pointer
  if(sizeof(*r) != sizeof(struct rtcdate))
    return -1;
  if(argptr(1, (void*) &r, sizeof(struct rtcdate)) != 0)
    return -1;
  if(sys_fstat() == -1)
    return -1;

  // validate pointer fields
  uint second, minute, hour, month, day, year;
  int isleap = 0;
  uint max_day[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

  if((second=r->second) < 0 || r->second >= 60)   return -1;
  if((minute=r->minute) < 0 || r->minute >= 60)   return -1;
  if((hour=r->hour) < 0 || r->hour >= 23)         return -1;
  if((month=r->month) <=0 || month > 12)          return -1;
  if((day=r->day) <= 0 || day > 31)               return -1;
  if(!isleap && (day > max_day[month-1]))         return -1;
  if(isleap && month == 2 && day > 29)	s		  return -1; // leap year check

  // encode base 10 values into binary code decimal (bcd)
  second = tobcd(second);
  minute = tobcd(minute);
  hour = tobcd(hour);
  month = tobcd(month);
  day = tobcd(day);
  year = tobcd(r->year);

  // prevent interrupts
  pushcli();

  // write values to the registers
  outb(0x70, 1<<7 | YEAR);
  outb(0x71, year);
  outb(0x70, 1<<7 | MONTH);
  outb(0x71, MONTH);
  outb(0x70, 1<<7 | DAY);
  outb(0x71, day);
  outb(0x70, 1<<7 | HOURS);
  outb(0x71, hour);
  outb(0x70, 1<<7 | MINS);
  outb(0x71, minute);
  outb(0x70, 1<<7 | SECS);
  outb(0x71, second);


  popcli();

  return 0;
}

int main(int argc, char* argv[]) {
	// run user input checks
	if(argc <= 6)
		printf("%s\n", "usage: setdate [year] [day] [month] [hour] [min] [sec]");

	struct rtcdate tmp;
	tmp.year = *argv[1];
	tmp.day = *argv[2];
	tmp.month = *argv[3];
	tmp.hour = *argv[4];
	tmp.minute = *argv[5];
	tmp.second = *argv[6];

	if(setdate(&tmp))
		printf("%s\n", "setdate failed");
	exit();
}