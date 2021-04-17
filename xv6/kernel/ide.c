// Simple PIO-based (non-DMA) IDE driver code.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"
#include "defs.h"
#include "hwinit.h"
#include "file.h"

#define SECTOR_SIZE   512
#define IDE_BSY       0x80
#define IDE_DRDY      0x40
#define IDE_DF        0x20
#define IDE_ERR       0x01

#define IDE_CMD_READ  0x20
#define IDE_CMD_WRITE 0x30
#define IDE_CMD_RDMUL 0xc4
#define IDE_CMD_WRMUL 0xc5

#define DMA 1 // MAYBE MOVE THIS AND LET IT BE CHANGED BY MAKE
#define CONFIG_ADDR   0xCF8
#define CONFIG_DATA   0xCFC

#if HW4_ide2

// i/o ports for second ide driver
#define PORT3         0x170
#define PORT4         0x376
// second ide controller registers
#define BADDR3        0x170
#define BADDR4        0x376

static int havedisk3;

#endif

// idequeue points to the buf now being read/written to the disk.
// idequeue->qnext points to the next buf to be processed.
// You must hold idelock while manipulating queue.

static struct spinlock idelock;
static struct buf *idequeue;

static int havedisk1;
static void idestart(struct buf*);

static uint base_addr;

// Wait for IDE disk to become ready.
static int
idewait(int checkerr)
{
  int r;

  while(((r = inb(0x1f7)) & (IDE_BSY|IDE_DRDY)) != IDE_DRDY)
    ;
  if(checkerr && (r & (IDE_DF|IDE_ERR)) != 0)
    return -1;
  return 0;
}

#if HW4_ide2
static int
idewait2(int checkerr)
{
  int r;

  while(((r = inb(BADDR3 + 7)) & (IDE_BSY|IDE_DRDY)) != IDE_DRDY)
    ;
  if(checkerr && (r & (IDE_DF|IDE_ERR)) != 0)
    return -1;
  return 0;
}
#endif

// basically outb and inb but with 4 bytes instead of one
void outl(uint port, uint *data) {
  for(int i=0; i < 4; i++) {
    uint tmp = (*data) & (0xFF << i*8);
    if(i>0) {
      tmp = tmp >> i*8;
    }
    outb(port+(i*8), tmp);
  }
}

uint inl(uint port) {
  uint tmp = 0;
  for(int i=0; i<4; i++) {
    tmp = tmp << 8;
    tmp = tmp | inb(port + 8*i);
  }
  return tmp;
}

// This was copied from the PCI wiki
// It reads words from the config area
uint pci_read_config(uint bus, uint slot, uint func, uint offset) {

  uint address = (uint) ((bus << 16) | (slot << 11) | (func << 8) 
    | (offset & 0xfc) | ((uint) 0x80000000));

  outl(CONFIG_ADDR, &address);
  uint tmp = (uint)((inl(CONFIG_DATA) >> ((offset & 2) * 8)) & 0xffff);
  
  return tmp; 
}

#if HW4_ddn
// reads contents of disk inode and places it in buf
int ideread(struct inode *ip, char *buf, int n) {

  int num_read = 0; // keeps track of number of bytes read

  if(n < 0) return -1;

  iunlock(ip); // copied from console.c

  // calculate disk block addr that corresponds with curr file offset
  // THIS IS A PLACEDHOLDER; IT DOESN'T WORK, FIGURE OUT SOLUTION
  uint blockno = ip -> addrs[NDIRECT];
  // END OF PLACEHOLDER

  // keeps reading blocks until total n bytes are read
  while(num_read < n) {

    // get block from disk we want to read from
    struct buf* b = bread(ip->dev, blockno);

    // copy data from inode into buf
    if(n - num_read != 0 && n-num_read < BSIZE) {
      // This should only run if n%BSIZE != 0
      //  and will only run during the LAST write
      // ex: Bsize=10; n=21; num_read=20;
      //  It will write the last 1 byte into buf
      memmove(buf, b->data, n-num_read);
      num_read += n-num_read;
    }
    else {
      memmove(buf, b->data, BSIZE);
      buf += BSIZE;       // increment offset we're copying into
      blockno++;          // increment which block we're reading from
      num_read += BSIZE;  // tracks number of bytes read from disk
    }
  }

  ilock(ip); // copied from console.c
  return n;
}

// write contents of buf into the disk inode
int idewrite(struct inode *ip, char *buf, int n) {

  int num_written = 0; // keeps track of number of bytes written

  if(n < 0) return -1;

  iunlock(ip); // copied from console.c

  // calculate disk block addr that corresponds with curr file offset
  // THIS IS A PLACEDHOLDER; IT DOESN'T WORK, FIGURE OUT SOLUTION
  uint blockno = ip -> addrs[NDIRECT];
  // END OF PLACEHOLDER

  // keeps reading blocks until total n bytes are read
  while(num_written < n) {

    // get block from disk we want to write to (it's locked)
    struct buf* b = bread(ip->dev, blockno);

    // DO UNLOCKING SHENANIGANS NEED TO HAPPEN?
    // brelse(buf *b) releases a buffer; also does cache queue stuff
    // or do I use releasesleep(&b->lock); ?

    // copy contents of buf into b to be written to disk
    if(n-num_written != 0 && n-num_written < BSIZE) {
      // If n is not a multiple of BSIZE, this will 
      //  write the remainder of the bytes
      memmove(b->data, buf, n-num_written);
      num_written += n-num_written;
    }
    else {
      memmove(b->data, buf, BSIZE);
      buf += BSIZE;         // increment offset we're copying from
      blockno++;            // increment which block we're writing to
      num_written += BSIZE; // tracks number of bytes written to disk
    }

    //writes b's contents to disk, b must be locked
    bwrite(b); 
  }

  ilock(ip); // copied from console.c
  return n;
}
#endif // end of HW4_ddn (HW4 ex2)

/*
  Changes for hw2
  TODO:
    Figure out how to connect the device for testing
    Add a macro to make with or w/o the DMA stuff
    Figure out how to write to cmd register
    Make sure the addresses and reading is done correctly
    General Testing

    attempted to do -device [the things listed]
    piix3-ide
    piix3-ide-xen
    piix4-ide

    it did not work :(
*/
void
ideinit(void)
{
  int i;
  uint vendor, device;
  uint func = 0, bus = 1, slot;

  initlock(&idelock, "ide");
  ioapicenable(IRQ_IDE, ncpu - 1);
  idewait(0);

  // Check if disk 1 is present
  outb(0x1f6, 0xe0 | (1<<4));
  for(i=0; i<1000; i++){
    if(inb(0x1f7) != 0){
      havedisk1 = 1;
      break;
    }
  }

  // Switch back to disk 0.
  outb(0x1f6, 0xe0 | (0<<4));

  #if HW4_ide2

  ioapicenable(IRQ_IDE2, ncpu - 1);
  idewait2(0);

  // check if disk 3 is present
  outb(BADDR3 + 6, 0xe0 | (1<<4));
  for(i=0; i<1000; i++){
    if(inb(BADDR3 + 7) != 0){
      havedisk3 = 1;
      break;
    }
  }

  // Switch back to disk 2
  outb(BADDR3 + 6, 0xe0 | (0<<4));

  #endif // end of HW4_ide2

  #if HW4_ddn
  devsw[IDE].write = idewrite;
  devsw[IDE].read = ideread;
  #endif

  // Runs pci code if flag is set during compilation
  #ifdef DMA 
    int pci_found = 0;
    // Checks to make sure the proper slot + id pair exists
    for(bus = 0; bus<256; bus++){
      for(slot=0; slot<32; slot++) {
        device = 0; 
        vendor = pci_read_config(bus, slot, func, 0);

        // DEBUG statements
        if(DEBUG && vendor != 0xFFFF)
          cprintf("Detected VendorID slot %d: %d\n", slot, vendor);
        device = pci_read_config(bus, slot, func, 2);
        if(DEBUG && device != 4607)
          cprintf("Detected DeviceID %d\n", device);

        if(vendor != 0xFFFF && vendor == 0x8086) {
          device = pci_read_config(bus, slot, func, 2);
        }
        if(device == 0x7010) {
          if(DEBUG)
            cprintf("device found!!!"); // doesn't reach here
          pci_found = 1;
          break;
        }
      }
    }
      if(pci_found) {
        // extract Base addr register [reg 0x08 offset 20; 32 bits]
        // include register and offset somehow
        uint addr = (uint) ((bus << 16) | (slot << 11) | (func << 8)
          | (0x20) | ((uint) 0x80000000));

        // check these addresses
        // might need to adjust base address based on the type...
        outl(CONFIG_ADDR, &addr);
        base_addr = inl(CONFIG_DATA);

        // convert base addr
        // int is_port_space = base_addr & 0x1; 
        base_addr = (base_addr >> 4) << 4;

        // Enabling DMA mode
        // read cmd reg at 0x4 in pci config space
        addr = (uint) ((bus << 16) | (slot << 11) | (func << 8) 
          | (0x4) | ((uint) 0x80000000));
        outl(CONFIG_ADDR, &addr);
        uint cmd = (uint)((inl(CONFIG_DATA) >> ((0 & 2) * 8)) & 0xffff);

        // set bit 0 (0x1)
        cmd = cmd | 0x1;

        // write back to the reg; not sure if this works as intended
        // outb(some_base_addr + reg - offset, cmd) ??
        // This specifies WHAT we want written
        outl(CONFIG_DATA, &cmd);
        // This specifies where we want it written
        outl(CONFIG_ADDR, &addr);
      }
      else {
        cprintf("PCI bus not detected. Further PCI bus operations will not work.\n");
      }
  #endif
}

// Start the request for b.  Caller must hold idelock.
static void
idestart(struct buf *b)
{
  if(b == 0)
    panic("idestart");
  if(b->blockno >= FSSIZE)
    panic("incorrect blockno");
  int sector_per_block =  BSIZE/SECTOR_SIZE;
  int sector = b->blockno * sector_per_block;
  int read_cmd = (sector_per_block == 1) ? IDE_CMD_READ :  IDE_CMD_RDMUL;
  int write_cmd = (sector_per_block == 1) ? IDE_CMD_WRITE : IDE_CMD_WRMUL;

  if (sector_per_block > 7) panic("idestart");

  idewait(0);
  outb(0x3f6, 0);  // generate interrupt
  outb(0x1f2, sector_per_block);  // number of sectors
  outb(0x1f3, sector & 0xff);
  outb(0x1f4, (sector >> 8) & 0xff);
  outb(0x1f5, (sector >> 16) & 0xff);
  outb(0x1f6, 0xe0 | ((b->dev&1)<<4) | ((sector>>24)&0x0f));
  if(b->flags & B_DIRTY){
    outb(0x1f7, write_cmd);
    outsl(0x1f0, b->data, BSIZE/4);
  } else {
    outb(0x1f7, read_cmd);
  }
}

#if HW4_ide2
static void
idestart2(struct buf *b)
{
  if(b == 0)
    panic("idestart");
  if(b->blockno >= FSSIZE)
    panic("incorrect blockno");
  int sector_per_block =  BSIZE/SECTOR_SIZE;
  int sector = b->blockno * sector_per_block;
  int read_cmd = (sector_per_block == 1) ? IDE_CMD_READ :  IDE_CMD_RDMUL;
  int write_cmd = (sector_per_block == 1) ? IDE_CMD_WRITE : IDE_CMD_WRMUL;

  if (sector_per_block > 7) panic("idestart");

  idewait(0);
  outb(BADDR4, 0);  // generate interrupt
  outb(BADDR3 + 2, sector_per_block);  // number of sectors
  outb(BADDR3 + 3, sector & 0xff);
  outb(BADDR3 + 4, (sector >> 8) & 0xff);
  outb(BADDR3 + 5, (sector >> 16) & 0xff);
  outb(BADDR3 + 6, 0xe0 | ((b->dev&1)<<4) | ((sector>>24)&0x0f));
  if(b->flags & B_DIRTY){
    outb(BADDR3 + 7, write_cmd);
    outsl(BADDR3 + 0, b->data, BSIZE/4);
  } else {
    outb(BADDR3 + 7, read_cmd);
  }
}
#endif

// Interrupt handler.
/*
  HW2 Ex 2
  TODO:
    read DMA controller status register
*/
void
ideintr(void)
{
  struct buf *b;

  // First queued buffer is the active request.
  acquire(&idelock);

  if((b = idequeue) == 0){
    release(&idelock);
    return;
  }
  idequeue = b->qnext;

  // Read data if needed.
  if(!(b->flags & B_DIRTY) && idewait(1) >= 0)
    insl(0x1f0, b->data, BSIZE/4);

  // Wake process waiting for this buf.
  b->flags |= B_VALID;
  b->flags &= ~B_DIRTY;
  wakeup(b);

  // Start disk on next buf in queue.
  if(idequeue != 0)
    idestart(idequeue);

  release(&idelock);
}

#if HW4_ide2
void
ideintr2(void)
{
  struct buf *b;

  // First queued buffer is the active request.
  acquire(&idelock);

  if((b = idequeue) == 0){
    release(&idelock);
    return;
  }
  idequeue = b->qnext;

  // Read data if needed.
  if(!(b->flags & B_DIRTY) && idewait2(1) >= 0)
    insl(BADDR3, b->data, BSIZE/4);

  // Wake process waiting for this buf.
  b->flags |= B_VALID;
  b->flags &= ~B_DIRTY;
  wakeup(b);

  // Start disk on next buf in queue.
  if(idequeue != 0)
    idestart2(idequeue);

  release(&idelock);
}
#endif

//PAGEBREAK!
// Sync buf with disk.
// If B_DIRTY is set, write buf to disk, clear B_DIRTY, set B_VALID.
// Else if B_VALID is not set, read buf from disk, set B_VALID.
/*
  HW2 
  TODO:
  1a.
    Set up PRDT
    Create table with one single entry (also has eot bit set)
    The table and entry need to be 4 byte aligned and not ross 64k
      (this is easy for the table but hard for the entry, struct change here)

  1b.
    Write addr to table in Phys desc table REGISTER (offsets 0x4-0x7 from BAR4
    Set data transfer direction in bit 3 of bus cmd ref
    Clear error and interrupt bits
  2.
    write cmd byte in cmd register (preserve transfer direction)
  3.
    Engange start/stop bit in cmd reg (bit 0 value 0x1)
  4.
    Interrupt handler (goto ide intr)
*/
void
iderw(struct buf *b)
{
  struct buf **pp;

  if(!holdingsleep(&b->lock))
    panic("iderw: buf not locked");
  if((b->flags & (B_VALID|B_DIRTY)) == B_VALID)
    panic("iderw: nothing to do");
  if(b->dev != 0 && !havedisk1)
    panic("iderw: ide disk 1 not present");

  acquire(&idelock);  //DOC:acquire-lock

  // Append b to idequeue.
  b->qnext = 0;
  for(pp=&idequeue; *pp; pp=&(*pp)->qnext)  //DOC:insert-queue
    ;
  *pp = b;

  // Start disk if necessary.
  if(idequeue == b)
    idestart(b);

  // Wait for request to finish.
  while((b->flags & (B_VALID|B_DIRTY)) != B_VALID){
    sleep(b, &idelock);
  }


  release(&idelock);
}
