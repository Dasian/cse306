// init: The initial user-level program

#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user.h"
#include "kernel/fs.h"
#include "kernel/spinlock.h"
#include "kernel/sleeplock.h"
#include "kernel/file.h"
#include "kernel/hwinit.h"

char *argv[] = { "sh", 0 };

// if defined it will run the HW3 init code
//  (run 3 shells instead of 1)
#if HW3_init
#define DEV_NAME_SIZE 25
#define NUM_DEV        3
struct dnode_entry {
  char name[25];
  short major;
  short minor;
};
struct dnode_entry table[3];

// sets major and minor device nums in table
void create_device(int i, short major, short minor) {
  table[i].major = major;
  table[i].minor = minor;
}
#endif

#if HW4_ddn
#define NUM_DEV 4
struct dnode_entry {
  char name[25];
  short major;
  short minor;
};
struct dnode_entry table[NUM_DEV];

// sets major and minor device nums in table
void create_device(int i, short major, short minor) {
  table[i].major = major;
  table[i].minor = minor;
}
#endif

int
main(void)
{
  int pid, wpid;
  #if HW3_init
  int fds[NUM_DEV];

  // table init
  strcpy(table[0].name, "console");
  create_device(0, CONSOLE, 1);

  strcpy(table[1].name, "/com1");
  create_device(1, COM, 1);

  strcpy(table[2].name, "/com2");
  create_device(2, COM, 2);
  #endif

  #if HW4_ddn // add the 4 disk devices
  strcpy(table[0].name, "/disk0");
  create_device(0, IDE, 0);
  strcpy(table[1].name, "/disk1");
  create_device(1, IDE, 1);
  strcpy(table[2].name, "/disk2");
  create_device(2, IDE, 2);
  strcpy(table[3].name, "/disk3");
  create_device(3, IDE, 3);
  
  // creating the disks
  for(int i=0; i<NUM_DEV; i++) {
    // if a device doesn't exist, create it
    if(open(table[i].name, O_RDWR) < 0) {
      mknod(table[i].name, table[i].major, table[i].minor);
      open(table[i].name, O_RDWR);
    }
  }
  #endif

  // This should be run in the generalization code
  // stock xv6 creating console device
  #if !HW3_init
  if(open("console", O_RDWR) < 0){
    mknod("console", CONSOLE, 1);
    open("console", O_RDWR);
  }
  dup(0);  // stdout
  dup(0);  // stderr

  for(;;){
      printf(1, "init: starting sh\n");
      pid = fork();
      if(pid < 0){
        printf(1, "init: fork failed\n");
        exit();
      }
      if(pid == 0){
        exec("sh", argv);
        printf(1, "init: exec sh failed\n");
        exit();
      }
      while((wpid=wait()) >= 0 && wpid != pid)
        printf(1, "zombie!\n");
    }
  #endif
  
  // iterate through the table of devices
  #if HW3_init
  int fd, i;
  int pids[NUM_DEV];
  for(i=0; i<NUM_DEV; i++) {

    // if a device doesn't exist, create it
    if((fd=open(table[i].name, O_RDWR)) < 0) {
      mknod(table[i].name, table[i].major, table[i].minor);
      fd = open(table[i].name, O_RDWR);
    }

    pid = fork();
    printf(1, "init: starting sh\n");
    
    // branch off child
    if(pid < 0){
      printf(1, "init: fork failed\n");
      exit();
    }
    if(pid == 0){ // create new shell on device
      close(0);
      close(1);
      close(2);
      dup(fd);
      dup(fd);
      dup(fd);
      exec("sh", argv);
      printf(1, "init: exec sh failed\n");
      exit();
    }
    if(DEBUG)
      printf(1, "init: %s pid %d\n", table[i].name, pid);

    // tracking vars for shell revival
    pids[i] = pid;
    int tmp;
    do {
      tmp = dup(fd);
    } while(tmp < 4);
    fds[i] = tmp;
    close(fd);
  }

  // reviving shells
  for(;;) {
    wpid = wait();
    for(i=0;i<NUM_DEV;i++)
      if(wpid == pids[i]) {
        if(pid < 0){
          printf(1, "init: fork failed\n");
          exit();
        }
        if(pid == 0){ 
          close(0);
          close(1);
          close(2);
          dup(fds[i]);
          dup(fds[i]);
          dup(fds[i]);
          exec("sh", argv);
          printf(1, "init: exec sh failed\n");
          exit();
        }
        pids[i] = pid;
      }
  }
  #endif
}
