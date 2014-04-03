
SEARCH_DIR("/cheviot/lib/gcc/arm-eabi/4.8.1/fpu");


SECTIONS
{
	. = 0x00008000;

	.init :
	{
		_text_start = .;
		*(.init)
	}

	.text :
	{
		*(.text)
	}
	
	. = ALIGN (4096);
	
	.rodata :
  	{
		*(.rodata)
		*(.rodata.*)
		*(.rodata1)
		_text_end = .;
  	}

	. = ALIGN (4096);
		
	.data :
	{
		_data_start = .;
		*(.data)
		_data_end = .;
	}

	. = ALIGN (4096);

	.bss :
	{
		_bss_start = .;
		*(.bss)
		*(COMMON)
		_bss_end = .;
	}


	_end = .;
	
}
