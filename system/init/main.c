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

#define NDEBUG

// Constants
#define ARG_SZ 256

// Globals
extern char **environ;

static char *argv[ARG_SZ];
static char linebuf[256];
static char *src = NULL;
static bool tty_set = false;

static char *dummy_env[1];


// Prototypes
int DoInit(void);
int cmdStart (void);
int cmdChdir (void);
int cmdMknod (void);
int cmdWaitfor (void);
int cmdSleep (void);
int cmdSettty (void);
int cmdPivot (void);
int cmdRemount (void);
int cmdSetEnv(void);
char *tokenize(char *line);
char *readLine(void);
void PrintGreeting(void);



int main(int argc, char **argv) {
  int rc;

  dummy_env[0] = NULL;
  environ = dummy_env;

  rc = DoInit();

  while (1) {
    Sleep(10);
  }
}


int DoInit(void) {
  int status;
  struct stat stat;
  char *line;
  char *cmd;
  char *buf;
  int sc;
  int pid;
  int startup_cfg_fd;
  
  
  if ((startup_cfg_fd = open("/etc/startup.cfg", O_RDWR)) < 0) {
    KLog("Failed to open /etc/startup.cfg");
    return -1;
  }
  
  if ((sc = fstat(startup_cfg_fd, &stat)) != 0) {
    KLog ("fstat failed %d", sc);
    close(startup_cfg_fd);
    return -1;
  }
  
  buf = malloc (stat.st_size + 1);
  
  if (buf == NULL) {
    KLog ("malloc failed");
    close(startup_cfg_fd);
    return -1;
  }

  read(startup_cfg_fd, buf, stat.st_size);
  close(startup_cfg_fd);
  
  buf[stat.st_size] = '\0';
  src = buf;
  
  while ((line = readLine()) != NULL) {
    cmd = tokenize(line);
    
    if (cmd == NULL || cmd[0] == '\0')
    {
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
      KLog ("UNKOWN CMD: %s", cmd);
    }
  }
 
 
  do {
    pid = waitpid(0, &status, 0);
  } while (sc == 0);
  
  // Shutdown system/reboot
  while (1) {
    Sleep(10);
  }
  
  return 0;
}

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
    KLog ("no arg");
    exit(-1);
  }
    
  pid = fork();
//  KLog ("pid = %d", pid);
  
  if (pid == 0) {  
    execve((const char *)argv[0], argv, NULL);
    
//    KLog ("execve failed argv[0] = %08x", argv[0]);
 //   KLog ("execve failed exe = %s", argv[0]);
    
    exit(-2);
  }
  else if (pid < 0) {
    exit(-1);
  }
  
//  KLog ("** start done, pid = %d", pid);
  return 0;
}


int cmdMknod (void) {
  char *fullpath;
  struct stat stat;
  uint32_t mode;
  char *typestr;
  char *modestr;
  int status;
  
  fullpath = tokenize(NULL);

  if (fullpath == NULL) {
    KLog ("fullpath = null");
    return -1;
  }
  
  modestr = tokenize(NULL);
  
  if (modestr == NULL) {
    KLog ("modestr = null");
    return -1;
  }

  typestr = tokenize(NULL);
  
  if (typestr == NULL) {
    KLog ("typestr = null");
    return -1;
  }
  
  mode = atoi(modestr);
  
  if (typestr[0] == 'c') {
    mode |= _IFCHR;    
  } else if (typestr[0] == 'b') {
    mode |= _IFBLK;
  } else {
    KLog ("******************** init: Unknown mknod type, %c", typestr[0]);
    return -1;
  }
  
  stat.st_size = 0;
  stat.st_uid = 0;
  stat.st_gid = 0;
  stat.st_mode = mode;
    
  status = MkNod(fullpath, 0, &stat);

  return status;
}

  // Need to be able to open regardless of dir/file or whatever.

int cmdChdir (void) {
  char *fullpath;
  
  fullpath = tokenize(NULL);

  if (fullpath == NULL) {
    return -1;
  }
  
  return chdir(fullpath);
}



int cmdWaitfor (void) {
  char *fullpath;
  struct stat stat;
  struct stat parent_stat;
  char path[PATH_MAX]; 
  char *parent_path;
  int fd = -1;
  struct pollfd pfd;
  int sc;
  
  fullpath = tokenize(NULL);
  
  if (fullpath == NULL) {
    KLog ("path is null");
    exit(-1);
  }

  strcpy (path, fullpath);
     
  parent_path = dirname(path);
  
  if (Stat(parent_path, &parent_stat) != 0) {
    KLog ("waitfor failed to fstat parent dir");
    exit(-1);
  }
  
  fd = open (fullpath, O_RDONLY);
  
  if (fd < 0) {
    KLog ("failed to open %s for waitfor", fullpath);
    exit(-1);
  }

  while (1) {
    pfd.fd = fd;
    pfd.events = POLLPRI;
    pfd.revents = 0;
    
    sc = Poll (&pfd, 1, -1);
  
    if (sc != 0) {
      goto exit;
    }
  
    if (Stat(fullpath, &stat) != 0) {
      KLog ("waitfor failed to stat file");
//      sleep (-1);
      continue;
    }
    
    if (stat.st_dev != parent_stat.st_dev) {
      KLog ("Found matching device");
      break;
    }
    
  }
  
  
exit:  
  close(fd);
  return 0;  
}



int cmdSleep (void) {
    int seconds = 0;
    char *tok;

    tok = tokenize(NULL);
  
    if (tok == NULL) {
      KLog ("cmdSleep arg failed");
      return -1;
    }

    seconds = atoi(tok);
    Sleep(seconds);    
    return 0;
}


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
  
  sc = PivotRoot (new_root, old_root);

  if (sc == 0) {
    Debug ("!!!!!!!! pivotted !!!!!!!!!!!!!!");  
  } else {
    Debug ("!!!!!!!! PIVOT FAILED !!!!!!!!!!!");  
  }

  return sc;
}


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
  
//  sc = Remount (new_path, old_path);

  if (sc == 0) {
    KLog ("!!!!!!!! remounted !!!!!!!!!!!!!!");  
  } else {
    KLog ("!!!!!!!! REMOUNT FAILED !!!!!!!!!!!");  
  }

  return sc;
}


int cmdSettty (void) {
  int fd;
  int old_fd;
  char *tty;
  
  Debug("***** cmdSetty ******");

  tty = tokenize(NULL);
  
  if (tty == NULL) {
    KLog ("************* cmdSettty tty = NULL");
    return -1;
  }
  
  old_fd = open(tty, O_RDWR);  
  
  if (old_fd == -1) {
    KLog ("************* cmdSettty failed open %s", tty); 
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

/*!
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



/*!
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
    
    return linebuf;
}


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


