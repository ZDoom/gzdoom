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
#include "c_dispch.h"
#include "c_bind.h"

#include "i_system.h"
#include "i_video.h"
#include "v_video.h"

#include "hu_stuff.h"

// State.
#include "doomstat.h"

// Data.
#include "dstrings.h"

#include "m_misc.h"

#include "cmdlib.h"

#include "g_game.h"

//
// M_WriteFile
//
#ifndef O_BINARY
#define O_BINARY 0
#endif

BOOL M_WriteFile (char const *name, void *source, int length)
{
	int 		handle;
	int 		count;
		
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
	byte		*buf;
		
	handle = open (name, O_RDONLY | O_BINARY, 0666);
	if (handle == -1)
		I_Error ("Couldn't read file %s", name);
	if (fstat (handle,&fileinfo) == -1)
		I_Error ("Couldn't read file %s", name);
	length = fileinfo.st_size;
	buf = Z_Malloc (length, PU_STATIC, NULL);
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
		
	for (i = 1; i < myargc; i++)
	{
		if (myargv[i][0] == '@')
		{
			FILE *	handle;
			int 	size;
			int 	k;
			int 	index;
			int 	indexinfile;
			char	*infile;
			char	*file;
			char	*moreargs[20];
			char	*firstargv;
						
			// READ THE RESPONSE FILE INTO MEMORY
			handle = fopen (&myargv[i][1],"rb");
			if (!handle)
				I_FatalError ("\nNo such response file!");

			Printf (PRINT_HIGH, "Found response file %s!\n", &myargv[i][1]);
			fseek (handle,0,SEEK_END);
			size = ftell(handle);
			fseek (handle,0,SEEK_SET);
			file = Malloc (size);
			fread (file,size,1,handle);
			fclose (handle);
						
			// KEEP ALL CMDLINE ARGS FOLLOWING @RESPONSEFILE ARG
			for (index = 0,k = i+1; k < myargc; k++)
				moreargs[index++] = myargv[k];
						
			firstargv = myargv[0];
			myargv = Malloc(sizeof(char *)*MAXARGVS);
			memset(myargv,0,sizeof(char *)*MAXARGVS);
			myargv[0] = firstargv;
						
			infile = file;
			indexinfile = k = 0;
			indexinfile++;	// SKIP PAST ARGV[0] (KEEP IT)
			do
			{
				myargv[indexinfile++] = infile+k;
				while(k < size &&
					  ((*(infile+k)>= ' '+1) && (*(infile+k)<='z')))
					k++;
				*(infile+k) = 0;
				while(k < size &&
					  ((*(infile+k)<= ' ') || (*(infile+k)>'z')))
					k++;
			} while(k < size);
						
			for (k = 0;k < index;k++)
				myargv[indexinfile++] = moreargs[k];
			myargc = indexinfile;
		
			// DISPLAY ARGS
			Printf (PRINT_HIGH, "%d command-line args:\n",myargc);
			for (k = 1; k < myargc; k++)
				Printf (PRINT_HIGH, "%s\n",myargv[k]);

			break;
		}
	}
}


//
// DEFAULTS
//
// [RH] Handled by console code now.

// [RH] Get configfile path.
// This file contains commands to set all
// archived cvars, bind commands to keys,
// and set other general game information.
char *GetConfigPath (void)
{
	int p;
	char *path;

	p = M_CheckParm ("-config");
	if (p && p < myargc - 1) {
		return copystring (myargv[p+1]);
	}

	if (M_CheckParm ("-cdrom"))
		return copystring ("c:\\zdoomdat\\zdoom.cfg");

	path = Malloc (strlen (progdir) + 11);

	strcpy (path, progdir);
	strcat (path, "zdoom.cfg");
	return path;
}

char *GetAutoexecPath (void)
{
	cvar_t *autovar;

	if (M_CheckParm ("-cdrom")) {
		return copystring ("c:\\zdoomdat\\autoexec.cfg");
	} else {
		char *path = Malloc (strlen (progdir) + 13);

		strcpy (path, progdir);
		strcat (path, "autoexec.cfg");

		autovar = cvar ("autoexec", path, CVAR_ARCHIVE);
		free (path);

		return autovar->string;
	}
}

//
// M_SaveDefaults
//

// [RH] Don't write a config file if M_LoadDefaults hasn't been called.
static BOOL DefaultsLoaded;

void M_SaveDefaults (void)
{
	FILE *f;
	char *configfile;

	if (!DefaultsLoaded)
		return;

	configfile = GetConfigPath ();

	// Make sure the user hasn't changed configver
	//cvar_set ("configver", VERSIONSTR);
	cvar_set ("configver", "117.2");

	if ( (f = fopen (configfile, "w")) ) {
		fprintf (f, "// Generated by ZDOOM - don't hurt anything\n");

		// Archive all cvars marked as CVAR_ARCHIVE
		C_ArchiveCVars (f);

		// Archive all active key bindings
		C_ArchiveBindings (f);

		// Archive all aliases
		C_ArchiveAliases (f);

		fclose (f);
	}
	free (configfile);
}


//
// M_LoadDefaults
//
extern byte scantokey[128];
extern int cvar_defflags;
extern cvar_t *dimamount;

void M_LoadDefaults (void)
{
	extern char DefBindings[];
	char*		configfile;
	char*		execcommand;
	cvar_t*		configver;

	// Set default key bindings. These will be overridden
	// by the bindings in the config file if it exists.
	AddCommandString (DefBindings);

	// Used to identify the version of the game that saved
	// a config file to compensate for new features that get
	// put into newer configfiles.
//	configver = cvar ("configver", VERSIONSTR, CVAR_ARCHIVE);
	configver = cvar ("configver", "117.2", CVAR_ARCHIVE);

	configfile = GetConfigPath ();
	execcommand = Malloc (strlen (configfile) + 8);
	sprintf (execcommand, "exec \"%s\"", configfile);
	free (configfile);
	cvar_defflags = CVAR_ARCHIVE;
	AddCommandString (execcommand);
	cvar_defflags = 0;
	free (execcommand);

	configfile = GetAutoexecPath ();
	execcommand = Malloc (strlen (configfile) + 8);
	sprintf (execcommand, "exec \"%s\"", configfile);
	AddCommandString (execcommand);
	free (execcommand);

	if (configver->value < 117.2f)
	{
		SetCVarFloat (dimamount, dimamount->value / 4);
		if (configver->value <= 113.0f)
		{
			AddCommandString ("bind t messagemode; bind \\ +showscores;"
							  "bind f12 spynext; bind sysrq screenshot");
			if (C_GetBinding (KEY_F5) && !stricmp (C_GetBinding (KEY_F5), "menu_video"))
				AddCommandString ("bind f5 menu_display");
		}
	}

	DefaultsLoaded = true;
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
	
	unsigned short		hres;
	unsigned short		vres;

	unsigned char		palette[48];
	
	char				reserved;
	char				color_planes;
	unsigned short		bytes_per_line;
	unsigned short		palette_type;
	
	char				filler[58];
	unsigned char		data;			// unbounded
} pcx_t;


//
// WritePCXfile
//
void WritePCXfile (char *filename, byte *data, int width, int height, unsigned int *palette)
{
	int 		i;
	int 		length;
	pcx_t*		pcx;
	byte*		pack;
		
	pcx = Z_Malloc (width*height*2+1000, PU_STATIC, NULL);

	pcx->manufacturer = 0x0a;			// PCX id
	pcx->version = 5;					// 256 color
	pcx->encoding = 1;					// uncompressed
	pcx->bits_per_pixel = 8;			// 256 color
	pcx->xmin = 0;
	pcx->ymin = 0;
	pcx->xmax = SHORT(width-1);
	pcx->ymax = SHORT(height-1);
	pcx->hres = SHORT(width);
	pcx->vres = SHORT(height);
	memset (pcx->palette,0,sizeof(pcx->palette));
	pcx->color_planes = 1;				// chunky image
	pcx->bytes_per_line = SHORT(width);
	pcx->palette_type = SHORT(1);		// not a grey scale [RH] Really!
	memset (pcx->filler,0,sizeof(pcx->filler));


	// pack the image
	pack = &pcx->data;
		
	for (i=0 ; i<width*height ; i++)
	{
		if ( (*data & 0xc0) != 0xc0)
			*pack++ = *data++;
		else
		{
			*pack++ = 0xc1;
			*pack++ = *data++;
		}
	}
	
	// write the palette
	*pack++ = 0x0c; 	// palette ID byte
	for (i=0 ; i<256 ; i++, palette++) {
		*pack++ = RPART(*palette);
		*pack++ = GPART(*palette);
		*pack++ = BPART(*palette);
	}
	
	// write output file
	length = pack - (byte *)pcx;
	M_WriteFile (filename, pcx, length);

	Z_Free (pcx);
}


//
// M_ScreenShot
//
static BOOL FindFreeName (char *lbmname, char *extension)
{
	int i;

	for (i=0 ; i<=9999 ; i++)
	{
		sprintf (lbmname, "DOOM%04d.%s", i, extension);
		if (!FileExists (lbmname))
			break;		// file doesn't exist
	}
	if (i==10000)
		return false;
	else
		return true;
}

extern unsigned IndexedPalette[256];

void M_ScreenShot (char *filename)
{
	byte *linear;
	char  autoname[32];
	char *lbmname;
	
	// find a file name to save it to
	if (!filename) {
		if (M_CheckParm ("-cdrom")) {
			strcpy (autoname, "C:\\ZDOOMDAT\\");
			lbmname = autoname + 12;
		} else {
			lbmname = autoname;
		}
		if (!FindFreeName (lbmname, "tga\0pcx" + (screen.is8bit << 2))) {
			Printf (PRINT_HIGH, "M_ScreenShot: Delete some screenshots\n");
			return;
		}
		filename = autoname;
	}

	if (screen.is8bit) {
		// munge planar buffer to linear
		linear = Malloc (screen.width * screen.height);
		I_ReadScreen (linear);
		
		// save the pcx file
		WritePCXfile (filename, linear,
					  screen.width, screen.height,
					  IndexedPalette);

		free (linear);
	} else {
		// save the tga file
		//I_WriteTGAfile (filename, &screen);
	}
	Printf (PRINT_HIGH, "screen shot\n");
}

void Cmd_Screenshot (void *plyr, int argc, char **argv)
{
	if (argc == 1)
		G_ScreenShot (NULL);
	else
		G_ScreenShot (argv[1]);
}
