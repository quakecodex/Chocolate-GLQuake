/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

/** 
 * \file gl_vidsdl.c
 * Functions to create and manage an OpenGL window. 
 * 
 * @author: James Johnson
 * @version: 0.1.0 2016-12-16
 */

#include "quakedef.h"
#include "winquake.h"
#include "sdlquake.h"
#include "../resources/resource.h"


#define MAX_MODE_LIST	128 /**< Maximum number of video modes supported by Quake. */
#define VID_ROW_SIZE	3
#define WARP_WIDTH		320
#define WARP_HEIGHT		200
#define MAXWIDTH		10000 /**< Maximum window width */
#define MAXHEIGHT		10000 /**< Maximum window height */
#define BASEWIDTH		320 /**< Minimum window width */
#define BASEHEIGHT		200 /**< Minimum window height */

#define MODE_WINDOWED			0	/**< Indicates a windowed video mode */
#define NO_MODE					(MODE_WINDOWED - 1) /**< Invalid or unset video mode */
#define MODE_FULLSCREEN_DEFAULT	(MODE_WINDOWED + 1) /**< Fullscreen video mode */

/**
 * Defines a video mode.
 */
typedef struct {
	modestate_t	type; /**< Type of video mode this is. MS_WINDOWED or MS_FULLSCREEN. */
	int			width; /**< Width of video mode. */
	int			height; /**< Height of video mode. */
	int			modenum; /**< Index into modelist where this mode is defined. */
	int			dib; /**< Whether or not this is a Win32 dib window. 1 = true, 0 = false.*/
	int			fullscreen; /**< Whether or not this is a fullscreen mode. 1 = true, 0 = false.*/
	int			bpp; /**< Bitdepth if this mode 16, 24 or 32. */
	int			halfscreen; /**< Half screen mode? */
	char		modedesc[17]; /**< Human readable description of the mode. */
} vmode_t;

const char		*gl_vendor; /**< GPU Vendor. */
const char		*gl_renderer; /**< OpenGL renderer. */
const char		*gl_version; /**< OpenGL version. */
const char		*gl_extensions; /**< List of available OpenGL extensions. */

qboolean		DDActive; /**< DirectDraw active? */
qboolean		scr_skipupdate; /**< Set to true to skip a backbuffer swap during rendering. */

static vmode_t	modelist[MAX_MODE_LIST]; /**< List of available video modes. */
static int		nummodes; /**< Number of modes in modelist. */
static vmode_t	badmode; /**< Represents a bad video mode. */

static qboolean	vid_initialized = false; /** Indicate if video sytem is initialized. */
static qboolean	windowed, leavecurrentmode;
static qboolean vid_canalttab = false;
static qboolean vid_wassuspended = false;
static int		windowed_mouse;
extern qboolean	mouseactive;  // from in_win.c
static HICON	hIcon;

int				DIBWidth, DIBHeight;

HWND			mainwindow, dibwindow;

int				vid_modenum = NO_MODE;
int				vid_realmode;
int				vid_default = MODE_WINDOWED;
static int		windowed_default;
unsigned char	vid_curpal[256*3];
static qboolean fullsbardraw = false;

static float	vid_gamma = 1.0;

glvert_t		glv;

cvar_t			gl_ztrick = {"gl_ztrick","1"};

viddef_t		vid;				// global video state

unsigned short	d_8to16table[256];
unsigned		d_8to24table[256];
unsigned char	d_15to8table[65536];

float			gldepthmin, gldepthmax; // Move? gl_rmain?

modestate_t		modestate = MS_UNINIT;

void VID_MenuDraw (void);
void VID_MenuKey (int key);

void AppActivate(BOOL fActive, BOOL minimize);
char *VID_GetModeDescription (int mode);
void ClearAllStates (void);
void VID_UpdateWindowStatus (void);
void GL_Init (void);

PROC glArrayElementEXT;
PROC glColorPointerEXT;
PROC glTexCoordPointerEXT;
PROC glVertexPointerEXT;

qboolean isPermedia = false;
qboolean gl_mtexable = false;

//====================================

cvar_t		vid_mode = {"vid_mode","0", false};
// Note that 0 is MODE_WINDOWED
cvar_t		_vid_default_mode = {"_vid_default_mode","0", true};
// Note that 3 is MODE_FULLSCREEN_DEFAULT
cvar_t		_vid_default_mode_win = {"_vid_default_mode_win","3", true};
cvar_t		vid_wait = {"vid_wait","0"};
cvar_t		vid_nopageflip = {"vid_nopageflip","0", true};
cvar_t		_vid_wait_override = {"_vid_wait_override", "0", true};
cvar_t		vid_config_x = {"vid_config_x","800", true};
cvar_t		vid_config_y = {"vid_config_y","600", true};
cvar_t		vid_stretch_by_2 = {"vid_stretch_by_2","1", true};
cvar_t		_windowed_mouse = {"_windowed_mouse","1", true};

int			window_center_x, window_center_y, window_x, window_y, window_width, window_height;
RECT		window_rect;

SDL_Surface *quakeicon; /** Quake icon for window title bar */

int VID_LoadQuakeIcon(void);
void VID_FreeQuakeIcon(void);

/**
 * Sets up a windowed video mode.
 * @param	modenum		Index into modelist array for which video mode to set. Must be 0.
 * @return	true (1) if the mode is set up.
 */
qboolean VID_SetWindowedMode (int modenum)
{
	int	lastmodestate, width, height;

	lastmodestate = modestate;
 
	DIBWidth = modelist[modenum].width;
	DIBHeight = modelist[modenum].height;
	width = DIBWidth;
	height = DIBHeight;

	modestate = MS_WINDOWED;

	vid.width = modelist[modenum].width;
	vid.height = modelist[modenum].height;

	if (vid.conheight > (unsigned int)modelist[modenum].height)
		vid.conheight = (unsigned int)modelist[modenum].height;
	if (vid.conwidth > (unsigned int)modelist[modenum].width)
		vid.conwidth = (unsigned int)modelist[modenum].width;
	vid.conwidth = modelist[modenum].width & 0xfff8; // Must be a multiple of 8 
	vid.conheight = vid.conwidth * vid.height / vid.width;
	

	vid.numpages = 2; 
	
	// Get a handle to the window. For compatability until SDL is fully implemented 
    mainwindow = GetActiveWindow(); // REMOVE

	return true;
}

/**
 * Sets up a fullscreen video mode.
 * @param	modenum		Index into modelist array for which video mode to set.
 * @return	true (1) if the mode is set up.
 */
qboolean VID_SetFullDIBMode (int modenum)
{
	int	lastmodestate;
	int result;

	if (!leavecurrentmode)
	{
		result = SDL_VideoModeOK(modelist[modenum].width, 
								 modelist[modenum].height,
								 modelist[modenum].bpp, 
								 SDL_OPENGL | SDL_FULLSCREEN);

		if (!result) {
			Sys_Error ("Couldn't set fullscreen DIB mode");
			exit(-1);
		}
	}

	lastmodestate = modestate;
	modestate = MS_FULLDIB;

	DIBWidth = modelist[modenum].width;
	DIBHeight = modelist[modenum].height;

	vid.width = modelist[modenum].width;
	vid.height = modelist[modenum].height;

	if (vid.conheight > (unsigned int)modelist[modenum].height)
		vid.conheight = (unsigned int)modelist[modenum].height;
	if (vid.conwidth > (unsigned int)modelist[modenum].width)
		vid.conwidth = (unsigned int)modelist[modenum].width;
	vid.conwidth = modelist[modenum].width & 0xfff8; /* Must be multiple of 8 */
	vid.conheight = vid.conwidth * vid.height / vid.width;

	vid.numpages = 2; 

	window_x = 0;
	window_y = 0;

	return true;
}


/**
 * Sets up the video mode.
 * @param	modenum		Index into modelist array for which video mode to set.
 * @param	palette		Array of RGB triplets specifying the video palette. 
 * Should be gamma corrected.
 * @return	true (1) if the mode is set up.
 */
int VID_SetMode (int modenum, unsigned char *palette)
{
	int				original_mode, temp;
	qboolean		stat;

	if ((windowed && (modenum != 0)) ||
		(!windowed && (modenum < 1)) ||
		(!windowed && (modenum >= nummodes)))
	{
		Sys_Error ("Bad video mode\n");
	}

	temp = scr_disabled_for_loading;
	scr_disabled_for_loading = true;

	CDAudio_Pause ();

	if (vid_modenum == NO_MODE)
		original_mode = windowed_default;
	else
		original_mode = vid_modenum;

	if (modelist[modenum].type == MS_WINDOWED)
	{
		if (_windowed_mouse.value && key_dest == key_game)
		{
			stat = VID_SetWindowedMode(modenum);
			IN_ActivateMouse ();
			IN_HideMouse ();
		}
		else
		{
			IN_DeactivateMouse ();
			IN_ShowMouse ();
			stat = VID_SetWindowedMode(modenum);
		}
	}
	else if (modelist[modenum].type == MS_FULLDIB)
	{
		stat = VID_SetFullDIBMode(modenum);
		IN_ActivateMouse ();
		IN_HideMouse ();
	}
	else
	{
		Sys_Error ("VID_SetMode: Bad mode type in modelist");
	}

	window_width = DIBWidth;
	window_height = DIBHeight;
	VID_UpdateWindowStatus ();

	CDAudio_Resume ();
	scr_disabled_for_loading = temp;
	stat = 1;
	if (!stat)
	{
		Sys_Error ("Couldn't set video mode");
	}

	vid_modenum = modenum;
	Cvar_SetValue ("vid_mode", (float)vid_modenum);

	Sleep (100);

	ClearAllStates ();

	if (!msg_suppress_1)
		Con_SafePrintf ("Video mode %s initialized.\n", VID_GetModeDescription (vid_modenum));

	VID_SetPalette (palette);

	vid.recalc_refdef = 1;

	return true;
}


/**
 * Calculate the window's size rect and saves it to a global window_rect struct.
 * Used by the renderer to calculate render target size.
 */
void VID_UpdateWindowStatus (void)
{
	window_rect.left = window_x;
	window_rect.top = window_y;
	window_rect.right = window_x + window_width;
	window_rect.bottom = window_y + window_height;
	window_center_x = (window_rect.left + window_rect.right) / 2;
	window_center_y = (window_rect.top + window_rect.bottom) / 2;
}

BINDTEXFUNCPTR bindTexFunc;

#define TEXTURE_EXT_STRING "GL_EXT_texture_object"

/**
 * Checks to see if glBindTexture function is available.
 */
void CheckTextureExtensions (void)
{
	char		*tmp;
	qboolean	texture_ext;
	HINSTANCE	hInstGL;

	texture_ext = FALSE;
	tmp = (unsigned char *)glGetString(GL_EXTENSIONS);
	while (*tmp)
	{
		if (strncmp((const char*)tmp, TEXTURE_EXT_STRING, strlen(TEXTURE_EXT_STRING)) == 0)
			texture_ext = TRUE;
		tmp++;
	}

	if (!texture_ext || COM_CheckParm ("-gl11") )
	{
		hInstGL = LoadLibrary("opengl32.dll");
		
		if (hInstGL == NULL)
			Sys_Error ("Couldn't load opengl32.dll\n");

		bindTexFunc = (void *)GetProcAddress(hInstGL,"glBindTexture");

		if (!bindTexFunc)
			Sys_Error ("No texture objects!");
		return;
	}

	if ((bindTexFunc = (BINDTEXFUNCPTR)
		wglGetProcAddress((LPCSTR) "glBindTextureEXT")) == NULL)
	{
		Sys_Error ("GetProcAddress for BindTextureEXT failed");
		return;
	}
}

int	texture_mode = GL_LINEAR;
int	texture_extension_number = 1;

#ifdef _WIN32
/**
 * Check if OpenGL multitexture functions are available
 */
void CheckMultiTextureExtensions(void) 
{
	if (strstr(gl_extensions, "GL_SGIS_multitexture ") && !COM_CheckParm("-nomtex")) {
		Con_Printf("Multitexture extensions found.\n");
		qglMTexCoord2fSGIS = (void *) wglGetProcAddress("glMTexCoord2fSGIS");
		qglSelectTextureSGIS = (void *) wglGetProcAddress("glSelectTextureSGIS");
		gl_mtexable = true;
	}
}
#else
/**
 * Check if OpenGL multitexture functions are available
 */
void CheckMultiTextureExtensions(void) 
{
	gl_mtexable = true;
}
#endif

/**
 * Initializes OpenGL. Get's vender, version and renderer strings. Checks extension
 * support and sets initial shading, texture blending and filtering modes.
 */
void GL_Init (void)
{
	char ext[2048];

	gl_vendor = glGetString (GL_VENDOR);
	Con_Printf ("GL_VENDOR: %s\n", gl_vendor);
	gl_renderer = glGetString (GL_RENDERER);
	Con_Printf ("GL_RENDERER: %s\n", gl_renderer);

	gl_version = glGetString (GL_VERSION);
	Con_Printf ("GL_VERSION: %s\n", gl_version);
	gl_extensions = glGetString (GL_EXTENSIONS);
	// Truncate extenstion string, to prevent buffer overrun
	// Sometimes GL extenstion string doesn't have terminating null.
	strncpy(ext, gl_extensions, 2048);
	ext[2047] = '\0';
	Con_Printf ("GL_EXTENSIONS: %s\n", ext);

//	Con_Printf ("%s %s\n", gl_renderer, gl_version);
	
    if (_strnicmp(gl_renderer,"PowerVR",7)==0)
         fullsbardraw = true;

    if (_strnicmp(gl_renderer,"Permedia",8)==0)
         isPermedia = true;

	CheckTextureExtensions ();
	CheckMultiTextureExtensions ();

	glClearColor (1,0,0,0);
	glCullFace(GL_FRONT);
	glEnable(GL_TEXTURE_2D);

	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.666f);

	glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	glShadeModel (GL_FLAT);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

/** 
 * Start rendering process. This is just used to tell the renderer the size of the window.
 * @param	x		Gets set to the top-left corner of the rendering window.
 * @param	y		Gets set to the top-left corner of the rendering window.
 * @param	width	Gets set to the width of the rendering window. 
 * @param	height	Gets set to the height corner of the rendering window.
 */
void GL_BeginRendering (int *x, int *y, int *width, int *height)
{
	extern cvar_t gl_clear;
	
	// Set rendering target size
	*x = 0;
	*y = 0;
	*width = vid.width;
	*height = vid.height;
}

/** 
 * Ends a rendering pass.
 */
void GL_EndRendering (void)
{
	if (!scr_skipupdate || block_drawing) {
		SDL_GL_SwapBuffers();
	}

	// handle the mouse state when windowed if that's changed
	if (modestate == MS_WINDOWED)
	{
		if (!_windowed_mouse.value) {
			if (windowed_mouse)	{
				IN_DeactivateMouse ();
				IN_ShowMouse ();
				windowed_mouse = false;
			}
		} else {
			windowed_mouse = true;
			if (key_dest == key_game && !mouseactive && ActiveApp) {
				IN_ActivateMouse ();
				IN_HideMouse ();
			} else if (mouseactive && key_dest != key_game) {
				IN_DeactivateMouse ();
				IN_ShowMouse ();
			}
		}
	}
	if (fullsbardraw)
		Sbar_Changed();
}

/**
 * Set the palette quake will use for rendering. Used to convert textures to full color for OpenGL.
 * @param	palette		Array of 256 RGB triplets defining the palette.
 */
void	VID_SetPalette (unsigned char *palette)
{
	byte	*pal;
	unsigned r,g,b;
	unsigned v;
	int     r1,g1,b1;
	int		j,k,l;
	unsigned short i;
	unsigned	*table;

//
// 8 8 8 encoding
//
	pal = palette;
	table = d_8to24table;
	for (i=0 ; i<256 ; i++)
	{
		r = pal[0];
		g = pal[1];
		b = pal[2];
		pal += 3;
		
//		v = (255<<24) + (r<<16) + (g<<8) + (b<<0);
//		v = (255<<0) + (r<<8) + (g<<16) + (b<<24);
		v = (255<<24) + (r<<0) + (g<<8) + (b<<16);
		*table++ = v;
	}
	d_8to24table[255] &= 0xffffff;	// 255 is transparent

	// JACK: 3D distance calcs - k is last closest, l is the distance.
	// FIXME: Precalculate this and cache to disk.
	for (i=0; i < (1<<15); i++) {
		/* Maps
			000000000000000
			000000000011111 = Red  = 0x1F
			000001111100000 = Blue = 0x03E0
			111110000000000 = Grn  = 0x7C00
		*/
		r = ((i & 0x1F) << 3)+4;
		g = ((i & 0x03E0) >> 2)+4;
		b = ((i & 0x7C00) >> 7)+4;
		pal = (unsigned char *)d_8to24table;
		for (v=0,k=0,l=10000*10000; v<256; v++,pal+=4) {
			r1 = r-pal[0];
			g1 = g-pal[1];
			b1 = b-pal[2];
			j = (r1*r1)+(g1*g1)+(b1*b1);
			if (j<l) {
				k=v;
				l=j;
			}
		}
		d_15to8table[i]=k;
	}
}

BOOL	gammaworks;

/**
 * Does nothing?
 */
void	VID_ShiftPalette (unsigned char *palette)
{
	extern	byte ramps[3][256];
	
//	VID_SetPalette (palette);

//	gammaworks = SetDeviceGammaRamp (maindc, ramps);
}

/** 
 * Deactivates the mouse for the default windowed video mode.
 */
void VID_SetDefaultMode (void)
{
	IN_DeactivateMouse ();
}

/**
 * Exits the current video mode. Shuts down the app window and SDL.
 */
void	VID_Shutdown (void)
{
	if (vid_initialized)
	{
		AppActivate(false, false);
	}

	/* Shutdown SDL */
	VID_FreeQuakeIcon();
	SDL_Quit();
}

/** Lookup table for scan code to Quake key conversion */
byte        scantokey[128] = 
					{ 
//  0           1       2       3       4       5       6       7 
//  8           9       A       B       C       D       E       F 
	0  ,    27,     '1',    '2',    '3',    '4',    '5',    '6', 
	'7',    '8',    '9',    '0',    '-',    '=',    K_BACKSPACE, 9, // 0 
	'q',    'w',    'e',    'r',    't',    'y',    'u',    'i', 
	'o',    'p',    '[',    ']',    13 ,    K_CTRL,'a',  's',      // 1 
	'd',    'f',    'g',    'h',    'j',    'k',    'l',    ';', 
	'\'' ,    '`',    K_SHIFT,'\\',  'z',    'x',    'c',    'v',      // 2 
	'b',    'n',    'm',    ',',    '.',    '/',    K_SHIFT,'*', 
	K_ALT,' ',   0  ,    K_F1, K_F2, K_F3, K_F4, K_F5,   // 3 
	K_F6, K_F7, K_F8, K_F9, K_F10, K_PAUSE  ,    0  , K_HOME, 
	K_UPARROW,K_PGUP,'-',K_LEFTARROW,'5',K_RIGHTARROW,'+',K_END, //4 
	K_DOWNARROW,K_PGDN,K_INS,K_DEL,0,0,             0,              K_F11, 
	K_F12,0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,        // 5 
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0, 
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,        // 6 
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0, 
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0         // 7 
					}; 

/** Lookup table for scan code to Quake key conversion while holding shift key */
byte        shiftscantokey[128] = 
					{ 
//  0           1       2       3       4       5       6       7 
//  8           9       A       B       C       D       E       F 
	0  ,    27,     '!',    '@',    '#',    '$',    '%',    '^', 
	'&',    '*',    '(',    ')',    '_',    '+',    K_BACKSPACE, 9, // 0 
	'Q',    'W',    'E',    'R',    'T',    'Y',    'U',    'I', 
	'O',    'P',    '{',    '}',    13 ,    K_CTRL,'A',  'S',      // 1 
	'D',    'F',    'G',    'H',    'J',    'K',    'L',    ':', 
	'"' ,    '~',    K_SHIFT,'|',  'Z',    'X',    'C',    'V',      // 2 
	'B',    'N',    'M',    '<',    '>',    '?',    K_SHIFT,'*', 
	K_ALT,' ',   0  ,    K_F1, K_F2, K_F3, K_F4, K_F5,   // 3 
	K_F6, K_F7, K_F8, K_F9, K_F10, K_PAUSE  ,    0  , K_HOME, 
	K_UPARROW,K_PGUP,'_',K_LEFTARROW,'%',K_RIGHTARROW,'+',K_END, //4 
	K_DOWNARROW,K_PGDN,K_INS,K_DEL,0,0,             0,              K_F11, 
	K_F12,0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,        // 5 
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0, 
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,        // 6 
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0, 
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0         // 7 
					}; 


/**
 * Maps a key from windows scan codes to Quake key codes.
 * @param	key	Scan code to map.
 * @return	The Quake key code.
 */
int MapKey (int key)
{
	key = (key>>16)&255;
	if (key > 127)
		return 0;
	if (scantokey[key] == 0)
		Con_DPrintf("key 0x%02x has no translation\n", key);
	return scantokey[key];
}

/**
 * Clears all current input from the queue
 */
void ClearAllStates (void)
{
	int		i;
	
// send an up event for each key, to make sure the server clears them all
	for (i=0 ; i<256 ; i++)
	{
		Key_Event (i, false);
	}

	Key_ClearStates ();
	IN_ClearStates ();
}

/**
 * Activates Quake's window so it's ready to run.
 * If the application is activating, then swap the system
 * into SYSPAL_NOSTATIC mode so that our palettes will display
 * correctly.
 * @param	fActive		True if the app window is activating.
 * @param	minimize	True if the window is minimized.
 */
void AppActivate(BOOL fActive, BOOL minimize)
{
	static BOOL	sound_active;

	ActiveApp = fActive;
	Minimized = minimize;

// enable/disable sound on focus gain/loss
	if (!ActiveApp && sound_active)
	{
		S_BlockSound ();
		sound_active = false;
	}
	else if (ActiveApp && !sound_active)
	{
		S_UnblockSound ();
		sound_active = true;
	}

	if (fActive)
	{
		if (modestate == MS_FULLDIB)
		{
			IN_ActivateMouse ();
			IN_HideMouse ();
			if (vid_canalttab && vid_wassuspended) {
				vid_wassuspended = false;
			}
		}
		else if ((modestate == MS_WINDOWED) && _windowed_mouse.value && key_dest == key_game)
		{
			IN_ActivateMouse ();
			IN_HideMouse ();
		}
	}

	if (!fActive)
	{
		if (modestate == MS_FULLDIB)
		{
			IN_DeactivateMouse ();
			IN_ShowMouse ();
			if (vid_canalttab) { 
				ChangeDisplaySettings (NULL, 0);
				vid_wassuspended = true;
			}
		}
		else if ((modestate == MS_WINDOWED) && _windowed_mouse.value)
		{
			IN_DeactivateMouse ();
			IN_ShowMouse ();
		}
	}
}


/**
 * Gets the number of video modes available for rendering.
 * @return Number of available video modes.
 */
int VID_NumModes (void)
{
	return nummodes;
}

	
/**
 * Returns a pointer to a modestate_t struct describing a video mode.
 * @param	modenum		Index into modelist array.
 * @return	modestate_t struct describing the video mode, or badmode if 
 * the video mode is invalide.
 */
vmode_t *VID_GetModePtr (int modenum)
{

	if ((modenum >= 0) && (modenum < nummodes))
		return &modelist[modenum];
	else
		return &badmode;
}


/**
 * Returns a human readable description of a video mode.
 * @param	mode	Index into Index into modelist array.
 * @return	A string containing the video mode description.
 */
char *VID_GetModeDescription (int mode)
{
	char		*pinfo;
	vmode_t		*pv;
	static char	temp[100];

	if ((mode < 0) || (mode >= nummodes))
		return NULL;

	if (!leavecurrentmode)
	{
		pv = VID_GetModePtr (mode);
		pinfo = pv->modedesc;
	}
	else
	{
		sprintf (temp, "Desktop resolution (%dx%d)",
				 modelist[MODE_FULLSCREEN_DEFAULT].width,
				 modelist[MODE_FULLSCREEN_DEFAULT].height);
		pinfo = temp;
	}

	return pinfo;
}


/**
 * Returns a human readable description of a video mode including 
 * the driver name description.
 * @param	mode	Index into Index into modelist array.
 * @return	A string containing the video mode description.
 */
// KJB: Added this to return the mode driver name in description for console
char *VID_GetExtModeDescription (int mode)
{
	static char	pinfo[40];
	vmode_t		*pv;

	if ((mode < 0) || (mode >= nummodes))
		return NULL;

	pv = VID_GetModePtr (mode);
	if (modelist[mode].type == MS_FULLDIB)
	{
		if (!leavecurrentmode)
		{
			sprintf(pinfo,"%s fullscreen", pv->modedesc);
		}
		else
		{
			sprintf (pinfo, "Desktop resolution (%dx%d)",
					 modelist[MODE_FULLSCREEN_DEFAULT].width,
					 modelist[MODE_FULLSCREEN_DEFAULT].height);
		}
	}
	else
	{
		if (modestate == MS_WINDOWED)
			sprintf(pinfo, "%s windowed", pv->modedesc);
		else
			sprintf(pinfo, "windowed");
	}

	return pinfo;
}


/**
 * Prints the current video mode description to the console.
 */
void VID_DescribeCurrentMode_f (void)
{
	Con_Printf ("%s\n", VID_GetExtModeDescription (vid_modenum));
}


/**
 * Prints number of video modes available to the console.
 */
void VID_NumModes_f (void)
{

	if (nummodes == 1)
		Con_Printf ("%d video mode is available\n", nummodes);
	else
		Con_Printf ("%d video modes are available\n", nummodes);
}


/**
 * Prints the current video mode description to the console.
 */
void VID_DescribeMode_f (void)
{
	int		t, modenum;
	
	modenum = Q_atoi (Cmd_Argv(1));

	t = leavecurrentmode;
	leavecurrentmode = 0;

	Con_Printf ("%s\n", VID_GetExtModeDescription (modenum));

	leavecurrentmode = t;
}


/**
 * Prints the current video mode description to the console.
 */
void VID_DescribeModes_f (void)
{
	int			i, lnummodes, t;
	char		*pinfo;
	vmode_t		*pv;

	lnummodes = VID_NumModes ();

	t = leavecurrentmode;
	leavecurrentmode = 0;

	for (i=1 ; i<lnummodes ; i++)
	{
		pv = VID_GetModePtr (i);
		pinfo = VID_GetExtModeDescription (i);
		Con_Printf ("%2d: %s\n", i, pinfo);
	}

	leavecurrentmode = t;
}

/**
 * Sets up the windowed video mode. (Mode 0) Adds it to modelist.
 */
void VID_InitDIB (void)
{
	modelist[0].type = MS_WINDOWED;

	if (COM_CheckParm("-width"))
		modelist[0].width = Q_atoi(com_argv[COM_CheckParm("-width")+1]);
	else
		modelist[0].width = 640;

	if (modelist[0].width < 320)
		modelist[0].width = 320;

	if (COM_CheckParm("-height"))
		modelist[0].height = Q_atoi(com_argv[COM_CheckParm("-height")+1]);
	else
		modelist[0].height = modelist[0].width * 240/320;

	if (modelist[0].height < 240)
		modelist[0].height = 240;

	sprintf (modelist[0].modedesc, "%dx%d",
			 modelist[0].width, modelist[0].height);

	modelist[0].modenum = MODE_WINDOWED;
	modelist[0].dib = 1;
	modelist[0].fullscreen = 0;
	modelist[0].halfscreen = 0;
	modelist[0].bpp = 0;

	nummodes = 1;
}


/** 
 * Gets a list of fullscreen video modes and adds them to modelist.
 */
void VID_InitFullDIB (void)
{
	int		i, modenum, originalnummodes;
	SDL_Rect** modes;
	SDL_PixelFormat pf; 

	originalnummodes = nummodes;
	modenum = 0;

	// Get 32-bit Modes 
	pf.BitsPerPixel = 32;
	modes = SDL_ListModes(&pf, SDL_OPENGL | SDL_FULLSCREEN);
	if (modes != (SDL_Rect **)0){
		for (i = 0; modes[i]; i++) {
			if (i >= MAX_MODE_LIST) 
				break;
			if ((modes[i]->w < BASEWIDTH) || (modes[i]->w > MAXWIDTH) || 
				(modes[i]->h < BASEHEIGHT) || (modes[i]->h > MAXHEIGHT)) {
				continue;
			}
			modelist[nummodes].type = MS_FULLDIB;
			modelist[nummodes].width = modes[i]->w;
			modelist[nummodes].height = modes[i]->h;
			modelist[nummodes].modenum = 0;
			modelist[nummodes].halfscreen = 0;
			modelist[nummodes].dib = 1;
			modelist[nummodes].fullscreen = 1;
			modelist[nummodes].bpp = pf.BitsPerPixel;
			sprintf (modelist[nummodes].modedesc, "%dx%dx%d",
					 modes[i]->w, modes[i]->h,
					 pf.BitsPerPixel);
			nummodes++;
		}
		
	}

	// Get 16-bit Modes 
	pf.BitsPerPixel = 16;
	modes = SDL_ListModes(&pf, SDL_OPENGL | SDL_FULLSCREEN);
	if (modes != (SDL_Rect **)0){
		for (i = 0; modes[i]; i++) {
			if (i >= MAX_MODE_LIST) 
				break;
			if ((modes[i]->w < BASEWIDTH) || (modes[i]->w > MAXWIDTH) || 
				(modes[i]->h < BASEHEIGHT) || (modes[i]->h > MAXHEIGHT)) {
				continue;
			}
			modelist[nummodes].type = MS_FULLDIB;
			modelist[nummodes].width = modes[i]->w;
			modelist[nummodes].height = modes[i]->h;
			modelist[nummodes].modenum = 0;
			modelist[nummodes].halfscreen = 0;
			modelist[nummodes].dib = 1;
			modelist[nummodes].fullscreen = 1;
			modelist[nummodes].bpp = pf.BitsPerPixel;
			sprintf (modelist[nummodes].modedesc, "%dx%dx%d",
					 modes[i]->w, modes[i]->h,
					 pf.BitsPerPixel);
			nummodes++;
		}
		
	}

	if (nummodes == originalnummodes)
		Con_SafePrintf ("No fullscreen DIB modes found\n");
}

/**
 * Adjusts the palette to match monitor gamma. (Gamma 1.0 by default.)
 * @param	pal		Arry of RGB triplets specifying the palette.
 */
static void Check_Gamma (unsigned char *pal)
{
	float	f, inf;
	unsigned char	palette[768];
	int		i;

	if ((i = COM_CheckParm("-gamma")) == 0) {
		if ((gl_renderer && strstr(gl_renderer, "Voodoo")) ||
			(gl_vendor && strstr(gl_vendor, "3Dfx")))
			vid_gamma = 1;
		else
			vid_gamma = 0.7f; // default to 0.7 on non-3dfx hardware
	} else
		vid_gamma = Q_atof(com_argv[i+1]);

	for (i=0 ; i<768 ; i++)
	{
		f = pow ( (pal[i]+1)/256.0 , vid_gamma );
		inf = f*255 + 0.5;
		if (inf < 0)
			inf = 0;
		if (inf > 255)
			inf = 255;
		palette[i] = inf;
	}

	memcpy (pal, palette, sizeof(palette));
}

/**
 * Initializes the video system. Creates a window and initializes OpenGL. 
 * @ param	palette	Pointer to an array of 256 RGB triplets (unsigned byte) specifying the current palette
 */
void	VID_Init (unsigned char *palette)
{
	int		i; /* Counter */
	int		existingmode; /* Current video mode */
	int		basenummodes;
	int width, height; /* Size of chosen mode in pixels */
	int bpp; /* Bit per pixel of chosen mode */
	int findbpp, done; /* Flags? */
	char	gldir[MAX_OSPATH]; /* Directory in ID1 to store converted textures for OpenGL */

	SDL_Surface* pBackbuffer = NULL; /* SDL backbuffer */
	Uint32 sdlFlags = 0; /* Video mode settings */

	/* Set up video mode cvars and commands */
	Cvar_RegisterVariable (&vid_mode);
	Cvar_RegisterVariable (&vid_wait);
	Cvar_RegisterVariable (&vid_nopageflip);
	Cvar_RegisterVariable (&_vid_wait_override);
	Cvar_RegisterVariable (&_vid_default_mode);
	Cvar_RegisterVariable (&_vid_default_mode_win);
	Cvar_RegisterVariable (&vid_config_x);
	Cvar_RegisterVariable (&vid_config_y);
	Cvar_RegisterVariable (&vid_stretch_by_2);
	Cvar_RegisterVariable (&_windowed_mouse);
	Cvar_RegisterVariable (&gl_ztrick);

	Cmd_AddCommand ("vid_nummodes", VID_NumModes_f);
	Cmd_AddCommand ("vid_describecurrentmode", VID_DescribeCurrentMode_f);
	Cmd_AddCommand ("vid_describemode", VID_DescribeMode_f);
	Cmd_AddCommand ("vid_describemodes", VID_DescribeModes_f);

	/* Initialize SDL */
	if ((SDL_Init(SDL_INIT_VIDEO) == -1)) 
	{
        Sys_Error("Unable to initialize SDL.\n");
		return;
    }

	/* Get list of windowed video modes */
	VID_InitDIB ();
	basenummodes = nummodes = 1;

	/* Get list of fullscreen video modes */
	VID_InitFullDIB ();

	/* Check if user specified video mode at the command line */
	if (COM_CheckParm("-window"))
	{
		windowed = true;
		vid_default = MODE_WINDOWED;
	}
	else
	{
		if (nummodes == 1)
			Sys_Error ("No RGB fullscreen modes available");

		windowed = false;

		if (COM_CheckParm("-mode"))
		{
			vid_default = Q_atoi(com_argv[COM_CheckParm("-mode")+1]);
		}
		else
		{
			if (COM_CheckParm("-current"))
			{
				modelist[MODE_FULLSCREEN_DEFAULT].width =
						GetSystemMetrics (SM_CXSCREEN);
				modelist[MODE_FULLSCREEN_DEFAULT].height =
						GetSystemMetrics (SM_CYSCREEN);
				vid_default = MODE_FULLSCREEN_DEFAULT;
				leavecurrentmode = 1;
			}
			else
			{
				if (COM_CheckParm("-width"))
				{
					width = Q_atoi(com_argv[COM_CheckParm("-width")+1]);
				}
				else
				{
					width = 640;
				}

				if (COM_CheckParm("-bpp"))
				{
					bpp = Q_atoi(com_argv[COM_CheckParm("-bpp")+1]);
					findbpp = 0;
				}
				else
				{
					bpp = 32;// Was 15
					findbpp = 1;
				}

				if (COM_CheckParm("-height"))
					height = Q_atoi(com_argv[COM_CheckParm("-height")+1]);

				/* Add user's forced mode to the list of available modes */
				if (COM_CheckParm("-force") && (nummodes < MAX_MODE_LIST))
				{
					modelist[nummodes].type = MS_FULLDIB;
					modelist[nummodes].width = width;
					modelist[nummodes].height = height;
					modelist[nummodes].modenum = 0;
					modelist[nummodes].halfscreen = 0;
					modelist[nummodes].dib = 1;
					modelist[nummodes].fullscreen = 1;
					modelist[nummodes].bpp = bpp;
					sprintf (modelist[nummodes].modedesc, "%dx%dx%d",
							 width, height,
							 bpp);

					for (i=nummodes, existingmode = 0 ; i<nummodes ; i++)
					{
						if ((modelist[nummodes].width == modelist[i].width)   &&
							(modelist[nummodes].height == modelist[i].height) &&
							(modelist[nummodes].bpp == modelist[i].bpp))
						{
							existingmode = 1;
							break;
						}
					}

					if (!existingmode)
					{
						nummodes++;
					}
				}

				done = 0;

				do
				{
					if (COM_CheckParm("-height"))
					{
						height = Q_atoi(com_argv[COM_CheckParm("-height")+1]);

						for (i=1, vid_default=0 ; i<nummodes ; i++)
						{
							if ((modelist[i].width == width) &&
								(modelist[i].height == height) &&
								(modelist[i].bpp == bpp))
							{
								vid_default = i;
								done = 1;
								break;
							}
						}
					}
					else
					{
						for (i=1, vid_default=0 ; i<nummodes ; i++)
						{
							if ((modelist[i].width == width) && (modelist[i].bpp == bpp))
							{
								vid_default = i;
								done = 1;
								break;
							}
						}
					}

					if (!done)
					{
						if (findbpp)
						{
							switch (bpp)
							{
							case 15:
								bpp = 16;
								break;
							case 16:
								bpp = 16;
								break;
							case 32:
								bpp = 32;
								break;
							case 24:
								bpp = 32;
								done = 1;
								break;
							}
						}
						else
						{
							done = 1;
						}
					}
				} while (!done);

				if (!vid_default)
				{
					printf("Size: %d, %d\n", width, height);
					Sys_Error ("Specified video mode not available");
				}
			}
		}
	}

	/* Done finding modes */
	vid_initialized = true;

	/* Check if user specified size of console */
	if ((i = COM_CheckParm("-conwidth")) != 0)
		vid.conwidth = Q_atoi(com_argv[i+1]);
	else
		vid.conwidth = 640;

	vid.conwidth &= 0xfff8; // make it a multiple of eight

	if (vid.conwidth < 320)
		vid.conwidth = 320;

	// pick a conheight that matches with correct aspect
	vid.conheight = vid.conwidth * 3 / 4;

	if ((i = COM_CheckParm("-conheight")) != 0)
		vid.conheight = Q_atoi(com_argv[i+1]);
	if (vid.conheight < 200)
		vid.conheight = 200;

	/* Set up basic video stuff */
	vid.maxwarpwidth = WARP_WIDTH; /* Where are these used? */
	vid.maxwarpheight = WARP_HEIGHT;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));
	
	/* Make sure colors display properly */
	Check_Gamma(palette);
	VID_SetPalette (palette);
	
	/* Create window at specified mode */
	VID_SetMode (vid_default, palette);

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	sdlFlags = SDL_OPENGL;
	if (modelist[vid_default].fullscreen)
		sdlFlags |= SDL_FULLSCREEN;
	pBackbuffer = SDL_SetVideoMode(modelist[vid_default].width, modelist[vid_default].height, 32, sdlFlags);
    if (pBackbuffer == NULL) 
        Sys_Error("Unable to set video mode.");

	SDL_WM_SetCaption("Chocolate GL Quake", "CGLQuake");
	if (VID_LoadQuakeIcon() == 0) 
		SDL_WM_SetIcon(quakeicon, NULL);

	/* Get the window handle for the other systems, input, etc */
	/* TODO: Remove */
	mainwindow = GetActiveWindow();
	AppActivate(TRUE, FALSE);

	/* Initialize OpenGL */
	GL_Init ();

	/* Set up GL directory */
	sprintf (gldir, "%s/glquake", com_gamedir);
	Sys_mkdir (gldir);

	vid_realmode = vid_modenum;

	/* Final checks and cleanup? */
	vid_menudrawfn = VID_MenuDraw;
	vid_menukeyfn = VID_MenuKey;

	strcpy (badmode.modedesc, "Bad mode"); /* badmode is used to indicate user chose a mode that doesn't exis */
	vid_canalttab = true;

	if (COM_CheckParm("-fullsbar"))
		fullsbardraw = true;
}


//========================================================
// Video menu stuff
//========================================================

extern void M_Menu_Options_f (void);
extern void M_Print (int cx, int cy, char *str);
extern void M_PrintWhite (int cx, int cy, char *str);
extern void M_DrawCharacter (int cx, int line, int num);
extern void M_DrawTransPic (int x, int y, qpic_t *pic);
extern void M_DrawPic (int x, int y, qpic_t *pic);

static int	vid_line, vid_wmodes;

typedef struct
{
	int		modenum;
	char	*desc;
	int		iscur;
} modedesc_t;

#define MAX_COLUMN_SIZE		9
#define MODE_AREA_HEIGHT	(MAX_COLUMN_SIZE + 2)
#define MAX_MODEDESCS		(MAX_COLUMN_SIZE*3)

static modedesc_t	modedescs[MAX_MODEDESCS];

/** 
 * Draws the video menu.
 */
void VID_MenuDraw (void)
{
	qpic_t		*p;
	char		*ptr;
	int			lnummodes, i, k, column, row;
	vmode_t		*pv;

	p = Draw_CachePic ("gfx/vidmodes.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);

	vid_wmodes = 0;
	lnummodes = VID_NumModes ();
	
	for (i=1 ; (i<lnummodes) && (vid_wmodes < MAX_MODEDESCS) ; i++)
	{
		ptr = VID_GetModeDescription (i);
		pv = VID_GetModePtr (i);

		k = vid_wmodes;

		modedescs[k].modenum = i;
		modedescs[k].desc = ptr;
		modedescs[k].iscur = 0;

		if (i == vid_modenum)
			modedescs[k].iscur = 1;

		vid_wmodes++;

	}

	if (vid_wmodes > 0)
	{
		M_Print (2*8, 36+0*8, "Fullscreen Modes (WIDTHxHEIGHTxBPP)");

		column = 8;
		row = 36+2*8;

		for (i=0 ; i<vid_wmodes ; i++)
		{
			if (modedescs[i].iscur)
				M_PrintWhite (column, row, modedescs[i].desc);
			else
				M_Print (column, row, modedescs[i].desc);

			column += 13*8;

			if ((i % VID_ROW_SIZE) == (VID_ROW_SIZE - 1))
			{
				column = 8;
				row += 8;
			}
		}
	}

	M_Print (3*8, 36 + MODE_AREA_HEIGHT * 8 + 8*2,
			 "Video modes must be set from the");
	M_Print (3*8, 36 + MODE_AREA_HEIGHT * 8 + 8*3,
			 "command line with -width <width>");
	M_Print (3*8, 36 + MODE_AREA_HEIGHT * 8 + 8*4,
			 "and -bpp <bits-per-pixel>");
	M_Print (3*8, 36 + MODE_AREA_HEIGHT * 8 + 8*6,
			 "Select windowed mode with -window");
}


/** 
 * Handles video menu input. Currently only allows going back. (Pressing ESC.)
 */
void VID_MenuKey (int key)
{
	switch (key)
	{
	case K_ESCAPE:
		S_LocalSound ("misc/menu1.wav");
		M_Menu_Options_f ();
		break;

	default:
		break;
	}
}

/**
 * Loads in the Quake icon for the title bar. Icon must be named "quake.bmp"
 * and be located in same directory as chocolate-glquake.exe. Use magenta
 * as the transparent color key (R:266, G:0, B:255).
 * @return	0 on success, -1 on error 
 */
int VID_LoadQuakeIcon(void) 
{
	SDL_Surface* pimage = NULL;
	SDL_Surface* ptemp = SDL_LoadBMP(".\\quake.bmp");

	quakeicon = NULL;

	if (!ptemp) 
	{
		Con_Printf("Unable to load title bar icon.\n");
		return -1;
	}

	if (ptemp) 
	{
		pimage = SDL_DisplayFormat(ptemp);
        SDL_FreeSurface(ptemp);

        /* Remove background color key */
		if (pimage != NULL) 
		{
			Uint32 colorKey = SDL_MapRGB(pimage->format, 255, 0, 255);
			SDL_SetColorKey(pimage, SDL_SRCCOLORKEY, colorKey);
		} else {
			Con_Printf("Unable to process title bar icon.\n");
			return -1;
		}
	}
	quakeicon = pimage;
	return 0;
}

/** 
 * Frees the resources taken by the Quake titlebar icon.
 */
void VID_FreeQuakeIcon(void) 
{
	if (quakeicon) 
	{
		SDL_FreeSurface(quakeicon);
		quakeicon = NULL;
	}
}