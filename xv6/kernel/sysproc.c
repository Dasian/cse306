#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

int sys_getdate(void) {
  struct rtcdate x;
  struct rtcdate *r = &x;
  if(argptr(0, (void*) &r, sizeof(struct rtcdate)) != 0)
    return -1;
  return getdate(r);
}

int sys_setdate(void) {
  struct rtcdate x;
  struct rtcdate *r = &x;
  if(argptr(0, (void*) &r, sizeof(struct rtcdate)) != 0)
    return -1;
  return setdate(r);
}

// no idea what does. please avert gaze
int sys_timerrate(void) {
  int x;
  int *hz = &x;
  if(argptr(0, (void*) &hz, sizeof(int)) != 0)
    return -1;
  return timerrate(hz);
}

int sys_lseek(void) {
  int fd;
  int offset;
  int origin;

  if(argint(0, &fd) < 0 || argint(1, &offset) < 0 || argint(2, &origin) < 0)
    return -1;

  return lseek(fd, offset, origin);
}

int sys_mmap(void) {
  int fd;
  int length;
  int offset;
  int flags;

  if(argint(0, &fd) < 0 || argint(1, &length) < 0 || argint(2, &offset)
    || argint(3, &flags))
    return -1;

  return (int)mmap(fd, length, offset, flags);
}

int sys_munmap(void) {
  void* addr;
  int length;

  // not sure how to implement the check here?
  if(argint(1, &length) < 0 || argptr(0, (void*)&addr, length) < 0)
    return -1;

  return munmap(addr, length);
}