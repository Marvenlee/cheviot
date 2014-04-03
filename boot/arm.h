#ifndef ARM_H
#define ARM_H


#include "types.h"
//#include "lists.h"


/*
 * VM types
 */


typedef int 		spinlock_t;
typedef uint32		vm_addr;
typedef uint32		vm_offset;
typedef uint32		vm_size;

typedef uint32		pte_t;

/*
 * iflag_t used to store interrupt enabled/disabled flag state
 */
 
typedef unsigned long iflag_t;








#define GPIO_BASE       0x20200000UL
#define GPIO_GPFSEL0    0
#define GPIO_GPFSEL1    1

#define GPIO_GPSET0     7
#define GPIO_GPSET1     8
#define GPIO_GPCLR0     10
#define GPIO_GPCLR1     11
 









/*
 * Prototypes
 */


void MailBoxWrite (uint32 v, uint32 id);
uint32 MailBoxRead(uint32 id);
uint32 MailBoxStatus();
__attribute__((naked)) void MemoryBarrier();
__attribute__((naked)) void DataCacheFlush();
__attribute__((naked)) void SynchronisationBarrier();
__attribute__((naked)) void DataSynchronisationBarrier();


void GetArmMemory (void);
void GetVideocoreMemory (void);
uint16 CalcColor (uint8 r, uint8 g, uint8 b);
void PutPixel (int x, int y, uint16 color);
uint32 *FindTag (uint32 tag, uint32 *msg);
void BlankScreen(void);
void GetScreenDimensions(void);
void SetScreenMode(void);

void LedOn (void);
void LedOff (void);
void BlinkLEDs (int cnt);







#endif
