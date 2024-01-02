# Current Status

Cheviot is a long way from being a complete OS.  In it's current state it boots
into a KSH shell with the IFS image mounted as root.  This allows some basic
coreutils commands to be used to test out the system calls and C library.


## Short term goals
  * Add Raspberry Pi 4 support.
  * HAL libraries for Pi 1 and Pi 4 boards, replace existing code in kernel, bootloader and drivers.
  * Extended file system (Ext2) support by finishing system/filesystem/extfs.
  * Finish file system syscalls. 
  * Update build scripts to use 'pseudo' to allow files in SD-Card image to have root UID and GID. 
  * Virtual memory management fixes.
  * File system locking and vnode reference count fixes
  * Directory Name Lookup Cache (Code is disabled due to bugs)  
  * Signal handling (API exists but handlers are not called)
  * Revert to stride scheduler fixes.
  * Support custom interrupt handlers, current drivers are polling
  * Support interrupt handlers running with user-space privileges  
    
## Long term goals

  * POSIX threads, currrently supports libtask coroutines, mainly used in serial driver.
  * Fine grained locking in kernel to replace the Big Kernel Lock.
  * Kernel preemption. Currently tasks are scheduled cooperatively in the kernel.
  * Replace VirtualAlloc functions with mmap, munmap and mprotect
  * Convert memory management segment array to linked lists.
  * Additional drivers for GPIO pins, etc.
  * XML device tree and mechanism to limit drivers access to RAM, interrupts and other drivers
  * Similar XML capability mechanism to limit syscalls
  * Self host GCC and binutils.
  * Port of a text editor
  * Multiple virtual protection rings, see future.md.
  
  
