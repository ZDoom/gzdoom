
#ifndef __GL_SECTIONS_H
#define __GL_SECTIONS_H

#include <algorithm>
#include "tarray.h"
#include "r_defs.h"

// Undefine Windows garbage.
#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

//==========================================================================
//
//
//
//==========================================================================

struct BoundingRect
{
	double left, top, right, bottom;

	bool contains(const BoundingRect & other)
	{
		return left <= other.left && top <= other.top && right >= other.right && bottom >= other.bottom;
	}

	bool intersects(const BoundingRect & other)
	{
		return !(other.left > right ||
			other.right < left ||
			other.top > bottom ||
			other.bottom < top);
	}

	void Union(const BoundingRect & other)
	{
		if (other.left < left) left = other.left;
		if (other.right > right) right = other.right;
		if (other.top < top) top = other.top;
		if (other.bottom > bottom) bottom = other.bottom;
	}

	double distanceTo(const BoundingRect &other)
	{
		if (intersects(other)) return 0;
		return std::max(std::min(fabs(left - other.right), fabs(right - other.left)),
			std::min(fabs(top - other.bottom), fabs(bottom - other.top)));
	}

	void addVertex(double x, double y)
	{
		if (x < left) left = x;
		if (x > right) right = x;
		if (y < top) top = y;
		if (y > bottom) bottom = y;
	}

	bool operator == (const BoundingRect &other)
	{
		return left == other.left && top == other.top && right == other.right && bottom == other.bottom;
	}
};


struct FSectionLine
{
	vertex_t *start;
	vertex_t *end;
	FSectionLine *partner;
	side_t *sidedef;
};

struct FSection
{
	// tbd: Do we need a list of subsectors here? Ideally the subsectors should not be used anywhere anymore except for finding out where a location is.
	TArrayView<FSectionLine> segments;
	TArrayView<side_t *>	 sides;		// contains all sidedefs, including the internal ones that do not make up the outer shape. (this list is not exclusive. A sidedef can be in multiple sections!)
	TArrayView<subsector_t *>	 subsectors;		// contains all sidedefs, including the internal ones that do not make up the outer shape. (this list is not exclusive. A sidedef can be in multiple sections!)
	sector_t				*sector;
	FLightNode				*lighthead;	// Light nodes (blended and additive)
	BoundingRect			 bounds;
	int						 validcount;
	short					 mapsection;
	char					 hacked;			// 1: is part of a render hack
};

class FSectionContainer
{
public:
	TArray<FSectionLine> allLines;
	TArray<FSection> allSections;
	//TArray<vertex_t*> allVertices;
	TArray<side_t *> allSides;
	TArray<subsector_t *> allSubsectors;
	TArray<int> allIndices;

	int *sectionForSubsectorPtr;			// stored inside allIndices
	int *sectionForSidedefPtr;				// also stored inside allIndices;

	FSection *SectionForSubsector(subsector_t *sub)
	{
		return SectionForSubsector(sub->Index());
	}
	FSection *SectionForSubsector(int ssindex)
	{
		return ssindex < 0 ? nullptr : &allSections[sectionForSubsectorPtr[ssindex]];
	}
	FSection *SectionForSidedef(side_t *side)
	{
		return SectionForSidedef(side->Index());
	}
	FSection *SectionForSidedef(int sindex)
	{
		return sindex < 0 ? nullptr : &allSections[sectionForSidedefPtr[sindex]];
	}
	void Clear()
	{
		allLines.Clear();
		allSections.Clear();
		allIndices.Clear();
	}
	void Reset()
	{
		Clear();
		allLines.ShrinkToFit();
		allSections.ShrinkToFit();
		allIndices.ShrinkToFit();
	}
};


void CreateSections(FSectionContainer &container);

#endif