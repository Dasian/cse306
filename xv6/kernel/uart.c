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
#include "memlayout.h"
#include "hwinit.h"
// #include "user/user.h"

// ports
#define COM1    0x3f8
#define COM2    0x2f8

#if HW3_multiprocessing
// additional vars copied from console.c
#define BACKSPACE 0x100
#define INPUT_BUF 128
#define C(x)  ((x)-'@')  // Control-x

static struct {
  struct spinlock lock;
  int locking;
} com1_cons;

struct inp {
  char buf[INPUT_BUF];
  uint r;  // Read index
  uint w;  // Write index
  uint e;  // Edit index
} input;

void uartputc_driver(int c);

#endif

// static void uartputc();

static int uart;    // is there a uart?
long int com_port; // tells the funcs which port to operate on
//struct inp* input;

// These two funcs are similar to consoleread/write
// Used to define action in devsw table
// These can't be copied from console.c
/*
  read and write need to distinguish btwn
  com1 and com2 through ip->minor
*/
#if HW3_multiprocessing
int uartwrite(struct inode *ip, char *buf, int n) {
  int i;

  iunlock(ip);
  acquire(&com1_cons.lock);
  for(i = 0; i < n; i++)
    uartputc_driver(buf[i] & 0xff);
  #if HW3_DEBUG
  cprintf("uartwrite: minor: %d pid: %d \n", ip->minor, myproc()->pid);
  #endif
  release(&com1_cons.lock);
  ilock(ip);

  return n;
}
/*
  I think the input variable is what is currently written 
  in the console?
  I just copied the input code from console, this file is
    basically a copy
*/
int uartread(struct inode *ip, char *dst, int n) {
  uint target;
  int c;

  iunlock(ip);
  target = n;
  acquire(&com1_cons.lock);
  while(n > 0){
    while(input.r == input.w){
      if(myproc()->killed){
        release(&com1_cons.lock);
        ilock(ip);
        return -1;
      }
      sleep(&input.r, &com1_cons.lock);
    }
    c = input.buf[input.r++ % INPUT_BUF];
    if(c == C('D')){  // EOF
      if(n < target){
        // Save ^D for next time, to make sure
        // caller gets a 0-byte result.
        input.r--;
      }
      break;
    }
    *dst++ = c;
    --n;
    if(c == '\n')
      break;
  }
  #if HW3_DEBUG
  cprintf("uartread: minor: %d pid: %d \n", ip->minor, myproc()->pid);
  #endif
  release(&com1_cons.lock);
  ilock(ip);

  return target - n;
}
#endif

void
uartinit(void)
{
  char *p;
  com_port = COM1; // defaults to com1

  // init code modeled after consoleinit() in consol.c
  #if HW3_multiprocessing
  initlock(&com1_cons.lock, "/com1");

  devsw[COM].write = uartwrite;
  devsw[COM].read = uartread;
  com1_cons.locking = 1;

  //ioapicenable(IRQ_KBD, 0);
  #endif
  // end of copied code

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

  #if HW3_COM2
  outb(COM2+2, 0);

  // 9600 baud, 8 data bits, 1 stop bit, parity off.
  outb(COM2+3, 0x80);    // Unlock divisor
  outb(COM2+0, 115200/9600);
  outb(COM2+1, 0);
  outb(COM2+3, 0x03);    // Lock divisor, 8 data bits.
  outb(COM2+4, 0);
  outb(COM2+1, 0x01);    // Enable receive interrupts.

  // If status is 0xFF, no serial port.
  if(inb(COM2+5) == 0xFF)
    return;
  uart = 1;

  // Acknowledge pre-existing interrupt conditions;
  // enable interrupts.
  inb(COM2+2);
  inb(COM2+0);
  ioapicenable(IRQ_COM2, 0);
  #endif

  // Announce that we're here.
  for(p="xv6...\n"; *p; p++)
    uartputc(*p);
}

void uartputc(int c) {
  int i;
  if(!uart)
    return;
  for(i = 0; i < 128 && !(inb(com_port+5) & 0x20); i++)
    microdelay(10);
  outb(com_port+0, c);
}

#if HW3_multiprocessing
void
uartputc_driver(int c)
{
  if(c == BACKSPACE){
    uartputc('\b'); uartputc(' '); uartputc('\b');
  } 
  else
    uartputc(c);
}
#endif

int
uartgetc(void)
{
  if(!uart)
    return -1;
  if(!(inb(com_port+5) & 0x01))
    return -1;
  return inb(com_port+0);
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
uartintr(int minor)
{
  #if HW3_DEBUG
    //acquire(&com1_cons.lock);
    cprintf("%s %d\n", "uartintr ", minor);
    //release(&com1_cons.lock);
  #endif


  // Additional mirroring code
  #if !HW3_multiprocessing
  consoleintr(uartgetc);
  #endif
  // end of mirroring code

  // These are the contents of consoleintr slightly modified
  #if HW3_multiprocessing
  int c, doprocdump = 0;

  if(minor == 1) {
    com_port = COM1;
   // cprintf("Changing com_port to 1\n");
    //input = &input1;
  }
  else if(minor == 2) {
    com_port = COM2;
    //cprintf("Changing com_port to 2\n");
    //input = &input2;
  }

  acquire(&com1_cons.lock);
  while((c = uartgetc()) >= 0){
    switch(c){
    case C('P'):  // Process listing.
      // procdump() locks cons.lock indirectly; invoke later
      doprocdump = 1;
      break;
    case C('U'):  // Kill line.
      while(input.e != input.w &&
            input.buf[(input.e-1) % INPUT_BUF] != '\n'){
        input.e = input.e -1;
        uartputc_driver(BACKSPACE);
      }
      break;
    case C('H'): case '\x7f':  // Backspace
      if(input.e != input.w){
        input.e = input.e-1;
        uartputc_driver(BACKSPACE);
      }
      break;
    default:
      if(c != 0 && input.e-input.r < INPUT_BUF){
        c = (c == '\r') ? '\n' : c;
        input.buf[input.e % INPUT_BUF] = c;
        input.e = input.e+1;
        uartputc_driver(c);
        if(c == '\n' || c == C('D') || input.e == input.r+INPUT_BUF){
          input.w = input.e;
          wakeup(&input.r);
        }
      }
      break;
    }
  }
  release(&com1_cons.lock);
  if(doprocdump) {
    procdump();  // now call procdump() wo. cons.lock held
  }
  #endif
}
