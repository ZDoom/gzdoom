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
//		Main loop menu stuff.
//		Default Config File.
//		PCX Screenshots.
//
//-----------------------------------------------------------------------------


#include "doomtype.h"
#include "version.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

#include <ctype.h>

#include "m_alloc.h"

#include "doomdef.h"

#include "z_zone.h"

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

#include "cmdlib.h"

#include "g_game.h"
#include "gi.h"

#include "gameconfigfile.h"

FGameConfigFile *GameConfig;

//
// M_WriteFile
//
#ifndef O_BINARY
#define O_BINARY 0
#endif

BOOL M_WriteFile (char const *name, void *source, int length)
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
int M_ReadFile (char const *name, byte **buffer)
{
	int handle, count, length;
	struct stat fileinfo;
	byte *buf;

	handle = open (name, O_RDONLY | O_BINARY, 0666);
	if (handle == -1)
		I_Error ("Couldn't read file %s", name);
	if (fstat (handle,&fileinfo) == -1)
		I_Error ("Couldn't read file %s", name);
	length = fileinfo.st_size;
	buf = (byte *)Z_Malloc (length, PU_STATIC, NULL);
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

#define MAXARGVS 100

void M_FindResponseFile (void)
{
	int i;

	for (i = 1; i < Args.NumArgs(); i++)
	{
		if (Args.GetArg(i)[0] == '@')
		{
			char	**argv;
			FILE	*handle;
			int 	size;
			int 	k;
			int 	index;
			int 	indexinfile;
			char	*infile;
			char	*file;

			// READ THE RESPONSE FILE INTO MEMORY
			handle = fopen (Args.GetArg(i) + 1,"rb");
			if (!handle)
				I_FatalError ("\nNo such response file!");

			Printf ("Found response file %s!\n", Args.GetArg(i) + 1);
			fseek (handle, 0, SEEK_END);
			size = ftell (handle);
			fseek (handle, 0, SEEK_SET);
			file = new char[size];
			fread (file, size, 1, handle);
			fclose (handle);

			argv = new char *[MAXARGVS];
			for (index = 0; index < i; index++)
				argv[index] = Args.GetArg (index);

			infile = file;
			k = 0;
			indexinfile = index;
			do
			{
				argv[indexinfile++] = infile+k;
				while(k < size &&
					  ((*(infile+k)>= ' '+1) && (*(infile+k)<='z')))
					k++;
				*(infile+k) = 0;
				while(k < size &&
					  ((*(infile+k)<= ' ') || (*(infile+k)>'z')))
					k++;
			} while(k < size);

			for (index = i + 1; index < Args.NumArgs (); index++)
				argv[indexinfile++] = Args.GetArg (index);

			DArgs newargs (indexinfile, argv);
			Args = newargs;

			delete[] file;
		
			// DISPLAY ARGS
			Printf ("%d command-line args:\n", Args.NumArgs ());
			for (k = 1; k < Args.NumArgs (); k++)
				Printf ("%s\n", Args.GetArg (k));

			break;
		}
	}
}


//
// DEFAULTS
//
// [RH] Handled by console code now.

#ifdef UNIX
char *GetUserFile (const char *file, bool nodir)
{
	char *home = getenv ("HOME");
	if (home == NULL || *home == '\0')
		I_FatalError ("Please set your HOME variable");

	char *path = new char[strlen (home) + 9 + strlen (file)];
	strcpy (path, home);
	if (home[strlen (home)-1] != '/')
		strcat (path, nodir ? "/" : "/.zdoom");
	else if (!nodir)
		strcat (path, ".zdoom");

	if (!nodir)
	{
		struct stat info;
		if (stat (path, &info) == -1)
		{
			if (mkdir (path, S_IRUSR | S_IWUSR | S_IXUSR) == -1)
			{
				I_FatalError ("Failed to create %s directory:\n%s",
							  path, strerror (errno));
			}
		}
		else
		{
			if (!S_ISDIR(info.st_mode))
			{
				I_FatalError ("%s must be a directory", path);
			}
		}
	}
	strcat (path, "/");
	strcat (path, file);
	return path;
}
#endif

//
// M_SaveDefaults
//

void STACK_ARGS M_SaveDefaults ()
{
	GameConfig->ArchiveGlobalData ();
	if (GameNames[gameinfo.gametype] != NULL)
	{
		GameConfig->ArchiveGameData (GameNames[gameinfo.gametype]);
	}
	GameConfig->WriteConfigFile ();
	delete GameConfig;
}


//
// M_LoadDefaults
//

void M_LoadDefaults ()
{
	GameConfig = new FGameConfigFile;
	GameConfig->DoGlobalSetup ();
	atterm (M_SaveDefaults);
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
void WritePCXfile (char *filename, DCanvas *canvas, const PalEntry *palette)
{
	int x, y;
	int runlen;
	BYTE color;
	pcx_t pcx;
	FILE *file;
	BYTE *data;
	int width, height, pitch;

	file = fopen (filename, "wb");
	if (file == NULL)
	{
		Printf ("Could not open %s\n", filename);
		return;
	}

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
	pcx.xmax = SHORT(width-1);
	pcx.ymax = SHORT(height-1);
	pcx.hdpi = SHORT(75);
	pcx.vdpi = SHORT(75);
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

	canvas->Unlock ();

	// write the palette
	putc (12, file);		// palette ID byte
	for (x = 0; x < 256; x++, palette++)
	{
		putc (palette->r, file);
		putc (palette->g, file);
		putc (palette->b, file);
	}

	fclose (file);
}


//
// M_ScreenShot
//
static BOOL FindFreeName (char *lbmname, const char *extension)
{
	int i;

	for (i=0 ; i<=9999 ; i++)
	{
		sprintf (lbmname, "DOOM%04d.%s", i, extension);
		if (!FileExists (lbmname))
			return true;		// file doesn't exist
	}
	return false;
}

CVAR(Bool, screenshot_quiet, false, CVAR_ARCHIVE);

void M_ScreenShot (char *filename)
{
	char  autoname[32];
	char *lbmname;

	// find a file name to save it to
	if (!filename)
	{
#ifndef UNIX
		if (Args.CheckParm ("-cdrom"))
		{
			strcpy (autoname, "C:\\ZDOOMDAT\\");
			lbmname = autoname + 12;
		}
		else
#endif
		{
			lbmname = autoname;
		}
		if (!FindFreeName (lbmname, "pcx"))
		{
			Printf ("M_ScreenShot: Delete some screenshots\n");
			return;
		}
		filename = autoname;
	}

	// save the pcx file
	D_Display (true);
	WritePCXfile (filename, screen, screen->GetPalette ());

	if (!*screenshot_quiet)
	{
		Printf ("Captured %s\n", lbmname);
	}
}

CCMD (screenshot)
{
	if (argc == 1)
		G_ScreenShot (NULL);
	else
		G_ScreenShot (argv[1]);
}
