#ifndef INTERFACES_VIDEO_H
#define INTERFACES_VIDEO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/syscalls.h>
#include <interfaces/port.h>





#define VIDEO_NUM_REFRESH					8

typedef struct
{
	int          	width, height;
	int				bpp;
	short    		refresh [VIDEO_NUM_REFRESH];
} videomode_t;



/*
 * MsgVideo commands
 */
 
#define PORT_TYPE_VIDEO						1

#define VIDEO_CMD_GETMODES					0
#define VIDEO_CMD_SETMODE					1
#define VIDEO_CMD_CREATESURFACE				2
#define VIDEO_CMD_DELETESURFACE				3
#define VIDEO_CMD_LOADSURFACE				4
#define VIDEO_CMD_SURFACEUPDATE				5
#define VIDEO_CMD_SURFACESHOW				6
#define VIDEO_CMD_SURFACEHIDE				7
#define VIDEO_CMD_SURFACEMOVE				8
#define VIDEO_CMD_SURFACETOFRONT			9
#define VIDEO_CMD_SURFACETOBACK				10
#define VIDEO_CMD_SETCURSOR					11
#define VIDEO_CMD_MOVECURSOR				12
#define VIDEO_CMD_MODEINFO					13



/*
 * Surface type flags
 */
 
#define SF_VISIBLE			(1<<0)
#define SF_BACKGROUND		(1<<1)
#define SF_POPUP			(1<<2)



/*
 *
 */

struct VideoMsg
{
	int cmd;
	int error;
	
	union
	{
		struct
		{
			int width;
			int height;
			int bpp;
			int refresh;
		} setmode;
	
		struct
		{
			int width;
			int height;
			bits32_t flags;
		} createsurface;
		
		struct
		{
			int handle;
		} deletesurface;
		
		struct
		{
			int handle;
			int width;
			int height;
		} loadsurface;
		
		
		struct
		{
			int handle;
		} surfaceshow;
		
		
		struct
		{
			int handle;
		} surfacehide;
		
		struct
		{
			int handle;
			int x, y;
		} surfacemove;
	
		struct
		{
			int handle;
		} surfacetofront;
		
		struct
		{
			int handle;
		} surfacetoback;
		
		struct
		{
			int opacity;
		} setopacity;
				
		struct
		{
			int handle;
			int x, y;
			int width;
			int height;
		} surfaceupdate;
		
		struct
		{
			int handle;
			int top, left;
			int width, height;
		} setcursor;
		
		struct
		{
			int x, y;
		} movecursor;
	} s;

	union
	{
		int getmodes;
		int setmode;
		int createsurface;
		int deletesurface;
		int loadsurface;
		int surfaceshow;
		int surfacehide;
		int surfacemove;
		int surfaceupdate;
		int surfacetofront;
		int surfacetoback;
		int setopacity;
		int setcursor;
		int movecursor;
				
		struct
		{
			int width;
			int height;
			int bpp;
			int refresh;
			int error;
			int result;
		} modeinfo;
	} r;
};








#ifdef __cplusplus
}
#endif

#endif
