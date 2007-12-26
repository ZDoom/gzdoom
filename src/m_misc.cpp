// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
//
// $Log:$
//
// DESCRIPTION:
//		Default Config File.
//		Screenshots.
//
//-----------------------------------------------------------------------------


#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <malloc.h>

#include "doomtype.h"
#include "version.h"

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

#include <ctype.h>

#include "m_alloc.h"

#include "doomdef.h"

#include "m_swap.h"
#include "m_argv.h"

#include "w_wad.h"

#include "c_cvars.h"
#include "c_dispatch.h"
#include "c_bind.h"

#include "i_system.h"
#include "i_video.h"
#include "v_video.h"

#include "hu_stuff.h"

// State.
#include "doomstat.h"

// Data.
#include "m_misc.h"
#include "m_png.h"

#include "cmdlib.h"

#include "g_game.h"
#include "gi.h"

#include "gameconfigfile.h"

FGameConfigFile *GameConfig;

CVAR(Bool, screenshot_quiet, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG);
CVAR(String, screenshot_type, "png", CVAR_ARCHIVE|CVAR_GLOBALCONFIG);
CVAR(String, screenshot_dir, "", CVAR_ARCHIVE|CVAR_GLOBALCONFIG);

extern void FreeKeySections();

static long ParseCommandLine (const char *args, int *argc, char **argv);

//
// M_WriteFile
//
#ifndef O_BINARY
#define O_BINARY 0
#endif

bool M_WriteFile (char const *name, void *source, int length)
{
	int handle;
	int count;

	handle = open ( name, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0666);

	if (handle == -1)
		return false;

	count = write (handle, source, length);
	close (handle);

	if (count < length)
		return false;

	return true;
}


//
// M_ReadFile
//
int M_ReadFile (char const *name, BYTE **buffer)
{
	int handle, count, length;
	struct stat fileinfo;
	BYTE *buf;

	handle = open (name, O_RDONLY | O_BINARY, 0666);
	if (handle == -1)
		I_Error ("Couldn't read file %s", name);
	if (fstat (handle,&fileinfo) == -1)
		I_Error ("Couldn't read file %s", name);
	length = fileinfo.st_size;
	buf = new BYTE[length];
	count = read (handle, buf, length);
	close (handle);

	if (count < length)
		I_Error ("Couldn't read file %s", name);

	*buffer = buf;
	return length;
}

//---------------------------------------------------------------------------
//
// PROC M_FindResponseFile
//
//---------------------------------------------------------------------------

void M_FindResponseFile (void)
{
	int i;

	for (i = 1; i < Args.NumArgs(); i++)
	{
		if (Args.GetArg(i)[0] == '@')
		{
			char	**argv;
			char	*file;
			int		argc;
			int		argcinresp;
			FILE	*handle;
			int 	size;
			long	argsize;
			int 	k;
			int 	index;

			// READ THE RESPONSE FILE INTO MEMORY
			handle = fopen (Args.GetArg(i) + 1,"rb");
			if (!handle)
			{ // [RH] Make this a warning, not an error.
				Printf ("No such response file (%s)!", Args.GetArg(i) + 1);
				continue;
			}

			Printf ("Found response file %s!\n", Args.GetArg(i) + 1);
			fseek (handle, 0, SEEK_END);
			size = ftell (handle);
			fseek (handle, 0, SEEK_SET);
			file = new char[size+1];
			fread (file, size, 1, handle);
			file[size] = 0;
			fclose (handle);

			argsize = ParseCommandLine (file, &argcinresp, NULL);
			argc = argcinresp + Args.NumArgs() - 1;

			if (argc != 0)
			{
				argv = (char **)M_Malloc (argc*sizeof(char *) + argsize);
				argv[i] = (char *)argv + argc*sizeof(char *);
				ParseCommandLine (file, NULL, argv+i);

				for (index = 0; index < i; ++index)
					argv[index] = Args.GetArg (index);

				for (index = i + 1, i += argcinresp; index < Args.NumArgs (); ++index)
					argv[i++] = Args.GetArg (index);

				DArgs newargs (i, argv);
				Args = newargs;
			}

			delete[] file;
		
			// DISPLAY ARGS
			Printf ("%d command-line args:\n", Args.NumArgs ());
			for (k = 1; k < Args.NumArgs (); k++)
				Printf ("%s\n", Args.GetArg (k));

			break;
		}
	}
}

// ParseCommandLine
//
// This is just like the version in c_dispatch.cpp, except it does not
// do cvar expansion.

static long ParseCommandLine (const char *args, int *argc, char **argv)
{
	int count;
	char *buffplace;

	count = 0;
	buffplace = NULL;
	if (argv != NULL)
	{
		buffplace = argv[0];
	}

	for (;;)
	{
		while (*args <= ' ' && *args)
		{ // skip white space
			args++;
		}
		if (*args == 0)
		{
			break;
		}
		else if (*args == '\"')
		{ // read quoted string
			char stuff;
			if (argv != NULL)
			{
				argv[count] = buffplace;
			}
			count++;
			args++;
			do
			{
				stuff = *args++;
				if (stuff == '\\' && *args == '\"')
				{
					stuff = '\"', args++;
				}
				else if (stuff == '\"')
				{
					stuff = 0;
				}
				else if (stuff == 0)
				{
					args--;
				}
				if (argv != NULL)
				{
					*buffplace = stuff;
				}
				buffplace++;
			} while (stuff);
		}
		else
		{ // read unquoted string
			const char *start = args++, *end;

			while (*args && *args > ' ' && *args != '\"')
				args++;
			end = args;
			if (argv != NULL)
			{
				argv[count] = buffplace;
				while (start < end)
					*buffplace++ = *start++;
				*buffplace++ = 0;
			}
			else
			{
				buffplace += end - start + 1;
			}
			count++;
		}
	}
	if (argc != NULL)
	{
		*argc = count;
	}
	return (long)(buffplace - (char *)0);
}


#ifdef unix
FString GetUserFile (const char *file, bool nodir)
{
	char *home = getenv ("HOME");
	if (home == NULL || *home == '\0')
		I_FatalError ("Please set your HOME environment variable");

	FString path = home;
	if (path[path.Len()-1] != '/')
		path += nodir ? "/" : "/"GAME_DIR;
	else if (!nodir)
		path += GAME_DIR;

	if (!nodir)
	{
		struct stat info;
		if (stat (path, &info) == -1)
		{
			if (mkdir (path, S_IRUSR | S_IWUSR | S_IXUSR) == -1)
			{
				I_FatalError ("Failed to create %s directory:\n%s",
					path.GetChars(), strerror (errno));
			}
		}
		else
		{
			if (!S_ISDIR(info.st_mode))
			{
				I_FatalError ("%s must be a directory", path.GetChars());
			}
		}
	}
	path += '/';
	path += file;
	return path;
}
#endif

//
// M_SaveDefaults
//

bool M_SaveDefaults (const char *filename)
{
	FString oldpath;
	bool success;

	if (filename != NULL)
	{
		oldpath = GameConfig->GetPathName();
		GameConfig->ChangePathName (filename);
	}
	GameConfig->ArchiveGlobalData ();
	if (GameNames[gameinfo.gametype] != NULL)
	{
		GameConfig->ArchiveGameData (GameNames[gameinfo.gametype]);
	}
	success = GameConfig->WriteConfigFile ();
	if (filename != NULL)
	{
		GameConfig->ChangePathName (filename);
	}
	return success;
}

void M_SaveDefaultsFinal ()
{
	while (!M_SaveDefaults (NULL) && I_WriteIniFailed ())
	{
		/* Loop until the config saves or I_WriteIniFailed() returns false */
	}
	delete GameConfig;
	GameConfig = NULL;
}

CCMD (writeini)
{
	const char *filename = (argv.argc() == 1) ? NULL : argv[1];
	if (!M_SaveDefaults (filename))
	{
		Printf ("Writing config failed: %s\n", strerror(errno));
	}
	else
	{
		Printf ("Config saved.\n");
	}
}

//
// M_LoadDefaults
//

void M_LoadDefaults ()
{
	GameConfig = new FGameConfigFile;
	GameConfig->DoGlobalSetup ();
	atterm (FreeKeySections);
	atterm (M_SaveDefaultsFinal);
}


//
// SCREEN SHOTS
//


typedef struct
{
	char				manufacturer;
	char				version;
	char				encoding;
	char				bits_per_pixel;

	unsigned short		xmin;
	unsigned short		ymin;
	unsigned short		xmax;
	unsigned short		ymax;
	
	unsigned short		hdpi;
	unsigned short		vdpi;

	unsigned char		palette[48];
	
	char				reserved;
	char				color_planes;
	unsigned short		bytes_per_line;
	unsigned short		palette_type;
	
	char				filler[58];
} pcx_t;


//
// WritePCXfile
//
void WritePCXfile (FILE *file, const DCanvas *canvas, const PalEntry *palette)
{
	int x, y;
	int runlen;
	BYTE color;
	pcx_t pcx;
	BYTE *data;
	int width, height, pitch;

	data = canvas->GetBuffer ();
	width = canvas->GetWidth ();
	height = canvas->GetHeight ();
	pitch = canvas->GetPitch ();

	pcx.manufacturer = 10;				// PCX id
	pcx.version = 5;					// 256 color
	pcx.encoding = 1;
	pcx.bits_per_pixel = 8;				// 256 color
	pcx.xmin = 0;
	pcx.ymin = 0;
	pcx.xmax = LittleShort(width-1);
	pcx.ymax = LittleShort(height-1);
	pcx.hdpi = LittleShort(75);
	pcx.vdpi = LittleShort(75);
	memset (pcx.palette, 0, sizeof(pcx.palette));
	pcx.reserved = 0;
	pcx.color_planes = 1;				// chunky image
	pcx.bytes_per_line = width + (width & 1);
	pcx.palette_type = 1;				// not a grey scale
	memset (pcx.filler, 0, sizeof(pcx.filler));

	fwrite (&pcx, 128, 1, file);

	// pack the image
	for (y = height; y > 0; y--)
	{
		color = *data++;
		runlen = 1;

		for (x = width - 1; x > 0; x--)
		{
			if (*data == color)
			{
				runlen++;
			}
			else
			{
				if (runlen > 1 || color >= 0xc0)
				{
					while (runlen > 63)
					{
						putc (0xff, file);
						putc (color, file);
						runlen -= 63;
					}
					if (runlen > 0)
					{
						putc (0xc0 + runlen, file);
					}
				}
				if (runlen > 0)
				{
					putc (color, file);
				}
				runlen = 1;
				color = *data;
			}
			data++;
		}

		if (runlen > 1 || color >= 0xc0)
		{
			while (runlen > 63)
			{
				putc (0xff, file);
				putc (color, file);
				runlen -= 63;
			}
			if (runlen > 0)
			{
				putc (0xc0 + runlen, file);
			}
		}
		if (runlen > 0)
		{
			putc (color, file);
		}

		if (width & 1)
			putc (0, file);

		data += pitch - width;
	}

	// write the palette
	putc (12, file);		// palette ID byte
	for (x = 0; x < 256; x++, palette++)
	{
		putc (palette->r, file);
		putc (palette->g, file);
		putc (palette->b, file);
	}
}

//
// WritePNGfile
//
void WritePNGfile (FILE *file, const DCanvas *canvas, const PalEntry *palette)
{
	if (!M_CreatePNG (file, canvas, palette) ||
		!M_AppendPNGText (file, "Software", GAMENAME DOTVERSIONSTR) ||
		!M_FinishPNG (file))
	{
		Printf ("Could not create screenshot.\n");
	}
}


//
// M_ScreenShot
//
static bool FindFreeName (FString &fullname, const char *extension)
{
	FString lbmname;
	int i;

	for (i = 0; i <= 9999; i++)
	{
		lbmname.Format ("%sDOOM%04d.%s", fullname.GetChars(), i, extension);
		if (!FileExists (lbmname.GetChars()))
		{
			fullname = lbmname;
			return true;		// file doesn't exist
		}
	}
	return false;
}

void M_ScreenShot (const char *filename)
{
	FString autoname;
	bool writepcx = screen->CanWritePCX() && (stricmp (screenshot_type, "pcx") == 0);	// PNG is the default

	// find a file name to save it to
	if (filename == NULL || filename[0] == '\0')
	{
#ifndef unix
		if (Args.CheckParm ("-cdrom"))
		{
			autoname = CDROM_DIR "\\";
		}
		else
#endif
		{
			int dirlen = (int)strlen (screenshot_dir);
			if (dirlen == 0)
			{
				autoname = progdir;
			}
			else if (dirlen > 0)
			{
				autoname = screenshot_dir;
				if (autoname[dirlen-1] != '/' && autoname[dirlen-1] != '\\')
				{
					autoname += '/';
				}
			}
		}
		if (!FindFreeName (autoname, writepcx ? "pcx" : "png"))
		{
			Printf ("M_ScreenShot: Delete some screenshots\n");
			return;
		}
	}
	else
	{
		autoname = filename;
		DefaultExtension (autoname, writepcx ? ".pcx" : ".png");
	}
	CreatePath(screenshot_dir);

	// save the screenshot
	screen->Save(autoname, writepcx);

	if (!screenshot_quiet)
	{
		Printf ("Captured %s\n", autoname.GetChars());
	}
}

bool DCanvas::CanWritePCX()
{
	return true;
}

void DCanvas::Save(const char *filename, bool writepcx)
{
	FILE *file;

	Lock (true);

	PalEntry palette[256];
	screen->GetFlashedPalette (palette);

	file = fopen (filename, "wb");
	if (file == NULL)
	{
		Printf ("Could not open %s\n", filename);
		Unlock ();
		return;
	}

	if (writepcx)
	{
		WritePCXfile (file, this, palette);
	}
	else
	{
		WritePNGfile (file, this, palette);
	}
	fclose (file);
	Unlock ();
}

CCMD (screenshot)
{
	if (argv.argc() == 1)
		G_ScreenShot (NULL);
	else
		G_ScreenShot (argv[1]);
}
