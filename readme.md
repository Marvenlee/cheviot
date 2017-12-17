Cheviot Microkernel
(c) 2014 Marven Gilhespie

*****************************************************************************

The Microkenel is under the Apache license, see source code for details.

The boot loader makes use of code by John Cronnin to access the SD card
and bootstrap the kernel.  It has it's own copyright.

The safe-string copying functions, Strlcat and Strlcpy are the
copyright of Todd C. Miller.

*****************************************************************************

The Cheviot Microkernel is in a project in a state of flux.  The original
goal back in around 2011 was to experiment with SASOS (Single Address Space
Operating System) ideas on x86 hardware.  This was to have a message passing
scheme where an area of memory was unmapped from one process and into another
at the same virtual address.

Then I got a Raspberry Pi in 2013 and wanted to start experimenting with it.
The code was ported over so that I could have a basic scheduler and some memory
management code working.  The SASOS and message passing code were ripped out
and I began converting to a normal process address space model whilst I thought
of alternate ways of doing message passing.

At the time I was also experimenting with coroutines to multithread servers
such as the filesystem.  That code is still there but does not align with any
plans for message passing.  It is left in as it may be a basis for future
designs.

In 2014 I decided to park the code as I concentrated on other things.

This commit has been to clean up the code and get whatever is in the code
actually running.

Currently I have kernel mapped at 2GB in virtual address space, timers and
interrupts appear to be working.  Syscalls are also working, though not all
are implemented.

Process creation and joining calls currently do not work, neither does
dynamic memory allocation.  The only processes are the idle task and the
root process.

The root process in part of the bootloader which in turn is responsible for
setting up initial page tables to bootstrap the kernel into a high virtual
address.

Some work on setting up the build environment in setupdev needs to be done.

Future notes will be posted on my blog at www.gilhespie.com.







