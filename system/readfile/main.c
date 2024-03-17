#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/debug.h>
#include <sys/dirent.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/syscalls.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/syslimits.h>
#include <sys/wait.h>
#include <sys/mount.h>



/* @brief   Main function of system init process
 *
 * @param   argc, unused
 * @param   argv, unused
 * @return  0 on success, non-zero on failure
 *
 * This process is embedded on the initial file system (IFS) along with the
 * kernel and a few other tools. It's purpose is to start any additional
 * servers and device drivers during the inital phase of system startup.
 * 
 * Once other servers that handle the on-disk root filesystem are initialized
 * then this performs a pivot-root, where the root "/" path is pivotted
 * from pointing to the IFS image's root to that of on-disk root filesystem.
 *
 * The IFS executable is the first process started by the kernel. The IFS forks
 * so that the child process becomes the IFS handler server and the parent/root
 * process performs an exec of "/sbin/init" to become this process.
 */
int main(int argc, char **argv)
{
  struct stat st;
  int sc;
	char *buf;
	int fd;
	ssize_t sz;
	
  printf("**** readfile ****\n");
  
  if (argc < 2) {
		printf("args missing\n");
		exit(-1);
	}
  
  printf("opening %s\n", argv[1]);
  
  if ((fd = open(argv[1], O_RDWR)) < 0) {
    log_error("failed to open %s", argv[1]);
    return -1;
  }

  
  if ((sc = fstat(fd, &st)) != 0) {
    close(fd);
    return -1;
  }
  

  printf("stat file size = %d\n", st.st_size);
    
  buf = malloc (st.st_size + 1);

	sz = read(fd, buf, st.st_size);
	
	printf ("nbytes read: %d\n", sz);

	printf ("---------\n");
	fflush(stdout);	
	sleep(2);
	
	for (int t=0; t<st.st_size; t++) {
		printf("%c", buf[t]);
	}

	sleep(1);
	fflush(stdout);
	sleep(3);

	printf("\n---- done ----\n");

	close(fd);
	return 0;
}




