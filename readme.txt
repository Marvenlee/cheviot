Cheviot Microkernel.by Marven Gilhespie

The Microkenel is under the Apache license, see source code for details.

The boot loader makes use of code by John Cronnin to access the SD card
and bootstrap the kernel.  It has it's own copyright.

The safe-string copying functions, Strlcat and Strlcpy are the
copyright of Todd C. Miller.

*****************************************************************************

The Cheviot Microkernel is a kernel that combines with an Executive/Root
process to provide basic OS functionality.


A brief description of the goals and features:

* Designed to run on Raspberry Pi hardware.

* Single address space operating system.  All processes share the same
  virtual space but are isolated into there own domains.

* Memory is managed as physically contiguous segments. A segment may
  only accessible by one process at a time.

* Physical memory segments can be compacted. As memory is phyiscally
  contiguous, it acts as a form of page coloring. It enables a CPU's
  large page sizes to be used to improve TLB  coverage for suitably
  aligned memory.

* Messages are passed as segments.  This makes the message passing
  functions simpler to use and to implement. The code is streamlined
  with only a few data structures involved.

* Passing segments avoids copying,  segments can be quickly passed
  between multiple processes

* Handles to Channels (interprocess communication links), timers and
  other kernel objects can be embedded in messages and passed to other
  processes.

* The Executive (or Root process) will implement the bulk of 
  the file system interface and file caching.  The executive will
  also manage a namespace for public message ports.
  
* The Executive's file system code will support zero-copy access to
  files, with parts of files mapped as segments..  

* There are no kernel threads.  All processes will be multithreaded
  by user-mode coroutines.  Libtask by Russ Cox provides great support
  for coroutines.  Coroutines are used to multithread servers such as
  the Executive and its file system.

* The microkernel implements an interrupt-model kernel with a single
  kernel stack per CPU.  Kernel calls are preemptive and restart from
  the beginning if an interrupt occurs.  Once a change needs to be
  committed to the microkernel data structures preemption is disabled.


*****************************************************************************

To-Do

* Add a patch for Libtask to this repository so that others can begin using
  this, even as a simple single tasking embedded OS.

* Add a directory containing the Newlib /sys/cheviot source files so
  that Newlib can be used by others.
  
* Begin writing the Executive/root process.  Test various system calls.

* Begin adding drivers/file system support.

* It is undecided whether the Executive will be the parent process of
  all processes in the system.  The reason for this is that Spawn() is
  a privileged call, The Executive loads the segments and passes them
  to Spawn.  Handles to message ports are passed to the new process.
  A 'Notification' event could be passed to the parent process to 
  simulate passing the handle.
  
  Alternatively the actual child process's handle could be passed by
  the Executive to the real parent process.
  

  
  




