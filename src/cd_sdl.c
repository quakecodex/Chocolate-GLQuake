/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

Quake is a trademark of Id Software, Inc., (c) 1996 Id Software, Inc. All
rights reserved.
*/

/** 
 * \file cd_sdl.c
 * Functions for playing CD music.
 * 
 * @author: James Johnson
 * @version: 0.1.2 2017-1-8
 */

#include "quakedef.h"
#include "sdlquake.h"

extern	cvar_t	bgmvolume;

static qboolean cdValid = false; /**< True if there is a CD in the drive. */
static qboolean	playing = false; /**< True when the CD is playing. */
/** True if the CD was playing before being paused, not stopped. Used to
track when resume command will work. */
static qboolean	wasPlaying = false;
static qboolean	initialized = false; /**< True when CD-ROM drive is initialized. */
/** True when CD-ROM drive is enabled and ready to play. */
static qboolean	enabled = false; 
static qboolean playLooping = false; /**< True if current track should be looped. */
static float	cdvolume; /**< CD Music volume. (0 or 1) */
static byte 	remap[100]; /**< Track lookup table. */
static byte		playTrack; /**< Track number of playing track. */
static byte		maxTrack; /**< Maximum tracks on the CD. */
static SDL_CD *cdinfo; /**< SDL CD-ROM structure. */

/**
 * Ejects the CD.
 */
static void CDAudio_Eject(void)
{
    if (SDL_CDEject(cdinfo) < 0)
		Con_DPrintf("SDL_CDEject failed (0)\n");
}

/**
 * Not supported by SDL CD.
 */
static void CDAudio_CloseDoor(void)
{
	// Do nothing
	Con_DPrintf("CDAudio: Close door not implemented.\n");
}


/**
 * Opens the CD ROM device and checks the number of tracks on the CD.
 */
static int CDAudio_GetAudioDiskInfo(void)
{
	cdValid = false;

	if (!cdinfo) {
		cdinfo = SDL_CDOpen(0);
		if (!cdinfo)
		{
			Con_DPrintf("CDAudio: drive not ready\n");
			return -1;
		}
	}

	if (!CD_INDRIVE(SDL_CDStatus(cdinfo))) {
		Con_DPrintf("CDAudio: No CD in drive\n");
		return -1;
	}

	if (cdinfo->numtracks < 1)
	{
		Con_DPrintf("CDAudio: no music tracks\n");
		return -1;
	}

	cdValid = true;
	maxTrack = cdinfo->numtracks;

	return 0;
}

/**
 * Plays a CD track.
 * @param track		The track number to play starting with 2. The 1st track on 
 *					the Quake CD is reserved for the game data. Vanilla Quake 
 *					has 10 tracks in total, numbered 2-11.
 * @param looping	True to loop the music, false to play once. Note that this 
 *					does not work on all systems. 
 */
void CDAudio_Play(byte track, qboolean looping)
{
	int dwReturn;
	CDstatus status;

	/* Check that the CD-ROM drive is initialized and there is a CD 
	in the tray */
	if (!enabled)
		return;
	
	if (!cdValid)
	{
		CDAudio_GetAudioDiskInfo();
		if (!cdValid)
			return;
	}
	
	/* SDL ignores data tracks, so it always starts at 1. The track number  
	needs to be adjusted to work with SDL. */
	track = remap[track];

	/* Skip if the track is already playing. */
	status = SDL_CDStatus(cdinfo);
	if (status == CD_PLAYING) 
	{
		if (track == playTrack)
			return;
		CDAudio_Stop();
	}

	/* Check to make sure the track is valid. */
	if (track < 1 || track > maxTrack)
	{
		Con_DPrintf("CDAudio: Bad track number %u.\n", track);
		return;
	}
	
	/* Play the track */
	dwReturn = SDL_CDPlay(cdinfo, cdinfo->track[track].offset, cdinfo->track[track].length);
	if (dwReturn < 0)
	{
		Con_DPrintf("SDL_CDPlayTracks failed (%i)\n", dwReturn);
		return;
	}
	
	playLooping = looping;
	playTrack = track;
	playing = true;

	/* There's no volume control for CD music, so pause the CD if the
	volume is off. */
	if (cdvolume == 0.0)
		CDAudio_Pause ();
}

/**
 * Stops CD playback.
 */
void CDAudio_Stop(void)
{
	if (!enabled)
		return;
	
	if (!playing)
		return;

    if (SDL_CDStop(cdinfo) < 0)
		Con_DPrintf("SDL_CDStop failed (-1)");

	wasPlaying = false;
	playing = false;
}


/**
 * Pauses CD playback.
 */
void CDAudio_Pause(void)
{
	if (!enabled)
		return;

	if (!playing)
		return;

    if (SDL_CDPause(cdinfo) < 0)
		Con_DPrintf("SDL_CDPause failed (-1)");

	wasPlaying = playing;
	playing = false;
}

/**
 * Resumes playback after a pause.
 */
void CDAudio_Resume(void)
{
	if (!enabled)
		return;
	
	if (!cdValid)
		return;

	if (!wasPlaying)
		return;

	if (SDL_CDResume(cdinfo) < 0)
	{
		Con_DPrintf("CDAudio: SDL_CDResume failed (-1)\n");
		return;
	}
	playing = true;
}

/**
 * Registers CD console commands.
 */
static void CD_f (void)
{
	char	*command;
	int		ret;
	int		n;

	if (Cmd_Argc() < 2)
		return;

	command = Cmd_Argv (1);

	/* Turn CD music on */
	if (Q_strcasecmp(command, "on") == 0)
	{
		enabled = true;
		return;
	}

	/* Turn CD music off */
	if (Q_strcasecmp(command, "off") == 0)
	{
		if (playing)
			CDAudio_Stop();
		enabled = false;
		return;
	}

	/* Reset CD Music to defaults */
	if (Q_strcasecmp(command, "reset") == 0)
	{
		enabled = true;
		if (playing)
			CDAudio_Stop();
		for (n = 0; n < 100; n++)
			remap[n] = n;
		CDAudio_GetAudioDiskInfo();
		return;
	}

	/* Remap CD track to user entered number */
	if (Q_strcasecmp(command, "remap") == 0)
	{
		ret = Cmd_Argc() - 2;
		if (ret <= 0)
		{
			for (n = 1; n < 100; n++)
				if (remap[n] != n)
					Con_Printf("  %u -> %u\n", n, remap[n]);
			return;
		}
		for (n = 1; n <= ret; n++)
			remap[n] = Q_atoi(Cmd_Argv (n+1));
		return;
	}

	/* Close the CD-ROM drive door */
	if (Q_strcasecmp(command, "close") == 0)
	{
		CDAudio_CloseDoor();
		return;
	}

	if (!cdValid)
	{
		CDAudio_GetAudioDiskInfo();
		if (!cdValid)
		{
			Con_Printf("No CD in player.\n");
			return;
		}
	}

	/* Play a track */
	if (Q_strcasecmp(command, "play") == 0)
	{
		CDAudio_Play((byte)Q_atoi(Cmd_Argv (2)), false);
		return;
	}

	/* Loop a track; 1 = loop, 0 = play once */
	if (Q_strcasecmp(command, "loop") == 0)
	{
		CDAudio_Play((byte)Q_atoi(Cmd_Argv (2)), true);
		return;
	}

	/* Stop playing a track */
	if (Q_strcasecmp(command, "stop") == 0)
	{
		CDAudio_Stop();
		return;
	}

	/* Pause the current track */
	if (Q_strcasecmp(command, "pause") == 0)
	{
		CDAudio_Pause();
		return;
	}

	/* Resume playing */
	if (Q_strcasecmp(command, "resume") == 0)
	{
		CDAudio_Resume();
		return;
	}

	/* Not implemented */
	if (Q_strcasecmp(command, "eject") == 0)
	{
		if (playing)
			CDAudio_Stop();
		CDAudio_Eject();
		cdValid = false;
		return;
	}

	/* Print out CD info */
	if (Q_strcasecmp(command, "info") == 0)
	{
		Con_Printf("%u tracks\n", maxTrack);
		Con_Printf("Volume is %f\n", cdvolume);
		if (playing)
		{
			Con_Printf("Currently %s track %u\n", 
						playLooping ? "looping" : "playing", 
						playTrack);
		}
		else if (wasPlaying)
		{
			Con_Printf("Paused %s track %u\n", 
						playLooping ? "looping" : "playing", 
						playTrack);
		}
		return;
	}
}

/**
 * Called once per frame. Checks for volume changes and restarts playback 
 * on looping tracks.
 */
void CDAudio_Update(void)
{
	if (!enabled)
		return;

	/* Set volume */
	if (bgmvolume.value != cdvolume)
	{
		if (cdvolume)
		{
			Cvar_SetValue ("bgmvolume", 0.0);
			cdvolume = bgmvolume.value;
			CDAudio_Pause ();
		}
		else
		{
			Cvar_SetValue ("bgmvolume", 1.0);
			cdvolume = bgmvolume.value;
			CDAudio_Resume ();
		}
	}
	
	/* Check if current track needs to be restarted. */
	if (playLooping) 
	{
		if ((SDL_CDStatus(cdinfo) != CD_PLAYING) && (SDL_CDStatus(cdinfo) != CD_PAUSED)) 
		{
			CDAudio_Play(playTrack, playLooping);
		}
	}
}


/**
 * Initializes the CD-ROM drive and gets track info.
 * @return	0 when initialization succeeds. -1 if there was an error.
 */
int CDAudio_Init(void)
{
	int n;

	if (cls.state == ca_dedicated)
		return -1;

	if (COM_CheckParm("-nocdaudio"))
		return -1;

	if (SDL_InitSubSystem(SDL_INIT_CDROM) < 0) 
	{
		Con_Printf("CDAudio_Init: unable to initialize SDL CD\n");
		return -1;
	}

	n = SDL_CDNumDrives();
	if (n < 1) {
		Con_Printf("CDAudio_Init: no CD-ROM drives present\n");
		return -1;
	}

	for (n = 0; n < 100; n++)
		remap[n] = n - 1;
	initialized = true;
	enabled = true;

	Cmd_AddCommand ("cd", CD_f);

	Con_Printf("CD Audio Initialized\n");

	return 0;
}


/**
 * Stops audio, closes the CD drive and shuts down SDL CD.
 */
void CDAudio_Shutdown(void)
{
	if (!initialized)
		return;
	CDAudio_Stop();
	if (cdinfo) 
	{
		SDL_CDClose(cdinfo);
		cdinfo = NULL;
	}
	SDL_QuitSubSystem(SDL_INIT_CDROM);
}
