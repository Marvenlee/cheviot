
SEARCH_DIR("/home/marven/build_tools/lib/gcc/arm-none-eabi/4.9.2/fpu");

ENTRY (_start);

SECTIONS 
{     	
	. = 0x80100000;						/* Virtual address of kernel */
                                        /* Kernel loaded at 1MB physical addr */

	.text : AT (_stext - 0x80000000)
	{ 
		_stext = .;
  		_stext_phys = _stext - 0x80000000;
	}	
	
  	.rodata : AT (_srodata - 0x80000000)
  	{
		_srodata = .;
		*(.rodata)
		*(.rodata.*)
		*(.rodata1)
	  	_etext = .;
  	}
  
    .ARM.extab ALIGN (0x1000) : AT (_sextab - 0x80000000)
    {
        _sextab = .;
        *(.ARM.extab)
    }

    .ARM.exidx ALIGN (0x1000) : AT (_sexidx - 0x80000000)
    {
        _sexidx = .;
        *(.ARM.exidx)
    }

  	
	.data ALIGN (0x1000) : AT (_sdata - 0x80000000)
	{ 
		_sdata = .;
		*(.data)
		_edata = .;
	}

	.bss : AT (_sbss - 0x80000000)
	{ 
		_sbss = .;
		*(.bss)
		*(COMMON)
		_ebss = .;
	}

} 








