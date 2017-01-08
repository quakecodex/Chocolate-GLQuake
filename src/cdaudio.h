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
 * \file cdaudio.h
 * Functions for playing CD music.
 * 
 * @author: James Johnson
 * @version: 0.1.2 2017-1-8
 */

/**
 * Initializes the CD-ROM drive and gets track info.
 * @return	0 when initialization succeeds. -1 if there was an error.
 */
int CDAudio_Init(void);

/**
 * Plays a CD track.
 * @param track		The track number to play starting with 2. The 1st track on 
 *					the Quake CD is reserved for the game data. Vanilla Quake 
 *					has 10 tracks in total, numbered 2-11.
 * @param looping	True to loop the music, false to play once. Note that this 
 *					does not work on all systems. 
 */
void CDAudio_Play(byte track, qboolean looping);

/**
 * Stops CD playback.
 */
void CDAudio_Stop(void);

/**
 * Pauses CD playback.
 */
void CDAudio_Pause(void);

/**
 * Resumes playback after a pause.
 */
void CDAudio_Resume(void);

/**
 * Stops audio, closes the CD drive and shuts down SDL CD.
 */
void CDAudio_Shutdown(void);

/**
 * Called once per frame. Checks for volume changes and restarts playback 
 * on looping tracks.
 */
void CDAudio_Update(void);
