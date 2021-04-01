// Intel 8250 serial port (UART).

#include "types.h"
#include "defs.h"
#include "param.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"

// ports
#define COM1    0x3f8
#define COM2    0x2f8

// additional vars copied from console.c
#define BACKSPACE 0x100
static ushort *crt = (ushort*)P2V(0xb8000);  // CGA memory

static int uart;    // is there a uart?

void
uartinit(void)
{
  char *p;

  // Turn off the FIFO
  outb(COM1+2, 0);

  // 9600 baud, 8 data bits, 1 stop bit, parity off.
  outb(COM1+3, 0x80);    // Unlock divisor
  outb(COM1+0, 115200/9600);
  outb(COM1+1, 0);
  outb(COM1+3, 0x03);    // Lock divisor, 8 data bits.
  outb(COM1+4, 0);
  outb(COM1+1, 0x01);    // Enable receive interrupts.

  // If status is 0xFF, no serial port.
  if(inb(COM1+5) == 0xFF)
    return;
  uart = 1;

  // Acknowledge pre-existing interrupt conditions;
  // enable interrupts.
  inb(COM1+2);
  inb(COM1+0);
  ioapicenable(IRQ_COM1, 0);

  // Announce that we're here.
  for(p="xv6...\n"; *p; p++)
    uartputc(*p);
}

// added static for hw3; not sure
static void
uartputc(int c)
{
  // Replacement code that allows for console editing before enter
  // Copied from cgaputc() in console.c
  int pos;

  // Cursor position: col + 80*row.
  outb(COM1, 14);
  pos = inb(COM1+1) << 8;
  outb(COM1, 15);
  pos |= inb(COM1+1);

  if(c == '\n')
    pos += 80 - pos%80;
  else if(c == BACKSPACE){
    if(pos > 0) --pos;
  } else
    crt[pos++] = (c&0xff) | 0x0700;  // black on white

  if(pos < 0 || pos > 25*80)
    panic("pos under/overflow");

  if((pos/80) >= 24){  // Scroll up.
    memmove(crt, crt+80, sizeof(crt[0])*23*80);
    pos -= 80;
    memset(crt+pos, 0, sizeof(crt[0])*(24*80 - pos));
  }

  outb(COM1, 14);
  outb(COM1+1, pos>>8);
  outb(COM1, 15);
  outb(COM1+1, pos);
  crt[pos] = ' ' | 0x0700;

  /*
  This code writes to the COM1 serial port
    First checks that the serial port is not busy

  Removed for some reason ? maybe don't remove ?
  
  int i;
  if(!uart)
    return;
  for(i = 0; i < 128 && !(inb(COM1+5) & 0x20); i++)
    microdelay(10);
  outb(COM1+0, c);
  */

}

static int
uartgetc(void)
{
  if(!uart)
    return -1;
  if(!(inb(COM1+5) & 0x01))
    return -1;
  return inb(COM1+0);
}

/*
uartintr() -> consoleintr() -> uartgetc() -> consputc()
consputc() -> uartputc()
consputc() -> cgaputc()

cgaputc() does what?

either cgaputc() or consoleintr() does the console editing
  cgaputc() is always called at the end of the chain
  Or it could be the default case in consoleintr()
*/
void
uartintr(void)
{
  if(DEBUG)
    printf("%s\n", "UART.c is being called!");
  // Additional mirroring code
  // consoleintr(uartgetc);
}
