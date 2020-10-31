#include "bootstrap.h"
#include "dbg.h"
#include "elf.h"
#include "globals.h"
#include "memory.h"
#include "types.h"
#include <stdint.h>
#include <string.h>

/*!
    C entry point of bootloader, Initialises sd card and fat filesystem driver.
    Loads kernel and any modules before setting up page tables and enabling
   paging
    with physical memory mapped to kernel address at 0x80000000.  Calls kernel
   entry
    point with a pointer to the initialised bootinfo structure.
*/

extern uint32_t rootfs_image_offset;
extern uint32_t rootfs_image_size;

void Main(void) {
  vm_addr kernel_ceiling;
  vm_addr ifs_image;

  void (*kernel_entry_point)(struct BootInfo * bi);

  gpio_regs = (struct bcm2835_gpio_registers *)GPIO_BASE;
  dbg_init();

  KLog("Hello World!");

  bootinfo.mem_size = init_mem();
  ifs_image = ALIGN_DOWN((bootinfo.mem_size - rootfs_image_size), 0x10000);

  KLOG("mem_size = %d", bootinfo.mem_size);
  KLOG("root_fs_image_offset = %x", rootfs_image_offset);
  KLOG("root_fs_image_size = %x", rootfs_image_size);
  KLOG("ifs_image new address = %08x\n", ifs_image);

  memcpy((void *)ifs_image, (uint8 *)0x8000 + rootfs_image_offset,
         rootfs_image_size);

  KLog("ifs_image = %08x", (vm_addr)ifs_image);

  if (elf_load((void *)ifs_image, rootfs_image_size,
               (void **)&kernel_entry_point, &kernel_ceiling) == -1) {
    KPanic("Cannot load kernel");
  }

  bootinfo.screen_buf = screen_buf;
  bootinfo.screen_width = screen_width;
  bootinfo.screen_height = screen_height;
  bootinfo.screen_pitch = screen_pitch;
  bootinfo.ifs_image = ifs_image;
  bootinfo.ifs_image_size = rootfs_image_size;

  BootstrapKernel(kernel_ceiling);
  kernel_entry_point(&bootinfo);
  while (1)
    ;
}
