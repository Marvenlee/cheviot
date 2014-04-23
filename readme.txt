Cheviot Microkernel.(c) 2014 Marven Gilhespie

*****************************************************************************

The Microkenel is under the Apache license, see source code for details.

The boot loader makes use of code by John Cronnin to access the SD card
and bootstrap the kernel.  It has it's own copyright.

The safe-string copying functions, Strlcat and Strlcpy are the
copyright of Todd C. Miller.

*****************************************************************************

The Cheviot Microkernel is a kernel that combines with a File system/Root
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

* No kernel threads.  All processes will be multithreaded by user-mode
  coroutines.  Libtask used for coroutines. Coroutines are used to
  multithread servers such as the Filesys server.

* The microkernel implements an interrupt-model kernel with a single
  kernel stack per CPU.  Kernel calls are preemptive and restart from
  the beginning if an interrupt occurs.  Once a change needs to be
  committed to the microkernel data structures preemption is disabled.


*****************************************************************************
*****************************************************************************

To-Do / Doing / Roadmap

* Need scripts to build arm-eabi GCC and binutils 
* Need scripts to patch and build libtask and newlib.

* Clean up and decide which handles are passed to newly spawned
  processes.

* Add kernel daemon to periodically coalesce and compact memory.
  Add notification to Filesys to scale memory used for cache.

* Add timeout to VirtualAlloc system calls,  Add preemption to 
  VirtualAlloc, use continuation if necessary, Mark memory as
  supervisor, clear (with preemption) and then change to user-mode
  permissions.
  
  It is biggest path through kernel with preemption disabled.
  
* Add system event handles for equivalent to SIGHUP, SIGTERM and
  low memory conditions
  
* Started writing Filesys server which is the root process.
  Each open file will have it's own channel and a coroutine to handle
  requests.
  
  Copyied over previous block buffer cache code and skeleton code
  from x86 kernel.  Will be replaced by segment cache, where each
  segment contain a part of a file or directory.
  
  Will map files loaded by bootloader as cached file segments, avoids
  needing a boot image/filesystem.  Path to these entries can be
  found using the directory lookup cache.
  
  Need to decide on file interface, how reads and writes occur.  How
  we do zero-copy reading and writing.  Whether we expose the interface
  to the cache such as using BRead(), GetBlk() and BRelse() 

  Need to add named channel/port handling.
   
  Ports possibly in /ports directory,  /ports/system and /ports/user.
  Partitions mounted in root directory by volume name.
  
  Need to limit what ports can be accessed by whom.  Originally intended
  to use the first 10 UIDs as rings.  May instead limit access for drivers
  and modules by making /ports/system directory owned by root.  Drivers
  and such can then be limited by chroot to directories within /ports/system.
  
* Get some processes running, drivers etc.  

