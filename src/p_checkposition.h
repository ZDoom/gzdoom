#ifndef P_CHECKPOS_H
#define P_CHECKPOS_H


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

	int				PushTime;

	FCheckPosition(bool rip=false)
	{
		DoRipping = rip;
		PushTime = 0;
		FromPMove = false;
	}
};


#endif
