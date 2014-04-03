#ifndef KERNEL_ERROR_H
#define KERNEL_ERROR_H

/*
 * errno codes potentially used in the kernel (using SetError() and GetError).
 * Syscalls also return one of these values of 0 in %ebx
 *
 * Values should be the same as that found in Newlib
 */
 
enum
{
    undefinedErr             = -1,
    
    handleErr               = -9,
    privilegeErr            = -10,
    paramErr                = -11,
    resourceErr             = -12,
    memoryErr               = -13,
    messageErr              = -14,
    connectionErr           = -15,
    alarmErr                = -16
};
 
 
 
 
 
 
 
 
 
 









#endif

