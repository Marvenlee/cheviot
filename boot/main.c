#include "types.h"
#include "block.h"
#include "fat.h"
#include "dbg.h"
#include "elf.h"
#include "memory.h"
#include "globals.h"
#include "bootstrap.h"
#include "root.h"
#include "module.h"


/*!
    C entry point of bootloader, Initialises sd card and fat filesystem driver.
    Loads kernel and any modules before setting up page tables and enabling paging
    with physical memory mapped to kernel address at 0x80000000.  Calls kernel entry
    point with a pointer to the initialised bootinfo structure.
*/
void Main (void)
{    
	void (*kernel_entry_point)(struct BootInfo *bi);

	gpio_regs = (struct bcm2835_gpio_registers *)GPIO_BASE;

	dbg_init();
	
	KLog ("Hello World!");
	
	bootinfo.mem_size = init_mem();
	BlinkLEDs(5);
	
	if (sd_card_init(&bdev) == -1) {
	    KPanic ("sd_card_init failed");
	}
	
	if (fat_init() == -1) {
	    KPanic ("fat_init failed");
	}

	KLOG ("Loading KERNEL");	
	if (elf_load ("BOOT/KERNEL", (void *)&kernel_entry_point) != 0) {
		KPanic ("Cannot load kernel");
    }
    	
	if (LoadModules() == -1) {
	    KPanic ("Failed during loading of modules");
	}
	
	bootinfo.root_entry_point = RootMain;
    bootinfo.root_stack_top = &root_stack_top;
	bootinfo.screen_width = screen_width;
	bootinfo.screen_height = screen_height;
	bootinfo.screen_pitch = screen_pitch;
	
	KLOG ("KERNEL entry point %#010x", (uint32) kernel_entry_point);	

	BootstrapKernel();
	kernel_entry_point(&bootinfo);
	while (1);
}


