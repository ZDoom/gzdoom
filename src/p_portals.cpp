/*
** p_portals.cpp
**
** portal stuff
**
**---------------------------------------------------------------------------
** Copyright 2015 Christoph Oelckers
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

#include "r_defs.h"
#include "r_state.h"
#include "r_sky.h"
#include "a_sharedglobal.h"
#include "i_system.h"
#include "doomerrors.h"
#include "c_dispatch.h"
#include "v_text.h"

struct FDisplacement
{
	fixed_t x, y;
	bool isSet;
	BYTE indirect;	// just for illustration.
};

struct FDisplacementTable
{
	TArray<FDisplacement> data;
	int size;

	void Create(int numgroups)
	{
		data.Resize(numgroups*numgroups);
		memset(&data[0], 0, numgroups*numgroups*sizeof(data[0]));
		size = numgroups;
	}

	FDisplacement &operator()(int x, int y)
	{
		return data[x + size*y];
	}
};

static FDisplacementTable Displacements;



//============================================================================
//
// CollectSectors
//
// Collects all sectors that are connected to any sector belonging to a portal
// because they all will need the same displacement values
//
//============================================================================

static bool CollectSectors(int groupid, sector_t *origin)
{
	if (origin->PortalGroup != 0) return false;	// already processed
	origin->PortalGroup = groupid;

	TArray<sector_t *> list(16);
	list.Push(origin);

	for (unsigned i = 0; i < list.Size(); i++)
	{
		sector_t *sec = list[i];

		for (int j = 0; j < sec->linecount; j++)
		{
			line_t *line = sec->lines[j];
			sector_t *other = line->frontsector == sec ? line->backsector : line->frontsector;
			if (other != NULL && other != sec && other->PortalGroup != groupid)
			{
				other->PortalGroup = groupid;
				list.Push(other);
			}
		}
	}
	return true;
}


//============================================================================
//
// AddDisplacementForPortal
//
// Adds the displacement for one portal to the displacement array
//
//============================================================================

static void AddDisplacementForPortal(AStackPoint *portal)
{
	int thisgroup = portal->Mate->Sector->PortalGroup;
	int othergroup = portal->Sector->PortalGroup;
	if (thisgroup == othergroup)
	{
		I_Error("Portal between sectors %d and %d has both sides in same group", portal->Sector->sectornum, portal->Mate->Sector->sectornum);
	}
	if (thisgroup <= 0 || thisgroup >= Displacements.size || othergroup <= 0 || othergroup >= Displacements.size)
	{
		I_Error("Portal between sectors %d and %d has invalid group", portal->Sector->sectornum, portal->Mate->Sector->sectornum);
	}

	FDisplacement & disp = Displacements(thisgroup, othergroup);
	if (!disp.isSet)
	{
		disp.x = portal->scaleX;
		disp.y = portal->scaleY;
		disp.isSet = true;
	}
	else
	{
		if (disp.x != portal->scaleX || disp.y != portal->scaleY)
		{
			I_Error("Portal between sectors %d and %d: Displacement mismatch", portal->Sector->sectornum, portal->Mate->Sector->sectornum);
		}
	}
}

//============================================================================
//
// ConnectGroups
//
// Do the indirect connections. This loop will run until it cannot find any new connections
//
//============================================================================

static void ConnectGroups()
{
	// Now 
	BYTE indirect = 1;
	bool changed;
	do
	{
		changed = false;
		for (int x = 1; x < Displacements.size; x++)
		{
			for (int y = 1; y < Displacements.size; y++)
			{
				FDisplacement &dispxy = Displacements(x, y);
				if (dispxy.isSet)
				{
					for (int z = 1; z < Displacements.size; z++)
					{
						FDisplacement &dispyz = Displacements(y, z);
						if (dispyz.isSet)
						{
							FDisplacement &dispxz = Displacements(x, z);
							if (dispxz.isSet)
							{
								if (dispxy.x + dispyz.x != dispxz.x || dispxy.y + dispyz.y != dispxz.y)
								{
									I_Error("Portal offset mismatch");	// need to find some sectors to report.
								}
							}
							else
							{
								dispxz.x = dispxy.x + dispyz.x;
								dispxz.y = dispxy.y + dispyz.y;
								dispxz.isSet = true;
								dispxz.indirect = indirect;
								changed = true;
							}
						}
					}
				}
			}
		}
		indirect++;
	} while (changed);
}


//============================================================================
//
// P_CreateLinkedPortals
//
// Creates the data structures needed for linked portals:
// A list of affected sectors for each portal
// A list of affected linedefs for each portal
// Removes portals from sloped sectors (as they cannot work on them)
// Group all sectors connected to one side of the portal
// Caclculate displacements between all created groups.
//
// Portals with the same offset but different anchors will not be merged.
//
//============================================================================

void P_CreateLinkedPortals()
{
	TThinkerIterator<AStackPoint> it;
	AStackPoint *mo;
	TArray<AStackPoint *> orgs;
	int id = 0;

	while ((mo = it.Next()))
	{
		if (mo->special1 == SKYBOX_LINKEDPORTAL)
		{
			if (mo->Mate != NULL)
			{
				orgs.Push(mo);
				mo->special2 = ++id;
			}
			else
			{
				// this should never happen, but if it does, the portal needs to be removed
				mo->Destroy();
			}
		}
	}
	for (int i = 0; i < numsectors; i++)
	{
		for (int j = 0; j < 2; j++)
		{
			ASkyViewpoint *box = sectors[i].SkyBoxes[j];
			if (box != NULL && box->special1 == SKYBOX_LINKEDPORTAL)
			{
				secplane_t &plane = j == 0 ? sectors[i].floorplane : sectors[i].ceilingplane;
				if (plane.a || plane.b)
				{
					// The engine cannot deal with portals on a sloped plane.
					sectors[i].SkyBoxes[j] = NULL;
				}
			}
		}
	}

	// Group all sectors, starting at each portal origin.
	id = 1;
	for (unsigned i = 0; i < orgs.Size(); i++)
	{
		if (CollectSectors(id, orgs[i]->Sector)) id++;
		if (CollectSectors(id, orgs[i]->Mate->Sector)) id++;
	}
	Displacements.Create(id);
	// Check for leftover sectors that connect to a portal
	for (int i=0;i<numsectors;i++)
	{
		for (int j = 0; j < 2; j++)
		{
			ASkyViewpoint *box = sectors[i].SkyBoxes[j];
			if (box != NULL)
			{
				if (box->special1 == SKYBOX_LINKEDPORTAL && box->Sector->PortalGroup == 0)
				{
					CollectSectors(box->Sector->PortalGroup, box->Sector);
					box = box->Mate;
					if (box->special1 == SKYBOX_LINKEDPORTAL && box->Sector->PortalGroup == 0)
					{
						CollectSectors(box->Sector->PortalGroup, box->Sector);
					}
				}
			}
		}
	}
	try
	{
		for (unsigned i = 0; i < orgs.Size(); i++)
		{
			AddDisplacementForPortal(orgs[i]);
		}

		for (int x = 1; x < Displacements.size; x++)
		{
			for (int y = x+1; y < Displacements.size; y++)
			{
				FDisplacement &dispxy = Displacements(x, y);
				FDisplacement &dispyx = Displacements(y, x);
				if (dispxy.isSet && dispyx.isSet &&
					(dispxy.x != -dispyx.x || dispxy.y != -dispyx.y))
				{
					I_Error("Link offset mismatch between groups %d and %d", x, y);	// need to find some sectors to report.
				}
				// todo: Find sectors that have no group but belong to a portal.
			}
		}
		ConnectGroups();
	}
	catch (CRecoverableError &err)
	{
		// If some setup error occurs print it here and disable all linked portals (They only work if everything checks out.)
		Printf("Unable to set up linked portals: %s\n", err.GetMessage());
		for (unsigned i = 0; i < orgs.Size(); i++)
		{
			orgs[i]->special1 = SKYBOX_PORTAL;
		}
	}
}

CCMD(dumpLinkTable)
{
	for (int x = 1; x < Displacements.size; x++)
	{
		for (int y = 1; y < Displacements.size; y++)
		{
			FDisplacement &disp = Displacements(x, y);
			Printf("%c%c(%6d, %6d)", TEXTCOLOR_ESCAPE, 'A'+disp.indirect, disp.x>>FRACBITS, disp.y>>FRACBITS);
		}
		Printf("\n");
	}
}

