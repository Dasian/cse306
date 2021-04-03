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
// #include "user/user.h"

// ports
#define COM1    0x3f8
#define COM2    0x2f8

#ifdef HW3
// additional vars copied from console.c
#define BACKSPACE 0x100
// static ushort *crt = (ushort*)P2V(0xb8000);  // CGA memory
static struct {
  struct spinlock lock;
  int locking;
} com1_cons;
#define INPUT_BUF 128
struct {
  char buf[INPUT_BUF];
  uint r;  // Read index
  uint w;  // Write index
  uint e;  // Edit index
} input;

#define C(x)  ((x)-'@')  // Control-x
#endif

// static void uartputc();

static int uart;    // is there a uart?

// These two funcs are similar to consoleread/write
// Used to define action in devsw table
// These can't be copied from console.c
/*
  read and write need to distinguish btwn
  com1 and com2 through ip->minor
*/
#ifdef HW3
int uartwrite(struct inode *ip, char *buf, int n) {
  int i;

  iunlock(ip);
  acquire(&com1_cons.lock);
  for(i = 0; i < n; i++)
    uartputc(buf[i] & 0xff);
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
  release(&com1_cons.lock);
  ilock(ip);

  return target - n;
}
#endif
void
uartinit(void)
{
  char *p;

  // init code modeled after consoleinit() in consol.c
  #ifdef HW3
  initlock(&com1_cons.lock, "com1_console");

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

  // Announce that we're here.
  for(p="xv6...\n"; *p; p++)
    uartputc(*p);
}

// added static for hw3; not sure
void
uartputc(int c)
{
  // Replacement code that allows for console editing before enter
  // Copied from cgaputc() in console.c
  /*
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
  */

  /*
  This code writes to the COM1 serial port
    First checks that the serial port is not busy

  Removed for some reason ? maybe don't remove ?
*/
  int i;
  if(!uart)
    return;
  for(i = 0; i < 128 && !(inb(COM1+5) & 0x20); i++)
    microdelay(10);
  outb(COM1+0, c);
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
  if(0)
    cprintf(" %s\n", "(Entering uartintr)");

  // Additional mirroring code
  #ifndef HW3
  consoleintr(uartgetc);
  #endif
  // end of mirroring code

  // These are the contents of consoleintr slightly modified
  #ifdef HW3
  int c, doprocdump = 0;

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
        input.e--;
        uartputc(BACKSPACE);
      }
      break;
    case C('H'): case '\x7f':  // Backspace
      if(input.e != input.w){
        input.e--;
        uartputc(BACKSPACE);
      }
      break;
    default:
      if(c != 0 && input.e-input.r < INPUT_BUF){
        c = (c == '\r') ? '\n' : c;
        input.buf[input.e++ % INPUT_BUF] = c;
        uartputc(c);
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
