#ifdef __SUNPRO_C
#pragma error_messages (off,E_EMPTY_TRANSLATION_UNIT)
#endif

#include "common.h"

#if !(defined(LIBXMP_NO_PROWIZARD) && defined(LIBXMP_NO_DEPACKERS))

#ifndef HAVE_MKSTEMP

/*
 * Copyright (c) 1995, 1996, 1997 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <fcntl.h>
#include <errno.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
#if defined(_MSC_VER) || defined(__WATCOMC__)
#include <io.h>
#else
#include <unistd.h>
#endif
#ifdef _MSC_VER
#include <process.h>
#define open _open
#endif
#ifndef O_BINARY
#define O_BINARY 0
#endif

int mkstemp(char *pattern)
{
	int start, i;
#ifdef _WIN32
	int val = GetCurrentProcessId();
#else
	pid_t val = getpid();
#endif

	start = strlen(pattern) - 1;

	while (pattern[start] == 'X') {
		pattern[start] = '0' + val % 10;
		val /= 10;
		start--;
	}

	do {
		int fd;
		fd = open(pattern, O_RDWR | O_CREAT | O_EXCL | O_BINARY, 0600);
		if (fd >= 0 || errno != EEXIST)
			return fd;
		i = start + 1;
		do {
			if (pattern[i] == 0)
				return -1;
			pattern[i]++;
			if (pattern[i] == '9' + 1)
				pattern[i] = 'a';
			if (pattern[i] <= 'z')
				break;
			pattern[i] = 'a';
			i++;
		} while (1);
	} while (1);
}

#endif

#endif
