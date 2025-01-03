/*
** c_cmds.cpp
** Miscellaneous game independent console commands.
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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

#ifndef _WIN32
#include <unistd.h>
#else
#include <direct.h>
#endif

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "c_console.h"
#include "c_dispatch.h"
#include "engineerrors.h"
#include "printf.h"
#include "files.h"
#include "filesystem.h"
#include "gstrings.h"
#include "version.h"
#include "fs_findfile.h"
#include "md5.h"
#include "i_specialpaths.h"
#include "i_system.h"
#include "cmdlib.h"

extern FILE* Logfile;

CCMD (quit)
{
	throw CExitEvent(0);
}

CCMD (exit)
{
	throw CExitEvent(0);
}

CCMD (gameversion)
{
	Printf ("%s @ %s\nCommit %s\n", GetVersionString(), GetGitTime(), GetGitHash());
}

CCMD (print)
{
	if (argv.argc() != 2)
	{
		Printf ("print <name>: Print a string from the string table\n");
		return;
	}
	const char *str = GStrings.CheckString(argv[1]);
	if (str == NULL)
	{
		Printf ("%s unknown\n", argv[1]);
	}
	else
	{
		Printf ("%s\n", str);
	}
}

UNSAFE_CCMD (exec)
{
	if (argv.argc() < 2)
		return;

	for (int i = 1; i < argv.argc(); ++i)
	{
		if (!C_ExecFile(argv[i]))
		{
			Printf ("Could not exec \"%s\"\n", argv[i]);
			break;
		}
	}
}

void execLogfile(const char *fn, bool append)
{
	if ((Logfile = fopen(fn, append? "a" : "w")))
	{
		const char *timestr = myasctime();
		Printf("Log started: %s\n", timestr);
	}
	else
	{
		Printf("Could not start log\n");
	}
}

UNSAFE_CCMD (logfile)
{

	if (Logfile)
	{
		const char *timestr = myasctime();
		Printf("Log stopped: %s\n", timestr);
		fclose (Logfile);
		Logfile = NULL;
	}

	if (argv.argc() >= 2)
	{
		execLogfile(argv[1], argv.argc() >=3? !!argv[2]:false);
	}
}

CCMD (error)
{
	if (argv.argc() > 1)
	{
		I_Error ("%s", argv[1]);
	}
	else
	{
		Printf ("Usage: error <error text>\n");
	}
}

UNSAFE_CCMD (error_fatal)
{
	if (argv.argc() > 1)
	{
		I_FatalError ("%s", argv[1]);
	}
	else
	{
		Printf ("Usage: error_fatal <error text>\n");
	}
}

//==========================================================================
//
// CCMD crashout
//
// Debugging routine for testing the crash logger.
// Useless in a win32 debug build, because that doesn't enable the crash logger.
//
//==========================================================================

#if !defined(_WIN32) || !defined(_DEBUG)
UNSAFE_CCMD (crashout)
{
	*(volatile int *)0 = 0;
}
#endif


UNSAFE_CCMD (dir)
{
	FString path;

	if (argv.argc() > 1)
	{
		path = NicePath(argv[1]);
	}
	else
	{
		path = I_GetCWD();;
	}
	auto base = ExtractFileBase(path.GetChars(), true);
	FString bpath;
	if (base.IndexOfAny("*?") >= 0)
	{
		bpath = ExtractFilePath(path.GetChars());
	}
	else
	{
		base = "*";
		bpath = path;
	}

	FileSys::FileList list;
	if (!FileSys::ScanDirectory(list, bpath.GetChars(), base.GetChars(), true))
	{ 
		Printf ("Nothing matching %s\n", path.GetChars());
	}
	else
	{
		Printf ("Listing of %s:\n", path.GetChars());
		for(auto& entry : list)
		{
			if (entry.isDirectory)
				Printf (PRINT_BOLD, "%s <dir>\n", entry.FileName.c_str());
			else
				Printf ("%s\n", entry.FileName.c_str());
		}
	}
}

//==========================================================================
//
// CCMD wdir
//
// Lists the contents of a loaded wad file.
//
//==========================================================================

CCMD (wdir)
{
	int wadnum;
	if (argv.argc() != 2) wadnum = -1;
	else 
	{
		wadnum = fileSystem.CheckIfResourceFileLoaded (argv[1]);
		if (wadnum < 0)
		{
			Printf ("%s must be loaded to view its directory.\n", argv[1]);
			return;
		}
	}
	for (int i = 0; i < fileSystem.GetNumEntries(); ++i)
	{
		if (wadnum == -1 || fileSystem.GetFileContainer(i) == wadnum)
		{
			Printf ("%10ld %s\n", fileSystem.FileLength(i), fileSystem.GetFileFullName(i));
		}
	}
}

//==========================================================================
//
// CCMD md5sum
//
// Like the command-line tool, because I wanted to make sure I had it right.
//
//==========================================================================

CCMD (md5sum)
{
	if (argv.argc() < 2)
	{
		Printf("Usage: md5sum <file> ...\n");
	}
	for (int i = 1; i < argv.argc(); ++i)
	{
		FileReader fr;
		if (!fr.OpenFile(argv[i]))
		{
			Printf("%s: %s\n", argv[i], strerror(errno));
		}
		else
		{
			MD5Context md5;
			uint8_t readbuf[8192];
			size_t len;

			while ((len = fr.Read(readbuf, sizeof(readbuf))) > 0)
			{
				md5.Update(readbuf, (unsigned int)len);
			}
			md5.Final(readbuf);
			for(int j = 0; j < 16; ++j)
			{
				Printf("%02x", readbuf[j]);
			}
			Printf(" *%s\n", argv[i]);
		}
	}
}

CCMD(printlocalized)
{
	if (argv.argc() > 1)
	{
		if (argv.argc() > 2)
		{
			FString lang = argv[2];
			lang.ToLower();
			if (lang.Len() >= 2)
			{
				Printf("%s\n", GStrings.GetLanguageString(argv[1], MAKE_ID(lang[0], lang[1], lang[2], 0)));
				return;
			}
		}
		Printf("%s\n", GStrings.GetString(argv[1]));
	}

}

