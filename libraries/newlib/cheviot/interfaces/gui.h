#ifndef INTERFACES_GUI_H
#define INTERFACES_GUI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/syscalls.h>
#include <unistd.h>




/*
 * GUI commands
 */

#define GUI_CMD_CREATESURFACE			1
#define GUI_CMD_DELETESURFACE			2

#define GUI_CMD_LOADSURFACE				3
#define GUI_CMD_SURFACESHOW				4
#define GUI_CMD_SURFACEHIDE				5
#define GUI_CMD_SURFACEMOVE				6
#define GUI_CMD_REFRESH					7

#define GUI_CMD_SETCURSOR				8
#define GUI_CMD_HIDECURSOR				9

#define GUI_CMD_GETINPUT				10

#define GUI_CMD_GETVIDEOMODES			11
#define GUI_CMD_SETMODE					12
#define GUI_CMD_MODEINFO				13

#define GUI_CMD_SURFACETOFRONT			14
#define GUI_CMD_SURFACETOBACK			15



// Need input settings, locale, preferred input, mouse speed etc.


typedef unsigned long 	color_t;


struct Rect
{
	int x;
	int y;
	int w;
	int h;
};



typedef struct Bitmap
{
	int width;
	int height;
	color_t *data;
} bitmap_t;




/*
 * HdrGUI
 */

struct GUIMsg
{
	int cmd;
	int error;
	
	union
	{
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
			int x, y;
			int width, height;
		} refresh;
		
		struct
		{
			int handle;
		} getinput;
		
		struct
		{
			int screen_handle;
			int handle;
		} getvideomodes;
		
		struct
		{
			int screen_handle;
			int width;
			int height;
			int bpp;
			int refresh;
		} setmode;
		
		struct
		{
			int screen_handle;
		} modeinfo;
	} s;
		

	union
	{
		int createsurface;
		int deletesurface;
		int loadsurface;
		int surfaceshow;
		int surfacehide;
		int surfacemove;
		int refresh;
		int getinput;
		int getvideomodes;
		int setmode;
		int modeinfo;
	} r;
};





/*
 * GUI prototypes
 */

int ConnectGUI (void);
int CreateSurface (int width, int height, bits32_t flags);
int DeleteSurface (int handle);
int LoadSurface (int handle, bitmap_t *bitmap);
int SurfaceUpdate (int handle, int x, int y, int w, int h);
int SurfaceShow (int handle);
int SurfaceHide (int handle);
int SurfaceMove (int handle, int x, int y);



#ifdef __cplusplus
}
#endif


#endif






