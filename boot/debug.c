#include <stdarg.h>
#include "types.h"
#include "dbg.h"
#include "string.h"
#include "globals.h"



/*
 * This debugger is part of the init process
 */

static void KClearScreen (uint16 color);
static void KScrollScreen (void);
static void KPrintString (char *s);
static void PrintChar (int x, int y, char ch);


#define KLOG_ENTRIES	256
#define KLOG_WIDTH   	256

extern unsigned char font8x8_basic[128][8];


bool debug_enabled = TRUE;
char klog_entry[KLOG_ENTRIES][KLOG_WIDTH];
int32 current_log;
uint16 pen_color = 0xffff;
uint16 bg_color = 0x0000;

int dbg_y = 0;

int	klog_to_screen = TRUE;
int log_idx = 0;






/*
 * InitDebug();
 */
 
void dbg_init (void)
{
	int32 t;
	
	BlinkLEDs(10);
		
	current_log = 0;
	klog_to_screen = TRUE;
	__debug_enabled = TRUE;
	
	GetScreenDimensions();
	SetScreenMode();
	
	KClearScreen(0x0010);
	BlinkLEDs(10);
	KClearScreen(0x0000);
		
	for (t=0; t< KLOG_ENTRIES; t++)
	{
		klog_entry[t][0] = '\0';
	}
}






void KPanic (char *string)
{
	if (string != NULL)
	{
		KLOG ("%s", string);
	}
	
	while (1);
}







/*
 * KLog();
 *
 * Used by the macros KPRINTF, KLOG, KASSERT and KPANIC to
 * print with printf formatting to the kernel's debug log buffer.
 * The buffer is a fixed size circular buffer, Once it is full 
 * the oldest entry is overwritten with the newest entry.
 *
 * Cannot be used during interrupt handlers or with interrupts
 * disabled s the dbg_slock must be acquired.  Same applies
 * to KPRINTF, KLOG, KASSERT and KPANIC.
 *
 * We can get away with using KLog() and above macros before
 * multitasking is enabled due to MutexLock() and MutexUnlock()
 * having a test to see if initialization is complete.
 */
 
void KLog (const char *format, ...)
{
	va_list ap;
	
	va_start (ap, format);

	if (__debug_enabled)
	{	
		Vsnprintf (&klog_entry[current_log][0], KLOG_WIDTH, format, ap);
		
		if (klog_to_screen == TRUE)
			KPrintString (&klog_entry[current_log][0]);
	
		current_log++;
		current_log %= KLOG_ENTRIES;
	}
	
	va_end (ap);
}








/*
 * KPrintString();
 */

static void KPrintString (char *s)
{
	int x = 0;
		
	if (dbg_y >= screen_height - 8)
	{
		KScrollScreen();
		dbg_y = screen_height - 8;
	}

	while (*s != '\0')
	{
		PrintChar (x, dbg_y, *s);
		x+=8;
		s++;
	}

	dbg_y += 8;
}




/* 
 * ClearScreen();
 */

static void KClearScreen (uint16 color)
{
	int x,y;
	
	for (y=0; y < screen_height; y++)
	{
		for (x=0; x<screen_width; x++)
		{
			*(uint16 *)(screen_buf + y * screen_pitch + x*2) = color;
		}
	}

	dbg_y = 0;
}




/*
 * ScrollScreen();
 */

static void KScrollScreen (void)
{
	long x,y;
	
	for (y=0; y < screen_height - 8; y++)
	{
		for (x=0; x<screen_width; x++)
		{
			*(uint16 *)(screen_buf + y * screen_pitch + x*2) =
							*(uint16 *)(screen_buf + (y+8) * screen_pitch + x*2);
		}
	}
	
	
	for (y= screen_height - 8; y < screen_height; y++)
	{
		for (x=0; x<screen_width; x++)
		{
			*(uint16 *)(screen_buf + y * screen_pitch + x*2) = 0;
		}
	}
}









static void PrintChar (int x, int y, char ch)
{
	uint8 r, c;
	uint8 bit_row;
	
	for (r = 0; r < 8; r++)
	{
		for (c = 0; c < 8; c ++)
		{
			bit_row = font8x8_basic[(unsigned char)ch][r];
			
			if ((1<<c) & bit_row)
			{
				*(uint16*)(screen_buf + (y+r) * screen_pitch + ((x + c)  * 2)) = pen_color;
			}
		}
	}
}







void BlinkError (void)
{
	int c;
	
	while (1)
	{
		LedOn();

		for (c=0; c<1600000; c++);

		LedOff();
		
		for (c=0; c<800000; c++);
	}
	

}




uint32 *FindTag (uint32 tag, uint32 *msg)
{
	uint32 skip_size;
	
	msg += 2;
	
	while (*msg != 0 && *msg != tag)
	{
		skip_size = (*(msg + 1) + 12) / 4;
		msg += skip_size;
	}

	if (*msg == 0)
		return NULL;
		
	return msg;
}



void BlankScreen(void)
{
	uint32 result;
	
	mailbuffer[0] = 7 * 4;
	mailbuffer[1] = 0;
	mailbuffer[2] = 0x00040002;
	mailbuffer[3] = 4;
	mailbuffer[4] = 0;
	mailbuffer[5] = 0;
	mailbuffer[6] = 0;

	do
	{
		MailBoxWrite ((uint32)mailbuffer, 8);
		result = MailBoxRead (8);
	} while (result == 0);
}








void GetScreenDimensions(void)
{
	uint32 result;
	
	screen_width = 640;
	screen_height = 480;
	
	mailbuffer[0] = 8 * 4;
	mailbuffer[1] = 0;
	mailbuffer[2] = 0x00040003;
	mailbuffer[3] = 8;
	mailbuffer[4] = 8;
	mailbuffer[5] = 0;
	mailbuffer[6] = 0;
	mailbuffer[7] = 0;

	do
	{
		MailBoxWrite ((uint32)mailbuffer, 8);
		result = MailBoxRead (8);
	} while (result == 0);
	
	screen_width = mailbuffer[5];
	screen_height = mailbuffer[6];
}






void SetScreenMode(void)
{
	uint32 result;
	uint32 *tva;
			
	mailbuffer[0] = 22 * 4;
	mailbuffer[1] = 0;
	mailbuffer[2] = 0x00048003;		// Physical size
	mailbuffer[3] = 8;
	mailbuffer[4] = 8;
	mailbuffer[5] = screen_width;
	mailbuffer[6] = screen_height;
	mailbuffer[7] = 0x00048004;		// Virtual size
	mailbuffer[8] = 8;
	mailbuffer[9] = 8;
	mailbuffer[10] = screen_width;
	mailbuffer[11] = screen_height;
	mailbuffer[12] = 0x00048005;	// Depth
	mailbuffer[13] = 4;
	mailbuffer[14] = 4;
	mailbuffer[15] = 16;									// 16 bpp
	mailbuffer[16] = 0x00040001;	// Allocate buffer
	mailbuffer[17] = 8;
	mailbuffer[18] = 8;
	mailbuffer[19] = 16;										// alignment
	mailbuffer[20] = 0;
	mailbuffer[21] = 0;
	
	do
	{
		MailBoxWrite ((uint32)mailbuffer, 8);
		result = MailBoxRead (8);
	} while (result == 0);
	

	if (mailbuffer[1] != 0x80000000)
		BlinkError();
	
	
	tva = FindTag (0x00040001, mailbuffer);

	if (tva == NULL)
		BlinkError();

	screen_buf = *(tva+3);
	
	
	mailbuffer[0] = 8 * 4;
	mailbuffer[1] = 0;
	mailbuffer[2] = 0x00040008;		// Get Pitch
	mailbuffer[3] = 4;
	mailbuffer[4] = 0;
	mailbuffer[5] = 0;
	mailbuffer[6] = 0;				// End Tag
	mailbuffer[7] = 0;
	

	do
	{
		MailBoxWrite ((uint32)mailbuffer, 8);
		result = MailBoxRead (8);
	} while (result == 0);


	tva = FindTag (0x00040008, mailbuffer);

	if (tva == NULL)
		BlinkError();

	screen_pitch = *(tva + 3);
}











uint16 CalcColor (uint8 r, uint8 g, uint8 b)
{
	uint16 c;
	
	b >>= 5;
	g >>= 5;
	r >>= 5;
	
	
	c = (r<<11) | (g << 5) | (b);
	return c;
}



void PutPixel (int x, int y, uint16 color)
{
	*(uint16*)(screen_buf + y*(screen_pitch) + x*2) = color;
}




/*
 *
 */

void MailBoxWrite (uint32 v, uint32 id)
{
   uint32 s;

   MemoryBarrier();
   DataCacheFlush();

   while (1)
   {
      s = MailBoxStatus();
   
      if ((s & 0x80000000) == 0)
         break;
   }

   MemoryBarrier();
   *((uint32 *)(0x2000b8a0)) = v | id;
   MemoryBarrier();
}




/*
 *
 */

uint32 MailBoxRead(uint32 id)
{
   uint32 s;
   volatile uint32 v;

   MemoryBarrier();
   DataCacheFlush();

   while (1)
   {
      while (1)
      {
         s = MailBoxStatus();
         if ((s & 0x40000000) == 0)
            break;
      }

	  MemoryBarrier();

      v = *((uint32 *)(0x2000b880));

      if ((v & 0x0000000f) == id)
         break;
   }
   return v & 0xfffffff0;
}




/*
 *
 */

uint32 MailBoxStatus()
{
   volatile uint32 v;
   MemoryBarrier();
   DataCacheFlush();
   v = *((uint32 *)(0x2000b898));
   MemoryBarrier();
   return v;
}








__attribute__((naked))	void MemoryBarrier()
{
	__asm("mov r0, #0");
	__asm("mcr p15, #0, r0, c7, c10, #5");
	__asm("mov pc, lr");
}

__attribute__((naked))	void DataCacheFlush()
{
	__asm("mov r0, #0");
	__asm("mcr p15, #0, r0, c7, c14, #0");
	__asm("mov pc, lr");
}



__attribute__((naked))	void SynchronisationBarrier()
{
	__asm("mov r0, #0");
	__asm("mcr p15, #0, r0, c7, c10, #4");
	__asm("mov pc, lr");
}

__attribute__((naked))	void DataSynchronisationBarrier()
{
	__asm("stmfd sp!, {r0-r8,r12,lr}");
	__asm("mcr p15, #0, ip, c7, c5, #0");
	__asm("mcr p15, #0, ip, c7, c5, #6");
	__asm("mcr p15, #0, ip, c7, c10, #4");
	__asm("mcr p15, #0, ip, c7, c10, #4");
	__asm("ldmfd sp!, {r0-r8,r12,pc}");
}








void LedOn(void)
{
	gpio = (uint32*)GPIO_BASE;
	gpio[GPIO_GPFSEL1] = (1 << 18);
	gpio[GPIO_GPCLR0] = (1 << 16);
}

    






void LedOff(void)
{
	gpio = (uint32*)GPIO_BASE;
	gpio[GPIO_GPFSEL1] = (1 << 18);
	gpio[GPIO_GPSET0] = (1 << 16);
}









static volatile uint32 tim;

void BlinkLEDs (int cnt)
{
	int t;
	
	for (t=0; t<cnt; t++)
	{
        for(tim = 0; tim < 500000; tim++);

	   	LedOn();
	   	
        for(tim = 0; tim < 500000; tim++);

	    LedOff();
	}
}


