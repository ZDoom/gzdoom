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

#ifdef __SUNPRO_C
#pragma error_messages (off,E_EMPTY_TRANSLATION_UNIT)
#endif

#include "common.h"

#if !(defined(LIBXMP_NO_PROWIZARD) && defined(LIBXMP_NO_DEPACKERS))

#if defined(_MSC_VER) || defined(__WATCOMC__)
#include <io.h>
#else
#include <unistd.h>
#endif
#ifdef HAVE_UMASK
#include <sys/types.h>
#include <sys/stat.h>
#endif

#include "tempfile.h"

#ifdef _WIN32

int mkstemp(char *);

static int get_temp_dir(char *buf, size_t size)
{
	static const char def[] = "C:\\WINDOWS\\TEMP";
	const char *tmp = getenv("TEMP");
	snprintf(buf, size, "%s\\", (tmp != NULL)? tmp : def);
	return 0;
}

#elif defined(__OS2__) || defined(__EMX__)

static int get_temp_dir(char *buf, size_t size)
{
	static const char def[] = "C:";
	const char *tmp = getenv("TMP");
	snprintf(buf, size, "%s\\", (tmp != NULL)? tmp : def);
	return 0;
}

#elif defined(__MSDOS__) || defined(_DOS)

static int get_temp_dir(char *buf, size_t size)
{
	strcpy(buf, "C:\\"); /* size-safe against XMP_MAXPATH */
	return 0;
}

#elif defined LIBXMP_AMIGA

static int get_temp_dir(char *buf, size_t size)
{
	strcpy(buf, "T:"); /* size-safe against XMP_MAXPATH */
	return 0;
}

#elif defined __ANDROID__

#include <sys/types.h>
#include <sys/stat.h>

static int get_temp_dir(char *buf, size_t size)
{
#define APPDIR "/sdcard/Xmp for Android"
	struct stat st;
	if (stat(APPDIR, &st) < 0) {
		if (mkdir(APPDIR, 0777) < 0)
			return -1;
	}
	if (stat(APPDIR "/tmp", &st) < 0) {
		if (mkdir(APPDIR "/tmp", 0777) < 0)
			return -1;
	}
	strncpy(buf, APPDIR "/tmp/", size);

	return 0;
}

#else /* unix */

static int get_temp_dir(char *buf, size_t size)
{
	const char *tmp = getenv("TMPDIR");

	if (tmp) {
		snprintf(buf, size, "%s/", tmp);
	} else {
		strncpy(buf, "/tmp/", size);
	}

	return 0;
}

#endif


FILE *make_temp_file(char **filename) {
	char tmp[XMP_MAXPATH];
	FILE *temp;
	int fd;

	if (get_temp_dir(tmp, XMP_MAXPATH) < 0)
		return NULL;

	strncat(tmp, "xmp_XXXXXX", XMP_MAXPATH - 10);

	if ((*filename = libxmp_strdup(tmp)) == NULL)
		goto err;

#ifdef HAVE_UMASK
	umask(0177);
#endif
	if ((fd = mkstemp(*filename)) < 0)
		goto err2;

	if ((temp = fdopen(fd, "w+b")) == NULL)
		goto err3;

	return temp;

    err3:
	close(fd);
    err2:
	free(*filename);
    err:
	return NULL;
}

/*
 * Windows doesn't allow you to unlink an open file, so we changed the
 * temp file cleanup system to remove temporary files after we close it
 */
void unlink_temp_file(char *temp)
{
	if (temp) {
		unlink(temp);
		free(temp);
	}
}

#endif
