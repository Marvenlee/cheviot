#ifndef TYPES_H
#define TYPES_H


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

typedef signed long			err32;
typedef unsigned long 		uintptr_t;



/*
 * Address alignment macros
 */

#define ALIGN_UP(val, alignment)								\
	(((val + alignment - 1)/alignment)*alignment)
#define ALIGN_DOWN(val, alignment)								\
			((val) - ((val) % (alignment)))






#endif

