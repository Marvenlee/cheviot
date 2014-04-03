#include "types.h"
#include "block.h"
#include "fat.h"
#include "dbg.h"
#include "elf.h"
#include "memory.h"
#include "globals.h"



/*
 *
 */

void Main (void)
{
	void (*kernel_entry_point)(struct BootInfo *bi);
	void (*procman_entry_point)(void);
	size_t nbytes_read;
	size_t filesz;
	void *mod;
	void *addr;
	vm_addr heap_base, heap_ceiling;
	vm_addr user_stack_base, user_stack_ceiling;

    // Initialize bootloader
	
	dbg_init();
	BlinkLEDs(10);
	sd_card_init(&bdev);
	fat_init();
	init_mem();

	// Load kernel at 1MB mark
	
	if (elf_load ("BOOT/KERNEL", (void *)&kernel_entry_point) != 0)
		KPanic ("Cannot load kernel");
	
	kernel_phdr_cnt = phdr_cnt;
	bmemcpy (kernel_phdr_table, phdr_table, sizeof (Elf32_PHdr) *  MAX_PHDR);
	
	user_base = 0x00800000;
	heap_base    = segment_table[segment_cnt-1].base;
	heap_ceiling = segment_table[segment_cnt-1].ceiling;
		
	if (heap_ceiling > user_base)
		heap_ceiling = user_base;
	
	addr = SegmentCreate (heap_base, heap_ceiling - heap_base, SEG_TYPE_ALLOC, MAP_FIXED | PROT_READWRITE);

	if (addr == NULL)
		KPanic ("Could not allocate HEAP!");

	KLog ("heap_base = %#010x, heap_ceiling = %#010x", heap_base, heap_ceiling);

    
    // Load root process (the Executive at 8MB mark
		
	if (elf_load ("BOOT/EXEC", (void *)&procman_entry_point) != 0)
		KPanic ("Cannot load process manager");

	procman_phdr_cnt = phdr_cnt;
	bmemcpy (procman_phdr_table, phdr_table, sizeof (Elf32_PHdr) * MAX_PHDR);


    // Allocate memory for module table, read startup.txt
    // and load modules above 8MB
    
	module_table = SegmentCreate (user_base, sizeof (struct Module) * NMODULE, SEG_TYPE_ALLOC, PROT_READWRITE);
	KLog ("module_table = %#010x", module_table);

	if ((vm_addr)module_table < user_base)
		KPanic ("Bad module_table");
	
	if (fat_open ("BOOT/STARTUP.TXT") != 0)
		KPanic ("Cannot open STARTUP.TXT");
	
	module_cnt = 0;
	
	while (module_cnt < NMODULE)
	{
		if ((nbytes_read = fat_getline(module_table[module_cnt].name, 64)) == 0)
			break;
		
		if (module_table[module_cnt].name[0] == '\0' ||
			module_table[module_cnt].name[0] == '\n' ||
			module_table[module_cnt].name[0] == '\r' ||
			module_table[module_cnt].name[0] == ' ')
		{
			KLOG ("Skipping blank lines");
			continue;
		}
		
		if (fat_open (module_table[module_cnt].name) != 0)
			KPanic ("Cannot open module");
		
		filesz = fat_get_filesize();
		
		mod = SegmentCreate (user_base, filesz, SEG_TYPE_ALLOC, PROT_READWRITE);
		
		if (fat_read (mod, filesz) == filesz)
		{
			module_table[module_cnt].addr = mod;
			module_table[module_cnt].size = filesz;
			module_cnt++;
		}
	}
	
	
	// Allocate a stack for the Executive
	
	user_stack_base = SegmentCreate (user_base, 65536, SEG_TYPE_ALLOC, PROT_READWRITE);	
	user_stack_ceiling = user_stack_base + 65536;

    
    // Pass the bootinfo, segment_table and module_table to kernel.
    
	bootinfo.procman_entry_point = procman_entry_point;
	bootinfo.user_stack_base = user_stack_base;
	bootinfo.user_stack_ceiling = user_stack_ceiling;
	bootinfo.user_base = user_base;
	bootinfo.heap_base = heap_base;
	bootinfo.heap_ceiling = heap_ceiling;
	bootinfo.screen_width = screen_width;
	bootinfo.screen_height = screen_height;
	bootinfo.screen_buf = screen_buf;
	bootinfo.screen_pitch = screen_pitch;
	bootinfo.module_table = module_table;
	bootinfo.module_cnt = module_cnt;
	bootinfo.segment_table = segment_table;
	bootinfo.segment_cnt = segment_cnt;
		
	KLOG ("Calling KERNEL entry point %#010x", (uint32) kernel_entry_point);
	
	kernel_entry_point(&bootinfo);
	while (1);
}










