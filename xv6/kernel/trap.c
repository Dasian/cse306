#include "hwinit.h" // hw preprocessors
#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"
#include "date.h"

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;

#if HW3_cpu_util
static struct rtcdate time;
static uint prev_second;
// struct spinlock datelock;
#endif

void
tvinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);

  #if HW3_cpu_util
  cmostime(&time);
  prev_second = time.second;
  // initlock(&datelock, "date");
  #endif

  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}

//PAGEBREAK: 41
void
trap(struct trapframe *tf)
{
  if(tf->trapno == T_SYSCALL){
    if(myproc()->killed)
      exit();
    myproc()->tf = tf;
    syscall();
    if(myproc()->killed)
      exit();
    return;
  }

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
      acquire(&tickslock);
      ticks++;
      #if HW3_cpu_util
      proc_update_ticks(); // incs p->cpu_ticks on all running procs
      #endif
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  #if HW4_ide2
  case T_IRQ0 + IRQ_IDE2:
    ideintr2();
    lapiceoi();
    break;
  #endif // end of HW4_ide2 
  #if !HW4_ide2
  case T_IRQ0 + IRQ_IDE+1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  #endif // end of not HW4_ide2
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr(1); // changed for hw3 to differentiate btwn com1/com2
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM2:
    uartintr(2);
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
  case T_IRQ0 + IRQ_SPURIOUS1:
    cprintf("cpu%d: spurious interrupt (%d) at %x:%x\n",
            cpuid(), tf->trapno, tf->cs, tf->eip);
    lapiceoi();
    break;
  #if HW5_pf_handler
  case T_PGFLT: // handles page faults

    struct proc* p = myproc();

    // get faulting address through some func? 
    // maybe rcr2() ?
    void* addr;

    // check if faulting addr lies in addr space mapped
    // by mmap (check the linked list)
    // While going through the map list, it also writes
    // dirty pages back (beginning to entry)
    struct mme *entry = p -> mme;
    do {
      if(entry->addr >= addr && (entry->addr + entry->sz) <= addr)
        break;

      // write dirty pages back to file

      entry -> next;
    } while(entry != 0);

    if(entry == 0) { // not a mapped entry; page fault exception
      if(myproc() == 0 || (tf->cs&3) == 0){
        // In kernel, it must be our mistake.
        cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
                tf->trapno, cpuid(), tf->eip, rcr2());
        //panic("trap");
        return;
      }
        // In user space, assume process misbehaved.
        cprintf("pid %d %s: trap %d err %d on cpu %d "
                "eip 0x%x addr 0x%x--kill proc\n",
                myproc()->pid, myproc()->name, tf->trapno,
                tf->err, cpuid(), tf->eip, rcr2());
        myproc()->killed = 1;
      }
    else { // addr is within mapped entry space

      // get page associated with the faulting address (addr)
      // note: entry only contains the starting addr of the region

      //  make page resident in memory
      //   - if page can't be allocated just kill proc
      //     and print error

      // Update page table entry (was previously invalid)

      // Copy on write stuff?

      // Write remaining dirty pages back (entry to end)
      do {
        if(entry -> dirty)
          // write back
        entry -> next;
      } while(entry != 0);
    }
  break;
  #endif
  //PAGEBREAK: 13
  default:
    if(myproc() == 0 || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      //panic("trap");
      return;
    }
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            myproc()->pid, myproc()->name, tf->trapno,
            tf->err, cpuid(), tf->eip, rcr2());
    myproc()->killed = 1;
  }

  #if HW3_cpu_util // updates p -> cpu_util every second
  // acquire(&datelock);
  cmostime(&time);
  // just assures that it's a legal second and no other shenanigans
  if(time.second != prev_second && time.second < 60 && (time.second - 
    prev_second <= 1 || (!time.second && prev_second==59))) {
    prev_second = time.second;
    proc_cpu_util();
  }
  // release(&datelock);
  #endif

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();

  // Invoke the scheduler on clock tick.
  if(tf->trapno == T_IRQ0+IRQ_TIMER)
    reschedule();

  // Check if the process has been killed since we yielded
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();
}
