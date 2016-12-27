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
// vid.h -- video driver defs

#define VID_CBITS	6
#define VID_GRADES	(1 << VID_CBITS)

// a pixel can be one, two, or four bytes
typedef byte pixel_t;

typedef struct vrect_s
{
	int				x,y,width,height;
	struct vrect_s	*pnext;
} vrect_t;

typedef struct
{
	pixel_t			*buffer;		// invisible buffer
	pixel_t			*colormap;		// 256 * VID_GRADES size
	unsigned short	*colormap16;	// 256 * VID_GRADES size
	int				fullbright;		// index of first fullbright color
	unsigned		rowbytes;	// may be > width if displayed in a window
	unsigned		width;		
	unsigned		height;
	float			aspect;		// width / height -- < 0 is taller than wide
	int				numpages;
	int				recalc_refdef;	// if true, recalc vid-based stuff
	pixel_t			*conbuffer;
	int				conrowbytes;
	unsigned		conwidth;
	unsigned		conheight;
	int				maxwarpwidth;
	int				maxwarpheight;
	pixel_t			*direct;		// direct drawing to framebuffer, if not
									//  NULL
} viddef_t;

extern	viddef_t	vid;				// global video state
extern	unsigned short	d_8to16table[256];
extern	unsigned	d_8to24table[256];
extern void (*vid_menudrawfn)(void);
extern void (*vid_menukeyfn)(int key);

/**
 * Set the palette quake will use for rendering. Used to convert textures to full color for OpenGL.
 * @param	palette		Array of 256 RGB triplets defining the palette.
 */
void	VID_SetPalette (unsigned char *palette);
// called at startup and after any gamma correction

/**
 * Shifts the pallete towards a color. (Red or white.) Not used in GL Quake
 * @param	palette		Array of 256 RGB triplets defining the palette.
 */
void	VID_ShiftPalette (unsigned char *palette);
// called for bonus and pain flashes, and for underwater color changes

/**
 * Initializes the video system. Creates a window and initializes OpenGL. 
 * @param	palette	Pointer to an array of 256 RGB triplets (unsigned byte) specifying the current palette
 */
void	VID_Init (unsigned char *palette);
// Called at startup to set up translation tables, takes 256 8 bit RGB values
// the palette data will go away after the call, so it must be copied off if
// the video driver will need it again

/**
 * Exits the current video mode. Shuts down the app window and SDL.
 */
void	VID_Shutdown (void);
// Called at shutdown

/**
 * Updates the video mode? Not used in GL Quake */
void	VID_Update (vrect_t *rects);
// flushes the given rectangles from the view buffer to the screen

/**
 * Sets up the video mode.
 * @param	modenum		Index into modelist array for which video mode to set.
 * @param	palette		Array of RGB triplets specifying the video palette. 
 * Should be gamma corrected.
 * @return	true (1) if the mode is set up.
 */
int VID_SetMode (int modenum, unsigned char *palette);
// sets the mode; only used by the Quake engine for resetting to mode 0 (the
// base mode) on memory allocation failures

