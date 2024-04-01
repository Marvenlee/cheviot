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
	
	
  printf("**** readfile test ****\n");
  
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
  
#if 0      
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
#else
	uint32_t *buffer_original, *buffer_out, *buffer_readback;
	
	buffer_original = virtualalloc(0x40000000, 1024 * 32, PROT_READ | PROT_WRITE);
	buffer_out = virtualalloc(0x40000000, 1024 * 32, PROT_READ | PROT_WRITE);
	buffer_readback = virtualalloc(0x40000000, 1024 * 32, PROT_READ | PROT_WRITE);

	
	if (buffer_original == NULL || buffer_out == NULL || buffer_readback == NULL) {
		printf("Failed to allocate buffer\n");
		close(fd);
		return 0;
	}	
	lseek64(fd, (uint64_t)2 * 1024 * 1024, SEEK_SET);	
	sc = read(fd, buffer_original, 32 * 1024);
	
	if (sc != 32 * 1024) {
		printf("Failed to read original :%d\n", sc);
		close(fd);
		return 0;	
	}
	
	for (int t=0;t<8092; t++) {
		buffer_out[t] = t | (t<<16);
	}

	printf("writing out test data\n");
	
	lseek64(fd, (uint64_t)2 * 1024 * 1024, SEEK_SET);
	sc = write(fd, buffer_out, 32 * 1024);

	if (sc != 32 * 1024) {
		printf("Failed to write test data :%d\n", sc);
		close(fd);
		return 0;	
	}


	printf("readback test data\n");
	
	lseek64(fd, (uint64_t)2 * 1024 * 1024, SEEK_SET);	
	sc = read(fd, buffer_readback, 32 * 1024);

	for (int t=0;t<8092; t++) {
		if (buffer_out[t] != buffer_readback[t]) {
			printf("readback failed at: %d, expected:%08x, got:%08x\n", t, buffer_out[t], buffer_readback[t]);
			close(fd);
			return 0;	
		}
	}
	
	printf("writing back original data\n");	
	lseek64(fd, (uint64_t)2 * 1024 * 1024, SEEK_SET);
	sc = write(fd, buffer_original, 32 * 1024);

	if (sc != 32 * 1024) {
		printf("Failed to write back original data :%d\n", sc);
		close(fd);
		return 0;	
	}

#endif





	printf("\n---- done ----\n");

	close(fd);
	return 0;
}




