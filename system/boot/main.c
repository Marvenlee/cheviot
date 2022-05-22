#include "bootstrap.h"
#include "dbg.h"
#include "elf.h"
#include "globals.h"
#include "memory.h"
#include "types.h"
#include <stdint.h>
#include <string.h>

#define KDEBUG

void memcpy (void *dst, void *src, size_t sz);

extern uint32_t rootfs_image_offset;
extern uint32_t rootfs_image_size;

/* @brief Boot the operating system and map the IFS image to top of memory
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

  gpio_regs = (struct bcm2835_gpio_registers *)GPIO_BASE;
  dbg_init();

  KLog("Bootloader");

  bootinfo.mem_size = init_mem();
  ifs_image = ALIGN_DOWN((bootinfo.mem_size - rootfs_image_size), 0x10000);

  memcpy((void *)ifs_image, (uint8 *)0x8000 + rootfs_image_offset,
         rootfs_image_size);

  if (elf_load((void *)ifs_image, rootfs_image_size, "kernel", &bootinfo.kernel_ehdr, &bootinfo.kernel_phdr[0], &kernel_ceiling) != 0) {
    KPanic("Cannot load kernel");
  }

  if (elf_find((void *)ifs_image, rootfs_image_size, "sbin", "ifs",  &ifs_exe_base, &ifs_exe_size) == -1) {
    KPanic("cannot find IFS.exe in IFS image");
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
 
  BootstrapKernel(kernel_ceiling);
  kernel_entry_point(&bootinfo);

  while (1) {
  }
}

