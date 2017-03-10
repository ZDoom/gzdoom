/*
** file_io.cpp
** ZDoom compatible file IO interface for WildMIDI
** (This file was completely redone to remove the low level IO code references)
**
**---------------------------------------------------------------------------
** Copyright 2010 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include <errno.h>

#include "../files.h"
#include "wm_error.h"
#include "file_io.h"
#include "pathexpander.h"
#include "cmdlib.h"

static PathExpander wmPathExpander;

unsigned char *_WM_BufferFile(const char *filename, unsigned long int *size, bool ismain) 
{
	FileReader *fp;
	int lumpnum;

	if (ismain)
	{
		wmPathExpander.openmode = PathExpander::OM_FILEORLUMP;
		wmPathExpander.clearPathlist();
#ifdef _WIN32
		wmPathExpander.addToPathlist("C:\\TIMIDITY");
		wmPathExpander.addToPathlist("\\TIMIDITY");
		wmPathExpander.addToPathlist(progdir);
#else
		wmPathExpander.addToPathlist("/usr/local/lib/timidity");
		wmPathExpander.addToPathlist("/etc/timidity");
		wmPathExpander.addToPathlist("/etc");
#endif
	}

	if (!(fp = wmPathExpander.openFileReader(filename, &lumpnum)))
		return NULL;

	if (ismain)
	{
		if (lumpnum > 0)
		{
			wmPathExpander.openmode = PathExpander::OM_LUMP;
			wmPathExpander.clearPathlist();	// when reading from a PK3 we don't want to use any external path
		}
		else
		{
			wmPathExpander.openmode = PathExpander::OM_FILE;
		}
	}



	if (fp == NULL)
	{
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, filename, errno);
		return NULL;
	}

	long fsize = fp->GetLength();

	if (fsize > WM_MAXFILESIZE) 
	{
		/* don't bother loading suspiciously long files */
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LONGFIL, filename, 0);
		return NULL;
	}

	unsigned char *data = (unsigned char*)malloc(fsize+1);
	if (data == NULL)
	{
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, NULL, errno);
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, filename, errno);
		return NULL;
	}

	fp->Read(data, fsize);
	delete fp;
	data[fsize] = 0;
	*size = fsize;
	return data;
}
