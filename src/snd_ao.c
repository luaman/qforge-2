/* libao sound output
 *
 * Copyright (c) 2004 Jamie Wilkinson <jaq@spacepants.org>
 *                    for The Quakeforge Project.
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
# include "config.h"
#endif

#ifdef HAVE_AO
# include <ao/ao.h>
#endif

#include "client.h"
#include "snd_loc.h"

static int snd_inited;
static int samplesize;

cvar_t * sndbits;
cvar_t * sndspeed;
cvar_t * sndchannels;
cvar_t * snddevice;

ao_device * device;

/* SNDDMA_Init: initialises cycling through a DMA bufffer and returns
 *  information on it
 */
qboolean SNDDMA_Init(void) {
    int driver_id;
    ao_sample_format format;
    ao_option * options = NULL;

    if (!snddevice) {
	sndbits = Cvar_Get("sndbits", "16", CVAR_ARCHIVE);
	sndspeed = Cvar_Get("sndspeed", "0", CVAR_ARCHIVE);
	sndchannels = Cvar_Get("sndchannels", "2", CVAR_ARCHIVE);
	snddevice = Cvar_Get("snddevice", "/dev/dsp", CVAR_ARCHIVE);
    }

    ao_initialize();

#if 1
    if ((driver_id = ao_default_driver_id()) == -1) {
#else
    if ((driver_id = ao_driver_id("wav")) == -1) {
#endif
	Com_Printf("Couldn't find a default driver for sound output\n");
	return 0;
    }

    /*ao_append_option(&options, "debug", "");*/

    format.bits = dma.samplebits = sndbits->value;
    format.rate = dma.speed = 44100;
    format.channels = dma.channels = sndchannels->value;
    format.byte_format = AO_FMT_NATIVE;

    switch (dma.speed) {
      case 44100:
	dma.samples = 2048 * dma.channels;
	break;
      case 22050:
	dma.samples = 1024 * dma.channels;
	break;
      default:
	dma.samples = 512 * dma.channels;
	break;
    }

#if 1
    if ((device = ao_open_live(driver_id, &format, options)) == NULL) {
#else
    if ((device = ao_open_file(driver_id, "foo.wav", 1, &format, options)) == NULL) {
#endif
	switch (errno) {
	  case AO_ENODRIVER:
	    Com_Printf("W: No ao driver for %d\n", driver_id);
	    break;
	  case AO_ENOTLIVE:
	    Com_Printf("W: Not a valid live output device\n");
	    break;
	  case AO_EBADOPTION:
	    Com_Printf("W: Valid option has invalid key\n");
	    break;
	  case AO_EOPENDEVICE:
	    Com_Printf("W: Cannot open device\n");
	    break;
	  case AO_EFAIL:
	    Com_Printf("W: Something broke during ao_open_live\n");
	    break;
	  case AO_ENOTFILE:
	    Com_Printf("W: Not a file\n");
	    break;
	  case AO_EOPENFILE:
	    Com_Printf("W: Can't open file\n");
	    break;
	  case AO_EFILEEXISTS:
	    Com_Printf("W: File exists already\n");
	    break;
	  default:
	    Com_Printf("W: whoa, bad trip dude\n");
	    break;
	}
	return 0;
    }

    samplesize = dma.samplebits >> 3;
    dma.buffer = malloc(dma.samples * samplesize);
    memset(dma.buffer, 0, dma.samples * samplesize);

    snd_inited = 1;
    dma.samplepos = 0;
    dma.submission_chunk = 1;

    return 1;
}

/* SNDDMA_GetDMAPos: get the current DMA position
 */
int SNDDMA_GetDMAPos(void) {
    if (snd_inited)
	return dma.samplepos;
    else
	Com_Printf("Sound not initialised\n");

    return 0;
}

/* SNDDMA_Shutdown: shutdown the DMA xfer
 */
void SNDDMA_Shutdown(void) {
    if (snd_inited) {
	ao_close(device);
	ao_shutdown();
	free(dma.buffer);
	dma.buffer = 0;
	snd_inited = 0;
    }
}

/* SNDDMA_Submit: send sound to device if buffer isn't really the dma buffer
 */
void SNDDMA_Submit(void) {
    if (!snd_inited)
	return;

    /* ao_play returns success, not number of samples successfully output
     * unlike alsa or arts, so we can only assume that the whole buffer
     * made it out... though this makes updating dma.samplepos easy */
    if (ao_play(device, dma.buffer, dma.samples * samplesize) == 0) {
	Com_Printf("W: error occurred while playing buffer\n");
	ao_close(device);
	ao_shutdown();
	snd_inited = 0;
    }
    dma.samplepos += dma.samples;
}

void SNDDMA_BeginPainting(void) {
}
