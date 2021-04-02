// init: The initial user-level program

#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user.h"
#include "kernel/file.h"

char *argv[] = { "sh", 0 };

#define DEV_NAME_SIZE 25;
#define NUM_DEV        3;
struct dnode_entry {
  char name[DEV_NAME_SIZE];
  int major;
  int minor;
};

int
main(void)
{
  int pid, wpid;
  struct dnode_entry table[NUM_DEV];

  // table init
  table[0].name = "console";
  table[0].major = CONSOLE;
  table[0].minor = 1;

  table[1].name = "/com1";
  table[1].major = COM;
  table[1].minor = 1;
  
  table[2].name = "/com2";
  table[2].major = COM;
  table[2].minor = 2;

  /*
  This should be run in the generalization code
  // stock xv6 creating console device
  if(open("console", O_RDWR) < 0){
    mknod("console", CONSOLE, 1);
    open("console", O_RDWR);
  }
  dup(0);  // stdout
  dup(0);  // stderr
  */

  /*
  Then init needs to fork twice, once for the shell to be started on 
  /console and another time for the shell to be started on /com1. 
  The proper device node has to be opened and dup() used to set up 
  stdin, stdout, and stderr each time. You will need to generalize 
  the code in init to accomplish this. 

  Perhaps the best way to do it 
  would be to define a table, where each entry gives the name of a 
  device node together with its major and minor device numbers. 
  When init starts, it could iterate through this table, ensuring 
  that each device node exists and starting a shell to run on that 
  device node. 
  */

  // iterate through the table of devices
  for(int i=0; i<NUM_DEV; i++) {

    // if a device doesn't exist, create it
    if(open(table[i].name, 0_RDWR) < 0) {
      mknod(table[i].name, table[i].major, table[i].minor);
      open(table[i].name, 0_RDWR);
    }

    // will run a shell on EVERY device
    dup(0); // stdout
    dup(0); // stderr
    // do i need stdin?
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
  }
}
