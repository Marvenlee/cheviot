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

	log_info("sizeof stat st: %d", sizeof (struct stat));
    
  buf = malloc (st.st_size + 1);
  
  if (buf == NULL) {
    log_error("malloc buf failed");
    close(startup_cfg_fd);
    return -1;
  }
  
  read(startup_cfg_fd, buf, st.st_size);
  buf[st.st_size] = '\0';

  close(startup_cfg_fd);  

  log_info("processing startup.cfg");

  src = buf;   

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
    } else if (strncmp("renamemount", cmd, 5) == 0) {
      cmdRenameMount();
    } else if (strncmp("setenv", cmd, 6) == 0) {
      cmdSetEnv();
    } else if (strncmp("settty", cmd, 6) == 0) {
      cmdSettty();
    } else if (strncmp("printgreeting", cmd, 6) == 0) {
      cmdPrintGreeting();
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


/* @brief   Read a line from startup.cfg
 *
 */
char *readLine(void)
{
    char *dst = linebuf;
    linebuf[255] = '\0';
    
    if (*src == '\0') {
        return NULL; 
    }
        
    while (*src != '\0' && *src != '\r' && *src != '\n') {
        *dst++ = *src++;
    }

    if (*src != '\0') {
        src++;
    }
    
    *dst = '\0';
    
#if 0
    log_info("startup.cfg: %s", linebuf);
#endif
    
    return linebuf;
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
int cmdRenameMount (void) {
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
  
  sc = renamemsgport(new_path, old_path);
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

  log_info("***** cmdSettty *****");

  tty = tokenize(NULL);
  
  if (tty == NULL) {
    log_info("tty token not present");
    return -1;
  }
  
  log_info("open tty, path:%s", tty);
  
  old_fd = open(tty, O_RDWR);  
  
  if (old_fd == -1) {
    log_info("open tty failed");    
    return -1;
  }
  
  fd = fcntl(old_fd, F_DUPFD, 20);

  log_info("fcntl old_fd=%d, res fd=%d", old_fd, fd);    

  close(old_fd);

  dup2(fd, STDIN_FILENO);
  dup2(fd, STDOUT_FILENO);
  dup2(fd, STDERR_FILENO);
  
  setbuf(stdout, NULL);
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


/* @brief   Print a CheviotOS greeting to stdout
 *
 */
void cmdPrintGreeting(void)
{
#if 1
  printf("\033[0;0H\033[0J\r\n\n");

  printf("  \033[34;1m   .oooooo.   oooo                               o8o                .   \033[37;1m      .oooooo.    .ooooooo.  \n");
  printf("  \033[34;1m  d8P'  `Y8b  `888                               `^'              .o8   \033[37;1m     d8P'  `Y8b  d8P'   `Y8b \n");
  printf("  \033[34;1m 888           888 .oo.    .ooooo.  oooo    ooo oooo   .ooooo.  .o888oo \033[37;1m    888      888 Y88bo.      \n");
  printf("  \033[34;1m 888           888P^Y88b  d88' `88b  `88.  .8'  `888  d88' `88b   888   \033[37;1m    888      888  `^Y8888o.  \n");
  printf("  \033[34;1m 888           888   888  888ooo888   `88..8'    888  888   888   888   \033[37;1m    888      888      `^Y88b \n");
  printf("  \033[34;1m `88b    ooo   888   888  888    .o    `888'     888  888   888   888 . \033[37;1m    `88b    d88' oo     .d8P \n");
  printf("  \033[34;1m  `Y8bood8P'  o888o o888o `Y8bod8P'     `8'     o888o `Y8bod8P'   `888' \033[37;1m     `Y8bood8P'  `^888888P'  \n");
  printf("\n");
  printf("\033[0m\n");
#endif
}


