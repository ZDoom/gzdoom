// Emacs style mode select   -*- C++ -*- 
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
// $Log:$
//
// DESCRIPTION:
//		Handles WAD file header, directory, lump I/O.
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//
// This file contains various levels of support 
// for using sprites and flats directly from a PWAD as well as some minor
// optimisations for patches. Because there are some PWADs that do arcane
// things with sprites, it is possible that this feature may not always
// work (at least, not until I become aware of them and support them) and
// so this feature can be turned off from the command line if necessary.
//
// See the file README.sprites for details.
//
// Martin Howe (martin.howe@dial.pipex.com), March 4th 1998
//

static const char
rcsid[] = "$Id: w_wad.c,v 1.5 1997/02/03 16:47:57 b1 Exp $";


#include <io.h>
#include <ctype.h>
#include <sys/types.h>
#include <string.h>
#include <malloc.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "doomtype.h"
#include "doomdef.h"
#include "m_swap.h"
#include "m_argv.h"
#include "i_system.h"
#include "z_zone.h"
#include "cmdlib.h"

#ifdef __GNUG__
#pragma implementation "w_wad.h"
#endif
#include "w_wad.h"


//
// GLOBALS
//

// Location of each lump on disk.
lumpinfo_t*		lumpinfo;
int				numlumps;

void**			lumpcache;


// Sprites, Flats, Patches
int		sprite_lists=0;			// number of sprite begin..end sections found
int		flat_lists=0;			// number of flat begin..end sections found
int		patch_lists=0;			// number of patch begin..end sections found
int		lax_list_syntax=1;		// use commonly found begin..end syntax
int		lax_sprite_rot=0;		// disengage sprite rotation/limit checks
int		group_sprites=0;		// enable sprite optimisations
int		group_flats=0;			// enable sprite optimisations
int		group_patches=0;		// enable sprite optimisations

int W_IsS_START(char *name)
//
// Is the name a sprite list start flag?
// If lax syntax match, fix up to standard syntax.
//
{
	int result,lax;

	result = ( (name[0]=='S') &&
			   (name[1]=='_') &&
			   (name[2]=='S') &&
			   (name[3]=='T') &&
			   (name[4]=='A') &&
			   (name[5]=='R') &&
			   (name[6]=='T') &&
			   (name[7]== 0 ) );
	lax    = ( (!result) &&
			   (name[0]=='S') &&
			   (name[1]=='S') &&
			   (name[2]=='_') &&
			   (name[3]=='S') &&
			   (name[4]=='T') &&
			   (name[5]=='A') &&
			   (name[6]=='R') &&
			   (name[7]=='T') );
	if (lax) {
		if (!lax_list_syntax)
			I_Error("W_IsS_START: flag SS_START encountered in sprites list");

		//fix up flag to standard syntax
		result=1;
		name[0]='S';
		name[1]='_';
		name[2]='S';
		name[3]='T';
		name[4]='A';
		name[5]='R';
		name[6]='T';
		name[7]= 0 ;
	}

	return result;
}

int W_IsS_END(char *name)
//
// Is the name a sprite list end flag?
// If lax syntax match, fix up to standard syntax.
//
{
	int result,lax;

	result = ( (name[0]=='S') &&
			   (name[1]=='_') &&
			   (name[2]=='E') &&
			   (name[3]=='N') &&
			   (name[4]=='D') &&
			   (name[5]== 0 ) &&
			   (name[6]== 0 ) &&
			   (name[7]== 0 ) );
	lax    = ( (!result) &&
			   (name[0]=='S') &&
			   (name[1]=='S') &&
			   (name[2]=='_') &&
			   (name[3]=='E') &&
			   (name[4]=='N') &&
			   (name[5]=='D') &&
			   (name[6]== 0 ) &&
			   (name[7]== 0 ) );
	if (lax) {
		if (!lax_list_syntax)
			I_Error("W_IsS_END: flag SS_END encountered in sprites list");

		//fix up flag to standard syntax
		result=1;
		name[0]='S';
		name[1]='_';
		name[2]='E';
		name[3]='N';
		name[4]='D';
		name[5]= 0 ;
		name[6]= 0 ;
		name[7]= 0 ;
	}

	return result;
}

int W_IsF_START(char *name)
//
// Is the name a flat list start flag?
// If lax syntax match, fix up to standard syntax.
//
{
	int result,lax;

	result = ( (name[0]=='F') &&
			   (name[1]=='_') &&
			   (name[2]=='S') &&
			   (name[3]=='T') &&
			   (name[4]=='A') &&
			   (name[5]=='R') &&
			   (name[6]=='T') &&
			   (name[7]== 0 ) );
	lax    =   ( (!result) &&
			   (name[0]=='F') &&
			   (name[1]=='F') &&
			   (name[2]=='_') &&
			   (name[3]=='S') &&
			   (name[4]=='T') &&
			   (name[5]=='A') &&
			   (name[6]=='R') &&
			   (name[7]=='T') );
	if (lax) {
		if (!lax_list_syntax)
			I_Error("W_IsF_START: flag FF_START encountered in flats list");

		//fix up flag to standard syntax
		result=1;
		name[0]='F';
		name[1]='_';
		name[2]='S';
		name[3]='T';
		name[4]='A';
		name[5]='R';
		name[6]='T';
		name[7]= 0 ;
	}

	return result;
}

int W_IsF_END(char *name)
//
// Is the name a flat list end flag?
// If lax syntax match, fix up to standard syntax.
//
{
  int result,lax;

	result = ( (name[0]=='F') &&
			   (name[1]=='_') &&
			   (name[2]=='E') &&
			   (name[3]=='N') &&
			   (name[4]=='D') &&
			   (name[5]== 0 ) &&
			   (name[6]== 0 ) &&
			   (name[7]== 0 ) );
	lax    = ( (!result) &&
			   (name[0]=='F') &&
			   (name[1]=='F') &&
			   (name[2]=='_') &&
			   (name[3]=='E') &&
			   (name[4]=='N') &&
			   (name[5]=='D') &&
			   (name[6]== 0 ) &&
			   (name[7]== 0 ) );
	if (lax) {
		if (!lax_list_syntax)
			I_Error("W_IsF_END: flag FF_END encountered in flats list");

		//fix up flag to standard syntax
		result=1;
		name[0]='F';
		name[1]='_';
		name[2]='E';
		name[3]='N';
		name[4]='D';
		name[5]= 0 ;
		name[6]= 0 ;
		name[7]= 0 ;
	}

	return result;
}

int W_IsP_START(char *name)
//
// Is the name a patch list start flag?
// If lax syntax match, fix up to standard syntax.
//
{
	int result,lax;

	result = ( (name[0]=='P') &&
			   (name[1]=='_') &&
			   (name[2]=='S') &&
			   (name[3]=='T') &&
			   (name[4]=='A') &&
			   (name[5]=='R') &&
			   (name[6]=='T') &&
			   (name[7]== 0 ) );
	lax    = ( (!result) &&
			   (name[0]=='P') &&
			   (name[1]=='P') &&
			   (name[2]=='_') &&
			   (name[3]=='S') &&
			   (name[4]=='T') &&
			   (name[5]=='A') &&
			   (name[6]=='R') &&
			   (name[7]=='T') );
	if (lax) {
		if (!lax_list_syntax)
			I_Error("W_IsP_START: flag PP_START encountered in patches list");

		//fix up flag to standard syntax
		result=1;
		name[0]='P';
		name[1]='_';
		name[2]='S';
		name[3]='T';
		name[4]='A';
		name[5]='R';
		name[6]='T';
		name[7]= 0 ;
	}

	return result;
}

int W_IsP_END(char *name)
//
// Is the name a patches list end flag?
// If lax syntax match, fix up to standard syntax.
//
{
	int result,lax;

	result = ( (name[0]=='P') &&
			   (name[1]=='_') &&
			   (name[2]=='E') &&
			   (name[3]=='N') &&
			   (name[4]=='D') &&
			   (name[5]== 0 ) &&
			   (name[6]== 0 ) &&
			   (name[7]== 0 ) );
	lax    = ( (!result) &&
			   (name[0]=='P') &&
			   (name[1]=='P') &&
			   (name[2]=='_') &&
			   (name[3]=='E') &&
			   (name[4]=='N') &&
			   (name[5]=='D') &&
			   (name[6]== 0 ) &&
			   (name[7]== 0 ) );
	if (lax) {
		if (!lax_list_syntax)
			I_Error("W_IsP_END: flag PP_END encountered in patches list");

		//fix up flag to standard syntax
		result=1;
		name[0]='P';
		name[1]='_';
		name[2]='E';
		name[3]='N';
		name[4]='D';
		name[5]= 0 ;
		name[6]= 0 ;
		name[7]= 0 ;
	}

	return result;
}

int W_IsNullName(char *name)
//
// Return 1 if the name is null
//
{
	return ( (name[0]==0) &&
			 (name[1]==0) &&
			 (name[2]==0) &&
			 (name[3]==0) &&
			 (name[4]==0) &&
			 (name[5]==0) &&
			 (name[6]==0) &&
			 (name[7]==0) );
}

int W_IsBadSpriteFlag(char *name)
//
// Return 1 if the name is one of the redundant sprite list flags S1_START,
// S1_END, S2_START or S2_END. These are oft-encountered hacks that some
// editors need and are not recognised or needed by Doom.
//
{
	return ( ((name[0]=='S') &&
			  (name[1]=='1') &&
			  (name[2]=='_') &&
			  (name[3]=='S') &&
			  (name[4]=='T') &&
			  (name[5]=='A') &&
			  (name[6]=='R') &&
			  (name[7]=='T'))
			||
			 ((name[0]=='S') &&
			  (name[1]=='1') &&
			  (name[2]=='_') &&
			  (name[3]=='E') &&
			  (name[4]=='N') &&
			  (name[5]=='D') &&
			  (name[6]== 0 ) &&
			  (name[7]== 0 ))
			||
			 ((name[0]=='S') &&
			  (name[1]=='2') &&
			  (name[2]=='_') &&
			  (name[3]=='S') &&
			  (name[4]=='T') &&
			  (name[5]=='A') &&
			  (name[6]=='R') &&
			  (name[7]=='T'))
			||
			 ((name[0]=='S') &&
			  (name[1]=='2') &&
			  (name[2]=='_') &&
			  (name[3]=='E') &&
			  (name[4]=='N') &&
			  (name[5]=='D') &&
			  (name[6]== 0 ) &&
			  (name[7]== 0 )) );
}

int W_IsBadFlatFlag(char *name)
//
// Return 1 if the name is one of the redundant flat list flags F1_START,
// F1_END, F2_START or F2_END. These are oft-encountered hacks that some
// editors need and are not recognised or needed by Doom.
//
{
	return ( ((name[0]=='F') &&
			  (name[1]=='1') &&
			  (name[2]=='_') &&
			  (name[3]=='S') &&
			  (name[4]=='T') &&
			  (name[5]=='A') &&
			  (name[6]=='R') &&
			  (name[7]=='T'))
			||
			 ((name[0]=='F') &&
			  (name[1]=='1') &&
			  (name[2]=='_') &&
			  (name[3]=='E') &&
			  (name[4]=='N') &&
			  (name[5]=='D') &&
			  (name[6]== 0 ) &&
			  (name[7]== 0 ))
			||
			 ((name[0]=='F') &&
			  (name[1]=='2') &&
			  (name[2]=='_') &&
			  (name[3]=='S') &&
			  (name[4]=='T') &&
			  (name[5]=='A') &&
			  (name[6]=='R') &&
			  (name[7]=='T'))
			||
			 ((name[0]=='F') &&
			  (name[1]=='2') &&
			  (name[2]=='_') &&
			  (name[3]=='E') &&
			  (name[4]=='N') &&
			  (name[5]=='D') &&
			  (name[6]== 0 ) &&
			  (name[7]== 0 )) );
}

int W_IsBadPatchFlag(char *name)
//
// Return 1 if the name is one of the redundant patch list flags P1_START,
// P1_END, P2_START or P2_END. These are oft-encountered hacks that some
// editors need and are not recognised or needed by Doom.
//
{
	return ( ((name[0]=='P') &&
			  (name[1]=='1') &&
			  (name[2]=='_') &&
			  (name[3]=='S') &&
			  (name[4]=='T') &&
			  (name[5]=='A') &&
			  (name[6]=='R') &&
			  (name[7]=='T'))
			||
			 ((name[0]=='P') &&
			  (name[1]=='1') &&
			  (name[2]=='_') &&
			  (name[3]=='E') &&
			  (name[4]=='N') &&
			  (name[5]=='D') &&
			  (name[6]== 0 ) &&
			  (name[7]== 0 ))
			||
			 ((name[0]=='P') &&
			  (name[1]=='2') &&
			  (name[2]=='_') &&
			  (name[3]=='S') &&
			  (name[4]=='T') &&
			  (name[5]=='A') &&
			  (name[6]=='R') &&
			  (name[7]=='T'))
			||
			 ((name[0]=='P') &&
			  (name[1]=='2') &&
			  (name[2]=='_') &&
			  (name[3]=='E') &&
			  (name[4]=='N') &&
			  (name[5]=='D') &&
			  (name[6]== 0 ) &&
			  (name[7]== 0 )) );
}

void W_FindAndDeleteLump(
	lumpinfo_t* first,		/* first lump in list - stop when get to it */
	lumpinfo_t* lump_p,		/* lump just after one to start at			*/
	char *name)				/* name of lump to remove if found			*/
//
// Find lump by name, starting before specifed pointer.
// Overwrite name with nulls if found. This is used to remove
// the originals of sprite entries duplicated in a PWAD, the
// sprite code doesn't like two sprite lumps of the same name
// existing in the sprites list. It may also speed things up
// slightly where flats and patches are concerned.
//
{
	union {
		char	s[9];
		int		x[2];
	} name8;

	int			v1;
	int			v2;

	// make the name into two integers for easy compares
	strncpy (name8.s,name,8);

	// in case the name was a full 8 chars
	name8.s[8] = 0;

	v1 = name8.x[0];
	v2 = name8.x[1];

	do {
		lump_p--;
	} while ((lump_p != first)
			 && ((*(int *)lump_p->name != v1)
				 || (*(int *)&lump_p->name[4] != v2)));

	if (   (*(int *)lump_p->name == v1)
		&& (*(int *)&lump_p->name[4] == v2))
		  memset(lump_p,0,sizeof(lumpinfo_t));
}

void W_GroupList(int (*startfunc)(char *name),
				 int (*endfunc)(char *name),
				 int (*badfunc)(char *name),
				 char *listtype)
//
//	Group the sprite/flat/patch list of the various WADs (including the
//	IWAD) into one. The supplied functions each specify one of the start,
//	end and redundant (bad) flag-recognising functions defined previously.
//
{
	int newnumlumps=0;
	lumpinfo_t* lumpinfo_copy;
	lumpinfo_t* lump_s;
	lumpinfo_t* lump_d; /* dest, source */
	lumpinfo_t* lump_t; /* temp */

	lumpinfo_copy=malloc(numlumps*sizeof(lumpinfo_t));
	if (!lumpinfo_copy)
		I_Error ("Couldn't malloc lumpinfo_copy");

	lump_s = lumpinfo;
	lump_d = lumpinfo_copy;

	/* skip to first s_start flag */
	while ( (lump_s < lumpinfo+numlumps)
		   &&
			(!(*startfunc)(lump_s->name)) ) lump_s++;
	/* copy the first s_start */
	memcpy(lump_d++,lump_s,sizeof(lumpinfo_t));
	/* prepare for loop below to skip over imaginary s_end */
	lump_d++;
	do
	{ /* skip through entries */
		if (!(*startfunc)(lump_s->name))
		{
			lump_s++;
		}
		else
		{
			lump_d--; /* skip back to overwrite previous s_end */
			memset(lump_s++,0,sizeof(lumpinfo_t)); /* zap S_START, go to next */
			/* copy rest of this sprite group, including the s_end */
			do {
				/* for each sprite, remove the original if it exists */
				W_FindAndDeleteLump(lumpinfo_copy,lump_d,lump_s->name);
				/* copy it */
				memcpy(lump_d,lump_s,sizeof(lumpinfo_t));
				/* zap the lump in the original list */
				memset(lump_s++,0,sizeof(lumpinfo_t));
			} while (!(*endfunc)((lump_d++)->name));
		}
	} while (lump_s < lumpinfo+numlumps);

	/* now copy other, non-sprite entries */
	lump_s = lumpinfo;
/*	lump_d = at next free slot in lumpinfo_copy */
	while (lump_s < lumpinfo+numlumps)  /* MAJOR CHANGE: this is now a while loop */
	{ /* skip through entries */        /* instead of a DO loop */
		if (!W_IsNullName(lump_s->name))
		{
			memcpy(lump_d++,lump_s,sizeof(lumpinfo_t));
		}
		lump_s++;
	};

	/* now replace original lumpinfo, squeezing out blanked sprites */
/*	lump_d = at next "free slot" in lumpinfo_copy */
	lump_t=lumpinfo;
	lump_s=lumpinfo_copy;
	while (lump_s < lump_d) /* MINOR CHANGE: condition was (lump_s != lump_d) */
	{
		if ((!W_IsNullName(lump_s->name)) && (!(*badfunc)(lump_s->name)) )
		{
			newnumlumps++;
			memcpy(lump_t++,lump_s,sizeof(lumpinfo_t));
		}
		lump_s++;
	}
	Printf("Grouped %s: old numlumps=%d, new numlumps=%d\n",
			listtype,numlumps,newnumlumps);
//	getchar();
	numlumps=newnumlumps;
	free(lumpinfo_copy);
	realloc(lumpinfo,numlumps*sizeof(lumpinfo_t));
	if (!lumpinfo)
		I_Error ("Couldn't realloc lumpinfo");
}

#define strcmpi	stricmp

int W_filelength (int handle) 
{
	struct stat	fileinfo;

	if (fstat (handle,&fileinfo) == -1)
		I_Error ("Error fstating");

	return fileinfo.st_size;
}



//
// LUMP BASED ROUTINES.
//

//
// W_AddFile
// All files are optional, but at least one file must be
//  found (PWAD, if all required lumps are present).
// Files with a .wad extension are wadlink files
//  with multiple lumps.
// Other files are single lumps with the base filename
//  for the lump name.
//
// If filename starts with a tilde, the file is handled
//  specially to allow map reloads.
// But: the reload feature is a fragile hack...

int				reloadlump;
char*			reloadname;


void W_AddFile (char *filename)
{
	wadinfo_t		header;
	lumpinfo_t*		lump_p;
	unsigned		i;
	int				handle;
	int				length;
	int				startlump;
	filelump_t*		fileinfo;
	filelump_t		singleinfo;
	int				storehandle;

	// open the file and add to directory

	// handle reload indicator.
	if (filename[0] == '~')
	{
		filename++;
		reloadname = filename;
		reloadlump = numlumps;
	}
		
	if ( (handle = open (filename,O_RDONLY | O_BINARY)) == -1)
	{
		Printf (" couldn't open %s\n",filename);
		return;
	}

	FixPathSeperator (filename);
	Printf (" adding %s",filename);
	startlump = numlumps;
	
	if (strcmpi (filename+strlen(filename)-3 , "wad" ) )
	{
		char name[256];
		// single lump file
		fileinfo = &singleinfo;
		singleinfo.filepos = 0;
		singleinfo.size = LONG(W_filelength(handle));
		ExtractFileBase (filename, name);
		strupr (name);
		strncpy (singleinfo.name, name, 8);
		numlumps++;
	} else {
		// WAD file
		read (handle, &header, sizeof(header));
		if (strncmp(header.identification,"IWAD",4))
		{
			// Homebrew levels?
			if (strncmp(header.identification,"PWAD",4))
			{
			I_Error ("Wad file %s doesn't have IWAD "
				 "or PWAD id\n", filename);
			}
			
			// ???modifiedgame = true;
		}
		header.numlumps = LONG(header.numlumps);
		header.infotableofs = LONG(header.infotableofs);
		length = header.numlumps*sizeof(filelump_t);
		fileinfo = alloca (length);
		lseek (handle, header.infotableofs, SEEK_SET);
		read (handle, fileinfo, length);
		numlumps += header.numlumps;
		Printf (" (%d lumps)", header.numlumps);
	}
	Printf ("\n");

	// Fill in lumpinfo
	lumpinfo = realloc (lumpinfo, numlumps*sizeof(lumpinfo_t));

	if (!lumpinfo)
		I_Error ("Couldn't realloc lumpinfo");

	lump_p = &lumpinfo[startlump];
	
	storehandle = reloadname ? -1 : handle;
	
	for (i=startlump ; i<numlumps ; i++,lump_p++, fileinfo++)
	{
		lump_p->handle = storehandle;
		lump_p->position = LONG(fileinfo->filepos);
		lump_p->size = LONG(fileinfo->size);
		strncpy (lump_p->name, fileinfo->name, 8);
		if (group_sprites)
		{
			sprite_lists += (W_IsS_START(lump_p->name));
			sprite_lists += (W_IsS_END(lump_p->name));
		}
		if (group_flats)
		{
			flat_lists += (W_IsF_START(lump_p->name));
			flat_lists += (W_IsF_END(lump_p->name));
		}
		if (group_patches)
		{
			patch_lists += (W_IsP_START(lump_p->name));
			patch_lists += (W_IsP_END(lump_p->name));
		}
	}
	
	if (reloadname)
		close (handle);
}




//
// W_Reload
// Flushes any of the reloadable lumps in memory
//  and reloads the directory.
//
void W_Reload (void)
{
	wadinfo_t		header;
	int				lumpcount;
	lumpinfo_t*		lump_p;
	unsigned		i;
	int				handle;
	int				length;
	filelump_t*		fileinfo;
	
	if (!reloadname)
		return;

	if ( (handle = open (reloadname,O_RDONLY | O_BINARY)) == -1)
		I_Error ("W_Reload: couldn't open %s",reloadname);

	read (handle, &header, sizeof(header));
	lumpcount = LONG(header.numlumps);
	header.infotableofs = LONG(header.infotableofs);
	length = lumpcount*sizeof(filelump_t);
	fileinfo = alloca (length);
	lseek (handle, header.infotableofs, SEEK_SET);
	read (handle, fileinfo, length);

	// Fill in lumpinfo
	lump_p = &lumpinfo[reloadlump];
	
	for (i=reloadlump ;
	 i<reloadlump+lumpcount ;
	 i++,lump_p++, fileinfo++)
	{
		if (lumpcache[i])
			Z_Free (lumpcache[i]);

		lump_p->position = LONG(fileinfo->filepos);
		lump_p->size = LONG(fileinfo->size);
	}
	
	close (handle);
}





//
// W_InitMultipleFiles
// Pass a null terminated list of files to use.
// All files are optional, but at least one file
//  must be found.
// Files with a .wad extension are idlink files
//  with multiple lumps.
// Other files are single lumps with the base filename
//  for the lump name.
// Lump names can appear multiple times.
// The name searcher looks backwards, so a later file
//  does override all earlier ones.
//
void W_InitMultipleFiles (char** filenames)
{
	int		size;

	// command line options for grouping sprites, flats & patches
	if (M_CheckParm("-gfixsfp"))
	{						//turn on basic list group options
		group_sprites=1;
		group_flats=1;
		group_patches=1;
	}
	if (M_CheckParm("-gfixall"))
	{
		group_sprites=1;	//turn on ALL list group options
		group_flats=1;
		group_patches=1;
		lax_sprite_rot=1;
	}
	group_sprites=(group_sprites || (M_CheckParm("-gfixspr")));
	group_flats=(group_flats || (M_CheckParm("-gfixflt")));
	group_patches=(group_patches || (M_CheckParm("-gfixpat")));
	lax_list_syntax=(lax_list_syntax && (!M_CheckParm("-gstrict")));
	lax_sprite_rot=(lax_sprite_rot || (M_CheckParm("-gignrot")));

	// open all the files, load headers, and count lumps
	numlumps = 0;

	// will be realloced as lumps are added
	lumpinfo = malloc(1);	

	for ( ; *filenames ; filenames++)
		W_AddFile (*filenames);

	if (!numlumps)
		I_Error ("W_InitFiles: no files found");

	// check for errors
	if ((sprite_lists %2) && group_sprites)
		I_Error("W_InitMultipleFiles: sprite begin..end flags don't match");
	if ((flat_lists %2)   && group_flats)
		I_Error("W_InitMultipleFiles: flat begin..end flags don't match");
	if ((patch_lists %2)  && group_patches)
		I_Error("W_InitMultipleFiles: patch begin..end flags don't match");

	//group lists
	if ((sprite_lists >2) && group_sprites)
		W_GroupList(&W_IsS_START,&W_IsS_END,&W_IsBadSpriteFlag,"sprites");
	if ((flat_lists >2)   && group_flats)
		W_GroupList(&W_IsF_START,&W_IsF_END,&W_IsBadFlatFlag,"flats");
	if ((patch_lists >2)  && group_patches)
		W_GroupList(&W_IsP_START,&W_IsP_END,&W_IsBadPatchFlag,"patches");
	if (lax_sprite_rot)
		Printf("Disengaging sprite rotation/limit checks\n");

	// set up caching
	size = numlumps * sizeof(*lumpcache);
	lumpcache = malloc (size);

	if (!lumpcache)
		I_FatalError ("Couldn't allocate lumpcache");

	memset (lumpcache, 0, size);
}




//
// W_InitFile
// Just initialize from a single file.
//
void W_InitFile (char* filename)
{
	char*	names[2];

	names[0] = filename;
	names[1] = NULL;
	W_InitMultipleFiles (names);
}



//
// W_NumLumps
//
int W_NumLumps (void)
{
	return numlumps;
}



//
// W_CheckNumForName
// Returns -1 if name not found.
//

int W_CheckNumForName (char* name)
{
	union {
		char	s[9];
		int		x[2];
	} name8;

	int			v1;
	int			v2;
	lumpinfo_t*	lump_p;

	// make the name into two integers for easy compares
	strncpy (name8.s,name,8);

	// in case the name was a full 8 chars
	name8.s[8] = 0;

	// case insensitive
	strupr (name8.s);

	v1 = name8.x[0];
	v2 = name8.x[1];


	// scan backwards so patch lump files take precedence
	lump_p = lumpinfo + numlumps;

	while (lump_p-- != lumpinfo)
	{
		if ( *(int *)lump_p->name == v1
			 && *(int *)&lump_p->name[4] == v2)
		{
			return lump_p - lumpinfo;
		}
	}

	// TFB. Not found.
	return -1;
}




//
// W_GetNumForName
// Calls W_CheckNumForName, but bombs out if not found.
//
int W_GetNumForName (char* name)
{
	int	i;

	i = W_CheckNumForName (name);

	if (i == -1)
		I_Error ("W_GetNumForName: %s not found!", name);

	return i;
}


//
// W_LumpLength
// Returns the buffer size needed to load the given lump.
//
int W_LumpLength (int lump)
{
	if (lump >= numlumps)
		I_Error ("W_LumpLength: %i >= numlumps",lump);

	return lumpinfo[lump].size;
}



//
// W_ReadLump
// Loads the lump into the given buffer,
//  which must be >= W_LumpLength().
//
void
W_ReadLump
( int		lump,
  void*		dest )
{
	int			c;
	lumpinfo_t*	l;
	int			handle;
	
	if (lump >= numlumps)
		I_Error ("W_ReadLump: %i >= numlumps",lump);

	l = lumpinfo+lump;
	
	// ??? I_BeginRead ();
	
	if (l->handle == -1)
	{
		// reloadable file, so use open / read / close
		if ( (handle = open (reloadname,O_RDONLY | O_BINARY)) == -1)
			I_Error ("W_ReadLump: couldn't open %s",reloadname);
	}
	else
		handle = l->handle;

		lseek (handle, l->position, SEEK_SET);
		c = read (handle, dest, l->size);

		if (c < l->size)
		I_Error ("W_ReadLump: only read %i of %i on lump %i",
			 c,l->size,lump);	

		if (l->handle == -1)
			close (handle);

	// ??? I_EndRead ();
}




//
// W_CacheLumpNum
//
void*
W_CacheLumpNum
( int		lump,
  int		tag )
{
	byte*	ptr;

	if ((unsigned)lump >= numlumps)
		I_Error ("W_CacheLumpNum: %i >= numlumps",lump);

	if (!lumpcache[lump])
	{
		// read the lump in

		//Printf ("cache miss on lump %i\n",lump);
		ptr = Z_Malloc (W_LumpLength (lump), tag, &lumpcache[lump]);
		W_ReadLump (lump, lumpcache[lump]);
	}
	else
	{
		//Printf ("cache hit on lump %i\n",lump);
		Z_ChangeTag (lumpcache[lump],tag);
	}

	return lumpcache[lump];
}



//
// W_CacheLumpName
//
void*
W_CacheLumpName
( char*		name,
  int		tag )
{
	return W_CacheLumpNum (W_GetNumForName(name), tag);
}


//
// W_Profile
//
int		info[2500][10];
int		profilecount;

void W_Profile (void)
{
	int			i;
	memblock_t*	block;
	void*		ptr;
	char		ch;
	FILE*		f;
	int			j;
	char		name[9];
	
	
	for (i=0 ; i<numlumps ; i++)
	{
		ptr = lumpcache[i];
		if (!ptr)
		{
			ch = ' ';
			continue;
		}
		else
		{
			block = (memblock_t *) ( (byte *)ptr - sizeof(memblock_t));
			if (block->tag < PU_PURGELEVEL)
				ch = 'S';
			else
				ch = 'P';
		}
		info[i][profilecount] = ch;
	}
	profilecount++;
	
	f = fopen ("waddump.txt","w");
	name[8] = 0;

	for (i=0 ; i<numlumps ; i++)
	{
		memcpy (name,lumpinfo[i].name,8);

		for (j=0 ; j<8 ; j++)
			if (!name[j])
				break;

		for ( ; j<8 ; j++)
			name[j] = ' ';

		fprintf (f,"%s ",name);

		for (j=0 ; j<profilecount ; j++)
			fprintf (f,"    %c",info[i][j]);

		fprintf (f,"\n");
	}
	fclose (f);
}


