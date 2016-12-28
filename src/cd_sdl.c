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
// Quake is a trademark of Id Software, Inc., (c) 1996 Id Software, Inc. All
// rights reserved.

#include "quakedef.h"
#include "sdlquake.h"

extern	cvar_t	bgmvolume;

static qboolean cdValid = false;
static qboolean	playing = false;
static qboolean	wasPlaying = false;
static qboolean	initialized = false;
static qboolean	enabled = false;
static qboolean playLooping = false;
static float	cdvolume;
static byte 	remap[100];
static byte		playTrack;
static byte		maxTrack;

static SDL_CD *cdinfo;

UINT	wDeviceID;

/**
 * Ejects the CD 
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
		cdinfo = SDL_CDOpen(wDeviceID);
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
 * Plays a track.
 */
void CDAudio_Play(byte track, qboolean looping)
{
	int dwReturn;
	int trackLength;
	CDstatus status;

	if (!enabled)
		return;
	
	if (!cdValid)
	{
		CDAudio_GetAudioDiskInfo();
		if (!cdValid)
			return;
	}

	track = remap[track];

	status = SDL_CDStatus(cdinfo);

	if (track < 1 || track > maxTrack)
	{
		Con_DPrintf("CDAudio: Bad track number %u.\n", track);
		return;
	}
	
	// don't try to play a non-audio track
	dwReturn = SDL_CDPlayTracks(cdinfo, track, 0, 0, cdinfo->track[track].length);
	if (dwReturn < 0)
	{
		Con_DPrintf("SDL_CDPlayTracks failed (%i)\n", dwReturn);
		return;
	}

	// get the length of the track to be played
	trackLength = cdinfo->track[track].length;
	
	playLooping = looping;
	playTrack = track;
	playing = true;

	if (cdvolume == 0.0)
		CDAudio_Pause ();
}


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


static void CD_f (void)
{
	char	*command;
	int		ret;
	int		n;

	if (Cmd_Argc() < 2)
		return;

	command = Cmd_Argv (1);

	if (Q_strcasecmp(command, "on") == 0)
	{
		enabled = true;
		return;
	}

	if (Q_strcasecmp(command, "off") == 0)
	{
		if (playing)
			CDAudio_Stop();
		enabled = false;
		return;
	}

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

	if (Q_strcasecmp(command, "play") == 0)
	{
		CDAudio_Play((byte)Q_atoi(Cmd_Argv (2)), false);
		return;
	}

	if (Q_strcasecmp(command, "loop") == 0)
	{
		CDAudio_Play((byte)Q_atoi(Cmd_Argv (2)), true);
		return;
	}

	if (Q_strcasecmp(command, "stop") == 0)
	{
		CDAudio_Stop();
		return;
	}

	if (Q_strcasecmp(command, "pause") == 0)
	{
		CDAudio_Pause();
		return;
	}

	if (Q_strcasecmp(command, "resume") == 0)
	{
		CDAudio_Resume();
		return;
	}

	if (Q_strcasecmp(command, "eject") == 0)
	{
		if (playing)
			CDAudio_Stop();
		CDAudio_Eject();
		cdValid = false;
		return;
	}

	if (Q_strcasecmp(command, "info") == 0)
	{
		Con_Printf("%u tracks\n", maxTrack);
		if (playing)
			Con_Printf("Currently %s track %u\n", playLooping ? "looping" : "playing", playTrack);
		else if (wasPlaying)
			Con_Printf("Paused %s track %u\n", playLooping ? "looping" : "playing", playTrack);
		Con_Printf("Volume is %f\n", cdvolume);
		return;
	}
}

void CDAudio_Update(void)
{
	if (!enabled)
		return;

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

	wDeviceID = 0;

	for (n = 0; n < 100; n++)
		remap[n] = n - 1;
	initialized = true;
	enabled = true;

	Cmd_AddCommand ("cd", CD_f);

	Con_Printf("CD Audio Initialized\n");

	return 0;
}


/**
 * Stops audio, closes the CD and shuts down SDL CD.
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
