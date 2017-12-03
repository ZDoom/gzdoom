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

#include "m_random.h"
#include "cmdlib.h"
#include "zstring.h"


#define RANDCHARS	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
#define NRANDCHARS	(sizeof(RANDCHARS) - 1)

static FRandom pr_tmpfile;

/** Replace each byte in string s with a random character from TEMPCHARS */
static char *set_randpart(char *s)
{
	size_t i;
	unsigned int r;


	for (i = 0; i < strlen(s); i++)
	{
		r = pr_tmpfile() % NRANDCHARS;
		s[i] = (RANDCHARS)[r];
	}
	return s;
}

/** 
 * Try and create a randomly-named file in directory `tmpdir`. 
 * If successful, allocate memory and set `tmpname_ptr` to full filepath, and return file pointer;
 * otherwise return NULL.
 * If `keep` is zero then create the file as temporary and it should not exist once closed.
 */
static FILE *mktempfile_internal(const char *tmpdir, const char *pfx, FString *tmpname_ptr, int keep)
/* PRE: 
 * pfx is not NULL and points to a valid null-terminated string
 * tmpname_ptr is not NULL.
 */
{
	FILE *fp;
	int fd;
	char randpart[] = "1234567890";
	int i;
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

	if (!tmpdir || !DirExists(tmpdir)) {
		errno = ENOENT;
		return NULL;
	}

	FString tempname;

	/* If we don't manage to create a file after 10 goes, there is something wrong... */
	for (i = 0; i < 10; i++)
	{
		tempname.Format("%s/%s%s", tmpdir, pfx, set_randpart(randpart));
		fd = OPEN_(tempname, oflag, pmode);
		if (fd != -1) break;
	}
	if (fd != -1)
	{	/* Success, so return user a proper ANSI C file pointer */
		fp = FDOPEN_(fd, "w+b");
		errno = 0;
	}
	else
	{	/* We failed */
		fp = NULL;
	}

	if (tmpname_ptr) *tmpname_ptr = tempname;
	return fp;
}

/**********************/
/* EXPORTED FUNCTIONS */
/**********************/

FILE *tmpfileplus(const char *dir, const char *prefix, FString *pathname, int keep)
{
	FILE *fp = mktempfile_internal(dir, (prefix ? prefix : "tmp."), pathname, keep);
	if (!fp && pathname) *pathname = "";
	return fp;
}

