#ifndef __RES_VOXEL_H
#define __RES_VOXEL_H

#include "doomdef.h"

// [RH] Voxels from Build

#define MAXVOXMIPS 5

struct kvxslab_t
{
	BYTE		ztop;			// starting z coordinate of top of slab
	BYTE		zleng;			// # of bytes in the color array - slab height
	BYTE		backfacecull;	// low 6 bits tell which of 6 faces are exposed
	BYTE		col[1/*zleng*/];// color data from top to bottom
};

struct FVoxelMipLevel
{
	FVoxelMipLevel();
	~FVoxelMipLevel();

	int			SizeX;
	int			SizeY;
	int			SizeZ;
	fixed_t		PivotX;		// 24.8 fixed point
	fixed_t		PivotY;		// ""
	fixed_t		PivotZ;		// ""
	int			*OffsetX;
	short		*OffsetXY;
	BYTE		*SlabData;
};

struct FVoxel
{
	int LumpNum;
	int NumMips;
	int VoxelIndex;			// Needed by GZDoom
	BYTE *Palette;
	FVoxelMipLevel Mips[MAXVOXMIPS];

	FVoxel();
	~FVoxel();
	void Remap();
	void RemovePalette();
};

struct FVoxelDef
{
	FVoxel *Voxel;
	int PlacedSpin;			// degrees/sec to spin actors without MF_DROPPED set
	int DroppedSpin;		// degrees/sec to spin actors with MF_DROPPED set
	int VoxeldefIndex;		// Needed by GZDoom
	fixed_t Scale;
	angle_t AngleOffset;	// added to actor's angle to compensate for wrong-facing voxels
};

extern TDeletingArray<FVoxel *> Voxels;	// used only to auto-delete voxels on exit.
extern TDeletingArray<FVoxelDef *> VoxelDefs;

FVoxel *R_LoadKVX(int lumpnum);
FVoxelDef *R_LoadVoxelDef(int lumpnum, int spin);
void R_InitVoxels();

#endif
