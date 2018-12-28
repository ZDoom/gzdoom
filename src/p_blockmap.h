#ifndef __P_BLOCKMAP_H
#define __P_BLOCKMAP_H

#include "doomtype.h"

class AActor;

// [RH] Like msecnode_t, but for the blockmap
struct FBlockNode
{
	AActor *Me;						// actor this node references
	int BlockIndex;					// index into blocklinks for the block this node is in
	int Group;						// portal group this link belongs to (can be different than the actor's own group
	FBlockNode **PrevActor;			// previous actor in this block
	FBlockNode *NextActor;			// next actor in this block
	FBlockNode **PrevBlock;			// previous block this actor is in
	FBlockNode *NextBlock;			// next block this actor is in

	static FBlockNode *Create (AActor *who, int x, int y, int group = -1);
	void Release ();

	static FBlockNode *FreeBlocks;
};

// BLOCKMAP
// Created from axis aligned bounding box
// of the map, a rectangular array of
// blocks of size 128x128.
// Used to speed up collision detection
// by spatial subdivision in 2D.
//

struct FBlockmap
{
	int*				blockmaplump;	// offsets in blockmap are from here

	int*				blockmap;
	int					bmapwidth;
	int					bmapheight; 	// in mapblocks
	double				bmaporgx;
	double				bmaporgy;		// origin of block map
	FBlockNode**		blocklinks; 	// for thing chains

	// mapblocks are used to check movement
	// against lines and things
	enum
	{
		MAPBLOCKUNITS = 128
	};

	inline int GetBlockX(double xpos)
	{
		return int((xpos - bmaporgx) / MAPBLOCKUNITS);
	}

	inline int GetBlockY(double ypos)
	{
		return int((ypos - bmaporgy) / MAPBLOCKUNITS);
	}

	inline bool isValidBlock(int x, int y) const
	{
		return ((unsigned int)x < (unsigned int)bmapwidth &&
			(unsigned int)y < (unsigned int)bmapheight);
	}

	inline int *GetLines(int x, int y) const
	{
		// There is an extra entry at the beginning of every block.
		// Apparently, id had originally intended for it to be used
		// to keep track of things, but the final code does not do that.
		int offset = y*bmapwidth + x;
		return blockmaplump + *(blockmap + offset) + 1;
	}

	bool VerifyBlockMap(int count, unsigned numlines);

	void Clear()
	{
		if (blockmaplump != nullptr)
		{
			delete[] blockmaplump;
			blockmaplump = nullptr;
		}
		if (blocklinks != nullptr)
		{
			delete[] blocklinks;
			blocklinks = nullptr;
		}
	}

	~FBlockmap()
	{
		Clear();
	}

};

#endif
