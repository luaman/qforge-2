/* libao sound output
*
* Copyright(c) 2004 Jamie Wilkinson <jaq@spacepants.org>
*                    for The Quakeforge Project.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or(at your option) any later version.
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

struct sndinfo * si;

ao_device * device;

/* SNDDMA_Init: initialises cycling through a DMA bufffer and returns
 *  information on it
 */
qboolean SNDDMA_Init(struct sndinfo * s){
	int driver_id;
	ao_sample_format format;
	ao_option * options = NULL;
	
	if(snd_inited)
		return 1;
		
	snd_inited = 0;
	
	si = s;
	
	ao_initialize();
	
	if((driver_id = ao_driver_id(si->device->string)) == -1){
		si->Com_Printf("ao: no driver %s found, trying default\n", si->device->string);
		if((driver_id = ao_default_driver_id()) == -1){
			Com_Printf("ao: no default driver found\n");
			return 0;
		}
	}
	
	format.bits = si->dma->samplebits = si->bits->value;
	format.rate = si->dma->speed = 44100;
	format.channels = si->dma->channels = si->channels->value;
	format.byte_format = AO_FMT_NATIVE;
	
	switch(si->dma->speed){
		case 44100:
			si->dma->samples = 2048 * si->dma->channels;
			break;
		case 22050:
			si->dma->samples = 1024 * si->dma->channels;
			break;
		default:
			si->dma->samples = 512 * si->dma->channels;
			break;
	}
	
	if((device = ao_open_live(driver_id, &format, options)) == NULL){
		switch(errno){
			case AO_ENODRIVER:
				Com_Printf("ao: no driver for %d\n", driver_id);
				break;
			case AO_ENOTLIVE:
				Com_Printf("ao: not a valid live output device\n");
				break;
			case AO_EBADOPTION:
				Com_Printf("ao: valid option has invalid key\n");
				break;
			case AO_EOPENDEVICE:
				Com_Printf("ao: cannot open device\n");
				break;
			case AO_EFAIL:
				Com_Printf("ao: something broke during ao_open_live\n");
				break;
			case AO_ENOTFILE:
				Com_Printf("ao: not a file\n");
				break;
			case AO_EOPENFILE:
				Com_Printf("ao: can't open file\n");
				break;
			case AO_EFILEEXISTS:
				Com_Printf("ao: file exists already\n");
				break;
			default:
				Com_Printf("ao: whoa, bad trip dude\n");
				break;
		}
		return 0;
	}
	
	samplesize = si->dma->samplebits >> 3;
	si->dma->buffer = malloc(si->dma->samples * samplesize);
	memset(si->dma->buffer, 0, si->dma->samples * samplesize);
	
	si->dma->samplepos = 0;
	si->dma->submission_chunk = 1;
	
	si->Com_Printf("ao: buffer size is %d, %d samples\n", si->dma->samples * samplesize, si->dma->samples);
	
	snd_inited = 1;
	return 1;
}

/* SNDDMA_GetDMAPos: get the current DMA position
 */
int SNDDMA_GetDMAPos(void){
	if(snd_inited)
		return si->dma->samplepos;
	else
		Com_Printf("Sound not initialised\n");
		
	return 0;
}

/* SNDDMA_Shutdown: shutdown the DMA xfer
 */
void SNDDMA_Shutdown(void){
	if(snd_inited){
		ao_close(device);
		ao_shutdown();
		free(si->dma->buffer);
		si->dma->buffer = 0;
		snd_inited = 0;
	}
}

/* SNDDMA_Submit: send sound to device if buffer isn't really the dma buffer
 */
void SNDDMA_Submit(void){
	if(!snd_inited)
		return;
		
	/* ao_play returns success, not number of samples successfully output
	 * unlike alsa or arts, so we can only assume that the whole buffer
	 * made it out... though this makes updating si->dma->samplepos easy */
	if(ao_play(device, si->dma->buffer, si->dma->samples * samplesize) == 0){
		Com_Printf("W: error occurred while playing buffer\n");
		ao_close(device);
		ao_shutdown();
		snd_inited = 0;
	}
	si->dma->samplepos += si->dma->samples;
}

void SNDDMA_BeginPainting(void){}
