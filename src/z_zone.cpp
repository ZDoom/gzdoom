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
//		Zone Memory Allocation. Neat.
//
//-----------------------------------------------------------------------------

#include <stddef.h>
#include <stdlib.h>
#include <malloc.h>

#include "z_zone.h"
#include "i_system.h"
#include "doomdef.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "stats.h"


/*
==============================================================================

						ZONE MEMORY ALLOCATION

There is never any space between memblocks, and there will never be two
contiguous free memblocks.

The rover can be left pointing at a non-empty block

It is of no value to free a cachable block, because it will get overwritten
automatically if needed

==============================================================================
*/

#define ZONEID		0x1d4a11

typedef struct
{
	size_t		size;		// total bytes malloced, including header
	memblock_t	blocklist;	// start / end cap for linked list
	memblock_t	*rover;
} memzone_t;

CVAR (Float, heapsize, 8, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)


static memzone_t *mainzone;
static int PurgeCount;
static size_t PurgeSize;

//
// Z_ClearZone
//
/*
void Z_ClearZone (memzone_t* zone)
{
	memblock_t *block;
		
	// set the entire zone to one free block
	zone->blocklist.next =
		zone->blocklist.prev =
		block = (memblock_t *)( (byte *)zone + sizeof(memzone_t) );
	
	zone->blocklist.user = (void *)zone;
	zone->blocklist.tag = PU_STATIC;
	zone->rover = block;
		
	block->prev = block->next = &zone->blocklist;
	
	// NULL indicates a free block.
	block->user = NULL; 

	block->size = zone->size - sizeof(memzone_t);
}
*/


static void STACK_ARGS Z_Close (void)
{
	free (mainzone);
	mainzone = NULL;
}

#include "m_argv.h"
//
// Z_Init
//
void Z_Init (void)
{
	memblock_t *block;
	size_t zonesize;

    char *p = Args.CheckValue ("-heapsize");
    if (p != NULL)
		zonesize = (size_t)(atof (p)*1024*1024);
	else
		zonesize = (size_t)(heapsize*1024*1024);

	mainzone = (memzone_t *)I_ZoneBase (&zonesize);
	mainzone->size = zonesize;

// set the entire zone to one free block

	mainzone->blocklist.next = mainzone->blocklist.prev = block =
		(memblock_t *)( (byte *)mainzone + sizeof(memzone_t) );
	mainzone->blocklist.user = (void **)mainzone;
	mainzone->blocklist.tag = PU_STATIC;
	mainzone->rover = block;
		
	block->prev = block->next = &mainzone->blocklist;
	block->user = NULL; // NULL indicates a free block.
	block->size = mainzone->size - sizeof(memzone_t);

	atterm (Z_Close);
}


//
// Z_Free
//
void Z_Free (void *ptr)
{
	memblock_t *block, *other;

//#ifdef _DEBUG
//	Z_CheckHeap ();
//#endif

	if (ptr == NULL)
	{ // [RH] Allow NULL pointers
		return;
	}

	block = (memblock_t *) ( (byte *)ptr - sizeof(memblock_t));

	if (block->id != ZONEID)
		I_FatalError ("Z_Free: freed a pointer without ZONEID");
	if (block->user == NULL)
		I_FatalError ("Z_Free: freed a freed pointer");

	if (block->user > (void **)2)
	{
		// smaller values are not pointers [Note: OS-dependent?]
		// clear the user's mark
		*block->user = NULL;
	}

	// mark as free
	block->user = NULL; 
	block->tag = 0;
	block->id = 0;
		
	other = block->prev;
	if (!other->user)
	{
		// merge with previous free block
		other->size += block->size;
		other->next = block->next;
		other->next->prev = other;

		if (block == mainzone->rover)
			mainzone->rover = other;

		block = other;
	}

	other = block->next;
	if (!other->user)
	{
		// merge the next free block onto the end
		block->size += other->size;
		block->next = other->next;
		block->next->prev = block;

		if (other == mainzone->rover)
			mainzone->rover = block;
	}

	// [RH] Move the rover back if we freed a block just behind it.
	if (block->next == mainzone->rover)
		mainzone->rover = block;
}


/*
========================
=
= Z_Malloc
=
= You can pass a NULL user if the tag is < PU_PURGELEVEL
========================
*/

#define MINFRAGMENT	64
#define ALIGN		8

#ifndef _DEBUG
void *Z_Malloc (size_t size, int tag, void *user)
#else
void *Z_Malloc2 (size_t size, int tag, void *user, const char *file, int line)
#endif
{
	size_t		startSize;
	size_t		collectSize;
	int 		extra;
	memblock_t	*start;
	memblock_t	*rover;
	memblock_t	*newblock;
	memblock_t	*base;

//#ifdef _DEBUG
//	Z_CheckHeap ();
//#endif

//
// scan through the block list, looking for the first free block
// of sufficient size, throwing out any purgable blocks along the way.
// [RH] Be smarter: Only throw out purgable blocks if doing so gives
// a large enough block.
//
	startSize = size;
	size += sizeof(memblock_t);	// account for size of block header
	size = (size + ALIGN - 1) & ~(ALIGN - 1);

//
// if there is a free block behind the rover, back up over it
//
	base = mainzone->rover;
	if (!base->prev->user)
		base = base->prev;

	rover = base;
	start = base->prev;
	collectSize = 0;

	do
	{
		if (rover == start)
		{
			// scanned all the way around the list
			Z_DumpHeap (0, 1000);
			I_FatalError ("Z_Malloc: failed on allocation of %i bytes", startSize);
		}

		if (rover == &mainzone->blocklist)
		{ // if looped around, restart the block
			base = rover;
			collectSize = 0;
		}

		if (rover->user == NULL || rover->tag >= PU_PURGELEVEL)
		{
		// found an unused or purgable block
			collectSize += rover->size;
			rover = rover->next;
		}
		else
		{
		// hit a block that can't be purged, so move past it
			base = rover = rover->next;
			collectSize = 0;
		}
	} while (collectSize < size);

	// found a block big enough

	// [RH] free any purgable blocks found along the way
	rover = base;

	while (base->user || base->size < size)
	{
		if (rover->user != NULL)
		{
			if (rover->tag >= PU_PURGELEVEL)
			{
			// free the rover block (adding the size to base)
				PurgeCount++;
				PurgeSize += rover->size;
				base = base->prev;	// the rover can be the base block
				Z_Free ((byte *)rover+sizeof(memblock_t));
				base = base->next;
				rover = base->next;
			}
			else
			{
				I_FatalError ("Z_Malloc: locked block unexpected\n");
			}
		}
		else
		{
			rover = rover->next;
		}
	}

	extra = (int)(base->size - size);
	
	if (extra > MINFRAGMENT)
	{
		// there will be a free fragment after the allocated block
		newblock = (memblock_t *) ((byte *)base + size );
		newblock->size = extra;
		
		// NULL indicates free block.
		newblock->user = NULL;	
		newblock->tag = 0;
		newblock->prev = base;
		newblock->next = base->next;
		newblock->next->prev = newblock;

		// [RH] Possibly move the rover back to this block
		if (mainzone->rover == newblock->next)
		{
			mainzone->rover = newblock;
		}

		base->next = newblock;
		base->size = size;
	}
		
	if (user)
	{
		// mark as an in use block
		base->user = (void **)user;
		*(void **)user = (void *) ((byte *)base + sizeof(memblock_t));
	}
	else
	{
		if (tag >= PU_PURGELEVEL)
			I_FatalError ("Z_Malloc: an owner is required for purgable blocks");

		base->user = (void **)2;	// mark as in use, but unowned
	}
	base->tag = tag;

#ifdef _DEBUG
	base->line = line;
	base->file = strstr (file, "src");
	if (base->file == NULL)
	{
		base->file = file;
	}
	else
	{
		base->file += 4;
	}
#else
	base->line = 0;
#endif

	mainzone->rover = base->next;	// next allocation will start looking here

	base->id = ZONEID;

//#ifdef _DEBUG
//	Z_CheckHeap ();
//#endif

	return (void *) ((byte *)base + sizeof(memblock_t));
}



/*
========================
=
= Z_FreeTags
=
========================
*/

void Z_FreeTags (int lowtag, int hightag)
{
	memblock_t* block;
	memblock_t* next;

//#ifdef _DEBUG
//	Z_CheckHeap ();
//#endif

	for (block = mainzone->blocklist.next ;
		 block != &mainzone->blocklist ;
		 block = next)
	{
		next = block->next;		// get link before freeing

		if (!block->user)
			continue;			// free block
		if (block->tag >= lowtag && block->tag <= hightag)
			Z_Free ((byte *)block+sizeof(memblock_t));
	}

	// [RH] reset the rover to the beginning of the heap
	mainzone->rover = mainzone->blocklist.next;
}



/*
========================
=
= Z_DumpHeap
=
= Note: TFileDumpHeap( stdout ) ?
========================
*/

void Z_DumpHeap (int lowtag, int hightag)
{
	memblock_t *block;
		
	Printf ("zone size: %i  location: %p\n", mainzone->size,mainzone);
	Printf ("tag range: %i to %i\n", lowtag, hightag);
		
	for (block = mainzone->blocklist.next ; ; block = block->next)
	{
		if (block->tag >= lowtag && block->tag <= hightag)
		{
			if (block->user)
			{
				Printf ("block:%p    size:%7i    user:%p    tag:%3i"
#ifdef _DEBUG
					"    %s:%d"
#endif
					"\n",
					block, block->size, block->user, block->tag
#ifdef _DEBUG
					, block->file, block->line
#endif
					);
			}
			else
			{
				Printf ("block:%p    size:%7i    free\n", block, block->size);
			}
		}
				
		if (block->next == &mainzone->blocklist)
		{
			// all blocks have been hit
			break;
		}
		
		if ( (byte *)block + block->size != (byte *)block->next)
			Printf ("ERROR: block size does not touch the next block\n");

		if ( block->next->prev != block)
			Printf ("ERROR: next block doesn't have proper back link\n");

		if (!block->user && !block->next->user)
			Printf ("ERROR: two consecutive free blocks\n");
	}
}


//
// Z_FileDumpHeap
//
void Z_FileDumpHeap (FILE *f)
{
	memblock_t* block;
		
	fprintf (f,"zone size: %i  location: %p\n",mainzone->size,mainzone);
		
	for (block = mainzone->blocklist.next ; ; block = block->next)
	{
		fprintf (f,"block:%p    size:%7i    user:%p    tag:%3i"
#ifdef _DEBUG
			"    %s:%d"
#endif
			"\n",
			block, block->size, block->user, block->tag
#ifdef _DEBUG
			, block->file, block->line
#endif
			);
				
		if (block->next == &mainzone->blocklist)
		{
			// all blocks have been hit
			break;
		}
		
		if ( (byte *)block + block->size != (byte *)block->next)
			fprintf (f,"ERROR: block size does not touch the next block\n");

		if ( block->next->prev != block)
			fprintf (f,"ERROR: next block doesn't have proper back link\n");

		if (!block->user && !block->next->user)
			fprintf (f,"ERROR: two consecutive free blocks\n");
	}
}


/*
========================
=
= Z_CheckHeap
=
========================
*/

void Z_CheckHeap (void)
{
	memblock_t *block, *prev;

	for (prev = &mainzone->blocklist,
		block = mainzone->blocklist.next;
		block->next != &mainzone->blocklist;
		block = block->next)
	{
		if ((byte *)block + block->size != (byte *)block->next)
			I_FatalError ("Z_CheckHeap: block size does not touch the next block\n");

		if (block->next->prev != block)
			I_FatalError ("Z_CheckHeap: next block doesn't have proper back link\n"
						  "(Next is %p, Back is %p, This is %p", block->next, block->next->prev, block);

		if (!block->user && !block->next->user)
			I_FatalError ("Z_CheckHeap: two consecutive free blocks\n");

		if (block->user && block->id != ZONEID)
			I_FatalError ("Z_CheckHeap: block does not have zone id\n");
	}
}




/*
========================
=
= Z_ChangeTag
=
========================
*/

void Z_ChangeTag2 (void *ptr, int tag)
{
	memblock_t *block;

	block = (memblock_t *) ( (byte *)ptr - sizeof(memblock_t));

	if (block->id != ZONEID)
		I_FatalError ("Z_ChangeTag: freed a pointer without ZONEID");

	if (tag >= PU_PURGELEVEL && (ptrdiff_t)block->user < 0x100)
		I_FatalError ("Z_ChangeTag: an owner is required for purgable blocks");

	block->tag = tag;
}



//
// Z_FreeMemory
//
static size_t numblocks;
static size_t largestpfree, pfree, usedpblocks;	// Purgable blocks
static size_t largestefree, efree, usedeblocks; // Empty (Unused) blocks
static size_t largestlsize, lsize, usedlblocks;	// Locked blocks

size_t Z_FreeMemory (void)
{
	memblock_t *block;
	size_t run = 0;
		
	numblocks =
		largestpfree = pfree = usedpblocks =
		largestefree = efree = usedeblocks =
		largestlsize = lsize = usedlblocks = 0;
	
//#ifdef _DEBUG
//	Z_CheckHeap ();
//#endif

	for (block = mainzone->blocklist.next ;
		 block != &mainzone->blocklist;
		 block = block->next)
	{
		numblocks++;

		if (block->user == NULL)
		{
			usedeblocks++;
			efree += block->size;
			run += block->size;
			if (block->size > largestefree)
			{
				largestefree = block->size;
			}
		}
		else if (block->tag >= PU_PURGELEVEL)
		{
			usedpblocks++;
			pfree += block->size;
			run += block->size;
		}
		else
		{
			usedlblocks++;
			lsize += block->size;
			if (block->size > largestlsize)
			{
				largestlsize = block->size;
			}
			run = 0;
		}
		if (run > largestpfree)
		{
			largestpfree = run;
		}
	}
	return pfree + efree;
}

CCMD (mem)
{
	Z_FreeMemory ();
	Printf ("%u blocks:\n"
			"used:      %5u (%9u,%9u)\n"
			"  purgable:%5u (%9u,%9u)\n"
			"  locked:  %5u (%9u,%9u)\n"
			"unused:    %5u (%9u,%9u)\n"
			"p-free:    %5u (%9u,%9u)\n",
			numblocks,

			usedpblocks+usedlblocks, pfree+lsize, largestlsize,

			usedpblocks, pfree, largestpfree,
			usedlblocks, lsize, largestlsize,
			usedeblocks, efree, largestefree,
			usedpblocks + usedeblocks, pfree + efree,
			largestpfree > largestefree ? largestpfree : largestefree
			);
}

ADD_STAT (purge, out)
{
	sprintf (out, "Purged%4d blocks,%6d bytes", PurgeCount, PurgeSize);
	if (PurgeCount)
	{
		Printf ("%s\n", out);
	}
	PurgeCount = 0;
	PurgeSize = 0;
}
