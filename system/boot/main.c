#include <string.h>
#include "bootstrap.h"
#include "debug.h"
#include "elf.h"
#include "globals.h"
#include "memory.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "machine/cheviot_hal.h"


// Macros for address alignment. TODO: Replace with libc roundup and rounddown
#define ALIGN_UP(val, alignment)                                               \
  ((((val) + (alignment)-1) / (alignment)) * (alignment))

#define ALIGN_DOWN(val, alignment) ((val) - ((val) % (alignment)))


// Prototypes (not picked up in string.h)
void *	 memchr (const void *, int, size_t);
int 	   memcmp (const void *, const void *, size_t);
void *	 memcpy (void *__restrict, const void *__restrict, size_t);
void *	 memmove (void *, const void *, size_t);
void *	 memset (void *, int, size_t);

// External references
extern uint32_t rootfs_image_offset;
extern uint32_t rootfs_image_size;


/* @brief   Bootloader for Raspberry Pi
 *
 * Boot the operating system and map the IFS image to top of memory
 * 
 * kernel.img contains this bootloader and the IFS file system image.
 * Bootloader starts at 0x8000 (32K)
 * IFS image directly follows bootloader code and is relocated to top of RAM by bootloader.
 * Loads Kernel physically mapped at 0x100000, saves ceiling address in bootinfo
 * Find /sbin/ifs.exe in IFS image and stores address and size in bootInfo.
 * Set up initial pagetables of bootloader and kernel.
 * Kernel is mapped at 0x80100000
 * Bootloader calls the kernel entry point and passes the bootinfo structure
 */
void Main(void) {
  vm_addr kernel_ceiling;
  vm_addr ifs_image;
  vm_addr ifs_exe_base;
  vm_addr ifs_exe_size;

  void (*kernel_entry_point)(struct BootInfo * bi);

  board_init();

  boot_log_info("Bootloader");

  bootinfo.mem_size = get_physical_mem_size();
  ifs_image = ALIGN_DOWN((bootinfo.mem_size - rootfs_image_size), 0x10000);

  memcpy((void *)ifs_image, (uint8_t *)0x8000 + rootfs_image_offset,
         rootfs_image_size);

  if (elf_load((void *)ifs_image, rootfs_image_size, "kernel", &bootinfo.kernel_ehdr, &bootinfo.kernel_phdr[0], &kernel_ceiling) != 0) {
    boot_panic("Cannot load kernel");
  }

  if (elf_find((void *)ifs_image, rootfs_image_size, "sbin", "ifs",  &ifs_exe_base, &ifs_exe_size) == -1) {
    boot_panic("cannot find IFS.exe in IFS image");
  }
  bootinfo.screen_buf = screen_buf;
  bootinfo.screen_width = screen_width;
  bootinfo.screen_height = screen_height;
  bootinfo.screen_pitch = screen_pitch;
  bootinfo.ifs_image = ifs_image;
  bootinfo.ifs_image_size = rootfs_image_size;
  bootinfo.ifs_exe_base = ifs_exe_base;
  bootinfo.ifs_exe_size = ifs_exe_size;
 
  kernel_entry_point = (void *)bootinfo.kernel_ehdr.e_entry;
 
  boot_log_info("kernel_entry: %08x", (uint32_t)kernel_entry_point);
 
  BootstrapKernel(kernel_ceiling);
  
  boot_log_info("Calling kernel_entry_point %08x",(uint32_t)kernel_entry_point);
  
  kernel_entry_point(&bootinfo);

  while (1) {
  }
}

