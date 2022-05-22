#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/debug.h>
#include <sys/dirent.h>
#include <sys/signal.h>
#include <sys/syscalls.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/syslimits.h>
#include <sys/wait.h>

/*
 * Root,  
 */

int main(int argc, char **argv) {
  int rc;
    
  rc = fork();

  if (rc > 0) {
    exec_init_task();


  } else {
    while (1)
    {
      sc = waitpid();
      
      if (errno == ENOCHLD)
      {
        Shutdown();        
      }
    }
  }
}








