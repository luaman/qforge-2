/* $Id$
 *
 * snd_mem.c: sound caching
 *
 * Copyright (C) 1997-2001 Id Software, Inc.
 * Copyright (c) 2003 The Quakeforge Project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "client.h"
#include "snd_loc.h"

#ifdef HAVE_MPG123
// Added mpg123 libraries for MP3 playback support */
//#include "mpglib/mpg123.h"
//#include "mpglib/mpglib.h"
#include "mpglib/libmpg123.h"
sfxcache_t *S_LoadMP3Sound(sfx_t *s);
#endif

int			cache_full_cycle;

byte *S_Alloc (int size);

/*
================
ResampleSfx
================
*/
void ResampleSfx (sfx_t *sfx, int inrate, int inwidth, byte *data)
{
	int		outcount;
	int		srcsample;
	float	stepscale;
	int		i;
	int		sample, samplefrac, fracstep;
	sfxcache_t	*sc;
	
	sc = sfx->cache;
	if (!sc)
		return;

	stepscale = (float)inrate / dma.speed;	// this is usually 0.5, 1, or 2

	outcount = sc->length / stepscale;
	sc->length = outcount;
	if (sc->loopstart != -1)
		sc->loopstart = sc->loopstart / stepscale;

	sc->speed = dma.speed;
	if (s_loadas8bit->value)
		sc->width = 1;
	else
		sc->width = inwidth;
	sc->stereo = 0;

// resample / decimate to the current source rate

	if (stepscale == 1 && inwidth == 1 && sc->width == 1)
	{
// fast special case
		for (i=0 ; i<outcount ; i++)
			((signed char *)sc->data)[i]
			= (int)( (unsigned char)(data[i]) - 128);
	}
	else
	{
// general case
		samplefrac = 0;
		fracstep = stepscale*256;
		for (i=0 ; i<outcount ; i++)
		{
			srcsample = samplefrac >> 8;
			samplefrac += fracstep;
			if (inwidth == 2)
				sample = LittleShort ( ((short *)data)[srcsample] );
			else
				sample = (int)( (unsigned char)(data[srcsample]) - 128) << 8;
			if (sc->width == 2)
				((short *)sc->data)[i] = sample;
			else
				((signed char *)sc->data)[i] = sample >> 8;
		}
	}
}

//=============================================================================

/*
==============
S_LoadSound
==============
*/
sfxcache_t *S_LoadSound (sfx_t *s)
{
    char	namebuffer[MAX_QPATH];
	byte	*data;
	wavinfo_t	info;
	int		len;
	float	stepscale;
	sfxcache_t	*sc;
	int		size;
	char	*name;

	if (s->name[0] == '*')
		return NULL;

// see if still in memory
	sc = s->cache;
	if (sc)
		return sc;

//Com_Printf ("S_LoadSound: %x\n", (int)stackbuf);
// load it in
	if (s->truename)
		name = s->truename;
	else
		name = s->name;

	if (name[0] == '#')
		strcpy(namebuffer, &name[1]);
	else
		Com_sprintf (namebuffer, sizeof(namebuffer), "sound/%s", name);

#ifdef HAVE_MPG123
	/* MP3 Support */
	len = strlen(name);
	if (!strcmp(name+len-4, ".mp3"))
	    return S_LoadMP3Sound(s);
#endif

//	Com_Printf ("loading %s\n",namebuffer);

	size = FS_LoadFile (namebuffer, (void **)&data);

	if (!data)
	{
		Com_DPrintf ("Couldn't load %s\n", namebuffer);
		return NULL;
	}

	info = GetWavinfo (s->name, data, size);
	if (info.channels != 1)
	{
		Com_Printf ("%s is a stereo sample\n",s->name);
		FS_FreeFile (data);
		return NULL;
	}

	stepscale = (float)info.rate / dma.speed;	
	len = info.samples / stepscale;

	len = len * info.width * info.channels;

	sc = s->cache = Z_Malloc (len + sizeof(sfxcache_t));
	if (!sc)
	{
		FS_FreeFile (data);
		return NULL;
	}
	
	sc->length = info.samples;
	sc->loopstart = info.loopstart;
	sc->speed = info.rate;
	sc->width = info.width;
	sc->stereo = info.channels;

	ResampleSfx (s, sc->speed, sc->width, data + info.dataofs);

	FS_FreeFile (data);

	return sc;
}

#ifdef HAVE_MPG123
/* mp3 support code block */
===========================================================================
MP3 Audio Support
By Robert 'Heffo' Heffernan
===========================================================================
*/

#define TEMP_BUFFER_SIZE 0x100000 //1mb temp buffer for MP3

/* MP3 New - Allocate & initialise a new mp3_t structure */
mp3_t * MP3_New(char *filename) {
    mp3_t *mp3;

    /* Allocate a piece of ram the size of the mp3_t structure */
    mp3 = malloc(sizeof(mp3_t));
    if (!mp3)
    return NULL; /* Couldn't allocate ram */
    /* Wipe the freshly allocated Ram */
    memset(mp3, 0, sizeof(mp3_t));

    /* Load the MP3 file via the filesystem for compatibility
     * set mp3->mp3data pointer memory location of loaded file
     * set mp3->mp3size value to size of file */
    mp3->mp3size = FS_LoadFile(filename, (void **)&mp3->mp3data);
    if (!mp3->mp3data) {
	/* the file couldn't be loaded so free the memory */
	free(mp3);

	/* then return nothing */
	return NULL;
    }

    /* mp3_t loaded and memory allocated return the mp3 */
    return mp3;
}

/* MP3_Free - Release an mp3_t structure, and free all memory used by it */
void MP3_Free(mp3_t *mp3) {
    if (!mp3)
	return;
    if (mp3->mp3data) /* mp3 still loaded */
	FS_FreeFile(mp3->mp3data); /* free mp3 */
    if (mp3->rawdata) /* raw audio left */
	free(mp3->rawdata);
    free(mp3);
}

/* ExtendRawBuffer - Allocates & extends a buffer for the raw decompressed audio data */
qboolean ExtendRawBuffer(mp3_t *mp3, int size) {
    byte *newbuf;
	
    if(!mp3)
	return false; 
		
    if(!mp3->rawdata) {
	mp3->rawdata = malloc(size);
	if(!mp3->rawdata)
	    return false; 
	mp3->rawsize = size;
	return true;
    }
    newbuf = malloc(mp3->rawsize + size);
    if(!newbuf)
	return false; 
    memcpy(newbuf, mp3->rawdata, mp3->rawsize);

    mp3->rawsize += size;
    free(mp3->rawdata);

    mp3->rawdata = newbuf;

    return true;
}

qboolean MP3_Process(mp3_t *mp3) {
    struct mpstr	mpeg;
    byte	sbuff[8192];
    byte	*tbuff, *tb;
    int	size, ret, tbuffused;

    if (!mp3)
	return false;
    if (!mp3->mp3data)
	return false;

    /* allocate for large temp decode buffer */
    tbuff = malloc(TEMP_BUFFER_SIZE);
    if (!tbuff)
	return false;

    /* Initialize MP3 Decoder */
    initMPGAUDIO(&mpeg);

    /* Decode the 1st frame of MP3 data, store in buffer */
    ret = decodeMPGAUDIO(&mpeg, mp3->mp3data, mp3->mp3size, tbuff, TEMP_BUFFER_SIZE, &size);
    /* Copy the MP3s format details (samplerate # chans) */
    mp3->samplerate = freqs[mpeg.fr.sampling_frequency];
    mp3->channels = mpeg.fr.stereo;

    /* Set a pointer to the start of the temporary buffer
     * and then offset it by the number of bytes decoded.
     * Also set the amount of the temp buffer that has been used. */
    tb = tbuff + size;
    tbuffused = size;

    /* Start a loop that ends when the MP3 decoder fails */
    while (ret == MP3_OK) {
	/* Decode the next frame into smaller temp */
	ret = decodeMPGAUDIO(&mpeg, NULL, 0, sbuff, 8192, &size);

	if(tbuffused + size >= TEMP_BUFFER_SIZE) {
	    if (!ExtendRawBuffer(mp3, tbuffused)) {
		free(tbuff);

		/* error shutdown MP3 decoder */
		/*ExitMP3(&mpeg);*/
		/*return failure */
		return false;
	    }

	    memcpy(mp3->rawdata + mp3->rawpos, tbuff, tbuffused);
	    tbuffused = 0;
	    tb = tbuff;
	    mp3->rawpos = mp3->rawsize;
	}

	memcpy(tb, sbuff, size);
	tb+=size;
	tbuffused+=size;
    }

    /* done decoding, flush buffer into sample buffer */
    if (!ExtendRawBuffer(mp3, tbuffused)) {
	/* Failed to extend sample buffer */
	free (tbuff);
	/* ExitMP3(&mpeg); */
	return false;
    }

    /* CPY lrg temp buff into extended sample buff */
    memcpy (mp3->rawdata + mp3->rawpos, tbuff, tbuffused);
    /* calculate samples based on size of decompressed MP3 / 2 (16b) */
    mp3->samples = (mp3->rawsize / 2) / mp3->channels;
    free(tbuff);
    /* ExitMP3(&mpeg); */

    return true;
}

/* S_LoadMP3Sound - A modified version of S_LoadSound but with MP3 support */
sfxcache_t *S_LoadMP3Sound(sfx_t *s) {
    char namebuffer[MAX_QPATH];
    mp3_t *mp3;
    float stepscale;
    sfxcache_t *sc;
    char *name;

    if (s->truename)
	name = s->truename;
    else
	name = s->name;
    if (name[0] == '#')
	strcpy(namebuffer, &name[1]);
    else
	Com_sprintf (namebuffer, sizeof(namebuffer), "sound/%s", name);

    mp3 = MP3_New(namebuffer);
    if (!mp3) {
	/* Couldn't allocate it, so show an error and return nothing */
	Com_DPrintf ("Couldn't load %s\n", namebuffer);
	return NULL;
    }

    /* Process mp3_t for error */
    if (!MP3_Process(mp3)) {
	/* Something did, so free the mp3_t */
	MP3_Free(mp3);
	Com_DPrintf ("Couldn't load %s\n", namebuffer);
	return NULL;
    }

    if (mp3->channels == 2) /* must be mono */ {
	/* stero */
	MP3_Free(mp3);
	Com_Printf ("%s is a stero sample\n", s->name);
	return NULL;
    }

    /* Align samplerate of MP3 and Quake */
    stepscale = (float)mp3->samplerate / dma.speed;

    /* Allocate a piece of zone memory to store the decompressed
     * & scaled MP3 aswell as it's attatched sfxcache_t structure */
    sc = s->cache = Z_Malloc ((mp3->rawsize /stepscale) + sizeof(sfxcache_t));
    if (!sc) {
	/* Couldn't allocate the zone */
	MP3_Free(mp3);
	Com_DPrintf ("Couldn't load %s\n", namebuffer);
	return NULL;
    }

    /* Copy a few details of MP3 into the sfxcache_t structure */
    sc->length = mp3->samples;
    sc->loopstart = -1;
    sc->speed = mp3->samplerate;
    sc->width = 2;
    sc->stereo = mp3->channels;

    /* Resample the decompressed MP3 to match Dominion's samplerate */
    ResampleSfx (s, sc->speed, sc->width, mp3->rawdata);

    //* Free the mp3_t structure as it's no longer needed */
    MP3_Free(mp3);
    /* Return sfxcache_t structure for the decoded & processed MP3 */
    return sc;
}
#endif /* HAVE_MPG123 */

/*
===============================================================================

WAV loading

===============================================================================
*/


byte	*data_p;
byte 	*iff_end;
byte 	*last_chunk;
byte 	*iff_data;
int 	iff_chunk_len;


short GetLittleShort(void)
{
	short val = 0;
	val = *data_p;
	val = val + (*(data_p+1)<<8);
	data_p += 2;
	return val;
}

int GetLittleLong(void)
{
	int val = 0;
	val = *data_p;
	val = val + (*(data_p+1)<<8);
	val = val + (*(data_p+2)<<16);
	val = val + (*(data_p+3)<<24);
	data_p += 4;
	return val;
}

void FindNextChunk(char *name)
{
	while (1)
	{
		data_p=last_chunk;

		if (data_p >= iff_end)
		{	// didn't find the chunk
			data_p = NULL;
			return;
		}
		
		data_p += 4;
		iff_chunk_len = GetLittleLong();
		if (iff_chunk_len < 0)
		{
			data_p = NULL;
			return;
		}
//		if (iff_chunk_len > 1024*1024)
//			Sys_Error ("FindNextChunk: %i length is past the 1 meg sanity limit", iff_chunk_len);
		data_p -= 8;
		last_chunk = data_p + 8 + ( (iff_chunk_len + 1) & ~1 );
		if (!strncmp((char *)data_p, name, 4))
			return;
	}
}

void FindChunk(char *name)
{
	last_chunk = iff_data;
	FindNextChunk (name);
}


void DumpChunks(void)
{
	char	str[5];
	
	str[4] = 0;
	data_p=iff_data;
	do
	{
		memcpy (str, data_p, 4);
		data_p += 4;
		iff_chunk_len = GetLittleLong();
		Com_Printf ("0x%p : %s (%d)\n", data_p - 4, str, iff_chunk_len);
		data_p += (iff_chunk_len + 1) & ~1;
	} while (data_p < iff_end);
}

/*
============
GetWavinfo
============
*/
wavinfo_t GetWavinfo (char *name, byte *wav, int wavlength)
{
	wavinfo_t	info;
	int     i;
	int     format;
	int		samples;

	memset (&info, 0, sizeof(info));

	if (!wav)
		return info;
		
	iff_data = wav;
	iff_end = wav + wavlength;

// find "RIFF" chunk
	FindChunk("RIFF");
	if (!(data_p && !strncmp((char *)data_p+8, "WAVE", 4)))
	{
		Com_Printf("Missing RIFF/WAVE chunks\n");
		return info;
	}

// get "fmt " chunk
	iff_data = data_p + 12;
// DumpChunks ();

	FindChunk("fmt ");
	if (!data_p)
	{
		Com_Printf("Missing fmt chunk\n");
		return info;
	}
	data_p += 8;
	format = GetLittleShort();
	if (format != 1)
	{
		Com_Printf("Microsoft PCM format only\n");
		return info;
	}

	info.channels = GetLittleShort();
	info.rate = GetLittleLong();
	data_p += 4+2;
	info.width = GetLittleShort() / 8;

// get cue chunk
	FindChunk("cue ");
	if (data_p)
	{
		data_p += 32;
		info.loopstart = GetLittleLong();
//		Com_Printf("loopstart=%d\n", sfx->loopstart);

	// if the next chunk is a LIST chunk, look for a cue length marker
		FindNextChunk ("LIST");
		if (data_p)
		{
			if (!strncmp ((char *)data_p + 28, "mark", 4))
			{	// this is not a proper parse, but it works with cooledit...
				data_p += 24;
				i = GetLittleLong ();	// samples in loop
				info.samples = info.loopstart + i;
//				Com_Printf("looped length: %i\n", i);
			}
		}
	}
	else
		info.loopstart = -1;

// find data chunk
	FindChunk("data");
	if (!data_p)
	{
		Com_Printf("Missing data chunk\n");
		return info;
	}

	data_p += 4;
	samples = GetLittleLong () / info.width;

	if (info.samples)
	{
		if (samples < info.samples)
			Com_Error (ERR_DROP, "Sound %s has a bad loop length", name);
	}
	else
		info.samples = samples;

	info.dataofs = data_p - wav;
	
	return info;
}

