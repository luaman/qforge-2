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

/* merged from sys_*.c -- jaq */
#if defined(__linux__) || defined(__sgi)
	#include <mntent.h>
#elif defined(__FreeBSD__) || defined(__bsd__)
	#include <fstab.h>
#elif defined(sun)
	#include <sys/file.h>
#endif

/* libtool dynamic loader */
#include <ltdl.h>

/* merged from sys_bsd.c -- jaq */
#ifndef RTLD_NOW
#define RTLD_NOW RTLD_LAZY
#endif

/* merged from sys_bsd.c -- jaq */
#ifdef __OpenBSD__
#define dlsym(X, Y) dlsym(X, "_"##Y)
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "qcommon.h"
#include "game.h"
#include "rw.h"

cvar_t *nostdout;

unsigned	sys_frame_time;

uid_t saved_euid;
qboolean stdin_active = true;

// =======================================================================
// General routines
// =======================================================================

void Sys_ConsoleOutput (char *string)
{
	if (nostdout && nostdout->value)
		return;

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
    lt_dlexit();
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

// change stdin to non blocking
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

static lt_dlhandle game_library = NULL;
typedef game_export_t * gameapi_t(game_import_t *);

void Sys_UnloadGame(void) {
    if (game_library) 
	lt_dlclose(game_library);
    game_library = NULL;
}

/* Loads the game dll */
void *Sys_GetGameAPI (void *parms) {
    gameapi_t * GetGameAPI;
    cvar_t * gamename;
    char path[MAX_OSPATH];
    char * str_p;
    
    setreuid(getuid(), getuid());
    setegid(getgid());
    
    if (game_library)
	Com_Error (ERR_FATAL, "Sys_GetGameAPI without Sys_UnloadingGame");

    gamename = Cvar_Get("gamedir", BASEDIRNAME, CVAR_LATCH|CVAR_SERVERINFO);

    Com_Printf("------- Loading %s -------\n", gamename->string);
    
    /* set the module search path */
    snprintf(path, MAX_OSPATH, ".:"PKGLIBDIR"/%s", gamename->string);
    lt_dlsetsearchpath(path);
        
    /* load the module */
    game_library = lt_dlopenext("game.so");
    
    if (game_library) {
	Com_MDPrintf("LoadLibrary (%s)\n", gamename->string);
    } else {
	Com_MDPrintf("LoadLibrary (%s)\n", gamename->string);
	//str_p = strchr(lt_dlerror(), ':'); // skip the path (already shown)
	str_p = (char *) lt_dlerror();
	if (str_p != NULL) {
	    Com_MDPrintf (" **");
	    while (*str_p)
		Com_MDPrintf ("%c", *(++str_p));
	    Com_MDPrintf ("\n");
	}
    }
    
    GetGameAPI = (gameapi_t *) lt_dlsym(game_library, "GetGameAPI");
    
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

int main (int argc, char **argv) {
    int time, oldtime, newtime;

    /* go back to real user for config loads */
    saved_euid = geteuid();
    seteuid(getuid());

    printf("QuakeIIForge %s\n", VERSION);

    /* initialiase libltdl */
    lt_dlinit();
    
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

#if 0
void Sys_CopyProtect(void)
{
/* merged in from sys_bsd.c -- jaq */
#ifdef __linux__
	FILE *mnt;
	struct mntent *ent;
#else /* __bsd__ */
	struct fstab * ent;
#endif
	char path[MAX_OSPATH];
	struct stat st;
	qboolean found_cd = false;

	static qboolean checked = false;

	if (checked)
		return;

	/* sys_irix.c -- jaq
	Com_Printf("XXX - Sys_CopyProtect disabled\n");
	checked = true;
	return;
	*/

/* merged in from sys_bsd.c */
#ifdef __linux__
	if ((mnt = setmntent("/etc/mtab", "r")) == NULL)
		Com_Error(ERR_FATAL, "Can't read mount table to determine mounted cd location.");

	while ((ent = getmntent(mnt)) != NULL) {
		if (strcmp(ent->mnt_type, "iso9660") == 0) {
#else /* __bsd__ */
	while ((ent = getfsent()) != NULL) {
		if (strcmp(ent->fs_vfstype, "cd9660") == 0) {
#endif
			// found a cd file system
			found_cd = true;
/* merged in from sys_bsd.c */
#ifdef __linux__
			sprintf(path, "%s/%s", ent->mnt_dir, "install/data/quake2.exe");
#else /* __bsd__ */
			sprintf(path, "%s/%s", ent->fs_file, "install/data/quake2.exe");
#endif
			if (stat(path, &st) == 0) {
				// found it
				checked = true;
/* merged in from sys_bsd.c */
#ifdef __linux__
				endmntent(mnt);
#else /* __bsd__ */
				endfsent();
#endif
				return;
			}
/* merged in from sys_bsd.c */
#ifdef __linux__
			sprintf(path, "%s/%s", ent->mnt_dir, "Install/Data/quake2.exe");
#else /* __bsd__ */
			sprintf(path, "%s/%s", ent->fs_file, "Install/Data/quake2.exe");
#endif
			
			if (stat(path, &st) == 0) {
				// found it
				checked = true;
/* merged in from sys_bsd.c */
#ifdef __linux__
				endmntent(mnt);
#else /* __bsd__ */
				endfsent();
#endif
				return;
			}
/* merged in from sys_bsd.c */
#ifdef __linux__
			sprintf(path, "%s/%s", ent->mnt_dir, "quake2.exe");
#else /* __bsd__ */
			sprintf(path, "%s/%s", ent->fs_file, "quake2.exe");
#endif
			if (stat(path, &st) == 0) {
				// found it
				checked = true;
/* merged in from sys_bsd.c */
#ifdef __linux__
				endmntent(mnt);
#else /* __bsd__ */
				endfsent();
#endif
				return;
			}
		}
	}
/* merged in from sys_bsd.c */
#ifdef __linux__
	endmntent(mnt);
#else /* __bsd__ */
	endfsent();
#endif

	if (found_cd)
		Com_Error (ERR_FATAL, "Could not find a Quake2 CD in your CD drive.");
	Com_Error (ERR_FATAL, "Unable to find a mounted iso9660 file system.\n"
		"You must mount the Quake2 CD in a cdrom drive in order to play.");
}
#endif

#if 0
/*
================
Sys_MakeCodeWriteable
================
*/
void Sys_MakeCodeWriteable (unsigned long startaddr, unsigned long length)
{

	int r;
	unsigned long addr;
	int psize = getpagesize();

	addr = (startaddr & ~(psize-1)) - psize;

//	fprintf(stderr, "writable code %lx(%lx)-%lx, length=%lx\n", startaddr,
//			addr, startaddr+length, length);

	r = mprotect((char*)addr, length + startaddr - addr + psize, 7);

	if (r < 0)
    		Sys_Error("Protection change failed\n");

}

#endif
