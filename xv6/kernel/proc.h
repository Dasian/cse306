// Per-CPU state
struct cpu {
  uchar apicid;                // Local APIC ID
  struct context *scheduler;   // Context used by scheduler and idle loop
  struct taskstate ts;         // Used by x86 to find stack for interrupt
  struct segdesc gdt[NSEGS];   // x86 global descriptor table
  volatile uint started;       // Has the CPU started?
  int ncli;                    // Depth of pushcli nesting.
  int intena;                  // Were interrupts enabled before pushcli?
  struct proc *proc;           // The process running on this cpu or null
};

extern struct cpu cpus[NCPU];
extern int ncpu;

//PAGEBREAK: 17
// Saved registers for kernel context switches.
// Don't need to save all the segment registers (%cs, etc),
// because they are constant across kernel contexts.
// Don't need to save %eax, %ecx, %edx, because the
// x86 convention is that the caller has saved them.
// Contexts are stored at the bottom of the stack they
// describe; the stack pointer is the address of the context.
// The layout of the context matches the layout of the stack in swtch.S
// at the "Switch stacks" comment. Switch doesn't save eip explicitly,
// but it is on the stack and allocproc() manipulates it.
struct context {
  uint edi;
  uint esi;
  uint ebx;
  uint ebp;
  uint eip;
};

enum procstate { UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// added for hw5
// entries in the memory mapped table
struct mme {
  void* addr;       // starting virtual addr of mapped memory
  int sz;           // size of allocated memory
  int MFLAGS;       // Mapped flags (Shared, private, file)
  int offset;       //The offset
  struct mme* prev;
  struct mme* next; 

  // should only be used if file flag is present in MFLAGS
  struct inode *ip;
  int fd;          // not sure if fd is needed
  int dirty;
};

// Per-process state
struct proc {
  uint sz;                     // Size of process memory (bytes)
  int cpu_ticks;               // Number of ticks while in running state
  int cpu_util;                // CPU utilization percentage
  int cpu_runnable_ticks;
  int cpu_wait;
  pde_t* pgdir;                // Page table
  struct mme* mmt_start;       // (HW5) Address of map table start
  struct mme* mme;             // (HW5) First entry in map table
  int space;                    // The space into the heap that has been used
  char *kstack;                // Bottom of kernel stack for this process
  enum procstate state;        // Process state
  int pid;                     // Process ID
  struct proc *parent;         // Parent process
  struct trapframe *tf;        // Trap frame for current syscall
  struct context *context;     // swtch() here to run process
  void *chan;                  // If non-zero, sleeping on chan
  int killed;                  // If non-zero, have been killed
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging)
};

#if HW3_cpu_util
void proc_cpu_util();
void proc_update_ticks();
#endif

// flags for mmap
#define MAP_PRIVATE 0
#define MAP_SHARED 1
#define MAP_FILE 2

// Process memory is laid out contiguously, low addresses first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap
