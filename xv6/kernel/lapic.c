// The local APIC manages internal (non-I/O) interrupts.
// See Chapter 8 & Appendix C of Intel processor manual volume 3.

#include "param.h"
#include "types.h"
#include "defs.h"
#include "date.h"
#include "memlayout.h"
#include "traps.h"
#include "mmu.h"
#include "x86.h"

// Local APIC registers, divided by 4 for use as uint[] indices.
#define ID      (0x0020/4)   // ID
#define VER     (0x0030/4)   // Version
#define TPR     (0x0080/4)   // Task Priority
#define EOI     (0x00B0/4)   // EOI
#define SVR     (0x00F0/4)   // Spurious Interrupt Vector
  #define ENABLE     0x00000100   // Unit Enable
#define ESR     (0x0280/4)   // Error Status
#define ICRLO   (0x0300/4)   // Interrupt Command
  #define INIT       0x00000500   // INIT/RESET
  #define STARTUP    0x00000600   // Startup IPI
  #define DELIVS     0x00001000   // Delivery status
  #define ASSERT     0x00004000   // Assert interrupt (vs deassert)
  #define DEASSERT   0x00000000
  #define LEVEL      0x00008000   // Level triggered
  #define BCAST      0x00080000   // Send to all APICs, including self.
  #define BUSY       0x00001000
  #define FIXED      0x00000000
#define ICRHI   (0x0310/4)   // Interrupt Command [63:32]
#define TIMER   (0x0320/4)   // Local Vector Table 0 (TIMER)
  #define X1         0x0000000B   // divide counts by 1
  #define PERIODIC   0x00020000   // Periodic
#define PCINT   (0x0340/4)   // Performance Counter LVT
#define LINT0   (0x0350/4)   // Local Vector Table 1 (LINT0)
#define LINT1   (0x0360/4)   // Local Vector Table 2 (LINT1)
#define ERROR   (0x0370/4)   // Local Vector Table 3 (ERROR)
  #define MASKED     0x00010000   // Interrupt masked
#define TICR    (0x0380/4)   // Timer Initial Count
#define TCCR    (0x0390/4)   // Timer Current Count
#define TDCR    (0x03E0/4)   // Timer Divide Configuration

volatile uint *lapic;  // Initialized in mp.c

//PAGEBREAK!
static void
lapicw(int index, int value)
{
  lapic[index] = value;
  lapic[ID];  // wait for write to finish, by reading
}

void
lapicinit(void)
{
  if(!lapic)
    return;

  int countdown = 10000000;
  // int target = 100;

  // Enable local APIC; set spurious interrupt vector.
  lapicw(SVR, ENABLE | (T_IRQ0 + IRQ_SPURIOUS));

  // The timer repeatedly counts down at bus frequency
  // from lapic[TICR] and then issues an interrupt.
  // If xv6 cared more about precise timekeeping,
  // TICR would be calibrated using an external time source.
  lapicw(TDCR, X1);
  lapicw(TIMER, PERIODIC | (T_IRQ0 + IRQ_TIMER));
  lapicw(TICR, countdown);

  // Disable logical interrupt lines.
  lapicw(LINT0, MASKED);
  lapicw(LINT1, MASKED);

  // Disable performance counter overflow interrupts
  // on machines that provide that interrupt entry.
  if(((lapic[VER]>>16) & 0xFF) >= 4)
    lapicw(PCINT, MASKED);

  // Map error interrupt to IRQ_ERROR.
  lapicw(ERROR, T_IRQ0 + IRQ_ERROR);

  // Clear error status register (requires back-to-back writes).
  lapicw(ESR, 0);
  lapicw(ESR, 0);

  // Ack any outstanding interrupts.
  lapicw(EOI, 0);

  // Send an Init Level De-Assert to synchronise arbitration ID's.
  lapicw(ICRHI, 0);
  lapicw(ICRLO, BCAST | INIT | LEVEL);
  while(lapic[ICRLO] & DELIVS)
    ;

  // Enable interrupts on the APIC (but not on the processor).
  lapicw(TPR, 0);
}

int
lapicid(void)
{
  if (!lapic)
    return 0;
  return lapic[ID] >> 24;
}

// Acknowledge interrupt.
void
lapiceoi(void)
{
  if(lapic)
    lapicw(EOI, 0);
}

// Spin for a given number of microseconds.
// On real hardware would want to tune this dynamically.
void
microdelay(int us)
{
}

#define CMOS_PORT    0x70
#define CMOS_RETURN  0x71

// Start additional processor running entry code at addr.
// See Appendix B of MultiProcessor Specification.
void
lapicstartap(uchar apicid, uint addr)
{
  int i;
  ushort *wrv;

  // "The BSP must initialize CMOS shutdown code to 0AH
  // and the warm reset vector (DWORD based at 40:67) to point at
  // the AP startup code prior to the [universal startup algorithm]."
  outb(CMOS_PORT, 0xF);  // offset 0xF is shutdown code
  outb(CMOS_PORT+1, 0x0A);
  wrv = (ushort*)P2V((0x40<<4 | 0x67));  // Warm reset vector
  wrv[0] = 0;
  wrv[1] = addr >> 4;

  // "Universal startup algorithm."
  // Send INIT (level-triggered) interrupt to reset other CPU.
  lapicw(ICRHI, apicid<<24);
  lapicw(ICRLO, INIT | LEVEL | ASSERT);
  microdelay(200);
  lapicw(ICRLO, INIT | LEVEL);
  microdelay(100);    // should be 10ms, but too slow in Bochs!

  // Send startup IPI (twice!) to enter code.
  // Regular hardware is supposed to only accept a STARTUP
  // when it is in the halted state due to an INIT.  So the second
  // should be ignored, but it is part of the official Intel algorithm.
  // Bochs complains about the second one.  Too bad for Bochs.
  for(i = 0; i < 2; i++){
    lapicw(ICRHI, apicid<<24);
    lapicw(ICRLO, STARTUP | (addr>>12));
    microdelay(200);
  }
}

#define CMOS_STATA   0x0a
#define CMOS_STATB   0x0b
#define CMOS_UIP    (1 << 7)        // RTC update in progress

#define SECS    0x00
#define MINS    0x02
#define HOURS   0x04
#define DAY     0x07
#define MONTH   0x08
#define YEAR    0x09

static uint cmos_read(uint reg)
{
  outb(CMOS_PORT,  reg);
  microdelay(200);

  return inb(CMOS_RETURN);
}

static void fill_rtcdate(struct rtcdate *r)
{
  r->second = cmos_read(SECS);
  r->minute = cmos_read(MINS);
  r->hour   = cmos_read(HOURS);
  r->day    = cmos_read(DAY);
  r->month  = cmos_read(MONTH);
  r->year   = cmos_read(YEAR);
}

// qemu seems to use 24-hour GWT and the values are BCD encoded
void cmostime(struct rtcdate *r)
{
  struct rtcdate t1, t2;
  int sb, bcd;

  sb = cmos_read(CMOS_STATB);

  bcd = (sb & (1 << 2)) == 0;

  // make sure CMOS doesn't modify time while we read it
  for(;;) {
    fill_rtcdate(&t1);
    if(cmos_read(CMOS_STATA) & CMOS_UIP)
        continue;
    fill_rtcdate(&t2);
    if(memcmp(&t1, &t2, sizeof(t1)) == 0)
      break;
  }

  // convert
  if(bcd) {
#define    CONV(x)     (t1.x = ((t1.x >> 4) * 10) + (t1.x & 0xf))
    CONV(second);
    CONV(minute);
    CONV(hour  );
    CONV(day   );
    CONV(month );
    CONV(year  );
#undef     CONV
  }

  *r = t1;
  r->year += 2000;
}

/*
  Added for hw1
  Returns the total number of ticks in one second
  tmp move all of this from lapic.c to proc.c
*/
uint tps() {

  // get init time
  struct rtcdate init_time;
  struct rtcdate time;
  cmostime(&init_time);

  // wait until the next second is reached
  //  so we don't start counting in the middle of a sec
  do {
    cmostime(&time);
  }while(time.second == init_time.second);

  // get first sysuptime (number of ticks since start)
  acquire(&tickslock);
  uint ticks1 = ticks;
  release(&tickslock);

  // wait for a second here
  do {
    cmostime(&init_time);
  }while(init_time.second == time.second);

  // get second sysuptime
  acquire(&tickslock);
  uint ticks2 = ticks;
  release(&tickslock);

  // subtract both tick times to get num of ticks in 1 sec
  return ticks2 - ticks1;
}

static int countdown = -1;
static int prev_target = -1;
/*
  Added for hw1
  Changes the num of ticks per sec to hz
  Returns 0 on success -1 otherwise
  TODO
    Test and debug
*/
int timerrate(int *hz) {
  // countdown, prev target ticks, and curr target ticks
  // are global vars in this file
  int target_ticks = *hz;
  uint curr_ticks = 0; // used to track num ticks per sec
  int successes = 0;
  if(countdown < 0)
    countdown = 10000000;
  int increment; // changes the increment amount of countdown
  double ratio;
  int new_countdown = -1;

  // can be changed to support different emulators
  //  but idk what the boolean var is
  increment = 100000;

  // Function checks; range, user space
  if(target_ticks < 1 || target_ticks > 1000) return -1;
  // do a userspace check here
  if(argptr(0, (void*) &hz, sizeof(int)) != 0)
    return -1;

  // Constantly modify countdown/update curr
  // Break when actual num of ticks is in the target range
  //  5 total times
  while(successes < 5) {
    // Find the current number of ticks per second (tps)
    curr_ticks = tps();

    /*
      maybe take the percentage of curr_ticks compared to target_ticks?

      multiply that percentage by the current countdown and once the 
      threshold is [90, 110]% then proceed with the current system?

      ratio = curr_ticks/target_ticks
      if ratio >=90 && ratio <= 110 -> do normal
      else
        if curr >> target -> make countdown *= ratio
        if curr << target -> make countdown /= ratio
    */

    ratio = ((double) curr_ticks) / ((double) target_ticks);
    if(ratio >= .9 && ratio <= 1.1) {
      // Change countdown according to curr_ticks value
      if(curr_ticks > target_ticks*(1.05)) {
        // make countdown larger to try to slow it down
        new_countdown = countdown + increment*(curr_ticks - target_ticks);
        cprintf("Current tps: %d Target tps: %d Old LAPIC: %d New LAPIC: %d\n",
          curr_ticks, target_ticks, countdown, new_countdown);
        countdown = new_countdown;
        lapicw(0x0380/4, countdown); // number is TICW in lapic.c
      }
      else if(curr_ticks < target_ticks*(.95)) {
        // make countdown smaller to try to speed it up
        new_countdown = countdown - increment/5*(target_ticks - curr_ticks);
        if(new_countdown < 0) {
          new_countdown = 10000000;
          cprintf("Countdown is negative, resetting\n");
        }
        cprintf("Current tps: %d Target tps: %d Old LAPIC: %d New LAPIC: %d\n",
          curr_ticks, target_ticks, countdown, new_countdown);
        countdown = new_countdown;
        lapicw(0x0380/4, countdown); // number is TICW in lapic.c
      }
      else
        successes++;
    }
    else {
      new_countdown = countdown * ratio;
      cprintf("Current tps: %d Target tps: %d Old LAPIC: %d New LAPIC: %d\n",
        curr_ticks, target_ticks, countdown, new_countdown);
      countdown = new_countdown;
      lapicw(0x0380/4, countdown); // number is TICW in lapic.c

    }
    if(countdown < 0)
      countdown = 10000000;
  }

  if(prev_target < 0) {
    prev_target = 100;
  }
  *hz = prev_target;
  prev_target = target_ticks;
  return 0;
} 