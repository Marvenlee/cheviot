
SEARCH_DIR("/cheviot/lib/gcc/arm-eabi/4.8.1/fpu");
SEARCH_DIR("/cheviot/lib/gcc/arm-eabi/lib");
INPUT (crt0.o);

ENTRY (_start);

SECTIONS
{
	. = 0x00800000;

	_text_start = .;
	
	.text :	{*(.text)}	
	.rodata : {*(.rodata) *(.rodata.*) *(.rodata1) }
	.ARM.extab : { *(.ARM.extab) }
	.ARM.exidx : { *(.ARM.exidx) }

	_text_end = .;
	
	
	. = ALIGN (0x8000);
	
	_data_start = .;
	.data :	{*(.data)}
	_data_end = .;
	
	__bss_start__ = .;
	_bss_start = .;
	.bss :	{*(.bss) *(COMMON)}
	_bss_end = .;
	__bss_end__ = .;
	_end = .;
	
}
