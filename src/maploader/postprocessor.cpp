/*
** postprocessor.cpp
** Level postprocessing
**
**---------------------------------------------------------------------------
** Copyright 2009 Randy Heit
** Copyright 2009-2018 Christoph Oelckers
** Copyright 2019 Alexey Lysiuk
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/
#include "doomstat.h"
#include "c_dispatch.h"
#include "gi.h"
#include "g_level.h"
#include "p_lnspec.h"
#include "p_tags.h"
#include "w_wad.h"
#include "textures.h"
#include "g_levellocals.h"
#include "actor.h"
#include "p_setup.h"
#include "maploader/maploader.h"
#include "types.h"
#include "vm.h"

//==========================================================================
//
// PostProcessLevel
//
//==========================================================================

class DLevelPostProcessor : public DObject
{
	DECLARE_ABSTRACT_CLASS(DLevelPostProcessor, DObject)
public:
	MapLoader *loader;
	FLevelLocals *Level;
};

IMPLEMENT_CLASS(DLevelPostProcessor, true, false);

void MapLoader::PostProcessLevel(FName checksum)
{
	auto lc = Create<DLevelPostProcessor>();
	lc->loader = this;
	lc->Level = Level;
	for(auto cls : PClass::AllClasses)
	{
		if (cls->IsDescendantOf(RUNTIME_CLASS(DLevelPostProcessor)))
		{
			PFunction *const func = dyn_cast<PFunction>(cls->FindSymbol("Apply", false));
			if (func == nullptr)
			{
				Printf("Missing 'Apply' method in class '%s', level compatibility object ignored\n", cls->TypeName.GetChars());
				continue;
			}

			auto argTypes = func->Variants[0].Proto->ArgumentTypes;
			if (argTypes.Size() != 3 || argTypes[1] != TypeName || argTypes[2] != TypeString)
			{
				Printf("Wrong signature of 'Apply' method in class '%s', level compatibility object ignored\n", cls->TypeName.GetChars());
				continue;
			}

			VMValue param[] = { lc, checksum.GetIndex(), &Level->MapName };
			VMCall(func->Variants[0].Implementation, param, 3, nullptr, 0);
		}
	}
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, OffsetSectorPlane)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_INT(sector);
	PARAM_INT(planeval);
	PARAM_FLOAT(delta);

	if ((unsigned)sector < self->Level->sectors.Size())
	{
		sector_t *sec = &self->Level->sectors[sector];
		secplane_t& plane = sector_t::floor == planeval? sec->floorplane : sec->ceilingplane;
		plane.ChangeHeight(delta);
		sec->ChangePlaneTexZ(planeval, delta);
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, ClearSectorTags)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_INT(sector);
	self->Level->tagManager.RemoveSectorTags(sector);
	return 0;
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, AddSectorTag)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_INT(sector);
	PARAM_INT(tag);

	if ((unsigned)sector < self->Level->sectors.Size())
	{
		self->Level->tagManager.AddSectorTag(sector, tag);
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, ClearLineIDs)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_INT(line);
	self->Level->tagManager.RemoveLineIDs(line);
	return 0;
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, AddLineID)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_INT(line);
	PARAM_INT(tag);

	if ((unsigned)line < self->Level->lines.Size())
	{
		self->Level->tagManager.AddLineID(line, tag);
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, GetThingCount)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	ACTION_RETURN_INT(self->loader->MapThingsConverted.Size());
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, AddThing)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_UINT(ednum);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(z);
	PARAM_INT(angle);
	PARAM_UINT(skills);
	PARAM_UINT(flags);

	auto &things = self->loader->MapThingsConverted;
	const unsigned newindex = things.Size();
	things.Resize(newindex + 1);

	auto &newthing = things.Last();
	memset(&newthing, 0, sizeof newthing);

	newthing.Gravity = 1;
	newthing.SkillFilter = skills;
	newthing.ClassFilter = 0xFFFF;
	newthing.RenderStyle = STYLE_Count;
	newthing.Alpha = -1;
	newthing.Health = 1;
	newthing.FloatbobPhase = -1;
	newthing.pos.X = x;
	newthing.pos.Y = y;
	newthing.pos.Z = z;
	newthing.angle = angle;
	newthing.EdNum = ednum;
	newthing.info = DoomEdMap.CheckKey(ednum);
	newthing.flags = flags;

	ACTION_RETURN_INT(newindex);
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, GetThingEdNum)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_UINT(thing);

	const int ednum = thing < self->loader->MapThingsConverted.Size()
		? self->loader->MapThingsConverted[thing].EdNum : 0;
	ACTION_RETURN_INT(ednum);
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, SetThingEdNum)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_UINT(thing);
	PARAM_UINT(ednum);

	if (thing < self->loader->MapThingsConverted.Size())
	{
		auto &mti = self->loader->MapThingsConverted[thing];
		mti.EdNum = ednum;
		mti.info = DoomEdMap.CheckKey(ednum);
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, GetThingPos)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_UINT(thing);

	const DVector3 pos = thing < self->loader->MapThingsConverted.Size()
		? self->loader->MapThingsConverted[thing].pos
		: DVector3(0, 0, 0);
	ACTION_RETURN_VEC3(pos);
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, SetThingXY)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_UINT(thing);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);

	if (thing < self->loader->MapThingsConverted.Size())
	{
		auto& pos = self->loader->MapThingsConverted[thing].pos;
		pos.X = x;
		pos.Y = y;
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, SetThingZ)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_UINT(thing);
	PARAM_FLOAT(z);

	if (thing < self->loader->MapThingsConverted.Size())
	{
		self->loader->MapThingsConverted[thing].pos.Z = z;
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, GetThingAngle)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_UINT(thing);

	const int angle = thing < self->loader->MapThingsConverted.Size()
		? self->loader->MapThingsConverted[thing].angle : 0;
	ACTION_RETURN_INT(angle);
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, SetThingAngle)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_UINT(thing);
	PARAM_INT(angle);

	if (thing < self->loader->MapThingsConverted.Size())
	{
		self->loader->MapThingsConverted[thing].angle = angle;
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, GetThingSkills)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_UINT(thing);

	const int skills = thing < self->loader->MapThingsConverted.Size()
		? self->loader->MapThingsConverted[thing].SkillFilter : 0;
	ACTION_RETURN_INT(skills);
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, SetThingSkills)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_UINT(thing);
	PARAM_UINT(skillmask);

	if (thing < self->loader->MapThingsConverted.Size())
	{
		self->loader->MapThingsConverted[thing].SkillFilter = skillmask;
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, GetThingFlags)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_UINT(thing);

	const unsigned flags = thing < self->loader->MapThingsConverted.Size()
		? self->loader->MapThingsConverted[thing].flags : 0;
	ACTION_RETURN_INT(flags);
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, SetThingFlags)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_UINT(thing);
	PARAM_UINT(flags);

	if (thing < self->loader->MapThingsConverted.Size())
	{
		self->loader->MapThingsConverted[thing].flags = flags;
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, GetThingSpecial)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_UINT(thing);

	const int special = thing < self->loader->MapThingsConverted.Size()
		? self->loader->MapThingsConverted[thing].special : 0;
	ACTION_RETURN_INT(special);
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, SetThingSpecial)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_UINT(thing);
	PARAM_INT(special);

	if (thing < self->loader->MapThingsConverted.Size())
	{
		self->loader->MapThingsConverted[thing].special = special;
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, GetThingArgument)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_UINT(thing);
	PARAM_UINT(index);

	const int argument = index < 5 && thing < self->loader->MapThingsConverted.Size()
		? self->loader->MapThingsConverted[thing].args[index] : 0;
	ACTION_RETURN_INT(argument);
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, GetThingStringArgument)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_UINT(thing);

	const FName argument = thing < self->loader->MapThingsConverted.Size()
		? self->loader->MapThingsConverted[thing].arg0str : NAME_None;
	ACTION_RETURN_INT(argument);
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, SetThingArgument)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_UINT(thing);
	PARAM_UINT(index);
	PARAM_INT(value);

	if (index < 5 && thing < self->loader->MapThingsConverted.Size())
	{
		self->loader->MapThingsConverted[thing].args[index] = value;
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, SetThingStringArgument)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_UINT(thing);
	PARAM_INT(value);

	if (thing < self->loader->MapThingsConverted.Size())
	{
		self->loader->MapThingsConverted[thing].arg0str = ENamedName(value);
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, GetThingID)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_UINT(thing);

	const int id = thing < self->loader->MapThingsConverted.Size()
		? self->loader->MapThingsConverted[thing].thingid : 0;
	ACTION_RETURN_INT(id);
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, SetThingID)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_UINT(thing);
	PARAM_INT(id);

	if (thing < self->loader->MapThingsConverted.Size())
	{
		self->loader->MapThingsConverted[thing].thingid = id;
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, SetVertex)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_UINT(vertex);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);

	if (vertex < self->Level->vertexes.Size())
	{
		self->Level->vertexes[vertex].p = DVector2(x, y);
	}
	self->loader->ForceNodeBuild = true;
	return 0;
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, SetLineVertexes)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_UINT(lineidx);
	PARAM_UINT(vertexidx1);
	PARAM_UINT(vertexidx2);

	if (lineidx < self->Level->lines.Size() &&
		vertexidx1 < self->Level->vertexes.Size() &&
		vertexidx2 < self->Level->vertexes.Size())
	{
		line_t *line = &self->Level->lines[lineidx];
		vertex_t *vertex1 = &self->Level->vertexes[vertexidx1];
		vertex_t *vertex2 = &self->Level->vertexes[vertexidx2];

		line->v1 = vertex1;
		line->v2 = vertex2;
	}
	self->loader->ForceNodeBuild = true;
	return 0;
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, FlipLineSideRefs)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_UINT(lineidx);

	if (lineidx < self->Level->lines.Size())
	{
		line_t *line = &self->Level->lines[lineidx];
		side_t *side1 = line->sidedef[1];
		side_t *side2 = line->sidedef[0];

		if (!!side1 && !!side2) // don't flip single-sided lines
		{
			sector_t *frontsector = line->sidedef[1]->sector;
			sector_t *backsector = line->sidedef[0]->sector;
			line->sidedef[0] = side1;
			line->sidedef[1] = side2;
			line->frontsector = frontsector;
			line->backsector = backsector;
		}
	}
	self->loader->ForceNodeBuild = true;
	return 0;
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, SetLineSectorRef)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_UINT(lineidx);
	PARAM_UINT(sideidx);
	PARAM_UINT(sectoridx);

	if (   sideidx < 2
		&& lineidx < self->Level->lines.Size()
		&& sectoridx < self->Level->sectors.Size())
	{
		line_t *line = &self->Level->lines[lineidx];
		side_t *side = line->sidedef[sideidx];
		side->sector = &self->Level->sectors[sectoridx];
		if (sideidx == 0) line->frontsector = side->sector;
		else line->backsector = side->sector;
	}
	self->loader->ForceNodeBuild = true;
	return 0;
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, GetDefaultActor)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_NAME(actorclass);
	ACTION_RETURN_OBJECT(GetDefaultByName(actorclass));
}

DEFINE_FIELD(DLevelPostProcessor, Level);

