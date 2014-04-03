/*
 * Copyright 2014  Marven Gilhespie
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Debugging functions and Debug() system call.
 */

#include <stdarg.h>
#include <kernel/types.h>
#include <kernel/dbg.h>
#include <kernel/utility.h>
#include <kernel/proc.h>
#include <kernel/error.h>
#include <kernel/globals.h>




/*
 * This debugger is part of the init process
 */

static void KClearScreen (uint16 color);
static void KScrollScreen (void);
static void KPrintString (char *s);
static void PrintChar (int x, int y, char ch);


#define KLOG_ENTRIES    256

#define KLOG_WIDTH      256

extern unsigned char font8x8_basic[128][8];


char klog_entry[KLOG_ENTRIES][KLOG_WIDTH];
int32 current_log;
uint16 pen_color = 0xffff;
uint16 bg_color = 0x0000;

int dbg_y = 0;

int klog_to_screen = TRUE;
int log_idx = 0;






/*
 * InitDebug();
 */
 
void InitDebug (void)
{
    int32 t;
    
        
    current_log = 0;
    klog_to_screen = TRUE;
    __debug_enabled = TRUE;
    
    GetScreenDimensions();
    SetScreenMode();
    
    KClearScreen(0x0740);
    BlinkLEDs(1);
    KClearScreen(0x0000);
        
    for (t=0; t< KLOG_ENTRIES; t++)
    {
        klog_entry[t][0] = '\0';
    }
}











/*
 * Debug();
 *
 * System call allowing applications to write strings to the kernel's debugger
 * buffer.
 */
 
SYSCALL void Debug (char *s)
{
    int t;
    char buf[80];
    char ch;
    
    
    for (t=0; t<80; t++)
    {
        CopyIn (&ch, s, 1);
        
        s++;
        buf[t] = ch;
        
        if (ch == '\0')
            break;
    }
            
    buf[79] = '\0';
    
    DisablePreemption();
    
    KLog ("dbg:= %s:", &buf[0]);
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

#ifndef NDEBUG

    if (__debug_enabled)
    {   
        Vsnprintf (&klog_entry[current_log][0], KLOG_WIDTH, format, ap);
        
        if (klog_to_screen == TRUE)
            KPrintString (&klog_entry[current_log][0]);
    
        current_log++;
        current_log %= KLOG_ENTRIES;
    }
#endif

    va_end (ap);
}




/*
 * KPanic();
 */

void KernelPanic(char *str)
{
    int32 y, log;
    int cnt;
    
    DisableInterrupts();

    pen_color = 0xffff;
    bg_color = 0x0f04;
    
    KClearScreen(bg_color);
    
    log = ((current_log - 1) + KLOG_ENTRIES) % KLOG_ENTRIES;
    
    KPrintString (" ******* KERNEL PANIC ********");
    KPrintString (str);
    
    for (cnt = 0, y=16; cnt < 23 && y < screen_height; y += 8, cnt++)
    {
        if (klog_entry[log][0] != '\0')
            KPrintString (&klog_entry[log][0]);
        
        log --;
        
        log = (log + KLOG_ENTRIES) % KLOG_ENTRIES;
    }


    while(1);
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


void KPrintXY (int x, int y, char *s)
{
    while (*s != '\0')
    {
        PrintChar (x, y, *s);
        x+=8;
        s++;
    }
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


