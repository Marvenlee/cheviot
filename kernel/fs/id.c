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



int GetPID (void) {
  struct Process *current;  

  current = GetCurrentProcess();
  return current->pid;    
}

int GetPPID (void) {
  struct Process *current;  

// Will need a proc lock if going to fine grained locking
  current = GetCurrentProcess();
  return current->parent->pid;
}

int GetUID (void) {
  struct Process *current;  

  current = GetCurrentProcess();
  return current->uid;    
}

int GetGID (void) {
  struct Process *current;  

  current = GetCurrentProcess();
  return current->gid;    
}

int GetEUID (void) {
  struct Process *current;  

  current = GetCurrentProcess();  
  return current->euid;    
}

int GetEGID (void) {
  struct Process *current;  

  current = GetCurrentProcess();
  return current->egid;    
}

int SetUID (int uid) {
  struct Process *current;  

  current = GetCurrentProcess();  
  if (current->uid != 0 && current->gid != 0) {
    return -EPERM;
  }
  
  current->uid = uid;
  return 0;    
}
   
int SetGID (int gid) {
  struct Process *current;  
 
  current = GetCurrentProcess();
  if (current->uid != 0 && current->gid != 0) {
    return -EPERM;
  }
  
  current->gid = gid;
 
  return 0;    
}

int SetPGRP (void) {
  struct Process *current;  

  current = GetCurrentProcess();
  current->pgrp = current->pid;
  return 0;
}

int GetPGRP (void) {
  struct Process *current;  

  current = GetCurrentProcess();
  return current->pgrp;
}





