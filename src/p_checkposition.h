#ifndef P_CHECKPOS_H
#define P_CHECKPOS_H


//============================================================================
//
// This is a dynamic array which holds its first MAX_STATIC entries in normal
// variables to avoid constant allocations which this would otherwise
// require.
// 
// When collecting touched portal groups the normal cases are either
// no portals == one group or
// two portals = two groups
// 
// Anything with more can happen but far less infrequently, so this
// organization helps avoiding the overhead from heap allocations
// in the vast majority of situations.
//
//============================================================================

struct FPortalGroupArray
{
	enum
	{
		MAX_STATIC = 2
	};

	FPortalGroupArray()
	{
		varused = 0;
	}

	void Clear()
	{
		data.Clear();
		varused = 0;
	}

	void Add(DWORD num)
	{
		if (varused < MAX_STATIC) entry[varused++] = num;
		else data.Push(num);
	}

	unsigned Size()
	{
		return varused + data.Size();
	}

	DWORD operator[](unsigned index)
	{
		return index < MAX_STATIC ? entry[index] : data[index - MAX_STATIC];
	}

private:
	DWORD entry[MAX_STATIC];
	unsigned varused;
	TArray<DWORD> data;
};


//============================================================================
//
// Used by P_CheckPosition and P_TryMove in place of the original
// set of global variables.
//
//============================================================================

struct FCheckPosition
{
	// in
	AActor			*thing;
	fixed_t			x;
	fixed_t			y;
	fixed_t			z;

	// out
	sector_t		*sector;
	fixed_t			floorz;
	fixed_t			ceilingz;
	fixed_t			dropoffz;
	FTextureID		floorpic;
	int				floorterrain;
	sector_t		*floorsector;
	FTextureID		ceilingpic;
	sector_t		*ceilingsector;
	bool			touchmidtex;
	bool			abovemidtex;
	bool			floatok;
	bool			FromPMove;
	line_t			*ceilingline;
	AActor			*stepthing;
	// [RH] These are used by PIT_CheckThing and P_XYMovement to apply
	// ripping damage once per tic instead of once per move.
	bool			DoRipping;
	TMap<AActor*, bool> LastRipped;

	FPortalGroupArray	Groups;
	int				PushTime;

	FCheckPosition(bool rip=false)
	{
		DoRipping = rip;
		PushTime = 0;
		FromPMove = false;
	}
};


#endif
