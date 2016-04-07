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

extern int*				blockmaplump;	// offsets in blockmap are from here

extern int*				blockmap;
extern int				bmapwidth;
extern int				bmapheight; 	// in mapblocks
extern double			bmaporgx;
extern double			bmaporgy;		// origin of block map
extern FBlockNode**		blocklinks; 	// for thing chains

inline int GetBlockX(double xpos)
{
	return int((xpos - bmaporgx) / MAPBLOCKUNITS);
}

inline int GetBlockY(double ypos)
{
	return int((ypos - bmaporgy) / MAPBLOCKUNITS);
}

#endif
