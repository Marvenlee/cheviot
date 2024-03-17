/*
 * Copyright 2022  Marven Gilhespie
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *http:"www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS",BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define KDEBUG 1

/*
 * ARM-specific process creation and deletion code.
 */
 
#include <kernel/board/arm.h>
#include <kernel/board/globals.h>
#include <kernel/board/init.h>
#include <kernel/board/task.h>
#include <kernel/dbg.h>
#include <kernel/globals.h>
#include <kernel/proc.h>
#include <kernel/types.h>
#include <kernel/utility.h>
#include <string.h>

#ifdef SYSCALL_TRACE

const char *syscall_names[] = {
  "sys_unknownsyscallhandler",
  "sys_debug",
  "sys_fork",  
  "sys_exit",
  "sys_waitpid",
  "sys_kill",
  "sys_setschedparams", 

  "sys_sleep",   
  "sys_alarm",
  "sys_nanosleep",
  "sys_gettimeofday",
  "sys_settimeofday",
    
  "sys_virtualalloc",
  "sys_virtualallocphys",
  "sys_virtualfree",
  "sys_virtualprotect",

  "sys_createinterrupt",
  "sys_maskinterrupt",
  "sys_unmaskinterrupt",

  "sys_set_syslog_file",
  "sys_exec",   

  "sys_createmsgport", 
  "sys_deprecatedsyscall",
    
  "sys_sendrec", 
  "sys_getmsg",  
  "sys_replymsg",
  "sys_readmsg",
  "sys_writemsg",
   
  "sys_open",   
  "sys_close",
  "sys_dup",
  "sys_dup2",
    
  "sys_read",
  "sys_write",
  "sys_lseek",
  "sys_lseek64",
    
  "sys_truncate",
  "sys_unlink",

  "sys_createdir",
  "sys_opendir",
  "sys_readdir",
  "sys_rewinddir",
  "sys_rmdir",

  "sys_rename",

  "sys_pipe",
  "sys_deprecatedsyscall",

  "sys_chdir",
  "sys_fchdir",  

  "sys_stat",
  "sys_fstat",

  "sys_symlink",
  "sys_readlink",

  "sys_chmod",
  "sys_chown",
  "sys_access",
  "sys_umask",

  "sys_getpid",
  "sys_getppid",
  "sys_getuid",
  "sys_getgid",
  "sys_geteuid",
  "sys_getegid",
    
  "sys_setuid",
  "sys_setgid",
  "sys_setpgrp",    
  "sys_getpgrp",
    
  "sys_virtualtophysaddr",
    
  "sys_signalnotify", 
  "sys_knotei",
    
  "sys_pivotroot",

  "sys_fcntl",  
  "sys_isatty",  
  "sys_ioctl",    
  "sys_sync",
  "sys_fsync",

  "sys_sigaction",
  "sys_sigprocmask",
  "sys_sigpending",
  "sys_sigsuspend",
    
  "sys_mknod",
  "sys_renamemsgport",
    
  "sys_chroot",

  "sys_kqueue", 
  "sys_kevent",

	"sys_setegid",
	"sys_seteuid",
	"sys_issetugid",
	"sys_setgroups",
	"sys_getgroups",
	"sys_getpriority",
	"sys_setpriority",
	"sys_deprecatedsyscall",
	"sys_fchmod",
	"sys_fchown",

	"sys_clock_gettime",

	"sys_rpi_mailbox",
	"sys_rpi_configure_gpio",
	"sys_rpi_set_gpio",

	"sys_clock_settime"
};


/* @brief		Debugging function to log system calls and registers
 *
 * TODO: print logging of system call
 */
void syscall_trace(int syscall, struct UserContext *context)
{
}


#endif

