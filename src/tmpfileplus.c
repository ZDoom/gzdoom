/* $Id: tmpfileplus.c $ */
/*
 * $Date: 2016-06-01 03:31Z $
 * $Revision: 2.0.0 $
 * $Author: dai $
 */

/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2012-16 David Ireland, DI Management Services Pty Ltd
 * <http://www.di-mgt.com.au/contact/>.
 */


/*
* NAME
*        tmpfileplus - create a unique temporary file
* 
* SYNOPSIS
*        FILE *tmpfileplus(const char *dir, const char *prefix, char **pathname, int keep)
*
* DESCRIPTION
*        The tmpfileplus() function opens a unique temporary file in binary
*        read/write (w+b) mode. The file is opened with the O_EXCL flag,
*        guaranteeing that the caller is the only user. The filename will consist
*        of the string given by `prefix` followed by 10 random characters. If
*        `prefix` is NULL, then the string "tmp." will be used instead. The file
*        will be created in an appropriate directory chosen by the first
*        successful attempt in the following sequence:
*        
*        a) The directory given by the `dir` argument (so the caller can specify
*        a secure directory to take precedence).
*        
*        b) The directory name in the environment variables:
*        
*          (i)   "TMP" [Windows only]   
*          (ii)  "TEMP" [Windows only]   
*          (iii) "TMPDIR" [Unix only]
*        
*        c) `P_tmpdir` as defined in <stdio.h> [Unix only] (in Windows, this is
*        usually "\", which is no good).
*        
*        d) The current working directory.
*        
*        If a file cannot be created in any of the above directories, then the
*        function fails and NULL is returned. 
*        
*        If the argument `pathname` is not a null pointer, then it will point to
*        the full pathname of the file. The pathname is allocated using `malloc`
*        and therefore should be freed by `free`.
*        
*        If `keep` is nonzero and `pathname` is not a null pointer, then the file
*        will be kept after it is closed. Otherwise the file will be
*        automatically deleted when it is closed or the program terminates. 
*
*
* RETURN VALUE
*        The tmpfileplus() function returns a pointer to the open file stream, 
*        or NULL if a unique file cannot be opened.
*
*
* ERRORS
*        ENOMEM Not enough memory to allocate filename.
*
*/

/* ADDED IN v2.0 */

/*
* NAME
*        tmpfileplus_f - create a unique temporary file with filename stored in a fixed-length buffer
*
* SYNOPSIS
*        FILE *tmpfileplus_f(const char *dir, const char *prefix, char *pathnamebuf, size_t pathsize, int keep);
*
* DESCRIPTION
*        Same as tmpfileplus() except receives filename in a fixed-length buffer. No allocated memory to free.

* ERRORS
*        E2BIG Resulting filename is too big for the buffer `pathnamebuf`.

*/

#include "tmpfileplus.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

/* Non-ANSI include files that seem to work in both MSVC and Linux */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef _WIN32
#include <io.h>
#endif

#ifdef _WIN32
/* MSVC nags to enforce ISO C++ conformant function names with leading "_",
 * so we define our own function names to avoid whingeing compilers...
 */
#define OPEN_ _open
#define FDOPEN_ _fdopen
#else
#define OPEN_ open
#define FDOPEN_ fdopen
#endif


/* DEBUGGING STUFF */
#if defined(_DEBUG) && defined(SHOW_DPRINTF)
#define DPRINTF1(s, a1) printf(s, a1)
#else
#define DPRINTF1(s, a1)
#endif


#ifdef _WIN32
#define FILE_SEPARATOR "\\"
#else
#define FILE_SEPARATOR "/"
#endif

#define RANDCHARS	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
#define NRANDCHARS	(sizeof(RANDCHARS) - 1)

/** Replace each byte in string s with a random character from TEMPCHARS */
static char *set_randpart(char *s)
{
	size_t i;
	unsigned int r;
	static unsigned int seed;	/* NB static */

	if (seed == 0)
	{	/* First time set our seed using current time and clock */
		seed = ((unsigned)time(NULL)<<8) ^ (unsigned)clock();
	}
	srand(seed++);	
	for (i = 0; i < strlen(s); i++)
	{
		r = rand() % NRANDCHARS;
		s[i] = (RANDCHARS)[r];
	}
	return s;
}

/** Return 1 if path is a valid directory otherwise 0 */
static int is_valid_dir(const char *path)
{
	struct stat st;
	if ((stat(path, &st) == 0) && (st.st_mode & S_IFDIR))
		return 1;
	
	return 0;
}

/** Call getenv and save a copy in buf */
static char *getenv_save(const char *varname, char *buf, size_t bufsize)
{
	char *ptr = getenv(varname);
	buf[0] = '\0';
	if (ptr)
	{
		strncpy(buf, ptr, bufsize);
		buf[bufsize-1] = '\0';
		return buf;
	}
	return NULL;
}

/** 
 * Try and create a randomly-named file in directory `tmpdir`. 
 * If successful, allocate memory and set `tmpname_ptr` to full filepath, and return file pointer;
 * otherwise return NULL.
 * If `keep` is zero then create the file as temporary and it should not exist once closed.
 */
static FILE *mktempfile_internal(const char *tmpdir, const char *pfx, char **tmpname_ptr, int keep)
/* PRE: 
 * pfx is not NULL and points to a valid null-terminated string
 * tmpname_ptr is not NULL.
 */
{
	FILE *fp;
	int fd;
	char randpart[] = "1234567890";
	size_t lentempname;
	int i;
	char *tmpname = NULL;
	int oflag, pmode;

/* In Windows, we use the _O_TEMPORARY flag with `open` to ensure the file is deleted when closed.
 * In Unix, we use the unlink function after opening the file. (This does not work in Windows,
 * which does not allow an open file to be unlinked.)
 */
#ifdef _WIN32
	/* MSVC flags */
	oflag =  _O_BINARY|_O_CREAT|_O_RDWR;
	if (!keep)
		oflag |= _O_TEMPORARY;
	pmode = _S_IREAD | _S_IWRITE;
#else 
	/* Standard POSIX flags */
	oflag = O_CREAT|O_RDWR;
	pmode = S_IRUSR|S_IWUSR;
#endif

	if (!tmpdir || !is_valid_dir(tmpdir)) {
		errno = ENOENT;
		return NULL;
	}

	lentempname = strlen(tmpdir) + strlen(FILE_SEPARATOR) + strlen(pfx) + strlen(randpart);
	DPRINTF1("lentempname=%d\n", lentempname);
	tmpname = malloc(lentempname + 1);
	if (!tmpname)
	{
		errno = ENOMEM;
		return NULL;
	}
	/* If we don't manage to create a file after 10 goes, there is something wrong... */
	for (i = 0; i < 10; i++)
	{
		sprintf(tmpname, "%s%s%s%s", tmpdir, FILE_SEPARATOR, pfx, set_randpart(randpart));
		DPRINTF1("[%s]\n", tmpname);
		fd = OPEN_(tmpname, oflag, pmode);
		if (fd != -1) break;
	}
	DPRINTF1("strlen(tmpname)=%d\n", strlen(tmpname));
	if (fd != -1)
	{	/* Success, so return user a proper ANSI C file pointer */
		fp = FDOPEN_(fd, "w+b");
		errno = 0;

#ifndef _WIN32
		/* [Unix only] And make sure the file will be deleted once closed */
		if (!keep) remove(tmpname);
#endif

	}
	else
	{	/* We failed */
		fp = NULL;
	}
	if (!fp)
	{
		free(tmpname);
		tmpname = NULL;
	}

	*tmpname_ptr = tmpname;
	return fp;
}

/**********************/
/* EXPORTED FUNCTIONS */
/**********************/

FILE *tmpfileplus(const char *dir, const char *prefix, char **pathname, int keep)
{
	FILE *fp = NULL;
	char *tmpname = NULL;
	char *tmpdir = NULL;
	const char *pfx = (prefix ? prefix : "tmp.");
	char *tempdirs[12] = { 0 };
	char env1[FILENAME_MAX+1] = { 0 };
	char env2[FILENAME_MAX+1] = { 0 };
	char env3[FILENAME_MAX+1] = { 0 };
	int ntempdirs = 0;
	int i;

	/* Set up a list of temp directories we will try in order */
	i = 0;
	tempdirs[i++] = (char *)dir;
#ifdef _WIN32
	tempdirs[i++] = getenv_save("TMP", env1, sizeof(env1));
	tempdirs[i++] = getenv_save("TEMP", env2, sizeof(env2));
#else
	tempdirs[i++] = getenv_save("TMPDIR", env3, sizeof(env3));
	tempdirs[i++] = P_tmpdir;
#endif
	tempdirs[i++] = ".";
	ntempdirs = i;

	errno = 0;

	/* Work through list we set up before, and break once we are successful */
	for (i = 0; i < ntempdirs; i++)
	{
		tmpdir = tempdirs[i];
		DPRINTF1("Trying tmpdir=[%s]\n", tmpdir);
		fp = mktempfile_internal(tmpdir, pfx, &tmpname, keep);
		if (fp) break;
	}
	/* If we succeeded and the user passed a pointer, set it to the alloc'd pathname: the user must free this */
	if (fp && pathname)
		*pathname = tmpname;
	else	/* Otherwise, free the alloc'd memory */
		free(tmpname);

	return fp;
}

/* Same as tmpfileplus() but with fixed length buffer for output filename and no memory allocation */
FILE *tmpfileplus_f(const char *dir, const char *prefix, char *pathnamebuf, size_t pathsize, int keep)
{
	char *tmpbuf = NULL;
	FILE *fp;

	/* If no buffer provided, do the normal way */
	if (!pathnamebuf || (int)pathsize <= 0) {
		return tmpfileplus(dir, prefix, NULL, keep);
	}
	/* Call with a temporary buffer */
	fp = tmpfileplus(dir, prefix, &tmpbuf, keep);
	if (fp && strlen(tmpbuf) > pathsize - 1) {
		/* Succeeded but not enough room in output buffer, so clean up and return an error */
		pathnamebuf[0] = 0;
		fclose(fp);
		if (keep) remove(tmpbuf);
		free(tmpbuf);
		errno = E2BIG;
		return NULL;
	}
	/* Copy name into buffer */
	strcpy(pathnamebuf, tmpbuf);
	free(tmpbuf);

	return fp;
}


