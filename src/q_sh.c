/* $Id$
 * 
 * used to be q_shlinux.c
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

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/time.h>

#include "glob.h"

#include "qcommon.h"

#if defined(__FreeBSD__)
#include <machine/param.h>
#endif

//===============================================================================

byte *membase;
int maxhunksize;
int curhunksize;

void *Hunk_Begin (int maxsize)
{
	// reserve a huge chunk of memory, but don't commit any yet
	maxhunksize = maxsize + sizeof(int);
	curhunksize = 0;
/* merged in from q_sh*.c -- jaq */
#if defined(__linux__)
	membase = mmap(0, maxhunksize, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
#elif defined(__bsd__)
	membase = mmap(0, maxhunksize, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0);
#elif defined(sun) || defined(__sgi)
	membase = malloc(maxhunksize);
#endif
	if (membase == NULL || membase == (byte *)-1)
		Sys_Error("unable to virtual allocate %d bytes", maxsize);

	*((int *)membase) = curhunksize;

	return membase + sizeof(int);
}

void *Hunk_Alloc (int size)
{
	byte *buf;

	// round to cacheline
	size = (size+31)&~31;
	if (curhunksize + size > maxhunksize)
		Sys_Error("Hunk_Alloc overflow");
	buf = membase + sizeof(int) + curhunksize;
	curhunksize += size;
	return buf;
}

#if defined(__linux__) || defined(sun)
int Hunk_End (void)
{
	byte *n;

/* from relnev 0.9 -- jaq */
#ifdef __FreeBSD__
	size_t old_size = maxhunksize;
	size_t new_size = curhunksize + sizeof(int);
	void * unmap_base;
	size_t unmap_len;

	new_size = round_page(new_size);
	old_size = round_page(old_size);
	if (new_size > old_size)
		n = 0; /* error */
	else if (new_size < old_size) {
		unmap_base = (caddr_t) (membase + new_size);
		unmap_len = old_size - new_size;
		n = munmap(unmap_base, unmap_len) + membase;
	}
#endif

#ifdef __linux__
	n = mremap(membase, maxhunksize, curhunksize + sizeof(int), 0);
#else /* sun */
	n = realloc(membase, curhunksize);
#endif
	if (n != membase)
		Sys_Error("Hunk_End:  Could not remap virtual block (%d)", errno);
	*((int *)membase) = curhunksize + sizeof(int);
	
	return curhunksize;
}
#else /* bsd and irix */
int Hunk_End (void)
{
#ifndef __sgi
	long pgsz, newsz, modsz;

	pgsz = sysconf(_SC_PAGESIZE);
	if (pgsz == -1)
		Sys_Error("Hunk_End: Sysconf() failed: %s", strerror(errno));

	newsz = curhunksize + sizeof(int);

	if (newsz > maxhunksize)
		Sys_Error("Hunk_End Overflow");
	else if (newsz < maxhunksize) {
		modsz = newsz % pgsz;
		if (modsz) newsz += pgsz - modsz;

		if (munmap(membase + newsz, maxhunksize - newsz) == -1)
			Sys_Error("Hunk_End: munmap() failed: %s", strerror(errno));
	}

	*((int *)membase) = curhunksize + sizeof(int);
#endif /* __sgi */
	return curhunksize;
}
#endif /* bsd or __sgi */

void Hunk_Free (void *base)
{
	byte *m;

	if (base) {
/* merged in from q_shsolaris.c -- jaq */
#ifdef sun
		/* FIXME: I'm not sure that 'sun' is the correct define -- jaq */
		free(base);
#else
		m = ((byte *)base) - sizeof(int);
	#ifdef __sgi
		free(m);
	#else
		if (munmap(m, *((int *)m)))
			Sys_Error("Hunk_Free: munmap failed (%d:%s)", errno, strerror(errno));
	#endif /* __sgi */
#endif /* sun */
	}
}

//===============================================================================


/*
================
Sys_Milliseconds
================
*/
int curtime;
int Sys_Milliseconds (void)
{
	struct timeval tp;
	struct timezone tzp;
	static int		secbase;

	gettimeofday(&tp, &tzp);
	
	if (!secbase)
	{
		secbase = tp.tv_sec;
		return tp.tv_usec/1000;
	}

	curtime = (tp.tv_sec - secbase)*1000 + tp.tv_usec/1000;
	
	return curtime;
}

void Sys_Mkdir (char *path)
{
    mkdir (path, 0777);
}

char *strlwr (char *s)
{
	char *p = s;
	while (*s) {
		*s = tolower(*s);
		s++;
	}
	return p;
}

//============================================

static	char	findbase[MAX_OSPATH];
static	char	findpath[MAX_OSPATH];
static	char	findpattern[MAX_OSPATH];
static	DIR		*fdir;

static qboolean CompareAttributes(char *path, char *name,
	unsigned musthave, unsigned canthave )
{
	struct stat st;
	char fn[MAX_OSPATH];

// . and .. never match
	if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
		return false;

/* FIXME: what's the point of the return? -- jaq */
#if 1
	sprintf(fn, "%s/%s", path, name);
#else
	return true;
#endif

	if (stat(fn, &st) == -1)
		return false; // shouldn't happen

	if ( ( st.st_mode & S_IFDIR ) && ( canthave & SFF_SUBDIR ) )
		return false;

	if ( ( musthave & SFF_SUBDIR ) && !( st.st_mode & S_IFDIR ) )
		return false;

	return true;
}

char *Sys_FindFirst (char *path, unsigned musthave, unsigned canhave)
{
	struct dirent *d;
	char *p;

	if (fdir)
		Sys_Error ("Sys_BeginFind without close");

//	COM_FilePath (path, findbase);
	strcpy(findbase, path);

	if ((p = strrchr(findbase, '/')) != NULL) {
		*p = 0;
		strcpy(findpattern, p + 1);
	} else
		strcpy(findpattern, "*");

	if (strcmp(findpattern, "*.*") == 0)
		strcpy(findpattern, "*");
	
	if ((fdir = opendir(findbase)) == NULL)
		return NULL;
	while ((d = readdir(fdir)) != NULL) {
		if (!*findpattern || glob_match(findpattern, d->d_name)) {
//			if (*findpattern)
//				printf("%s matched %s\n", findpattern, d->d_name);
			if (CompareAttributes(findbase, d->d_name, musthave, canhave)) {
				sprintf (findpath, "%s/%s", findbase, d->d_name);
				return findpath;
			}
		}
	}
	return NULL;
}

char *Sys_FindNext (unsigned musthave, unsigned canhave)
{
	struct dirent *d;

	if (fdir == NULL)
		return NULL;
	while ((d = readdir(fdir)) != NULL) {
		if (!*findpattern || glob_match(findpattern, d->d_name)) {
//			if (*findpattern)
//				printf("%s matched %s\n", findpattern, d->d_name);
			if (CompareAttributes(findbase, d->d_name, musthave, canhave)) {
				sprintf (findpath, "%s/%s", findbase, d->d_name);
				return findpath;
			}
		}
	}
	return NULL;
}

void Sys_FindClose (void)
{
	if (fdir != NULL)
		closedir(fdir);
	fdir = NULL;
}


//============================================

