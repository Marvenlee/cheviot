#ifndef KERNEL_SYSINFO_H
#define KERNEL_SYSINFO_H


#include <kernel/types.h>
#include <kernel/proc.h>



typedef struct KLimits
{
    int max_processes;
    int max_ports;
    int max_ports_per_process;
    int max_tickets;
    int max_tickets_per_process;
    int priority_factor;
    int max_mem;                    // Not sure how to enforce (due to IPC)
    int max_mem_per_process;        // Not sure how to enforce (due to IPC)
    int max_timers;
    int max_interrupt_handlers;
    
    // Add fields for current limits per-user
    // Have field in each proc for per-process values??????
} ulimits_t;






typedef struct 
{
    bits32_t flags;
    int process_cnt;
    int cpu_cnt;
    int total_pages;
    int page_size;
    int virtualalloc_alignment;
    int max_memareas;
    int max_handles;
    int power_state;
    int max_tickets;
} sysinfo_t;





/*
 *
 */
 
SYSCALL int SystemInfo (sysinfo_t *si);

SYSCALL int SetUserLimits (int uid, ulimits_t *ulimits);
SYSCALL int RemoveUserLimits (int uid);

struct KLimits *GetCurrentLimits (void);



#endif

