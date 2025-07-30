/* Extended Module Player
 * Copyright (C) 1996-2022 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "common.h"
#include <errno.h>

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

int libxmp_get_filetype (const char *path)
{
	DWORD result = GetFileAttributesA(path);
	if (result == (DWORD)(-1)) {
	    errno = ENOENT;
	    return XMP_FILETYPE_NONE;
	}
	return (result & FILE_ATTRIBUTE_DIRECTORY) ? XMP_FILETYPE_DIR : XMP_FILETYPE_FILE;
}

#elif defined(__OS2__) || defined(__EMX__)

#define INCL_DOSFILEMGR
#include <os2.h>

int libxmp_get_filetype (const char *path)
{
	FILESTATUS3 fs;
	if (DosQueryPathInfo(path, FIL_STANDARD, &fs, sizeof(fs)) != 0) {
	    errno = ENOENT;
	    return XMP_FILETYPE_NONE;
	}
	return (fs.attrFile & FILE_DIRECTORY) ? XMP_FILETYPE_DIR : XMP_FILETYPE_FILE;
}

#elif defined(__DJGPP__)

#include <dos.h>
#include <io.h>

int libxmp_get_filetype (const char *path)
{
	int attr = _chmod(path, 0);
	/* Root directories on some non-local drives (e.g. CD-ROM), as well as
	 * devices may fail _chmod, but we are not interested in such cases. */
	if (attr < 0) return XMP_FILETYPE_NONE;
	/* we shouldn't hit _A_VOLID ! */
	return (attr & (_A_SUBDIR|_A_VOLID)) ? XMP_FILETYPE_DIR : XMP_FILETYPE_FILE;
}

#elif defined(__WATCOMC__) && defined(_DOS)

#include <dos.h>
#include <direct.h>

int libxmp_get_filetype (const char *path)
{
	unsigned int attr;
	if (_dos_getfileattr(path, &attr)) return XMP_FILETYPE_NONE;
	return (attr & (_A_SUBDIR|_A_VOLID)) ? XMP_FILETYPE_DIR : XMP_FILETYPE_FILE;
}

#elif defined(__amigaos4__)

#define __USE_INLINE__
#include <proto/dos.h>

int libxmp_get_filetype (const char *path)
{
	int typ = XMP_FILETYPE_NONE;
	struct ExamineData *data = ExamineObjectTags(EX_StringNameInput, path, TAG_END);
	if (data) {
	    if (EXD_IS_FILE(data)) {
		typ = XMP_FILETYPE_FILE;
	    } else
	    if (EXD_IS_DIRECTORY(data)) {
		typ = XMP_FILETYPE_DIR;
	    }
	    FreeDosObject(DOS_EXAMINEDATA, data);
	}
	if (typ == XMP_FILETYPE_NONE) errno = ENOENT;
	return typ;
}

#elif defined(LIBXMP_AMIGA)

#include <proto/dos.h>

int libxmp_get_filetype (const char *path)
{
	int typ = XMP_FILETYPE_NONE;
	BPTR lock = Lock((const STRPTR)path, ACCESS_READ);
	if (lock) {
	    struct FileInfoBlock *fib = (struct FileInfoBlock *) AllocDosObject(DOS_FIB,NULL);
	    if (fib) {
		if (Examine(lock, fib)) {
		    typ = (fib->fib_DirEntryType < 0) ? XMP_FILETYPE_FILE : XMP_FILETYPE_DIR;
		}
		FreeDosObject(DOS_FIB, fib);
	    }
	    UnLock(lock);
	}
	if (typ == XMP_FILETYPE_NONE) errno = ENOENT;
	return typ;
}

#else /* unix (ish): */

#include <sys/types.h>
#include <sys/stat.h>

int libxmp_get_filetype (const char *path)
{
	struct stat st;
	memset(&st, 0, sizeof(st)); /* silence sanitizers.. */
	if (stat(path, &st) < 0) return XMP_FILETYPE_NONE;
	if (S_ISDIR(st.st_mode)) return XMP_FILETYPE_DIR;
	if (S_ISREG(st.st_mode)) return XMP_FILETYPE_FILE;
	return XMP_FILETYPE_NONE;
}

#endif

