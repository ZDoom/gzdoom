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
// $Log:$
//
// DESCRIPTION:
//		Handles WAD file header, directory, lump I/O.
//
//-----------------------------------------------------------------------------


static const char
rcsid[] = "$Id: w_wad.c,v 1.5 1997/02/03 16:47:57 b1 Exp $";


#if defined(NORMALUNIX)
#include <ctype.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <alloca.h>
#define O_BINARY				0
#elif defined(_WIN32)
#include <ctype.h>
#include <sys/types.h>
#include <string.h>
#include <malloc.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#endif

#include "doomtype.h"
#include "doomdef.h"
#include "m_swap.h"
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
lumpinfo_t* 		lumpinfo;				
int 				numlumps;

void**				lumpcache;



//
// LUMP BASED ROUTINES.
//

//
// W_AddFile
// All files are optional, but at least one file must be
//	found (PWAD, if all required lumps are present).
// Files with a .wad extension are wadlink files
//	with multiple lumps.
// Other files are single lumps with the base filename
//	for the lump name.
//
// If filename starts with a tilde, the file is handled
//	specially to allow map reloads.
// But: the reload feature is a fragile hack...

int 				reloadlump;
char*				reloadname;


void W_AddFile (char *filename)
{
	wadinfo_t			header;
	lumpinfo_t* 		lump_p;
	unsigned			i;
	FILE			   *handle;
	int 				length;
	int 				startlump;
	packfile_t* 		fileinfo;
	packfile_t			singleinfo;
	FILE			   *storehandle;
	
	// open the file and add to directory

	// handle reload indicator.
	if (filename[0] == '~')
	{
		filename++;
		reloadname = filename;
		reloadlump = numlumps;
	}
				
	if ((handle = fopen (filename, "rb")) == NULL)
	{
		Printf (" couldn't open %s\n",filename);
		return;
	}

	Printf (" adding %s",filename);
	startlump = numlumps;
		
	if (strcmpi (filename+strlen(filename)-3 , "wad" ) )
	{
		char name[1024];

		Printf ("\n");
		// single lump file
		fileinfo = &singleinfo;
		singleinfo.filepos = 0;
		singleinfo.filelen = LONG(Q_filelength(handle));
		FixPathSeperator (filename);
		ExtractFileBase (filename, name);
		strncpy (singleinfo.name, name, 55);
		singleinfo.name[55] = 0;
		numlumps++;
	}
	else 
	{
		filelump_t *wadlumps;

		// WAD file
		fread (&header, sizeof(header), 1, handle);
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
		wadlumps = malloc (length);
		fileinfo = alloca (header.numlumps * sizeof(packfile_t));
		fseek (handle, header.infotableofs, SEEK_SET);
		fread (wadlumps, 1, length, handle);
		numlumps += header.numlumps;
		for (i = 0; i < header.numlumps; i++) {
			strncpy (fileinfo[i].name, wadlumps[i].name, 8);
			fileinfo[i].name[8] = 0;
			fileinfo[i].filepos = wadlumps[i].filepos;
			fileinfo[i].filelen = wadlumps[i].size;
		}
		free (wadlumps);

		Printf (" (%d lumps)\n", header.numlumps);
	}

	
	// Fill in lumpinfo
	lumpinfo = realloc (lumpinfo, numlumps*sizeof(lumpinfo_t));

	if (!lumpinfo)
		I_Error ("Couldn't realloc lumpinfo");

	lump_p = &lumpinfo[startlump];
		
	storehandle = reloadname ? NULL : handle;
		
	for (i=startlump ; (int)i<numlumps ; i++,lump_p++, fileinfo++)
	{
		lump_p->handle = storehandle;
		lump_p->position = LONG(fileinfo->filepos);
		lump_p->size = LONG(fileinfo->filelen);
		strcpy (lump_p->name, fileinfo->name);
	}
		
	if (reloadname)
		fclose (handle);
}




//
// W_Reload
// Flushes any of the reloadable lumps in memory
//	and reloads the directory.
//
void W_Reload (void)
{
	wadinfo_t			header;
	int 				lumpcount;
	lumpinfo_t* 		lump_p;
	unsigned			i;
	FILE			   *handle;
	int 				length;
	packfile_t* 		fileinfo;
	filelump_t*			wadlumps;
		
	if (!reloadname)
		return;
				
	if ((handle = fopen (reloadname, "rb")) == NULL)
		I_Error ("W_Reload: couldn't open %s",reloadname);

	fread (&header, sizeof(header), 1, handle);
	lumpcount = LONG(header.numlumps);
	header.infotableofs = LONG(header.infotableofs);
	length = lumpcount * sizeof(filelump_t);
	wadlumps = malloc (length);
	fileinfo = alloca (header.numlumps * sizeof(packfile_t));
	fseek (handle, header.infotableofs, SEEK_SET);
	fread (wadlumps, 1, length, handle);
	numlumps += header.numlumps;
	for (i = 0; i < header.numlumps; i++) {
		strncpy (fileinfo[i].name, wadlumps[i].name, 8);
		fileinfo[i].name[8] = 0;
		fileinfo[i].filepos = wadlumps[i].filepos;
		fileinfo[i].filelen = wadlumps[i].size;
	}
	free (wadlumps);
	
	// Fill in lumpinfo
	lump_p = &lumpinfo[reloadlump];
		
	for (i=reloadlump ;
		 (int)i<reloadlump+lumpcount ;
		 i++,lump_p++, fileinfo++)
	{
		if (lumpcache[i])
			Z_Free (lumpcache[i]);

		lump_p->position = LONG(fileinfo->filepos);
		lump_p->size = LONG(fileinfo->filelen);
	}
		
	fclose (handle);
}

/*

   LEGAL STUFF FOR MY OWN PROTECTION:

   This modification is for use only in accordance with any applicable
   copyright or licence agreement, in particular the Doom source code licence
   that was provided by iD Software when they released the source code.

   This code is experimental; I have solved the memory leak problems present in the
   previous version and I have had no problems with it. However, because I am doing this
   for a hobby and not professionally, I cannot GUARANTEE that it will not crash your
   system, unlikely as that may be, SO IF YOU USE THIS CODE DIRECTLY YOU DO SO ****AT 
   YOUR OWN RISK**** IT IS A CONDITION OF USE THAT YOU ACCEPT THIS RISK; IF YOU ARE
   NOT WILLING TO ACCEPT THE RISK,YOU ARE NOT PERMITTED TO USE THIS CODE.

                          YOU HAVE BEEN WARNED!!!!!

   This information is provided in good faith based on personal experience,
   knowledge or belief, so "errors and omissions excepted" is the rule here;
   in other words, like most facilities provided free as being potentially
   helpful to like-minded people it may be incorrect or incomplete.

   Martin Howe, 26th January, 1998. All trademarks acknowledged.

THE FOLLOWING STUFF SHOULD BE INSERTED INTO W_WAD.C JUST BEFORE
THE ROUTINE W_InitMultipleFiles AND A CALL TO W_GroupSprites() SHOULD BE
INSERTED IN W_InitMultipleFiles AS THE LAST ACTION BEFORE RETURNING.
The word CHANGE in uppercase flags where the changes (only two) have been made;
only W_GroupSprites() is changed.

TO DO:
   * This code could be more efficient.
     The code that reads in a file should set a flag to indicate if a PWAD
     has sprites - this would mean not having to call W_GroupSprites() when
     unnecessary.

   * Although not strictly necessary, the same should probably be done for
     flats and patches; there must be a reason why the Doom code assumes that
     flats and patches only have one list each like sprites do.

   * A similar function for PNAMES and TEXTUREn list merging would also make
     life easier for PWAD developers and make using multiple PWADS that have
     walls and doors easier to use together (ie., without having to merge them
      with DeuTex or similar

   * Ensure that other instances in the program where WAD files are loaded
     are covered. I don't _think_ this happens with normal game play, but I am
     not 100% certain of that.

   * Support PWADS that use SS_START and SS_END rather than S_START and S_END.
     Ditto for PP_START/END and FF_START/END if applicable.
   */

int W_IsS_START(lumpinfo_t* lump_p)
{
	return !stricmp (lump_p->name, "S_START");
}

int W_IsS_END(lumpinfo_t* lump_p)
{
	return !stricmp (lump_p->name, "S_END");
}

int W_IsNULLNAME(lumpinfo_t* lump_p)
{
	return !lump_p->name[0];
}

void W_FindAndDeleteLump(
   lumpinfo_t* first,    /* first lump in list - stop when get to it */
   lumpinfo_t* lump_p,   /* lump just after one to start at          */
   char *name)           /* name of lump to remove if found          */
//
// Find lump by name, starting before specifed pointer.
// Overwrite name with nulls if found. This is used to remove
// the originals of sprite entries duplicated in a PWAD, the
// sprite code doesn't like two sprite lumps of the same name
// existing in the sprites list.
//
{
	do {
		lump_p--;
	} while ((lump_p != first) && stricmp (lump_p->name, name));

	if (!stricmp (lump_p->name, name))
		  memset(lump_p,0,sizeof(lumpinfo_t));
}

void W_GroupSprites(void)
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
	while ( (lump_s < lumpinfo+numlumps) && (!W_IsS_START(lump_s)) )
		lump_s++;

	/* copy the first s_start */
	memcpy(lump_d++,lump_s,sizeof(lumpinfo_t));

	/* prepare for loop below to skip over imaginary s_end */
	lump_d++;
	do
	{ /* skip through entries */
		if (!W_IsS_START(lump_s))
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
			} while (!W_IsS_END(lump_d++));
		}
	} while (lump_s < lumpinfo+numlumps);

	/* now copy other, non-sprite entries */
	lump_s = lumpinfo;
/*  lump_d = at next free slot in lumpinfo_copy */
	while (lump_s < lumpinfo+numlumps)  /* MAJOR CHANGE: this is now a while loop */
	{ /* skip through entries */        /* instead of a DO loop */
		if (!W_IsNULLNAME(lump_s))
		{
			memcpy(lump_d++,lump_s,sizeof(lumpinfo_t));
		}
		lump_s++;
	};

	/* now replace original lumpinfo, squeezing out blanked sprites */
/*  lump_d = at next "free slot" in lumpinfo_copy */
	lump_t=lumpinfo;
	lump_s=lumpinfo_copy;
	while (lump_s < lump_d) /* MINOR CHANGE: condition was (lump_s != lump_d) */
	{ 
		if (!W_IsNULLNAME(lump_s))
		{
			newnumlumps++;
			memcpy(lump_t++,lump_s,sizeof(lumpinfo_t));
		}
		lump_s++;
	}
	Printf("ONL=%d NNL=%d\n",numlumps,newnumlumps);
//    getchar();
	numlumps=newnumlumps;
	free(lumpinfo_copy);
	realloc(lumpinfo,numlumps*sizeof(lumpinfo_t));
	if (!lumpinfo)
		I_Error ("Couldn't realloc lumpinfo");
}


//
// W_InitMultipleFiles
// Pass a null terminated list of files to use.
// All files are optional, but at least one file
//	must be found.
// Files with a .wad extension are idlink files
//	with multiple lumps.
// Other files are single lumps with the base filename
//	for the lump name.
// Lump names can appear multiple times.
// The name searcher looks backwards, so a later file
//	does override all earlier ones.
//
void W_InitMultipleFiles (char** filenames)
{
	int size;
	
	// open all the files, load headers, and count lumps
	numlumps = 0;

	// will be realloced as lumps are added
	lumpinfo = malloc(1);		

	for ( ; *filenames ; filenames++)
		W_AddFile (*filenames);

	if (!numlumps)
		I_Error ("W_InitFiles: no files found");
	
	// set up caching
	size = numlumps * sizeof(*lumpcache);
	lumpcache = malloc (size);
	
	if (!lumpcache)
		I_Error ("Couldn't allocate lumpcache");

	memset (lumpcache, 0, size);

	W_GroupSprites ();
}




//
// W_InitFile
// Just initialize from a single file.
//
void W_InitFile (char* filename)
{
	char *names[2];

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
	lumpinfo_t* lump_p;

	// scan backwards so patch lump files take precedence
	lump_p = lumpinfo + numlumps;

	while (lump_p-- != lumpinfo)
	{
		// [RH] Lump names can now be up to 55 chars internally.
		// I'd rather do a normal string compare than compare two ints.
		if (!stricmp (lump_p->name, name))
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
int W_GetNumForName (char *name)
{
	int i;

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
//	which must be >= W_LumpLength().
//
void W_ReadLump (int lump, void *dest)
{
	int 		c;
	lumpinfo_t *l;
	FILE	   *handle;
		
	if (lump >= numlumps)
		I_Error ("W_ReadLump: %i >= numlumps",lump);

	l = lumpinfo+lump;
		
	// ??? I_BeginRead ();
		
	if (l->handle == NULL)
	{
		// reloadable file, so use open / read / close
		if ((handle = fopen (reloadname, "rb")) == NULL)
			I_Error ("W_ReadLump: couldn't open %s",reloadname);
	}
	else
		handle = l->handle;
				
	fseek (handle, l->position, SEEK_SET);
	c = fread (dest, 1, l->size, handle);

	if (c < l->size)
		I_Error ("W_ReadLump: only read %i of %i on lump %i",
				 c,l->size,lump);		

	if (l->handle == NULL)
		fclose (handle);
				
	// ??? I_EndRead ();
}




//
// W_CacheLumpNum
//
void *W_CacheLumpNum (int lump, int tag)
{
	byte *ptr;

	if ((unsigned)lump >= (unsigned)numlumps)
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
void *W_CacheLumpName (char *name, int tag)
{
	return W_CacheLumpNum (W_GetNumForName(name), tag);
}


//
// W_Profile
//
int 			info[2500][10];
int 			profilecount;

void W_Profile (void)
{
	int 		i;
	memblock_t* block;
	void*		ptr;
	char		ch;
	FILE*		f;
	int 		j;

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

	f = fopen ("waddump.txt", "w");

	for (i=0 ; i<numlumps ; i++)
	{
		fprintf (f, "%55s ", lumpinfo[i].name);

		for (j=0 ; j<profilecount ; j++)
			fprintf (f,"    %c",info[i][j]);

		fprintf (f,"\n");
	}
	fclose (f);
}


