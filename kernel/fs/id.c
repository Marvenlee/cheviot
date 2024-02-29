/*
 * Copyright 2014  Marven Gilhespie
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <sys/types.h>
#include <kernel/proc.h>
#include <kernel/dbg.h>

int sys_getpid (void) {
  struct Process *current;  

  current = get_current_process();
  return current->pid;    
}

int sys_getppid (void) {
  struct Process *current;  

// Will need a proc lock if going to fine grained locking
  current = get_current_process();
  return current->parent->pid;
}

int sys_getuid (void) {
  struct Process *current;  

  current = get_current_process();
  return current->uid;    
}

int sys_getgid (void) {
  struct Process *current;  

  current = get_current_process();
  return current->gid;    
}

int sys_geteuid (void) {
  struct Process *current;  

  current = get_current_process();  
  return current->euid;    
}

int sys_getegid (void) {
  struct Process *current;  

  current = get_current_process();
  return current->egid;    
}

int sys_setuid (int uid) {
  struct Process *current;  

  current = get_current_process();  
  if (current->uid != 0 && current->gid != 0) {
    return -EPERM;
  }
  
  current->uid = uid;
  return 0;    
}
   
int sys_setgid (int gid) {
  struct Process *current;  
 
  current = get_current_process();
  if (current->uid != 0 && current->gid != 0) {
    return -EPERM;
  }
  
  current->gid = gid;
 
  return 0;    
}

int sys_setpgrp (void) {
  struct Process *current;  

  current = get_current_process();
  current->pgrp = current->pid;
  return 0;
}

int sys_getpgrp (void) {
  struct Process *current;  

  current = get_current_process();
  return current->pgrp;
}



int sys_setegid (int gid)
{
	Info("sys_setegid");
	return 0;
}

int sys_seteuid(int uid)
{
	Info("sys_seteuid");
	return 0;
}

int sys_issetugid(void)
{
	Info("sys_issetugid");
	return 0;
}

int sys_setgroups (void)
{
	Info("sys_setgroups");
	return -ENOSYS;
}

int sys_getgroups(void)
{
	Info("sys_getgroups");
	return -ENOSYS;
}



