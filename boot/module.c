#include "types.h"
#include "dbg.h"
#include "root.h"
#include "globals.h"
#include "module.h"
#include "fat.h"

// Prototypes
static char *StrGetLine (char *string);


/*!
    Determines if the BOOT/STARTUP.TXT file exists and reads it into memory.
    This file contains a list of modules to load from the SD card, one file
    per line.  It then proceeds to load the modules into the memory above the
    bootloader and below 478k.
*/
int LoadModules (void)
{
    size_t filesz;
    char *startup;
    char *path;
    void *mod;

	module_table = bmalloc (sizeof (struct Module) * MAX_MODULE);
	KLog ("module_table = %#010x", module_table);
    
	if (fat_open ("BOOT/STARTUP.TXT") != 0)	{
	    KLog ("No STARTUP.TXT, skipping modules");
	    return 0;
	}
	
    filesz = fat_get_filesize();    
    startup = bmalloc (filesz + 1);
    *(startup + filesz) = '\0';

    if (fat_read (startup, filesz) != filesz) {
        KPanic ("Failed to read STARTUP.TXT file");
    }
        
    path = startup;
    
	while ((path = StrGetLine(path)) != NULL && module_cnt < MAX_MODULE) {	    
		if (fat_open (path) != 0) {
			KLog (": %s, sz: %d", path, filesz);
			KPanic ("Cannot open module");
		}
		
		filesz = fat_get_filesize();        

        if ((mod = bmalloc (filesz)) == NULL) {
            KLog (": %s, sz: %d", path, filesz);
            KPanic ("Cannot allocate module");
        }
        
		if (fat_read (mod, filesz) != filesz) {
            KLog (": %s, sz: %d", path, filesz);
            KPanic ("Cannot read module");
    	}

		module_table[module_cnt].name = path;
		module_table[module_cnt].addr = mod;
		module_table[module_cnt].size = filesz;
	}
	
	return module_cnt;
}


/*!
    Extract and null terminate the next line from the string argument.
    Effectively acts like a simple strtok for newlines.
*/
static char *StrGetLine (char *string)
{
    char *line = string;
    char *c;
    bool blank_line = FALSE;
    static bool eof = FALSE;
    
    if (eof == TRUE) {
        return NULL;
    }
    
    do
    {    
        for (c = line; *c != '\0' && *c != '\n'; c++);        

        if (*c == '\0') {
            eof = TRUE;
        }
        
        *c = '\0';
    
        if (*line == '\0' || *line == '#') {
            blank_line = TRUE;
            
            if (eof == TRUE) {
                return NULL;
            }
            
            line = c + 1;
        } 
        else {
            blank_line = FALSE;
        }        
    } while (blank_line == TRUE && eof == FALSE);
    
    return line;    
}
	


