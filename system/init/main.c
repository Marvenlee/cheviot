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
#include "init.h"

// Globals
static char *argv[ARG_SZ];
static char linebuf[256];
static char *src = NULL;
static bool tty_set = false;
static char *dummy_env[1];


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
  int status;
  struct stat st;
  char *line;
  char *cmd;
  char *buf;
  int sc;
  int pid;
  int startup_cfg_fd;

  log_info("**** /sbin/init started ****");

  dummy_env[0] = NULL;
  environ = dummy_env;
  
  log_info("opening etc/startup.cfg");
  
  if ((startup_cfg_fd = open("/etc/startup.cfg", O_RDWR)) < 0) {
    log_error("failed to open etc/startup.cfg");
    return -1;
  }

  log_info("opened etc/startup.cfg");
  
  if ((sc = fstat(startup_cfg_fd, &st)) != 0) {
    close(startup_cfg_fd);
    return -1;
  }
  
  log_info("statted etc/startup.cfg");
  log_info("startup.cfg size = %d", st.st_size);
    
  buf = malloc (st.st_size + 1);
  
  if (buf == NULL) {
    log_error("malloc buf failed");
    close(startup_cfg_fd);
    return -1;
  }

  
  read(startup_cfg_fd, buf, st.st_size);

  close(startup_cfg_fd);  
  buf[st.st_size] = '\0';
  src = buf;
   
  log_info("processing startup.cfg");
   
  while ((line = readLine()) != NULL) {
    cmd = tokenize(line);
    
    if (cmd == NULL || cmd[0] == '\0') {
      continue;
    }
    
    if (strncmp("#", cmd, 1) == 0) {
      // Comment
    } else if (strncmp("start", cmd, 5) == 0) {
      cmdStart();
    } else if (strncmp("waitfor", cmd, 7) == 0) {
      cmdWaitfor();
    } else if (strncmp("chdir", cmd, 5) == 0) {
      cmdChdir();
    } else if (strncmp("mknod", cmd, 5) == 0) {
      cmdMknod();
    } else if (strncmp("sleep", cmd, 5) == 0) {
      cmdSleep();
    } else if (strncmp("pivot", cmd, 5) == 0) {
      cmdPivot();
    } else if (strncmp("remount", cmd, 5) == 0) {
      cmdRemount();
    } else if (strncmp("setenv", cmd, 6) == 0) {
      cmdSetEnv();
    } else if (strncmp("settty", cmd, 6) == 0) {
      cmdSettty();
    } else {
      log_error("init script, unknown command: %s", cmd);
    }
  }
  
  do {
    pid = waitpid(0, &status, 0);
  } while (sc == 0);
  
  // TODO: Shutdown system/reboot
  while (1) {
    sleep(10);
  }
  
  return 0;
}


/* @brief   Handle a "start" command in startup.cfg
 *
 * format: start <exe_name> [optional args ...]
 */
int cmdStart (void) {
  char *tok;
  int pid;

  for (int t=0; t<ARG_SZ; t++)
  {
    tok = tokenize(NULL);
    argv[t] = tok;
    
    if (tok == NULL) {
      break;
    }
  }

  if (argv[0] == NULL) {
    exit(-1);
  }
    
  pid = fork();
  
  if (pid == 0) {  
    execve((const char *)argv[0], argv, NULL);
    exit(-2);
  }
  else if (pid < 0) {
    exit(-1);
  }
  
  return 0;
}


/* @brief   Handle a "mknod" command in startup.cfg
 * 
 * format: mknod <path> <st_mode> <type>
 */
int cmdMknod (void) {
  char *fullpath;
  struct stat st;
  uint32_t mode;
  char *typestr;
  char *modestr;
  int status;

  fullpath = tokenize(NULL);

  if (fullpath == NULL) {
    return -1;
  }
  
  modestr = tokenize(NULL);
  
  if (modestr == NULL) {
    return -1;
  }

  typestr = tokenize(NULL);
  
  if (typestr == NULL) {
    return -1;
  }
  
  mode = atoi(modestr);
  
  if (typestr[0] == 'c') {
    mode |= _IFCHR;    
  } else if (typestr[0] == 'b') {
    mode |= _IFBLK;
  } else {
    return -1;
  }
  
  st.st_size = 0;
  st.st_uid = 0;
  st.st_gid = 0;
  st.st_mode = mode;
  
  status = mknod2(fullpath, 0, &st);

  return status;
}


/* @brief   Handle a "chdir" command in startup.cfg
 *
 * format: chdir <pathname>
 */
int cmdChdir (void) {
  char *fullpath;
  
  fullpath = tokenize(NULL);

  if (fullpath == NULL) {
    return -1;
  }

  return chdir(fullpath);
}


/* @brief   Handle a "waitfor" command in startup.cfg
 *
 * format: waitfor <path>
 *
 * Waits for a filesystem to be mounted at <path>
 */
int cmdWaitfor (void) {
  char *fullpath;
  struct stat st;
  struct stat parent_stat;
  char path[PATH_MAX]; 
  char *parent_path;
  int fd = -1;
  struct pollfd pfd;
  int sc;

  fullpath = tokenize(NULL);
  
  if (fullpath == NULL) {
    exit(-1);
  }

  strcpy (path, fullpath);
     
  parent_path = dirname(path);
  
  if (stat(parent_path, &parent_stat) != 0) {
    exit(-1);
  }
  
  fd = open (fullpath, O_RDONLY);
  
  if (fd < 0) {
    exit(-1);
  }


#if 0
  int kq;
  struct kevent ev;
  struct timespec ts;

  if ((kq = kqueue()) != 0) {
    return -1;
  }

  ts.tv_sec = 0;
  ts.tv_nanoseconds = 200 * 1000000;

  // FIXME: Use EVFILT_FS, no filehandle needed, is notified on all
  // global mount/unmount changes.
      
  EV_SET(&ev, fd, EVFILT_VNODE, EV_ADD | EV_ENABLE, 0, 0, 0); 
  kevent(kq, &ev, 1,  NULL, 0, NULL);

  while(1) {
    if (stat(fullpath, &st) == 0 && st.st_dev != parent_stat.st_dev) {
      break;
    }

    kevent(kq, NULL, 0, &ev, 1, &ts);
  }
  
  close(kq);    

#else
  while (1) {
    if (stat(fullpath, &st) == 0 && st.st_dev != parent_stat.st_dev) {
      break;
    }    

    sleep(1);
  }
#endif

  close(fd);  
  return 0;  
}


/* @brief   Handle a "sleep" command in startup.cfg
 *
 * format: sleep <seconds>
 */
int cmdSleep (void) {
    int seconds = 0;
    char *tok;    

    tok = tokenize(NULL);
  
    if (tok == NULL) {
      return -1;
    }

    seconds = atoi(tok);
    sleep(seconds);    
    return 0;
}


/* @brief   Handle a "pivot" command in startup.cfg
 *
 * format: pivot <new_root_path> <old_root_path>
 */
int cmdPivot (void) {
  char *new_root;
  char *old_root;
  int sc;
  
  new_root = tokenize(NULL);  
  if (new_root == NULL) {
    return -1;
  }
  
  old_root = tokenize(NULL);
  if (old_root == NULL) {
    return -1;
  }
  
  sc = pivotroot (new_root, old_root);
  return sc;
}

/* @brief   Handle a "remount" command in startup.cfg
 *
 * format: remount <new_path> <old_path>
 */
int cmdRemount (void) {
  char *new_path;
  char *old_path;
  int sc;
  
  new_path = tokenize(NULL);  
  if (new_path == NULL) {
    return -1;
  }
  
  old_path = tokenize(NULL);
  if (old_path == NULL) {
    return -1;
  }
  
  sc = movemount (new_path, old_path);
  return sc;
}


/* @brief   Handle a "setty" command in startup.cfg
 *
 * format: settty <tty_device_path>
 */
int cmdSettty (void) {
  int fd;
  int old_fd;
  char *tty;

  tty = tokenize(NULL);
  
  if (tty == NULL) {
    return -1;
  }
  
  old_fd = open(tty, O_RDWR);  
  
  if (old_fd == -1) {
    return -1;
  }
  
  fd = fcntl(old_fd, F_DUPFD, 5);

  close(old_fd);

  dup2(fd, STDIN_FILENO);
  dup2(fd, STDOUT_FILENO);
  dup2(fd, STDERR_FILENO);
  
  setbuf(stdout, NULL);  
  PrintGreeting();

  tty_set = true;
  return 0;
}


/* @brief   Handle a "setenv" command in startup.cfg
 *
 * format: setenv <name> <value>
 */
int cmdSetEnv(void)
{
  char *name;
  char *value;
  
  name = tokenize(NULL);  
  if (name == NULL) {
    return -1;
  }
  
  value = tokenize(NULL);
  if (value == NULL) {
    return -1;
  }
  
  setenv (name, value, 1);
}


/* @brief   Split a line of text from startup.cfg into nul-terminated tokens
 *
 */
char *tokenize(char *line)
{
    static char *ch;
    char separator;
    char *start;
    
    if (line != NULL) {
        ch = line;
    }
    
    while (*ch != '\0') {
        if (*ch != ' ') {
           break;
        }
        ch++;        
    }
    
    if (*ch == '\0') {
        return NULL;
    }        
    
    if (*ch == '\"') {
        separator = '\"';
        ch++;
    } else {
        separator = ' ';
    }

    start = ch;

    while (*ch != '\0') {
        if (*ch == separator) {
            *ch = '\0';
            ch++;
            break;
        }           
        ch++;
    }
            
    return start;    
}


/* @brief   Read a line from startup.cfg
 *
 */
char *readLine(void)
{
    char *dst = linebuf;
    linebuf[255] = '\0';
    
    if (*src == '\0') {
        log_info("no more file to read, src:%08x", src);
        return NULL; 
    }
        
    while (*src != '\0' && *src != '\r' && *src != '\n') {
        *dst++ = *src++;
    }

    if (*src != '\0') {
        src++;
    }
    
    *dst = '\0';
    
    log_info("startup.cfg: %s", linebuf);
    
    return linebuf;
}


/* @brief   Print a test-card/greeting to stdout
 *
 */
void PrintGreeting(void)
{
	printf("\033[0;0H\033[0J");	
	printf("            \033[33mxodxxxxxooolooxkxxdcx\033[m\n");
	printf("              \033[32mkkkxo::do:cdxkkox\033[m\n");
	printf("            \033[32mXl;:cc::::::::cc;cOKWW\033[m\n");
	printf("          \033[32mKkoc:c:::::::::::::::::oxO\033[m\n");
	printf("        \033[32mOo:::;;;:ccccccccccccc:;;;;;cxK\033[m\n");
	printf("      \033[32mOc;;:::cccccccccccccccccccccc;;,,d\033[m\n");
	printf("     \033[32mk;,:ccccccccccccccccccccccccccccc:':X\033[m\n");
	printf("    \033[32md,:ccccccccccccccccccccccccccccccccc:c0\033[m\n");
	printf("   \033[32mo:ccccccccccc:c:;::;;;;:;:::ccccccccccccx\033[m\n");
	printf("  \033[32mdclccccc:;;:::;;;;:;,,,,::;;;:::;;:cccccc:k\033[m\n");
	printf("  \033[32mccccc:;;;,,'\033[m;okkOKNd....kNKOkxc'\033[32m;;;:;:ccccl\033[m\n");
	printf("  \033[32mccc:;;;,.   \033[mcWMMMMMX'..:NMMMMMk.\033[32m ..,;;,;ccl\033[m    hello AGAIN\n");
	printf("  \033[32mlc;;;,.     \033[mcWMMMMMW:..oMMMMWWO..\033[32m   ..;;;:l\033[m\n");
	printf("  \033[32mo;;:':'     \033[m:NMMMMX0l..xN0WMMMO..\033[32m    ..,:,o\033[m\n");
	printf("   \033[32m,c,;,'    \033[m..0MMMWl'c .dk'KMWMx  \033[32m    ..'::x\033[m\n");
	printf("    \033[32mlc;'..    \033[m.oWMMMX0c .cXKWMMWc. \033[32m    ..:ox\033[m\n");
	printf("       \033[32m0c.    \033[m..dWMMMN,...kMMMWx.  \033[32m   .,cm\033[m\n");
	printf("       \033[32mO:,.    \033[m..xWWWd....,KWKc.  \033[32m   .':;O\033[m\n");
	printf("      \033[32mO;,:c;.\033[m    .':,...   ... \033[32m    .,cc;;c\033[m\n");
	printf("    \033[32md:,:ccccc,.... ..      .......;cllcc,;;d\033[m\n");
	printf("    \033[32mKc:,:lcccc:codl;,......,:lxOXKlcllc;:c0\033[m\n");
	printf("     \033[32mXo:;:cccd0     \033[31mWK,.''cXW\033[32m      kc:;:oK\033[m\n");
	printf("      \033[32mWxlcoO      \033[31mW0c,'','',:xN\033[32m     0dxK\033[m\n");
	printf("                 \033[31mOl,',,,,,,,,':O\033[m\n\n");
}


