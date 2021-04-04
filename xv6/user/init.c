// init: The initial user-level program

#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user.h"
#include "kernel/fs.h"
#include "kernel/spinlock.h"
#include "kernel/sleeplock.h"
#include "kernel/file.h"

char *argv[] = { "sh", 0 };

// if defined it will run the HW3 init code
//  (run 3 shells instead of 1)
#define HW3_init       1

#define DEV_NAME_SIZE 25
#define NUM_DEV        3
#define DEBUG          0
struct dnode_entry {
  char name[25];
  short major;
  short minor;
};

int
main(void)
{
  int pid, wpid;
  struct dnode_entry table[3];
  int fds[NUM_DEV];

  // table init
  strcpy(table[0].name, "console");
  table[0].major = CONSOLE;
  table[0].minor = 1;

  strcpy(table[1].name, "/com1");
  table[1].major = COM;
  table[1].minor = 1;

  strcpy(table[2].name, "/com2");
  table[2].major = COM;
  table[2].minor = 2;

  
  // This should be run in the generalization code
  // stock xv6 creating console device
  #ifndef HW3_init
  if(open("/console", O_RDWR) < 0){
    mknod("/console", CONSOLE, 1);
    open("/console", O_RDWR);
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
  #ifdef HW3_init
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
