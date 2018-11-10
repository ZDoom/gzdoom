// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2015-2018 Christoph Oelckers
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//



#include "g_levellocals.h"
#include "hw_vertexbuilder.h"
#include "earcut.hpp"


//=============================================================================
//
// Creates vertex meshes for sector planes
//
//=============================================================================

//=============================================================================
//
//
//
//=============================================================================

static void CreateVerticesForSubsector(subsector_t *sub, VertexContainer &gen, int qualifier)
{
	if (sub->numlines < 3) return;
	
	uint32_t startindex = gen.indices.Size();
	
	if ((sub->flags & SSECF_HOLE) && sub->numlines > 3)
	{
		// Hole filling "subsectors" are not necessarily convex so they require real triangulation.
		// These things are extremely rare so performance is secondary here.
		
		using Point = std::pair<double, double>;
		std::vector<std::vector<Point>> polygon;
		std::vector<Point> *curPoly;

		polygon.resize(1);
		curPoly = &polygon.back();
		curPoly->resize(sub->numlines);

		for (unsigned i = 0; i < sub->numlines; i++)
		{
			(*curPoly)[i] = { sub->firstline[i].v1->fX(), sub->firstline[i].v1->fY() };
		}
		auto indices = mapbox::earcut(polygon);
		for (auto vti : indices)
		{
			gen.AddIndexForVertex(sub->firstline[vti].v1, qualifier);
		}
	}
	else
	{
		int firstndx = gen.AddVertex(sub->firstline[0].v1, qualifier);
		int secondndx = gen.AddVertex(sub->firstline[1].v1, qualifier);
		for (unsigned int k = 2; k < sub->numlines; k++)
		{
			gen.AddIndex(firstndx);
			gen.AddIndex(secondndx);
			auto ndx = gen.AddVertex(sub->firstline[k].v1, qualifier);
			gen.AddIndex(ndx);
			secondndx = ndx;
		}
	}
}

//=============================================================================
//
//
//
//=============================================================================

static void TriangulateSection(FSection &sect, VertexContainer &gen, int qualifier)
{
	if (sect.segments.Size() < 3) return;
	
	// todo
}

//=============================================================================
//
//
//
//=============================================================================


static void CreateVerticesForSection(FSection &section, VertexContainer &gen, bool useSubsectors)
{
	section.vertexindex = gen.indices.Size();

	if (useSubsectors)
	{
		for (auto sub : section.subsectors)
		{
			CreateVerticesForSubsector(sub, gen, -1);
		}
	}
	else
	{
		TriangulateSection(section, gen, -1);
	}
	section.vertexcount = gen.indices.Size() - section.vertexindex;
}

//==========================================================================
//
// Creates the vertices for one plane in one subsector
//
//==========================================================================

static void CreateVerticesForSector(sector_t *sec, VertexContainer &gen)
{
	auto sections = level.sections.SectionsForSector(sec);
	for (auto &section :sections)
	{
		CreateVerticesForSection( section, gen, true);
	}
}


TArray<VertexContainer> BuildVertices()
{
	TArray<VertexContainer> verticesPerSector(level.sectors.Size(), true);
	for (unsigned i=0; i<level.sectors.Size(); i++)
	{
		CreateVerticesForSector(&level.sectors[i], verticesPerSector[i]);
	}
	return verticesPerSector;
}
