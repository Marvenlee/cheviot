#ifndef INTERFACES_INPUT_H
#define INTERFACES_INPUT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/syscalls.h>
#include <interfaces/port.h>


/*
 * Message type and commands
 */
 
#define PORT_TYPE_INPUT						1


#define MOUSE_LEFT			(1<<0)
#define MOUSE_RIGHT			(1<<1)

struct InputMouse
{
	bits32_t buttons;
	int x, y;
};	




// #define SCANCODE_OVERRUN			-1


struct InputKey
{
	int scan_code;
};



#define TYPE_MOUSE					1
#define TYPE_KEYBOARD				2

struct Input
{
	int type;
	
	union
	{
		struct InputMouse mouse;
		struct InputKey key;
	} data;
};



/*
 * MsgInput
 *
 * Standard message sent and received from
 * input device drivers.
 */

#define INPUT_CMD_GETINPUT			0
#define INPUT_CMD_FLUSHINPUT		1

struct MsgInput
{
	int cmd;
	int error;
		
	union
	{
		struct
		{
			struct Input input;
		} getinput;
	} s;
	
	union
	{
		int getinput;
		int flushinput;
	} r;
};



#ifdef __cplusplus
}
#endif

#endif
