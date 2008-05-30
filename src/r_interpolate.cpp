// Stuff from BUILD to interpolate floors and ceilings
//
// "Build Engine & Tools" Copyright (c) 1993-1997 Ken Silverman
// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.
// This code has been modified from its original form.

#include "r_data.h"
#include "p_3dmidtex.h"
#include "stats.h"

struct FActiveInterpolation
{
	FActiveInterpolation *Next;
	void *Address;
	EInterpType Type;

	friend FArchive &operator << (FArchive &arc, FActiveInterpolation *&interp);

	static int CountInterpolations ();
	static int CountInterpolations (int *usedbuckets, int *minbucketfill, int *maxbucketfill);

private:
	fixed_t oldipos[2], bakipos[2];

	void CopyInterpToOld();
	void CopyBakToInterp();
	void DoAnInterpolation(fixed_t smoothratio);

	static size_t HashKey(EInterpType type, void *interptr);
	static FActiveInterpolation *FindInterpolation(EInterpType, void *interptr, FActiveInterpolation **&interp_p);

	friend void updateinterpolations();
	friend void setinterpolation(EInterpType, void *interptr, bool dolinks);
	friend void stopinterpolation(EInterpType, void *interptr, bool dolinks);
	friend void dointerpolations(fixed_t smoothratio);
	friend void restoreinterpolations();
	friend void clearinterpolations();
	friend void SerializeInterpolations(FArchive &arc);

	static FActiveInterpolation *curiposhash[INTERPOLATION_BUCKETS];
};



static bool didInterp;

FActiveInterpolation *FActiveInterpolation::curiposhash[INTERPOLATION_BUCKETS];

void FActiveInterpolation::CopyInterpToOld ()
{
	switch (Type)
	{
	case INTERP_SectorFloor:
		oldipos[0] = ((sector_t*)Address)->floorplane.d;
		oldipos[1] = ((sector_t*)Address)->floortexz;
		break;
	case INTERP_SectorCeiling:
		oldipos[0] = ((sector_t*)Address)->ceilingplane.d;
		oldipos[1] = ((sector_t*)Address)->ceilingtexz;
		break;
	case INTERP_Vertex:
		oldipos[0] = ((vertex_t*)Address)->x;
		oldipos[1] = ((vertex_t*)Address)->y;
		break;
	case INTERP_FloorPanning:
		oldipos[0] = ((sector_t*)Address)->floor_xoffs;
		oldipos[1] = ((sector_t*)Address)->floor_yoffs;
		break;
	case INTERP_CeilingPanning:
		oldipos[0] = ((sector_t*)Address)->ceiling_xoffs;
		oldipos[1] = ((sector_t*)Address)->ceiling_yoffs;
		break;
	case INTERP_WallPanning_Top:
	case INTERP_WallPanning_Mid:
	case INTERP_WallPanning_Bottom:
		oldipos[0] = ((side_t*)Address)->GetTextureYOffset(Type - INTERP_WallPanning_Top);
		oldipos[1] = ((side_t*)Address)->GetTextureXOffset(Type - INTERP_WallPanning_Top);
		break;
	}
}

void FActiveInterpolation::CopyBakToInterp ()
{
	switch (Type)
	{
	case INTERP_SectorFloor:
		((sector_t*)Address)->floorplane.d = bakipos[0];
		((sector_t*)Address)->floortexz = bakipos[1];
		break;
	case INTERP_SectorCeiling:
		((sector_t*)Address)->ceilingplane.d = bakipos[0];
		((sector_t*)Address)->ceilingtexz = bakipos[1];
		break;
	case INTERP_Vertex:
		((vertex_t*)Address)->x = bakipos[0];
		((vertex_t*)Address)->y = bakipos[1];
		break;
	case INTERP_FloorPanning:
		((sector_t*)Address)->floor_xoffs = bakipos[0];
		((sector_t*)Address)->floor_yoffs = bakipos[1];
		break;
	case INTERP_CeilingPanning:
		((sector_t*)Address)->ceiling_xoffs = bakipos[0];
		((sector_t*)Address)->ceiling_yoffs = bakipos[1];
		break;
	case INTERP_WallPanning_Top:
	case INTERP_WallPanning_Mid:
	case INTERP_WallPanning_Bottom:
		((side_t*)Address)->SetTextureYOffset(Type - INTERP_WallPanning_Top, bakipos[0]);
		((side_t*)Address)->SetTextureXOffset(Type - INTERP_WallPanning_Top, bakipos[1]);
		break;
	}
}

void FActiveInterpolation::DoAnInterpolation (fixed_t smoothratio)
{
	fixed_t *adr1, *adr2, pos;
	int v1, v2;

	switch (Type)
	{
	case INTERP_SectorFloor:
		adr1 = &((sector_t*)Address)->floorplane.d;
		adr2 = &((sector_t*)Address)->floortexz;
		break;
	case INTERP_SectorCeiling:
		adr1 = &((sector_t*)Address)->ceilingplane.d;
		adr2 = &((sector_t*)Address)->ceilingtexz;
		break;
	case INTERP_Vertex:
		adr1 = &((vertex_t*)Address)->x;
		adr2 = &((vertex_t*)Address)->y;
		break;
	case INTERP_FloorPanning:
		adr1 = &((sector_t*)Address)->floor_xoffs;
		adr2 = &((sector_t*)Address)->floor_yoffs;
		break;
	case INTERP_CeilingPanning:
		adr1 = &((sector_t*)Address)->ceiling_xoffs;
		adr2 = &((sector_t*)Address)->ceiling_yoffs;
		break;
	case INTERP_WallPanning_Top:
	case INTERP_WallPanning_Mid:
	case INTERP_WallPanning_Bottom:
		v1 = ((side_t*)Address)->GetTextureYOffset(Type - INTERP_WallPanning_Top);
		v2 = ((side_t*)Address)->GetTextureXOffset(Type - INTERP_WallPanning_Top);
		adr1 = &v1;
		adr2 = &v2;
		break;
	default:
		return;
	}

	pos = bakipos[0] = *adr1;
	*adr1 = oldipos[0] + FixedMul (pos - oldipos[0], smoothratio);

	pos = bakipos[1] = *adr2;
	*adr2 = oldipos[1] + FixedMul (pos - oldipos[1], smoothratio);

	switch (Type)
	{
	case INTERP_WallPanning_Top:
	case INTERP_WallPanning_Mid:
	case INTERP_WallPanning_Bottom:
		((side_t*)Address)->SetTextureYOffset(Type - INTERP_WallPanning_Top, v1);
		((side_t*)Address)->SetTextureXOffset(Type - INTERP_WallPanning_Top, v2);
		break;
	default:
		return;
	}

}

size_t FActiveInterpolation::HashKey (EInterpType type, void *interptr)
{
	return (size_t)type * ((size_t)interptr>>5);
}

int FActiveInterpolation::CountInterpolations ()
{
	int d1, d2, d3;
	return CountInterpolations (&d1, &d2, &d3);
}

int FActiveInterpolation::CountInterpolations (int *usedbuckets, int *minbucketfill, int *maxbucketfill)
{
	int count = 0;
	int inuse = 0;
	int minuse = INT_MAX;
	int maxuse = INT_MIN;

	for (int i = INTERPOLATION_BUCKETS-1; i >= 0; --i)
	{
		int use = 0;
		FActiveInterpolation *probe = FActiveInterpolation::curiposhash[i];
		if (probe != NULL)
		{
			inuse++;
		}
		while (probe != NULL)
		{
			count++;
			use++;
			probe = probe->Next;
		}
		if (use > 0 && use < minuse)
		{
			minuse = use;
		}
		if (use > maxuse)
		{
			maxuse = use;
		}
	}
	*usedbuckets = inuse;
	*minbucketfill = minuse == INT_MAX ? 0 : minuse;
	*maxbucketfill = maxuse;
	return count;
}

FActiveInterpolation *FActiveInterpolation::FindInterpolation (EInterpType type, void *interptr, FActiveInterpolation **&interp_p)
{
	size_t hash = HashKey (type, interptr) % INTERPOLATION_BUCKETS;
	FActiveInterpolation *probe, **probe_p;

	for (probe_p = &curiposhash[hash], probe = *probe_p;
		probe != NULL;
		probe_p = &probe->Next, probe = *probe_p)
	{
		if (probe->Address > interptr)
		{ // We passed the place it would have been, so it must not be here.
			probe = NULL;
			break;
		}
		if (probe->Address == interptr && probe->Type == type)
		{ // Found it.
			break;
		}
	}
	interp_p = probe_p;
	return probe;
}

void clearinterpolations()
{
	for (int i = INTERPOLATION_BUCKETS-1; i >= 0; --i)
	{
		for (FActiveInterpolation *probe = FActiveInterpolation::curiposhash[i];
			probe != NULL; )
		{
			FActiveInterpolation *next = probe->Next;
			delete probe;
			probe = next;
		}
		FActiveInterpolation::curiposhash[i] = NULL;
	}
}

void updateinterpolations()  //Stick at beginning of domovethings
{
	for (int i = INTERPOLATION_BUCKETS-1; i >= 0; --i)
	{
		for (FActiveInterpolation *probe = FActiveInterpolation::curiposhash[i];
			probe != NULL; probe = probe->Next)
		{
			probe->CopyInterpToOld ();
		}
	}
}

void setinterpolation(EInterpType type, void *posptr, bool dolinks)
{
	FActiveInterpolation **interp_p;
	FActiveInterpolation *interp = FActiveInterpolation::FindInterpolation (type, posptr, interp_p);
	if (interp != NULL) return; // It's already active
	interp = new FActiveInterpolation;
	interp->Type = type;
	interp->Address = posptr;
	interp->Next = *interp_p;
	*interp_p = interp;
	interp->CopyInterpToOld ();

	if (dolinks)
	{
		if (type == INTERP_SectorFloor) 
		{
			P_Start3dMidtexInterpolations((sector_t*)posptr, false);
			P_StartLinkedSectorInterpolations((sector_t*)posptr, false);
		}
		else if (type == INTERP_SectorCeiling) 
		{
			P_Start3dMidtexInterpolations((sector_t*)posptr, true);
			P_StartLinkedSectorInterpolations((sector_t*)posptr, true);
		}
	}
}

void stopinterpolation(EInterpType type, void *posptr, bool dolinks)
{
	FActiveInterpolation **interp_p;
	FActiveInterpolation *interp = FActiveInterpolation::FindInterpolation (type, posptr, interp_p);
	if (interp != NULL)
	{
		*interp_p = interp->Next;
		delete interp;

		if (dolinks)
		{
			if (type == INTERP_SectorFloor) 
			{
				P_Stop3dMidtexInterpolations((sector_t*)posptr, false);
				P_StopLinkedSectorInterpolations((sector_t*)posptr, false);
			}
			else if (type == INTERP_SectorCeiling) 
			{
				P_Stop3dMidtexInterpolations((sector_t*)posptr, true);
				P_StopLinkedSectorInterpolations((sector_t*)posptr, true);
			}
		}
	}
}

void dointerpolations(fixed_t smoothratio)       //Stick at beginning of drawscreen
{
	if (smoothratio == FRACUNIT)
	{
		didInterp = false;
		return;
	}

	didInterp = true;

	for (int i = INTERPOLATION_BUCKETS-1; i >= 0; --i)
	{
		for (FActiveInterpolation *probe = FActiveInterpolation::curiposhash[i];
			probe != NULL; probe = probe->Next)
		{
			probe->DoAnInterpolation (smoothratio);
		}
	}
}

void restoreinterpolations()  //Stick at end of drawscreen
{
	if (didInterp)
	{
		didInterp = false;
		for (int i = INTERPOLATION_BUCKETS-1; i >= 0; --i)
		{
			for (FActiveInterpolation *probe = FActiveInterpolation::curiposhash[i];
				probe != NULL; probe = probe->Next)
			{
				probe->CopyBakToInterp ();
			}
		}
	}
}

void SerializeInterpolations (FArchive &arc)
{
	FActiveInterpolation *interp;
	int numinterpolations;
	int i;

	if (arc.IsStoring ())
	{
		numinterpolations = FActiveInterpolation::CountInterpolations();
		arc.WriteCount (numinterpolations);
		for (i = INTERPOLATION_BUCKETS-1; i >= 0; --i)
		{
			for (interp = FActiveInterpolation::curiposhash[i];
				interp != NULL; interp = interp->Next)
			{
				arc << interp;
			}
		}
	}
	else
	{
		clearinterpolations ();
		numinterpolations = arc.ReadCount ();
		for (i = numinterpolations; i > 0; --i)
		{
			FActiveInterpolation **interp_p;
			arc << interp;
			if (FActiveInterpolation::FindInterpolation (interp->Type, interp->Address, interp_p) == NULL)
			{ // Should always return NULL, because there should never be any duplicates stored.
				interp->Next = *interp_p;
				*interp_p = interp;
			}
		}
	}
}

FArchive &operator << (FArchive &arc, FActiveInterpolation *&interp)
{
	BYTE type;
	union
	{
		vertex_t *vert;
		sector_t *sect;
		side_t *side;
		void *ptr;
	} ptr;

	if (arc.IsStoring ())
	{
		type = interp->Type;
		ptr.ptr = interp->Address;
		arc << type;
		switch (type)
		{
		case INTERP_Vertex:			arc << ptr.vert;	break;
		case INTERP_WallPanning_Top:	
		case INTERP_WallPanning_Mid:	
		case INTERP_WallPanning_Bottom:	
									arc << ptr.side;	break;
		default:					arc << ptr.sect;	break;
		}
	}
	else
	{
		interp = new FActiveInterpolation;
		arc << type;
		interp->Type = (EInterpType)type;
		switch (type)
		{
		case INTERP_Vertex:			arc << ptr.vert;	break;
		case INTERP_WallPanning_Top:	
		case INTERP_WallPanning_Mid:	
		case INTERP_WallPanning_Bottom:	
									arc << ptr.side;	break;
		default:					arc << ptr.sect;	break;
		}
		interp->Address = ptr.ptr;
	}
	return arc;
}

ADD_STAT (interpolations)
{
	int inuse, minuse, maxuse, total;
	FString out;
	total = FActiveInterpolation::CountInterpolations (&inuse, &minuse, &maxuse);
	out.Format ("%d interpolations  buckets:%3d  min:%3d  max:%3d  avg:%3d  %d%% full  %d%% buckfull",
		total, inuse, minuse, maxuse, inuse?total/inuse:0, total*100/INTERPOLATION_BUCKETS, inuse*100/INTERPOLATION_BUCKETS);
	return out;
}
