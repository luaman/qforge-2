/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_ALSA
#include <alsa/asoundlib.h>
#endif

#include "client.h"
#include "snd_loc.h"

static int snd_inited;

static snd_pcm_t * pcm_handle;
static snd_pcm_hw_params_t * hw_params;

#define BUFFER_SIZE 4096

int tryrates[] = { 44100, 22051, 11025, 8000 };

/* sound info */
static struct sndinfo * si;

qboolean SNDDMA_Init(struct sndinfo * s) {
    int i;
    int err;

    if (snd_inited)
	return 1;

    snd_inited = 0;

    si = s;

    si->dma->samples = 1024;

    if ((err = snd_pcm_open(&pcm_handle, si->device->string, 
			    SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
	si->Com_Printf("ALSA snd error, cannot open device %s (%s)\n", 
		   si->device->string,
		   snd_strerror(err));
	return 0;
    }
  
    if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0) {
	si->Com_Printf("ALSA snd error, cannot allocate hw params (%s)\n",
		   snd_strerror(err));
	return 0;
    }
  
    if ((err = snd_pcm_hw_params_any(pcm_handle, hw_params)) < 0) {
	si->Com_Printf("ALSA snd error, cannot init hw params (%s)\n",
		   snd_strerror(err));
	snd_pcm_hw_params_free(hw_params);
	return 0;
    }
  
    if ((err = snd_pcm_hw_params_set_access(pcm_handle, hw_params, 
					    SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
	si->Com_Printf("ALSA snd error, cannot set access (%s)\n",
		   snd_strerror(err));
	snd_pcm_hw_params_free(hw_params);
	return 0;
    }
  
    si->dma->samplebits = si->bits->value;
    if (si->dma->samplebits == 16 || si->dma->samplebits != 8) {
	if ((err = snd_pcm_hw_params_set_format(pcm_handle, hw_params,
						SND_PCM_FORMAT_S16_LE)) < 0) {
	    si->Com_Printf("ALSA snd error, 16 bit sound not supported, trying 8\n");
	    si->dma->samplebits = 8;
	}
    }
    if (si->dma->samplebits == 8) {
	if ((err = snd_pcm_hw_params_set_format(pcm_handle, hw_params,
						SND_PCM_FORMAT_U8)) < 0) {
	    si->Com_Printf("ALSA snd error, cannot set sample format (%s)\n",
		       snd_strerror(err));
	    snd_pcm_hw_params_free(hw_params);
	    return 0;
	}
    }

    si->dma->speed = (int)si->speed->value;
    if (!si->dma->speed) {
	for (i = 0; i < sizeof(tryrates); i++) {
	    int dir = 0;
	    int test = tryrates[i];

	    if ((err = snd_pcm_hw_params_set_rate_near(pcm_handle, hw_params,
						       &test, &dir)) < 0) {
		si->Com_Printf("ALSA snd error, cannot set sample rate %d (%s)\n",
			       tryrates[i], snd_strerror(err));
	    } else {
		si->dma->speed = test;
		if (dir != 0) {
		    si->Com_Printf("alsa: The rate %d Hz is not supported by your hardware, using %d Hz instead.\n", test, err);
		}
		break;
	    }
	}
    }
    if (!si->dma->speed) {
	si->Com_Printf("ALSA snd error couldn't set rate.\n");
	snd_pcm_hw_params_free(hw_params);
	return 0;
    }

    si->dma->channels = (int)si->channels->value;
    if (si->dma->channels < 1 || si->dma->channels > 2)
	si->dma->channels = 2;
    if ((err = snd_pcm_hw_params_set_channels(pcm_handle, hw_params, si->dma->channels)) < 0) {
	si->Com_Printf("ALSA snd error couldn't set channels %d (%s).\n",
		   si->dma->channels, snd_strerror(err));
	snd_pcm_hw_params_free(hw_params);
	return 0;
    }

    if ((err = snd_pcm_hw_params(pcm_handle, hw_params)) < 0) {
	si->Com_Printf("ALSA snd error couldn't set params (%s).\n",snd_strerror(err));
	snd_pcm_hw_params_free(hw_params);
	return 0;
    }
  
    si->dma->buffer = malloc(BUFFER_SIZE);
    memset(si->dma->buffer, 0, BUFFER_SIZE);

    si->dma->samplepos = 0;
    si->dma->samples = BUFFER_SIZE / (si->dma->samplebits / 8);
    si->dma->submission_chunk = 1;

    si->Com_Printf("alsa: buffer size is %d, %d samples\n", BUFFER_SIZE, si->dma->samples);

    snd_inited = 1;
    return 1;
}

int SNDDMA_GetDMAPos(void) {
    if (snd_inited)
	return si->dma->samplepos;
    else
	si->Com_Printf("Sound not inizialized\n");
    return 0;
}

void SNDDMA_Shutdown(void) {
    if (snd_inited) {
	snd_pcm_drop(pcm_handle);
	snd_pcm_close(pcm_handle);
	snd_inited = 0;
    }
    free(si->dma->buffer);
    si->dma->buffer = NULL;
}

/* SNDDMA_Submit
 * Send sound to device if buffer isn't really the dma buffer
 */
void SNDDMA_Submit(void) {
    int written;
  
    if(!snd_inited)
	return;
  
    if ((written = snd_pcm_writei(pcm_handle, si->dma->buffer, si->dma->samples * (si->dma->samplebits / 8))) < 0) {
	snd_pcm_prepare(pcm_handle);
	si->Com_Printf("alsa: buffer underrun\n");
    }
    si->dma->samplepos += written / (si->dma->samplebits / 8);
}


void SNDDMA_BeginPainting(void) {    
}
