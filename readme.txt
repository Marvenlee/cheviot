Cheviot Microkernel.(c) 2014 Marven Gilhespie

*****************************************************************************

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

* Add compaction kernel task to periodically coalesce and compact memory.

* Possibly add Unix signals and some way of notifying the Executive of
  changing levels of free memory so as to dynamically resize the file
  cache.  Add timeouts to the VirtualAlloc calls.
  
* Begin to get the Executive/root process running.  Begin porting some
of the previous file system code from i386 kernel.

* Begin adding drivers/file system support.

* It is undecided whether the Executive will be the parent process of
  all processes in the system.  Spawn() is a privileged call and must
  be called by the Executive.
  
  The Executive can either pass the handle to the real parent process
  or can be the parent and keep the handle, create a Notification and
  pass that to the real parent process.
  

  
  




