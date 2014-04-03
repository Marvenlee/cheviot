#ifndef KERNEL_TYPES_H
#define KERNEL_TYPES_H


#ifndef NULL
#define NULL				(void *)0
#endif

#ifndef null
#define null				(void *)0
#endif

#ifndef TRUE
#define TRUE				1
#endif

#ifndef FALSE
#define FALSE				0
#endif


#ifndef SYSCALL
#define SYSCALL
#endif


#define public
#define private 			static
#define import				extern


typedef unsigned char		bool;
typedef char *				strptr;
typedef signed char 		int8;
typedef unsigned char 		uint8;

typedef signed short		int16;
typedef unsigned short		uint16;

typedef signed long			int32;
typedef unsigned long		uint32;

typedef signed long long 	int64;
typedef unsigned long long 	uint64;

typedef long				off_t;

typedef unsigned int		size_t;
typedef int 				ssize_t;


typedef uint32		vm_addr;
typedef uint32		vm_offset;
typedef uint32		vm_size;


typedef unsigned char		bits8_t;
typedef unsigned short		bits16_t;
typedef unsigned long		bits32_t;

/*
typedef unsigned long 		uintptr_t;
typedef signed long			intptr_t;
typedef unsigned short		mode_t;

typedef	unsigned short		uid_t;
typedef	unsigned short		gid_t;
 
typedef long				time_t;

typedef unsigned long		signals_t;

typedef unsigned char		bits8_t;
typedef unsigned short		bits16_t;
typedef unsigned long		bits32_t;

typedef long long			uuid_t;

*/



#endif

