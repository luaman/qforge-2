/* $Id$
 *
 * used to be sys_linux.c
 * 
 * Copyright (C) 1997-2001 Id Software, Inc.
 * Copyright (c) 2002 The Quakeforge Project.
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

#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <errno.h>

#ifdef HAVE_DLOPEN
# include <dlfcn.h>
#endif

/* This should probably go into configure
 * BSDs and Linux have SIGIO, Solaris needs SIGPOLL
 * This should probably be changed the other way round
 * SIGIO as default and SIGPOLL only for Solrais */
#ifndef SIGPOLL
#define SIGPOLL SIGIO
#endif

#include "qcommon.h"
#include "game.h"
#include "rw.h"

#ifdef SOL8_XIL_WORKAROUND
#include <xil/xil.h>
typedef struct {
    qboolean restart_sound;
    int s_rate;
    int s_width;
    int s_channels;

    int width;
    int height;
    byte * pic;
    byte * pic_pending;

    /* order 1 huffman stuff */
    int * hnodes1; /* [256][256][2] */
    int numhnodes1[256];
    int h_used[512];
    int h_count[512];
} cinematics_t;
#endif
    

cvar_t *nostdout;

unsigned	sys_frame_time;

uid_t saved_euid;
qboolean stdin_active = true;
char display_name[1024];
/* FIXME: replace with configure test for hrtime_t */
#ifdef __sun__
hrtime_t base_hrtime;
#endif

// =======================================================================
// General routines
// =======================================================================

void Sys_ConsoleOutput (char *string)
{
	if (nostdout && nostdout->value)
		return;

	if (!string)
	    return;

	if (string[0] == 1 || string[0] == 2) {
	    /* coloured text on the graphical console, ignore it */
	    string++;
	}

	fputs(string, stdout);
}

void Sys_Printf (char *fmt, ...) {
	va_list argptr;
	char text[1024];
	unsigned char * p;

	va_start (argptr,fmt);
	vsnprintf (text,1024,fmt,argptr);
	va_end (argptr);

	/* relnev 0.9 deleted -- jaq */
	/* if (strlen(text) > sizeof(text))
		Sys_Error("memory overwrite in Sys_Printf"); */

    if (nostdout && nostdout->value)
        return;

	for (p = (unsigned char *)text; *p; p++) {
		*p &= 0x7f;
		if ((*p > 128 || *p < 32) && *p != 10 && *p != 13 && *p != 9)
			printf("[%02x]", *p);
		else
			putc(*p, stdout);
	}
}

void Sys_Quit (void) {
    CL_Shutdown();
    Qcommon_Shutdown();

    fcntl(0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);
    _exit(0);
}

void Sys_Init(void)
{
/*#ifdef USE_ASM
//	Sys_SetFPCW();
#endif
*/
}

void Sys_Error (char *error, ...)
{ 
    va_list     argptr;
    char        string[1024];

    /* change stdin to non blocking */
    fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);

	CL_Shutdown ();
	Qcommon_Shutdown ();
    
    va_start (argptr,error);
    vsnprintf (string,1024,error,argptr);
    va_end (argptr);
	fprintf(stderr, "Error: %s\n", string);

	_exit (1);
} 

void Sys_Warn (char *warning, ...)
{ 
    va_list     argptr;
    char        string[1024];
    
    va_start (argptr,warning);
    vsnprintf (string,1024,warning,argptr);
    va_end (argptr);
	fprintf(stderr, "Warning: %s", string);
} 

/*
============
Sys_FileTime

returns -1 if not present
============
*/
int	Sys_FileTime (char *path)
{
	struct	stat	buf;
	
	if (stat (path,&buf) == -1)
		return -1;
	
	return buf.st_mtime;
}

void floating_point_exception_handler(int whatever)
{
//	Sys_Warn("floating point exception\n");
	signal(SIGFPE, floating_point_exception_handler);
}

char *Sys_ConsoleInput(void)
{
    static char text[256];
    int     len;
	fd_set	fdset;
    struct timeval timeout;

	if (!dedicated || !dedicated->value)
		return NULL;

	if (!stdin_active)
		return NULL;

	FD_ZERO(&fdset);
	FD_SET(0, &fdset); // stdin
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	if (select (1, &fdset, NULL, NULL, &timeout) == -1 || !FD_ISSET(0, &fdset))
		return NULL;

	len = read (0, text, sizeof(text));
	if (len == 0) { // eof!
		stdin_active = false;
		return NULL;
	}

	if (len < 1)
		return NULL;
	text[len-1] = 0;    // rip off the /n and terminate

	return text;
}

/*****************************************************************************/

static void * game_library = NULL;
typedef game_export_t * gameapi_t(game_import_t *);

void Sys_UnloadGame(void) {
    if (game_library) 
	dlclose(game_library);
    game_library = NULL;
}

/* Loads the game dll */
void *Sys_GetGameAPI (void *parms) {
    gameapi_t * GetGameAPI;
    FILE * fp;
    char name[MAX_OSPATH];
    char * path;
    
    setreuid(getuid(), getuid());
    setegid(getgid());
    
    if (game_library)
	Com_Error (ERR_FATAL, "Sys_GetGameAPI without Sys_UnloadGame");

    Com_Printf("------- Loading game.so -------\n");

    /* now run through the search paths */
    path = NULL;
    while (1) {
	path = FS_NextPath(path);
	if (!path)
	    return NULL;
	snprintf(name, MAX_OSPATH, "%s/game.so", path);

	/* skip it if it doesn't exist */
	fp = fopen(name, "rb");
	if (fp == NULL)
	    continue;
	fclose(fp);

	game_library = dlopen(name, RTLD_NOW);
	if (game_library) {
	    Com_MDPrintf("LoadLibrary (%s)\n", name);
	    break;
	} else {
	    Com_MDPrintf("LoadLibrary (%s)\n", name);
	    Com_MDPrintf("%s\n", dlerror());
	}
    }
    
    GetGameAPI = (gameapi_t *) dlsym(game_library, "GetGameAPI");
    
    if (!GetGameAPI) {
	Sys_UnloadGame ();		
	return NULL;
    }
    
    return GetGameAPI(parms);
}

/*****************************************************************************/

void Sys_AppActivate (void)
{
}

void Sys_SendKeyEvents (void)
{
#ifndef DEDICATED_ONLY
	if (KBD_Update_fp)
		KBD_Update_fp();
#endif

	// grab frame time 
	sys_frame_time = Sys_Milliseconds();
}

/*****************************************************************************/
#ifdef SOL8_XIL_WORKAROUND
XilSystemState xil_state;
#endif

int main (int argc, char **argv) {
    int time, oldtime, newtime;

#ifdef SOL8_XIL_WORKAROUND
    {
	extern cinematics_t cin;

	if ((xil_state = xil_open()) == NULL) {
	    fprintf(stderr, "Can't open XIL\n");
	    exit(1);
	}
	memset(&cin, 0, sizeof(cin));
    }
#endif

    /* save the contents of the DISPLAY environment variable.
     * if we don't, it gets overwritten after reloading
     * the renderer libraries (solaris) */
    {
	char * tmp_name = getenv("DISPLAY");
	if (tmp_name == NULL) {
	    display_name[0] = '\0';
	} else {
	    strncpy(display_name, tmp_name, sizeof(display_name));
	}
    }

    /* go back to real user for config loads */
    saved_euid = geteuid();
    seteuid(getuid());

    printf("QuakeIIForge %s\n", VERSION);

    Qcommon_Init(argc, argv);

    /* sys_irix.c had this and the fcntl line 3 lines down commented out */
    fcntl(0, F_SETFL, fcntl (0, F_GETFL, 0) | FNDELAY);

    nostdout = Cvar_Get("nostdout", "0", 0);
    if (!nostdout->value)
	fcntl(0, F_SETFL, fcntl (0, F_GETFL, 0) | FNDELAY);

    /* main loop */
    oldtime = Sys_Milliseconds ();
    while (1) {
	/* find time spent rendering last frame */
	do {
	    newtime = Sys_Milliseconds();
	    time = newtime - oldtime;
	} while (time < 1);
        Qcommon_Frame(time);
	oldtime = newtime;
    }
}

size_t verify_fread(void * ptr, size_t size, size_t nitems, FILE * fp) {
    size_t ret;
    int err;

    clearerr(fp);
    ret = fread(ptr, size, nitems, fp);
    err = errno;
    if (ret != nitems) {
	printf("verify_fread(...,%zu,%zu,...): return value: %zu\n", size, nitems, ret);
	if (ret == 0 && ferror(fp)) {
	    printf("   error: %s\n", strerror(err));
	    printf("   fileno=%d\n", fileno(fp));
	}
	/* sleep(5); */
    }
    return ret;
}

size_t verify_fwrite(void * ptr, size_t size, size_t nitems, FILE * fp) {
    size_t ret;
    int err;

    clearerr(fp);
    ret = fwrite(ptr, size, nitems, fp);
    err = errno;
    if (ret != nitems) {
	printf("verify_fwrite(...,%zu,%zu,...) = %zu\n", size, nitems, ret);
	if (ret == 0 && ferror(fp)) {
	    printf("   error: %s\n", strerror(err));
	    printf("   fileno=%d\n", fileno(fp));
	}
    }
    /* sleep(5); */
    return ret;
}
