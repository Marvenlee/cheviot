#include "memory.h"
#include "arm.h"
#include "dbg.h"
#include "globals.h"
#include "types.h"

/*
    Initialise bmalloc() heap,  determine amount of physical memory.
 */
vm_size init_mem(void)
{
  uint32 result;
  vm_size size;

  mailbuffer[0] = 8 * 4;
  mailbuffer[1] = 0;
  mailbuffer[2] = 0x00010005;
  mailbuffer[3] = 8;
  mailbuffer[4] = 0;
  mailbuffer[5] = 0;
  mailbuffer[6] = 0;
  mailbuffer[7] = 0;

  do {
    MailBoxWrite((uint32)mailbuffer, 8);
    result = MailBoxRead(8);
  } while (result == 0);

  // TODO: Assuming memory starts at 0x00000000
  //	mem_base = mailbuffer[5];
  size = mailbuffer[6];
  return size;
}

