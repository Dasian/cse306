#include "types.h"
#include "hwinit.h" // hw modification 
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "date.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);
static void sched(void);
static struct proc *roundrobin(void);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  #if HW3_cpu_util // sets default values for tracking 
  p -> cpu_util = 0;
  p -> cpu_ticks = 0;
  p -> cpu_runnable_ticks = 0;
  p -> cpu_wait = 0;
  #endif

  // allocate and set values for map pages table
  p -> mme = -1;              // no entries in table
  p -> mmt_start = kalloc();  // page where the table is located
  p->space = 1;                //Offset for below the kernbase

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU idle loop.
// Each CPU calls idle() after setting itself up.
// Idle never returns.  It loops, executing a HLT instruction in each
// iteration.  The HLT instruction waits for an interrupt (such as a
// timer interrupt) to occur.  Actual work gets done by the CPU when
// the scheduler is invoked to switch the CPU from the idle loop to
// a process context.
void
idle(void)
{
  sti(); // Enable interrupts on this processor
  for(;;) {
    if(!(readeflags()&FL_IF))
      panic("idle non-interruptible");
    hlt(); // Wait for an interrupt
  }
}

// The process scheduler.
//
// Assumes ptable.lock is held, and no other locks.
// Assumes interrupts are disabled on this CPU.
// Assumes proc->state != RUNNING (a process must have changed its
// state before calling the scheduler).
// Saves and restores intena because the original xv6 code did.
// (Original comment:  Saves and restores intena because intena is
// a property of this kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would break in the few
// places where a lock is held but there's no process.)
//
// When invoked, does the following:
//  - choose a process to run
//  - swtch to start running that process (or idle, if none)
//  - eventually that process transfers control
//      via swtch back to the scheduler.

static void
sched(void)
{
  int intena;
  struct proc *p;
  struct context **oldcontext;
  struct cpu *c = mycpu();
  
  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(c->ncli != 1)
    panic("sched locks");
  if(readeflags()&FL_IF)
    panic("sched interruptible");

  // Determine the current context, which is what we are switching from.
  if(c->proc) {
    if(c->proc->state == RUNNING)
      panic("sched running");
    oldcontext = &c->proc->context;
  } else {
    oldcontext = &(c->scheduler);
  }

  // Choose next process to run.
  if((p = roundrobin()) != 0) {
    // Switch to chosen process.  It is the process's job
    // to release ptable.lock and then reacquire it
    // before jumping back to us.
    p->state = RUNNING;
    switchuvm(p);
    if(c->proc != p) {
      c->proc = p;
      intena = c->intena;
      swtch(oldcontext, p->context);
      mycpu()->intena = intena;  // We might return on a different CPU.
    }
  } else {
    // No process to run -- switch to the idle loop.
    switchkvm();
    if(oldcontext != &(c->scheduler)) {
      c->proc = 0;
      intena = c->intena;
      swtch(oldcontext, c->scheduler);
      mycpu()->intena = intena;
    }
  }
}

// Round-robin scheduler.
// The same variable is used by all CPUs to determine the starting index.
// It is protected by the process table lock, so no additional lock is
// required.
static int rrindex;

static struct proc *
roundrobin()
{
  // Loop over process table looking for process to run.
  for(int i = 0; i < NPROC; i++) {
    struct proc *p = &ptable.proc[(i + rrindex + 1) % NPROC];
    if(p->state != RUNNABLE)
      continue;
    rrindex = p - ptable.proc;
    return p;
  }
  return 0;
}

// Called from timer interrupt to reschedule the CPU.
void
reschedule(void)
{
  struct cpu *c = mycpu();

  acquire(&ptable.lock);
  if(c->proc) {
    if(c->proc->state != RUNNING)
      panic("current process not in running state");
    c->proc->state = RUNNABLE;
  }
  sched();
  // NOTE: there is a race here.  We need to release the process
  // table lock before idling the CPU, but as soon as we do, it
  // is possible that an an event on another CPU could cause a process
  // to become ready to run.  The undesirable (but non-catastrophic)
  // consequence of such an occurrence is that this CPU will idle until
  // the next timer interrupt, when in fact it could have been doing
  // useful work.  To do better than this, we would need to arrange
  // for a CPU releasing the process table lock to interrupt all other
  // CPUs if there could be any runnable processes.
  release(&ptable.lock);
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    #if HW3_cpu_util
    cprintf(" cpu_ticks %d", p -> cpu_ticks);
    cprintf(" cpu_util %d%%", p -> cpu_util);
    cprintf(" cpu wait %d%%", p -> cpu_wait);
    #endif
    cprintf("\n");
  }
}

#if HW3_cpu_util
void proc_update_ticks() {
  struct proc* p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if(p != 0) { // makes sure the process exists
      if(p -> state == RUNNING) {
        p -> cpu_ticks = p -> cpu_ticks + 1;
      }
      else if(p -> state == RUNNABLE) {
        p -> cpu_runnable_ticks += 1;
      }
    }
  }
  release(&ptable.lock);
  return;
}

/*
  Updates the cpu_util variable for every process in ptable
  This happens every second and is called from trap.c
  Locking is also contained within this function

  K should be used to achieve a "time constant" of SCHED_DECAY
    Seconds. If a process doesn't use any cpu the after SCHED_DECAY
    Seconds, cpu_util should've decayed to 1/10 of original.
*/
void proc_cpu_util() {
  #ifndef SCHED_DECAY
  // double k = .5; // incorporate SCHED_DELAY somehow?
  #endif

  #if 0 // unable to add the <math.h< library
  double exp = 1/SCHED_DECAY;
  double k = pow(1/10, exp);
  #endif
  
  double k = .5;
  struct proc *p;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if(p != 0) {
      // cpu utilization %
      p -> cpu_util *= k;
      p -> cpu_util += p -> cpu_ticks;
      if(p -> cpu_util > 100)
        p -> cpu_util = 100;
      p -> cpu_ticks = 0;
      // avg time in runnable state per sec
      if(p -> cpu_wait == 0) {
        p -> cpu_wait = p -> cpu_runnable_ticks;
      }
      else {
        p -> cpu_wait += p -> cpu_runnable_ticks;
        p -> cpu_wait /= 2;
      }
      p -> cpu_runnable_ticks = 0;
    }
  }

  release(&ptable.lock);
  return;
}
#endif

/* 
  Added for hw1
  Retrieves the current date and time and writes it in the appropriate addr
  Returns 0 on success and -1 otherwise
*/
int getdate(struct rtcdate *r) {
  
  // checks if pointer is valid
  if(sizeof(*r) != sizeof(struct rtcdate))
    return -1;
  if(argptr(0, (void*) &r, sizeof(struct rtcdate)) < 0)
    return -1;

  // retrieves date and time and writes it into r
  cmostime(r);
  return 0;
}

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
*/
int setdate(struct rtcdate *r) {
  // validate pointer
  if(sizeof(*r) != sizeof(struct rtcdate))
    return -1;
  if(argptr(0, (void*) &r, sizeof(struct rtcdate)) < 0)
    return -1;

  // validate pointer fields
  uint second, minute, hour, month, day, year;
  int isleap = 0;
  if((r -> year)%4 == 0)
    isleap = 1;
  uint max_day[12] = {31, 28+isleap, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

  if((second=r->second) < 0 || r->second >= 60)   return -1;
  if((minute=r->minute) < 0 || r->minute >= 60)   return -1;
  if((hour=r->hour) < 0 || r->hour > 23)          return -1;
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

  // prevent interrupts
  pushcli();

  // write values to the registers
  outb(0x70, 1<<7 | 0x09);
  outb(0x71, year);
  outb(0x70, 1<<7 | 0x08);
  outb(0x71, month);
  outb(0x70, 1<<7 | 0x07);
  outb(0x71, day);
  outb(0x70, 1<<7 | 0x04);
  outb(0x71, hour);
  outb(0x70, 1<<7 | 0x02);
  outb(0x71, minute);
  outb(0x70, 1<<7 | 0x00);
  outb(0x71, second);


  popcli();

  return 0;
}

// implemented for HW4_ddn but not used unless the macro is set
int lseek(int fd, int offset, int origin) {
  // match a fd to a proceess' file
  struct file* f = myproc() -> ofile[fd];

  // set the offset for that file to offset + origin
  switch(origin) {
    case SEEK_SET:
      f -> off = offset;
    break;
    case SEEK_CURR:
      f -> off += offset;
    break;
    case SEEK_END:
      f -> off = f->ip->size + offset;
    break;
    default:
    return -1;
  }
  return offset;
}

// Everything below is implemented for HW5

/*
  Helper function for mmap. Allocates newsz-oldsz space in pages.
  The page table is updated and the starting address of the 
  memory to be mapped is returned on success, -1 on failure.

  This code is nearly an exact copy of allocuvm() in vm.c
  with some minor adjustments (return value and setting addr)

  Should this be adjusted to add bookkeeping stuff?
  Only one page could be marked dirty and only
  that page should be written back into memory.
  The memory permissions should be changed no?
*/
void* mmap_alloc(pde_t *pgdir, uint oldsz, uint newsz) {
  void* addr;
  char *mem;
  uint a;

  if(newsz >= KERNBASE)
    return (void*) -1;
  if(newsz < oldsz)
    return (void*) -1;

  a = PGROUNDUP(oldsz);
  for(; a < newsz; a += PGSIZE){
    mem = kalloc();
    // starting addr of allocated memory
    if(a == PGROUNDUP(oldsz))
      addr = mem;
    if(mem == 0){
      cprintf("mmap_alloc out of memory\n");
      deallocuvm(pgdir, newsz, oldsz);
      return (void*) -1;
    }
    memset(mem, 0, PGSIZE);
    // Change perm here (mappages also adds PTE_P flag)
    // I just removed | PTE_W
    // Write through and present flags? 
    if(mappages(pgdir, (char*)a, PGSIZE, V2P(mem), PTE_U) < 0){
      cprintf("mmap_alloc out of memory (2)\n");
      deallocuvm(pgdir, newsz, oldsz);
      kfree(mem);
      return (void*) -1;
    }
  }
  return addr;
}

/*
  Finds an empty entry slot in a processes memory mapped table
  Takes as input the starting address of the allocated page that
   houses the table
  Returns address of free entry or -1 on failure
*/
struct mme* find_free_mmt_entry(struct mme* start) {
  struct mme* entry = start;
  for(int i=0; i<PGSIZE; i+=sizeof(struct mme))
    if((entry+i) -> prev == 0)
      return entry;
  return (struct mme*) -1;
}

/*
  Write input page back to file
  *ONLY CALL IF THE ENTRY IS DIRTY

  entry is the node in the map linked list
  addr is the addr of the CHANGED PAGE to be written back
*/
int mmap_write_dirty(struct mme* entry, void* addr) {
  int tmp;

  ilock(entry -> ip);
  tmp = writei(entry -> ip, addr, entry -> offset+((int)entry->addr - (int)addr), PGSIZE);
  iunlock(entry -> ip);

  return tmp;
}

/*
  Places a file into main memory OR reserves anonymous memory.
  The mappings are updated in the page table and 
  additional mapping info is stored in the mem mapped table in
  the proc struct

  Notes:
   Should there be some kind of dealloc on failure?
   How to get proper map table entry? 
*/
void* mmap(int fd, int length, int offset, int flags) {

  // checks valid args and makes sure MAP_FILE and MAP_SHARED aren't
  // both true at the same time (flags == 3)
  if(length < 0 || fd < 0 || flags < 0 || flags > 2)
    return (void*) -1;

  void* addr;
  struct proc *p = myproc();
  struct file *f;
  struct mme *entry;  // entry in the memory mapped table 

  // find a free entry in the mmt block
  entry = find_free_mmt_entry(p->mmt_start);
  if(entry < (struct mme*) -1) {
    cprintf("mmap table out of memory\n");
    return (void*) -1;
  }

  //Assigning the memory to the virtual memory
  addr = (KERNBASE - p->space);                   //(For first entry it is KERNBASE - 1)
  addr = PGROUNDDOWN((uint)addr);
  if(addr <= p -> sz)                             //Checks if the addr goes into the process stuff
    return (void*) -1;
  p->space = (KERNBASE - (uint)addr) + length;                   //For the next memory to use
  // allocate and map memory of size length
  // mem is already zero'd out
  //addr = mmap_alloc(p->pgdir, p->sz, p->sz + length);
  //if(addr < 0)
  //  return (void*) -1;

  switch(flags) {
    case MAP_FILE:

    // obtain the file
    f = p -> ofile[fd];
    if(length + offset > f -> ip -> size)
      return (void*) -1;

    // keep track of file information in mem mapped table
    entry -> ip = f -> ip;
    entry -> fd = fd;
    entry -> dirty = 0;
    entry -> offset = offset;
    break;
  }

  // no additional processing needed for MAP_SHARED/MAP_PRIVATE

  // update mem mapped table with new entry
  entry -> MFLAGS = flags; // keeps track of private/shared/file
  entry -> addr = addr;
  entry -> sz = length;

  // add entry to map table list
  if(p -> mme == -1) { // no nodes in linked list
    entry -> prev = entry;
    entry -> next = 0;
  }
  else { // entry becomes the new head of the ll
    entry -> prev = entry;
    entry -> next = p -> mme;
    p -> mme -> prev = entry;
  }
  // entry is the first node in list always
  p -> mme = entry;

  return addr;
}

/*
  Unmap a region previously mapped using mmap()

  Returns 0 on success and -1 on failure ig since it didn't specify
*/
int munmap(void* addr, int length) {
  struct proc *p = myproc();
  struct mme *mme = p -> mme;
  pte_t *pte;
  //int len;
  int isFile = 0;

  // Removing entry from map table
  // no entries exist in the table
  if(mme == -1)
    return -1;

  // find entry in table
  do {
    mme = mme -> next;
  } while(mme != 0 && mme -> addr != addr);

  // entry with corresponding addr doesn't exist in the table
  if(mme == 0)
    return -1;
  //len = mme -> sz;
  if(mme->MFLAGS & MAP_FILE)              //Check if the mapping is for MAP_FILE
  {
    isFile = 1;
  }

  if(isFile == 1)
  {
    for(int i = 0; i < length; i = i + PGSIZE)         //Each table entry takes up PGSIZE so increment by that much
    {
      if(pte = walkpgdir(p->pgdir, addr, 0) == 0)
        return -1;
      if(*pte & PTE_D)
      {
        //Do the writing to the file if it is dirty
        //mmap_write_dirty(mme, )
      }
    }
  }

  // Removing mme from linked list
  if(mme -> prev == mme && mme -> next == 0) { // both head and tail
    memset(mme, 0, sizeof(struct mme));
    // no entries exist in the table
    p -> mme = -1;
  }
  else {
    if(mme -> prev == mme) { // mme is head 
      mme -> next -> prev = mme -> next;
      // update first entry
      p -> mme = mme;
    }
    else if(mme -> next == 0) { // mme is tail
      mme -> prev -> next = 0;
    }
    else { // mme is body
      mme -> prev -> next = mme -> next;
      mme -> next -> prev = mme -> prev;
    }
    memset(mme, 0, sizeof(struct mme));
  }

  // zero out memory? security hazard if we don't
  memset(addr, 0, length);

  return 0;
}