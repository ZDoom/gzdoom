//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1994-1996 Raven Software
// Copyright 1999-2016 Randy Heit
// Copyright 2002-2018 Christoph Oelckers
// Copyright 2010 James Haley
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//		Map loader
//

#include "p_setup.h"
#include "mapdata.h"
#include "maploader.h"
#include "maploader_internal.h"
#include "files.h"
#include "i_system.h"
#include "i_time.h"
#include "doomerrors.h"
#include "info.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "w_wad.h"
#include "cmdlib.h"
#include "m_argv.h"
#include "g_levellocals.h"
#include "p_blockmap.h"

CVAR(Bool, genblockmap, false, CVAR_SERVERINFO | CVAR_GLOBALCONFIG);

//===========================================================================
//
// [RH] My own blockmap builder, not Killough's or TeamTNT's.
//
// Killough's turned out not to be correct enough, and I had
// written this for ZDBSP before I discovered that, so
// replacing the one he wrote for MBF seemed like the easiest
// thing to do. (Doom E3M6, near vertex 0--the one furthest east
// on the map--had problems.)
//
// Using a hash table to get the minimum possible blockmap size
// seems like overkill, but I wanted to change the code as little
// as possible from its ZDBSP incarnation.
//
//===========================================================================

static unsigned int BlockHash (TArray<int> *block)
{
	int hash = 0;
	int *ar = &(*block)[0];
	for (size_t i = 0; i < block->Size(); ++i)
	{
		hash = hash * 12235 + ar[i];
	}
	return hash & 0x7fffffff;
}

static bool BlockCompare (TArray<int> *block1, TArray<int> *block2)
{
	size_t size = block1->Size();

	if (size != block2->Size())
	{
		return false;
	}
	if (size == 0)
	{
		return true;
	}
	int *ar1 = &(*block1)[0];
	int *ar2 = &(*block2)[0];
	for (size_t i = 0; i < size; ++i)
	{
		if (ar1[i] != ar2[i])
		{
			return false;
		}
	}
	return true;
}

static void CreatePackedBlockmap (TArray<int> &BlockMap, TArray<int> *blocks, int bmapwidth, int bmapheight)
{
	int buckets[4096];
	int *hashes, hashblock;
	TArray<int> *block;
	int zero = 0;
	int terminator = -1;
	int *array;
	int i, hash;
	int hashed = 0, nothashed = 0;

	hashes = new int[bmapwidth * bmapheight];

	memset (hashes, 0xff, sizeof(int)*bmapwidth*bmapheight);
	memset (buckets, 0xff, sizeof(buckets));

	for (i = 0; i < bmapwidth * bmapheight; ++i)
	{
		block = &blocks[i];
		hash = BlockHash (block) % 4096;
		hashblock = buckets[hash];
		while (hashblock != -1)
		{
			if (BlockCompare (block, &blocks[hashblock]))
			{
				break;
			}
			hashblock = hashes[hashblock];
		}
		if (hashblock != -1)
		{
			BlockMap[4+i] = BlockMap[4+hashblock];
			hashed++;
		}
		else
		{
			hashes[i] = buckets[hash];
			buckets[hash] = i;
			BlockMap[4+i] = BlockMap.Size ();
			BlockMap.Push (zero);
			array = &(*block)[0];
			for (size_t j = 0; j < block->Size(); ++j)
			{
				BlockMap.Push (array[j]);
			}
			BlockMap.Push (terminator);
			nothashed++;
		}
	}

	delete[] hashes;

//	printf ("%d blocks written, %d blocks saved\n", nothashed, hashed);
}

enum
{
	BLOCKBITS = 7,
	BLOCKSIZE = 128
};

 void MapLoader::CreateBlockMap ()
{
	TArray<int> *BlockLists, *block, *endblock;
	int adder;
	int bmapwidth, bmapheight;
	double dminx, dmaxx, dminy, dmaxy;
	int minx, maxx, miny, maxy;
	int line;

	if (vertexes.Size() == 0)
		return;

	// Find map extents for the blockmap
	dminx = dmaxx = vertexes[0].fX();
	dminy = dmaxy = vertexes[0].fY();

	for (auto &vert : vertexes)
	{
			 if (vert.fX() < dminx) dminx = vert.fX();
		else if (vert.fX() > dmaxx) dmaxx = vert.fX();
			 if (vert.fY() < dminy) dminy = vert.fY();
		else if (vert.fY() > dmaxy) dmaxy = vert.fY();
	}

	minx = int(dminx);
	miny = int(dminy);
	maxx = int(dmaxx);
	maxy = int(dmaxy);

	bmapwidth =	 ((maxx - minx) >> BLOCKBITS) + 1;
	bmapheight = ((maxy - miny) >> BLOCKBITS) + 1;

	TArray<int> BlockMap (bmapwidth * bmapheight * 3 + 4);

	adder = minx;			BlockMap.Push (adder);
	adder = miny;			BlockMap.Push (adder);
	adder = bmapwidth;		BlockMap.Push (adder);
	adder = bmapheight;		BlockMap.Push (adder);

	BlockLists = new TArray<int>[bmapwidth * bmapheight];

	for (line = 0; line < (int)lines.Size(); ++line)
	{
		int x1 = int(lines[line].v1->fX());
		int y1 = int(lines[line].v1->fY());
		int x2 = int(lines[line].v2->fX());
		int y2 = int(lines[line].v2->fY());
		int dx = x2 - x1;
		int dy = y2 - y1;
		int bx = (x1 - minx) >> BLOCKBITS;
		int by = (y1 - miny) >> BLOCKBITS;
		int bx2 = (x2 - minx) >> BLOCKBITS;
		int by2 = (y2 - miny) >> BLOCKBITS;

		block = &BlockLists[bx + by * bmapwidth];
		endblock = &BlockLists[bx2 + by2 * bmapwidth];

		if (block == endblock)	// Single block
		{
			block->Push (line);
		}
		else if (by == by2)		// Horizontal line
		{
			if (bx > bx2)
			{
				swapvalues (block, endblock);
			}
			do
			{
				block->Push (line);
				block += 1;
			} while (block <= endblock);
		}
		else if (bx == bx2)	// Vertical line
		{
			if (by > by2)
			{
				swapvalues (block, endblock);
			}
			do
			{
				block->Push (line);
				block += bmapwidth;
			} while (block <= endblock);
		}
		else				// Diagonal line
		{
			int xchange = (dx < 0) ? -1 : 1;
			int ychange = (dy < 0) ? -1 : 1;
			int ymove = ychange * bmapwidth;
			int adx = abs (dx);
			int ady = abs (dy);

			if (adx == ady)		// 45 degrees
			{
				int xb = (x1 - minx) & (BLOCKSIZE-1);
				int yb = (y1 - miny) & (BLOCKSIZE-1);
				if (dx < 0)
				{
					xb = BLOCKSIZE-xb;
				}
				if (dy < 0)
				{
					yb = BLOCKSIZE-yb;
				}
				if (xb < yb)
					adx--;
			}
			if (adx >= ady)		// X-major
			{
				int yadd = dy < 0 ? -1 : BLOCKSIZE;
				do
				{
					int stop = (Scale ((by << BLOCKBITS) + yadd - (y1 - miny), dx, dy) + (x1 - minx)) >> BLOCKBITS;
					while (bx != stop)
					{
						block->Push (line);
						block += xchange;
						bx += xchange;
					}
					block->Push (line);
					block += ymove;
					by += ychange;
				} while (by != by2);
				while (block != endblock)
				{
					block->Push (line);
					block += xchange;
				}
				block->Push (line);
			}
			else					// Y-major
			{
				int xadd = dx < 0 ? -1 : BLOCKSIZE;
				do
				{
					int stop = (Scale ((bx << BLOCKBITS) + xadd - (x1 - minx), dy, dx) + (y1 - miny)) >> BLOCKBITS;
					while (by != stop)
					{
						block->Push (line);
						block += ymove;
						by += ychange;
					}
					block->Push (line);
					block += xchange;
					bx += xchange;
				} while (bx != bx2);
				while (block != endblock)
				{
					block->Push (line);
					block += ymove;
				}
				block->Push (line);
			}
		}
	}

	BlockMap.Reserve (bmapwidth * bmapheight);
	CreatePackedBlockmap (BlockMap, BlockLists, bmapwidth, bmapheight);
	delete[] BlockLists;

	blockmap.blockmaplump.Resize(BlockMap.Size());
	for (unsigned int ii = 0; ii < BlockMap.Size(); ++ii)
	{
		blockmap.blockmaplump[ii] = BlockMap[ii];
	}
}



//===========================================================================
//
// P_VerifyBlockMap
//
// haleyjd 03/04/10: do verification on validity of blockmap.
//
//===========================================================================

bool MapLoader::VerifyBlockMap(int count)
{
	auto &blockmaplump = blockmap.blockmaplump;
	int x, y;
	int *maxoffs = &blockmaplump[count];

	int bmapwidth = blockmaplump[2];
	int bmapheight = blockmaplump[3];

	for(y = 0; y < bmapheight; y++)
	{
		for(x = 0; x < bmapwidth; x++)
		{
			int offset;
			int *list, *tmplist;
			int *blockoffset;

			offset = y * bmapwidth + x;
			blockoffset = &blockmaplump[offset + 4];


			// check that block offset is in bounds
			if(blockoffset >= maxoffs)
			{
				Printf(PRINT_HIGH, "VerifyBlockMap: block offset overflow\n");
				return false;
			}

			offset = *blockoffset;         

			// check that list offset is in bounds
			if(offset < 4 || offset >= count)
			{
				Printf(PRINT_HIGH, "VerifyBlockMap: list offset overflow\n");
				return false;
			}

			list   = &blockmaplump[offset];

			// scan forward for a -1 terminator before maxoffs
			for(tmplist = list; ; tmplist++)
			{
				// we have overflowed the lump?
				if(tmplist >= maxoffs)
				{
					Printf(PRINT_HIGH, "VerifyBlockMap: open blocklist\n");
					return false;
				}
				if(*tmplist == -1) // found -1
					break;
			}

			// there's some node builder which carelessly removed the initial 0-entry.
			// Rather than second-guessing the intent, let's just discard such blockmaps entirely
			// to be on the safe side.
			if (*list != 0)
			{
				Printf(PRINT_HIGH, "VerifyBlockMap: first entry is not 0.\n");
				return false;
			}

			// scan the list for out-of-range linedef indicies in list
			for(tmplist = list; *tmplist != -1; tmplist++)
			{
				if((unsigned)*tmplist >= lines.Size())
				{
					Printf(PRINT_HIGH, "VerifyBlockMap: index >= numlines\n");
					return false;
				}
			}
		}
	}

	return true;
}

//===========================================================================
//
// P_LoadBlockMap
//
// killough 3/1/98: substantially modified to work
// towards removing blockmap limit (a wad limitation)
//
// killough 3/30/98: Rewritten to remove blockmap limit
//
//===========================================================================

void MapLoader::LoadBlockMap (MapData * map)
{
	int count = map->Size(ML_BLOCKMAP);

	if (ForceNodeBuild || genblockmap ||
		count/2 >= 0x10000 || count == 0 ||
		Args->CheckParm("-blockmap")
		)
	{
		DPrintf (DMSG_SPAMMY, "Generating BLOCKMAP\n");
		CreateBlockMap ();
	}
	else
	{
		uint8_t *data = new uint8_t[count];
		map->Read(ML_BLOCKMAP, data);
		const short *wadblockmaplump = (short *)data;
		int i;

		count/=2;
		blockmap.blockmaplump.Resize(count);

		// killough 3/1/98: Expand wad blockmap into larger internal one,
		// by treating all offsets except -1 as unsigned and zero-extending
		// them. This potentially doubles the size of blockmaps allowed,
		// because Doom originally considered the offsets as always signed.

		blockmap.blockmaplump[0] = LittleShort(wadblockmaplump[0]);
		blockmap.blockmaplump[1] = LittleShort(wadblockmaplump[1]);
		blockmap.blockmaplump[2] = (uint32_t)(LittleShort(wadblockmaplump[2])) & 0xffff;
		blockmap.blockmaplump[3] = (uint32_t)(LittleShort(wadblockmaplump[3])) & 0xffff;

		for (i = 4; i < count; i++)
		{
			short t = LittleShort(wadblockmaplump[i]);          // killough 3/1/98
			blockmap.blockmaplump[i] = t == -1 ? (uint32_t)0xffffffff : (uint32_t) t & 0xffff;
		}
		delete[] data;

		if (!VerifyBlockMap(count))
		{
			DPrintf (DMSG_SPAMMY, "Generating BLOCKMAP\n");
			CreateBlockMap();
		}

	}

	blockmap.bmaporgx = blockmap.blockmaplump[0];
	blockmap.bmaporgy = blockmap.blockmaplump[1];
	blockmap.bmapwidth = blockmap.blockmaplump[2];
	blockmap.bmapheight = blockmap.blockmaplump[3];

	// clear out mobj chains
	count = blockmap.bmapwidth*blockmap.bmapheight;
	blockmap.blocklinks.Resize(count);
	memset (blockmap.blocklinks.Data(), 0, count*sizeof(blockmap.blocklinks[0]));
	blockmap.blockmap = &blockmap.blockmaplump[4];
}

