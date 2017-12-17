#ifndef MODULE_H
#define MODULE_H

#include "types.h"


#define MAX_MODULE		16

struct Module
{
	char *name;     // Pathname to executable:  FIXME: Currently entire line of startup.txt
	char *args;     // TODO argument string
	void *addr;
	size_t size;
};


int LoadModules (void);


#endif

