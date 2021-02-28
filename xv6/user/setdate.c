// Driver for setdate()
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"
#include "kernel/syscall.h"
#include "kernel/syscall.c"

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
    Figure out how to disable all interrupts and write to registers
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
  uint max_day[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

  if((second=r->second) < 0 || r->second >= 60)   return -1;
  if((minute=r->minute) < 0 || r->minute >= 60)   return -1;
  if((hour=r->hour) < 0 || r->hour >= 23)         return -1;
  if((month=r->month) <=0 || month > 12)          return -1;
  if((day=r->day) <= 0 || day > 31)               return -1;
  if(day > max_day[month-1])                      return -1;

  // encode base 10 values into binary code decimal (bcd)
  second = tobcd(second);
  minute = tobcd(minute);
  hour = tobcd(hour);
  month = tobcd(month);
  day = tobcd(day);
  year = tobcd(r->year);

  uint time[6] = {second, minute, hour, month, day, year};

  // prevent interrupts
  pushcli();

  // write values to the registers
  for(int i=0; i<6; i++) {
    outb(0x70, 1<<7 | i);
    outb(0x71, time[i]);
  }

  popcli();

  return 0;
}

int main(int argc, char* argv[]) {
	exit();
}