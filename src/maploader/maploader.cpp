//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1994-1996 Raven Software
// Copyright 1999-2016 Randy Heit
// Copyright 2002-2018 Christoph Oelckers
// Copyright 2010 James Haley
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//		Map loader
//

#include "p_setup.h"
#include "maploader.h"
#include "files.h"
#include "i_system.h"
#include "g_levellocals.h"
#include "r_sky.h"
#include "p_lnspec.h"
#include "p_tags.h"
#include "v_text.h"
#include "doomstat.h"
#include "edata.h"
#include "w_wad.h"
#include "gi.h"

enum
{
    MISSING_TEXTURE_WARN_LIMIT = 20
};

//===========================================================================
//
// P_LoadVertexes
//
//===========================================================================

void MapLoader::LoadVertexes (MapData * map)
{
	// Determine number of vertices:
	//	total lump length / vertex record length.
	unsigned numvertexes = map->Size(ML_VERTEXES) / sizeof(mapvertex_t);

	if (numvertexes == 0)
	{
		I_Error ("Map has no vertices.\n");
	}

	// Allocate memory for buffer.
	vertexes.Alloc(numvertexes);

	auto &fr = map->Reader(ML_VERTEXES);
		
	// Copy and convert vertex coordinates, internal representation as fixed.
	for (auto &v : vertexes)
	{
		int16_t x = fr.ReadInt16();
		int16_t y = fr.ReadInt16();

		v.set(double(x), double(y));
	}
}

//===========================================================================
//
// Sets a sidedef's texture and prints a message if it's not present.
// (Passing index separately is for UDMF which does not have sectors allocated yet)
//
//===========================================================================

void MapLoader::SetTexture (sector_t *sector, int index, int position, const char *name, FMissingTextureTracker &track, bool truncate)
{
    static const char *positionnames[] = { "floor", "ceiling" };
    char name8[9];
    if (truncate)
    {
        strncpy(name8, name, 8);
        name8[8] = 0;
        name = name8;
    }
    
    FTextureID texture = TexMan.CheckForTexture (name, ETextureType::Flat,
                                                 FTextureManager::TEXMAN_Overridable|FTextureManager::TEXMAN_TryAny);
    
    if (!texture.Exists())
    {
        if (++track[name].Count <= MISSING_TEXTURE_WARN_LIMIT)
        {
            Printf(TEXTCOLOR_RED"Unknown %s texture '"
                   TEXTCOLOR_ORANGE "%s" TEXTCOLOR_RED
                   "' in sector %d\n",
                   positionnames[position], name, index);
        }
        texture = TexMan.GetDefaultTexture();
    }
    sector->SetTexture(position, texture);
}


//===========================================================================
//
// P_LoadSectors
//
//===========================================================================

void MapLoader::LoadSectors (MapData *map, FMissingTextureTracker &missingtex)
{
    mapsector_t            *ms;
    sector_t*            ss;
    int                    defSeqType;
    int                    lumplen = map->Size(ML_SECTORS);
    
    unsigned numsectors = lumplen / sizeof(mapsector_t);
    sectors.Alloc(numsectors);
    memset (sectors.Data(), 0, numsectors*sizeof(sector_t));
    
    if (level && (level->flags & LEVEL_SNDSEQTOTALCTRL))
        defSeqType = 0;
    else
        defSeqType = -1;
    
    TArray<uint8_t> msp(lumplen, true);
    map->Read(ML_SECTORS, msp.Data());
    ms = (mapsector_t*)msp.Data();
    ss = sectors.Data();
    
    // Extended properties
    sectors[0].e = new extsector_t[numsectors];
    
    for (unsigned i = 0; i < numsectors; i++, ss++, ms++)
    {
        ss->e = &sectors[0].e[i];
        if (!map->HasBehavior) ss->Flags |= SECF_FLOORDROP;
        ss->SetPlaneTexZ(sector_t::floor, (double)LittleShort(ms->floorheight));
        ss->floorplane.set(0, 0, 1., -ss->GetPlaneTexZ(sector_t::floor));
        ss->SetPlaneTexZ(sector_t::ceiling, (double)LittleShort(ms->ceilingheight));
        ss->ceilingplane.set(0, 0, -1., ss->GetPlaneTexZ(sector_t::ceiling));
        SetTexture(ss, i, sector_t::floor, ms->floorpic, missingtex, true);
        SetTexture(ss, i, sector_t::ceiling, ms->ceilingpic, missingtex, true);
        ss->lightlevel = LittleShort(ms->lightlevel);
        if (map->HasBehavior)
            ss->special = LittleShort(ms->special);
        else    // [RH] Translate to new sector special
            ss->special = P_TranslateSectorSpecial (LittleShort(ms->special));
        if (tagManager) tagManager->AddSectorTag(i, LittleShort(ms->tag));
        ss->thinglist = nullptr;
        ss->touching_thinglist = nullptr;        // phares 3/14/98
        ss->sectorportal_thinglist = nullptr;
        ss->touching_renderthings = nullptr;
        ss->seqType = defSeqType;
        ss->SeqName = NAME_None;
        ss->nextsec = -1;    //jff 2/26/98 add fields to support locking out
        ss->prevsec = -1;    // stair retriggering until build completes
        memset(ss->SpecialColors, -1, sizeof(ss->SpecialColors));
        
        ss->SetAlpha(sector_t::floor, 1.);
        ss->SetAlpha(sector_t::ceiling, 1.);
        ss->SetXScale(sector_t::floor, 1.);    // [RH] floor and ceiling scaling
        ss->SetYScale(sector_t::floor, 1.);
        ss->SetXScale(sector_t::ceiling, 1.);
        ss->SetYScale(sector_t::ceiling, 1.);
        
        ss->heightsec = NULL;    // sector used to get floor and ceiling height
        // killough 3/7/98: end changes
        
        ss->gravity = 1.f;    // [RH] Default sector gravity of 1.0
        ss->ZoneNumber = 0xFFFF;
        ss->terrainnum[sector_t::ceiling] = ss->terrainnum[sector_t::floor] = -1;
        
        
        
        // [RH] Sectors default to white light with the default fade.
        //        If they are outside (have a sky ceiling), they use the outside fog.
        ss->Colormap.LightColor = PalEntry(255, 255, 255);
        ss->Colormap.FadeColor = PalEntry(0, 0, 0);
        if (level)
        {
            if (level->outsidefog != 0xff000000 && (ss->GetTexture(sector_t::ceiling) == skyflatnum || (ss->special&0xff) == Sector_Outside))
            {
                ss->Colormap.FadeColor.SetRGB(level->outsidefog);
            }
            else  if (level->flags & LEVEL_HASFADETABLE)
            {
                ss->Colormap.FadeColor= 0x939393;    // The true color software renderer needs this. (The hardware renderer will ignore this value if LEVEL_HASFADETABLE is set.)
            }
            else
            {
                ss->Colormap.FadeColor.SetRGB(level->fadeto);
            }
        }
        
        
        // killough 8/28/98: initialize all sectors to normal friction
        ss->friction = ORIG_FRICTION;
        ss->movefactor = ORIG_FRICTION_FACTOR;
        ss->sectornum = i;
        ss->ibocount = -1;
    }
}

//===========================================================================
//
// P_LoadLineDefs
//
// killough 4/4/98: split into two functions, to allow sidedef overloading
//
// [RH] Actually split into four functions to allow for Hexen and Doom
//        linedefs.
//
//===========================================================================

void P_AdjustLine (line_t *ld) // This is also called from the polyobject code.
{
    vertex_t *v1, *v2;
    
    v1 = ld->v1;
    v2 = ld->v2;
    
    ld->setDelta(v2->fX() - v1->fX(), v2->fY() - v1->fY());
    
    if (v1->fX() < v2->fX())
    {
        ld->bbox[BOXLEFT] = v1->fX();
        ld->bbox[BOXRIGHT] = v2->fX();
    }
    else
    {
        ld->bbox[BOXLEFT] = v2->fX();
        ld->bbox[BOXRIGHT] = v1->fX();
    }
    
    if (v1->fY() < v2->fY())
    {
        ld->bbox[BOXBOTTOM] = v1->fY();
        ld->bbox[BOXTOP] = v2->fY();
    }
    else
    {
        ld->bbox[BOXBOTTOM] = v2->fY();
        ld->bbox[BOXTOP] = v1->fY();
    }
}

//===========================================================================
//
// [RH] Set line id (as appropriate) here
// for Doom format maps this must be done in P_TranslateLineDef because
// the tag doesn't always go into the first arg.
//
//===========================================================================

void MapLoader::SetLineID (int i, line_t *ld)
{
    if (maptype == MAPTYPE_HEXEN)
    {
        int setid = -1;
        switch (ld->special)
        {
            case Line_SetIdentification:
                if (!level || !(level->flags2 & LEVEL2_HEXENHACK))
                {
                    setid = ld->args[0] + 256 * ld->args[4];
                    ld->flags |= ld->args[1]<<16;
                }
                else
                {
                    setid = ld->args[0];
                }
                ld->special = 0;
                break;
                
            case TranslucentLine:
                setid = ld->args[0];
                ld->flags |= ld->args[3]<<16;
                break;
                
            case Teleport_Line:
            case Scroll_Texture_Model:
                setid = ld->args[0];
                break;
                
            case Polyobj_StartLine:
                setid = ld->args[3];
                break;
                
            case Polyobj_ExplicitLine:
                setid = ld->args[4];
                break;
                
            case Plane_Align:
                if (!(ib_compatflags & BCOMPATF_NOSLOPEID)) setid = ld->args[2];
                break;
                
            case Static_Init:
                if (ld->args[1] == Init_SectorLink) setid = ld->args[0];
                break;
                
            case Line_SetPortal:
                setid = ld->args[1]; // 0 = target id, 1 = this id, 2 = plane anchor
                break;
        }
        if (setid != -1)
        {
            if (tagManager) tagManager->AddLineID(i, setid);
        }
    }
}

//===========================================================================
//
//
//
//===========================================================================

void MapLoader::SaveLineSpecial (line_t *ld)
{
    if (ld->sidedef[0] == NULL)
        return;
    
    uint32_t sidenum = ld->sidedef[0]->Index();
    // killough 4/4/98: support special sidedef interpretation below
    // [RH] Save Static_Init only if it's interested in the textures
    if    (ld->special != Static_Init || ld->args[1] == Init_Color)
    {
        sidetemp[sidenum].a.special = ld->special;
        sidetemp[sidenum].a.tag = ld->args[0];
    }
    else
    {
        sidetemp[sidenum].a.special = 0;
    }
}

//===========================================================================
//
//
//
//===========================================================================

void MapLoader::FinishLoadingLineDef(line_t *ld, int alpha)
{
    bool additive = false;
    
    ld->frontsector = ld->sidedef[0] != NULL ? ld->sidedef[0]->sector : NULL;
    ld->backsector  = ld->sidedef[1] != NULL ? ld->sidedef[1]->sector : NULL;
    double dx = (ld->v2->fX() - ld->v1->fX());
    double dy = (ld->v2->fY() - ld->v1->fY());
    int linenum = ld->Index();
    
    if (ld->frontsector == NULL)
    {
        Printf ("Line %d has no front sector\n", linemap[linenum]);
    }
    
    // [RH] Set some new sidedef properties
    int len = (int)(g_sqrt (dx*dx + dy*dy) + 0.5f);
    
    if (ld->sidedef[0] != NULL)
    {
        ld->sidedef[0]->linedef = ld;
        ld->sidedef[0]->TexelLength = len;
        
    }
    if (ld->sidedef[1] != NULL)
    {
        ld->sidedef[1]->linedef = ld;
        ld->sidedef[1]->TexelLength = len;
    }
    
    switch (ld->special)
    {                        // killough 4/11/98: handle special types
        case TranslucentLine:            // killough 4/11/98: translucent 2s textures
            // [RH] Second arg controls how opaque it is.
            if (alpha == SHRT_MIN)
            {
                alpha = ld->args[1];
                additive = !!ld->args[2];
            }
            else if (alpha < 0)
            {
                alpha = -alpha;
                additive = true;
            }
            
            double dalpha = alpha / 255.;
            if (!ld->args[0])
            {
                ld->alpha = dalpha;
                if (additive)
                {
                    ld->flags |= ML_ADDTRANS;
                }
            }
            else
            {
                for (unsigned j = 0; j < lines.Size(); j++)
                {
                    if (tagManager && tagManager->LineHasID(j, ld->args[0]))
                    {
                        lines[j].alpha = dalpha;
                        if (additive)
                        {
                            lines[j].flags |= ML_ADDTRANS;
                        }
                    }
                }
            }
            ld->special = 0;
            break;
    }
}

//===========================================================================
//
// killough 4/4/98: delay using sidedefs until they are loaded
//
//===========================================================================

void MapLoader::FinishLoadingLineDefs ()
{
    for (auto &line : lines)
    {
        FinishLoadingLineDef(&line, sidetemp[line.sidedef[0]->Index()].a.alpha);
    }
}

//===========================================================================
//
//
//
//===========================================================================

void MapLoader::SetSideNum (side_t **sidenum_p, uint16_t sidenum)
{
    if (sidenum == NO_INDEX)
    {
        *sidenum_p = NULL;
    }
    else if (sidecount < (int)sides.Size())
    {
        sidetemp[sidecount].a.map = sidenum;
        *sidenum_p = &sides[sidecount++];
    }
    else
    {
        I_Error ("%d sidedefs is not enough\n", sidecount);
    }
}

//===========================================================================
//
//
//
//===========================================================================

void MapLoader::LoadLineDefs (MapData * map)
{
    int i, skipped;
    line_t *ld;
    int lumplen = map->Size(ML_LINEDEFS);
    maplinedef_t *mld;
    
    int numlines = lumplen / sizeof(maplinedef_t);
    linemap.Resize(numlines+1); // +1 is for bogus source data
    
    TArray<maplinedef_t> mldf(numlines, true);
    map->Read(ML_LINEDEFS, mldf.Data());
    
    // [RH] Count the number of sidedef references. This is the number of
    // sidedefs we need. The actual number in the SIDEDEFS lump might be less.
    // Lines with 0 length are also removed.
    
    for (skipped = sidecount = i = 0; i < numlines; )
    {
        mld = &mldf[i];
        unsigned v1 = LittleShort(mld->v1);
        unsigned v2 = LittleShort(mld->v2);
        
        if (v1 >= vertexes.Size() || v2 >= vertexes.Size())
        {
            I_Error ("Line %d has invalid vertices: %d and/or %d.\nThe map only contains %u vertices.", i+skipped, v1, v2, vertexes.Size());
        }
        else if (v1 == v2 || (vertexes[v1].fPos() == vertexes[v2].fPos()))
        {
            Printf ("Removing 0-length line %d\n", i+skipped);
            mldf.Delete(i);
            ForceNodeBuild = true;
            skipped++;
            numlines--;
        }
        else
        {
            // patch missing first sides instead of crashing out.
            // Visual glitches are better than not being able to play.
            if (LittleShort(mld->sidenum[0]) == NO_INDEX)
            {
                Printf("Line %d has no first side.\n", i);
                mld->sidenum[0] = 0;
            }
            sidecount++;
            if (LittleShort(mld->sidenum[1]) != NO_INDEX)
                sidecount++;
            linemap[i] = i+skipped;
            i++;
        }
    }
    lines.Alloc(numlines);
    memset(&lines[0], 0, numlines * sizeof(line_t));
    
    AllocateSideDefs (map, sidecount);
    
    mld = mldf.Data();
    ld = &lines[0];
    for (i = 0; i < numlines; i++, mld++, ld++)
    {
        ld->alpha = 1.;    // [RH] Opaque by default
        ld->portalindex = UINT_MAX;
        ld->portaltransferred = UINT_MAX;
        
        // [RH] Translate old linedef special and flags to be
        //        compatible with the new format.
        
        mld->special = LittleShort(mld->special);
        mld->tag = LittleShort(mld->tag);
        mld->flags = LittleShort(mld->flags);
        P_TranslateLineDef (ld, mld, -1);
        // do not assign the tag for Extradata lines.
        if (ld->special != Static_Init || (ld->args[1] != Init_EDLine && ld->args[1] != Init_EDSector))
        {
            if (tagManager) tagManager->AddLineID(i, mld->tag);
        }

        if (ld->special == Static_Init && ld->args[1] == Init_EDLine)
        {
            ProcessEDLinedef(ld, mld->tag);
        }

        
        ld->v1 = &vertexes[LittleShort(mld->v1)];
        ld->v2 = &vertexes[LittleShort(mld->v2)];
        
        SetSideNum (&ld->sidedef[0], LittleShort(mld->sidenum[0]));
        SetSideNum (&ld->sidedef[1], LittleShort(mld->sidenum[1]));
        
        P_AdjustLine (ld);
        SaveLineSpecial (ld);
        if (level)
        {
            if (level->flags2 & LEVEL2_CLIPMIDTEX) ld->flags |= ML_CLIP_MIDTEX;
            if (level->flags2 & LEVEL2_WRAPMIDTEX) ld->flags |= ML_WRAP_MIDTEX;
            if (level->flags2 & LEVEL2_CHECKSWITCHRANGE) ld->flags |= ML_CHECKSWITCHRANGE;
        }
    }
}

//===========================================================================
//
// [RH] Same as P_LoadLineDefs() except it uses Hexen-style LineDefs.
//
//===========================================================================

void MapLoader::LoadLineDefs2 (MapData * map)
{
    int i, skipped;
    line_t *ld;
    int lumplen = map->Size(ML_LINEDEFS);
    maplinedef2_t *mld;
    
    int numlines = lumplen / sizeof(maplinedef2_t);
    linemap.Resize(numlines+1); // +1 is for bogus source data
    
    TArray<maplinedef2_t> mldf(numlines, true);
    map->Read(ML_LINEDEFS, mldf.Data());
    
    // [RH] Remove any lines that have 0 length and count sidedefs used
    for (skipped = sidecount = i = 0; i < numlines; )
    {
        mld = &mldf[i];
        
        if (mld->v1 == mld->v2 || (vertexes[LittleShort(mld->v1)].fPos() == vertexes[LittleShort(mld->v2)].fPos()))
        {
            Printf ("Removing 0-length line %d\n", i+skipped);
            mldf.Delete(i);
            skipped++;
            numlines--;
        }
        else
        {
            // patch missing first sides instead of crashing out.
            // Visual glitches are better than not being able to play.
            if (LittleShort(mld->sidenum[0]) == NO_INDEX)
            {
                Printf("Line %d has no first side.\n", i);
                mld->sidenum[0] = 0;
            }
            sidecount++;
            if (LittleShort(mld->sidenum[1]) != NO_INDEX)
                sidecount++;
            linemap[i] = i+skipped;
            i++;
        }
    }
    if (skipped > 0)
    {
        ForceNodeBuild = true;
    }
    lines.Alloc(numlines);
    memset(&lines[0], 0, numlines * sizeof(line_t));
    
    AllocateSideDefs (map, sidecount);
    
    mld = mldf.Data();
    ld = &lines[0];
    for (i = 0; i < numlines; i++, mld++, ld++)
    {
        int j;
        
        ld->portalindex = UINT_MAX;
        ld->portaltransferred = UINT_MAX;
        
        for (j = 0; j < 5; j++)
            ld->args[j] = mld->args[j];
        
        ld->flags = LittleShort(mld->flags);
        ld->special = mld->special;
        
        ld->v1 = &vertexes[LittleShort(mld->v1)];
        ld->v2 = &vertexes[LittleShort(mld->v2)];
        ld->alpha = 1.;    // [RH] Opaque by default
        
        SetSideNum (&ld->sidedef[0], LittleShort(mld->sidenum[0]));
        SetSideNum (&ld->sidedef[1], LittleShort(mld->sidenum[1]));
        
        P_AdjustLine (ld);
        SetLineID(i, ld);
        SaveLineSpecial (ld);
        if (level)
        {
            if (level->flags2 & LEVEL2_CLIPMIDTEX) ld->flags |= ML_CLIP_MIDTEX;
            if (level->flags2 & LEVEL2_WRAPMIDTEX) ld->flags |= ML_WRAP_MIDTEX;
            if (level->flags2 & LEVEL2_CHECKSWITCHRANGE) ld->flags |= ML_CHECKSWITCHRANGE;
        }

        // convert the activation type
        ld->activation = 1 << GET_SPAC(ld->flags);
        if (ld->activation == SPAC_AnyCross)
        { // this is really PTouch
            ld->activation = SPAC_Impact | SPAC_PCross;
        }
        else if (ld->activation == SPAC_Impact)
        { // In non-UMDF maps, Impact implies PCross
            ld->activation = SPAC_Impact | SPAC_PCross;
        }
        ld->flags &= ~ML_SPAC_MASK;
    }
}


//===========================================================================
//
//
//
//===========================================================================

void MapLoader::AllocateSideDefs (MapData *map, int count)
{
    int i;
    
    sides.Alloc(count);
    memset(&sides[0], 0, count * sizeof(side_t));
    
    sidetemp.Alloc(MAX<int>(count, vertexes.Size()));
    for (i = 0; i < count; i++)
    {
        sidetemp[i].a.special = sidetemp[i].a.tag = 0;
        sidetemp[i].a.alpha = SHRT_MIN;
        sidetemp[i].a.map = NO_SIDE;
    }
    int numsides = map->Size(ML_SIDEDEFS) / sizeof(mapsidedef_t);
    if (count < numsides)
    {
        Printf ("Map has %d unused sidedefs\n", numsides - count);
    }
    numsides = count;
    sidecount = 0;
}

//===========================================================================
//
// [RH] Group sidedefs into loops so that we can easily determine
// what walls any particular wall neighbors.
//
//===========================================================================

void MapLoader::LoopSidedefs (bool firstloop)
{
    int i;
    
    int numsides = sides.Size();
    sidetemp.Resize(MAX<int>(vertexes.Size(), numsides));
    
    for (i = 0; i < (int)vertexes.Size(); ++i)
    {
        sidetemp[i].b.first = NO_SIDE;
        sidetemp[i].b.next = NO_SIDE;
    }
    for (; i < numsides; ++i)
    {
        sidetemp[i].b.next = NO_SIDE;
    }
    
    for (i = 0; i < numsides; ++i)
    {
        // For each vertex, build a list of sidedefs that use that vertex
        // as their left edge.
        line_t *line = sides[i].linedef;
        int lineside = (line->sidedef[0] != &sides[i]);
        int vert = lineside ? line->v2->Index() : line->v1->Index();
        
        sidetemp[i].b.lineside = lineside;
        sidetemp[i].b.next = sidetemp[vert].b.first;
        sidetemp[vert].b.first = i;
        
        // Set each side so that it is the only member of its loop
        sides[i].LeftSide = NO_SIDE;
        sides[i].RightSide = NO_SIDE;
    }
    
    // For each side, find the side that is to its right and set the
    // loop pointers accordingly. If two sides share a left vertex, the
    // one that forms the smallest angle is assumed to be the right one.
    for (i = 0; i < numsides; ++i)
    {
        uint32_t right;
        line_t *line = sides[i].linedef;
        
        // If the side's line only exists in a single sector,
        // then consider that line to be a self-contained loop
        // instead of as part of another loop
        if (line->frontsector == line->backsector)
        {
            const side_t* const rightside = line->sidedef[!sidetemp[i].b.lineside];
            
            if (NULL == rightside)
            {
                // There is no right side!
                if (firstloop) Printf ("Line %d's right edge is unconnected\n", linemap[line->Index()]);
                continue;
            }
            
            right = rightside->Index();
        }
        else
        {
            if (sidetemp[i].b.lineside)
            {
                right = line->v1->Index();
            }
            else
            {
                right = line->v2->Index();
            }
            
            right = sidetemp[right].b.first;
            
            if (right == NO_SIDE)
            {
                // There is no right side!
                if (firstloop) Printf ("Line %d's right edge is unconnected\n", linemap[line->Index()]);
                continue;
            }
            
            if (sidetemp[right].b.next != NO_SIDE)
            {
                int bestright = right;    // Shut up, GCC
                DAngle bestang = 360.;
                line_t *leftline, *rightline;
                DAngle ang1, ang2, ang;
                
                leftline = sides[i].linedef;
                ang1 = leftline->Delta().Angle();
                if (!sidetemp[i].b.lineside)
                {
                    ang1 += 180;
                }
                
                while (right != NO_SIDE)
                {
                    if (sides[right].LeftSide == NO_SIDE)
                    {
                        rightline = sides[right].linedef;
                        if (rightline->frontsector != rightline->backsector)
                        {
                            ang2 = rightline->Delta().Angle();
                            if (sidetemp[right].b.lineside)
                            {
                                ang2 += 180;
                            }
                            
                            ang = (ang2 - ang1).Normalized360();
                            
                            if (ang != 0 && ang <= bestang)
                            {
                                bestright = right;
                                bestang = ang;
                            }
                        }
                    }
                    right = sidetemp[right].b.next;
                }
                right = bestright;
            }
        }
        assert((unsigned)i<(unsigned)numsides);
        assert(right<(unsigned)numsides);
        sides[i].RightSide = right;
        sides[right].LeftSide = i;
    }
    
    // We keep the sidedef init info around until after polyobjects are initialized,
    // so don't delete just yet.
}

//===========================================================================
//
//
//
//===========================================================================

int MapLoader::DetermineTranslucency (int lumpnum)
{
    auto tranmap = Wads.OpenLumpReader (lumpnum);
    uint8_t index;
    PalEntry newcolor;
    PalEntry newcolor2;
    
    tranmap.Seek (GPalette.BlackIndex * 256 + GPalette.WhiteIndex, FileReader::SeekSet);
    tranmap.Read (&index, 1);
    
    newcolor = GPalette.BaseColors[GPalette.Remap[index]];
    
    tranmap.Seek (GPalette.WhiteIndex * 256 + GPalette.BlackIndex, FileReader::SeekSet);
    tranmap.Read (&index, 1);
    newcolor2 = GPalette.BaseColors[GPalette.Remap[index]];
    if (newcolor2.r == 255)    // if black on white results in white it's either
        // fully transparent or additive
    {
        if (developer >= DMSG_NOTIFY)
        {
            char lumpname[9];
            lumpname[8] = 0;
            Wads.GetLumpName (lumpname, lumpnum);
            Printf ("%s appears to be additive translucency %d (%d%%)\n", lumpname, newcolor.r,
                    newcolor.r*100/255);
        }
        return -newcolor.r;
    }
    
    if (developer >= DMSG_NOTIFY)
    {
        char lumpname[9];
        lumpname[8] = 0;
        Wads.GetLumpName (lumpname, lumpnum);
        Printf ("%s appears to be translucency %d (%d%%)\n", lumpname, newcolor.r,
                newcolor.r*100/255);
    }
    return newcolor.r;
}

//===========================================================================
//
// Sets a sidedef's texture and prints a message if it's not present.
//
//===========================================================================

void MapLoader::SetTexture (side_t *side, int position, const char *name, FMissingTextureTracker &track)
{
    static const char *positionnames[] = { "top", "middle", "bottom" };
    static const char *sidenames[] = { "first", "second" };
    
    FTextureID texture = TexMan.CheckForTexture (name, ETextureType::Wall,
                                                 FTextureManager::TEXMAN_Overridable|FTextureManager::TEXMAN_TryAny);
    
    if (!texture.Exists())
    {
        if (++track[name].Count <= MISSING_TEXTURE_WARN_LIMIT)
        {
            // Print an error that lists all references to this sidedef.
            // We must scan the linedefs manually for all references to this sidedef.
            for(unsigned i = 0; i < lines.Size(); i++)
            {
                for(int j = 0; j < 2; j++)
                {
                    if (lines[i].sidedef[j] == side)
                    {
                        Printf(TEXTCOLOR_RED"Unknown %s texture '"
                               TEXTCOLOR_ORANGE "%s" TEXTCOLOR_RED
                               "' on %s side of linedef %d\n",
                               positionnames[position], name, sidenames[j], i);
                    }
                }
            }
        }
        texture = TexMan.GetDefaultTexture();
    }
    side->SetTexture(position, texture);
}


//===========================================================================
//
// [RH] Figure out blends for deep water sectors
//
//===========================================================================

void MapLoader::SetTexture (side_t *side, int position, uint32_t *blend, const char *name)
{
    FTextureID texture;
    if ((*blend = R_ColormapNumForName (name)) == 0)
    {
        texture = TexMan.CheckForTexture (name, ETextureType::Wall,
                                          FTextureManager::TEXMAN_Overridable|FTextureManager::TEXMAN_TryAny);
        if (!texture.Exists())
        {
            char name2[9];
            char *stop;
            strncpy (name2, name, 8);
            name2[8] = 0;
            *blend = strtoul (name2, &stop, 16);
            texture = FNullTextureID();
        }
        else
        {
            *blend = 0;
        }
    }
    else
    {
        texture = FNullTextureID();
    }
    side->SetTexture(position, texture);
}

//===========================================================================
//
//
//
//===========================================================================

void MapLoader::SetTextureNoErr (side_t *side, int position, uint32_t *color, const char *name, bool *validcolor, bool isFog)
{
    FTextureID texture;
    *validcolor = false;
    texture = TexMan.CheckForTexture (name, ETextureType::Wall,
                                      FTextureManager::TEXMAN_Overridable|FTextureManager::TEXMAN_TryAny);
    if (!texture.Exists())
    {
        char name2[9];
        char *stop;
        strncpy (name2, name+1, 7);
        name2[7] = 0;
        if (*name != '#')
        {
            *color = strtoul (name, &stop, 16);
            texture = FNullTextureID();
            *validcolor = (*stop == 0) && (stop >= name + 2) && (stop <= name + 6);
            return;
        }
        else    // Support for Legacy's color format!
        {
            int l=(int)strlen(name);
            texture = FNullTextureID();
            *validcolor = false;
            if (l>=7)
            {
                for(stop=name2;stop<name2+6;stop++) if (!isxdigit(*stop)) *stop='0';
                
                int factor = l==7? 0 : clamp<int> ((name2[6]&223)-'A', 0, 25);
                
                name2[6]=0; int blue=strtol(name2+4,NULL,16);
                name2[4]=0; int green=strtol(name2+2,NULL,16);
                name2[2]=0; int red=strtol(name2,NULL,16);
                
                if (!isFog)
                {
                    if (factor==0)
                    {
                        *validcolor=false;
                        return;
                    }
                    factor = factor * 255 / 25;
                }
                else
                {
                    factor=0;
                }
                
                *color=MAKEARGB(factor, red, green, blue);
                texture = FNullTextureID();
                *validcolor = true;
                return;
            }
        }
        texture = FNullTextureID();
    }
    side->SetTexture(position, texture);
}


//===========================================================================
//
//
//
//===========================================================================

void MapLoader::ProcessSideTextures(bool checktranmap, side_t *sd, sector_t *sec, intmapsidedef_t *msd, int special, int tag, short *alpha, FMissingTextureTracker &missingtex)
{
    switch (special)
    {
        case Transfer_Heights:    // variable colormap via 242 linedef
            // [RH] The colormap num we get here isn't really a colormap,
            //      but a packed ARGB word for blending, so we also allow
            //      the blend to be specified directly by the texture names
            //      instead of figuring something out from the colormap.
            if (sec != NULL)
            {
                SetTexture (sd, side_t::bottom, &sec->bottommap, msd->bottomtexture);
                SetTexture (sd, side_t::mid, &sec->midmap, msd->midtexture);
                SetTexture (sd, side_t::top, &sec->topmap, msd->toptexture);
            }
            break;
            
        case Static_Init:
            // [RH] Set sector color and fog
            // upper "texture" is light color
            // lower "texture" is fog color
        {
            uint32_t color = MAKERGB(255,255,255), fog = 0;
            bool colorgood, foggood;
            
            SetTextureNoErr (sd, side_t::bottom, &fog, msd->bottomtexture, &foggood, true);
            SetTextureNoErr (sd, side_t::top, &color, msd->toptexture, &colorgood, false);
            SetTexture(sd, side_t::mid, msd->midtexture, missingtex);
            
            if (colorgood | foggood)
            {
                for (unsigned s = 0; s < sectors.Size(); s++)
                {
                    if (tagManager && tagManager->SectorHasTag(s, tag))
                    {
                        if (colorgood)
                        {
                            sectors[s].Colormap.LightColor.SetRGB(color);
                            sectors[s].Colormap.BlendFactor = APART(color);
                        }
                        if (foggood) sectors[s].Colormap.FadeColor.SetRGB(fog);
                    }
                }
            }
        }
            break;
            
        case Sector_Set3DFloor:
            if (msd->toptexture[0]=='#')
            {
                sd->SetTexture(side_t::top, FNullTextureID() +(int)(-strtoll(&msd->toptexture[1], NULL, 10)));    // store the alpha as a negative texture index
                // This will be sorted out by the 3D-floor code later.
            }
            else
            {
                SetTexture(sd, side_t::top, msd->toptexture, missingtex);
            }
            
            SetTexture(sd, side_t::mid, msd->midtexture, missingtex);
            SetTexture(sd, side_t::bottom, msd->bottomtexture, missingtex);
            break;
            
        case TranslucentLine:    // killough 4/11/98: apply translucency to 2s normal texture
            if (checktranmap)
            {
                int lumpnum;
                
                if (strnicmp ("TRANMAP", msd->midtexture, 8) == 0)
                {
                    // The translator set the alpha argument already; no reason to do it again.
                    sd->SetTexture(side_t::mid, FNullTextureID());
                }
                else if ((lumpnum = Wads.CheckNumForName (msd->midtexture)) > 0 &&
                         Wads.LumpLength (lumpnum) == 65536)
                {
                    *alpha = (short)DetermineTranslucency (lumpnum);
                    sd->SetTexture(side_t::mid, FNullTextureID());
                }
                else
                {
                    SetTexture(sd, side_t::mid, msd->midtexture, missingtex);
                }
                
                SetTexture(sd, side_t::top, msd->toptexture, missingtex);
                SetTexture(sd, side_t::bottom, msd->bottomtexture, missingtex);
                break;
            }
            // Fallthrough for Hexen maps is intentional
            
        default:            // normal cases
            
            SetTexture(sd, side_t::mid, msd->midtexture, missingtex);
            SetTexture(sd, side_t::top, msd->toptexture, missingtex);
            SetTexture(sd, side_t::bottom, msd->bottomtexture, missingtex);
            break;
    }
}

//===========================================================================
//
// killough 4/4/98: delay using texture names until
// after linedefs are loaded, to allow overloading.
// killough 5/3/98: reformatted, cleaned up
//
//===========================================================================

void MapLoader::LoadSideDefs2 (MapData *map, FMissingTextureTracker &missingtex)
{
    TArray<uint8_t> msdf(map->Size(ML_SIDEDEFS), true);
    map->Read(ML_SIDEDEFS, msdf.Data());
    
    for (unsigned i = 0; i < sides.Size(); i++)
    {
        mapsidedef_t *msd = ((mapsidedef_t*)msdf.Data()) + sidetemp[i].a.map;
        side_t *sd = &sides[i];
        sector_t *sec;
        
        // [RH] The Doom renderer ignored the patch y locations when
        // drawing mid textures. ZDoom does not, so fix the laser beams in Strife.
        if (gameinfo.gametype == GAME_Strife &&
            strncmp (msd->midtexture, "LASERB01", 8) == 0)
        {
            msd->rowoffset += 102;
        }
        
        sd->SetTextureXOffset(LittleShort(msd->textureoffset));
        sd->SetTextureYOffset(LittleShort(msd->rowoffset));
        sd->SetTextureXScale(1.);
        sd->SetTextureYScale(1.);
        sd->linedef = NULL;
        sd->Flags = 0;
        sd->UDMFIndex = i;
        
        // killough 4/4/98: allow sidedef texture names to be overloaded
        // killough 4/11/98: refined to allow colormaps to work as wall
        // textures if invalid as colormaps but valid as textures.
        
        if ((unsigned)LittleShort(msd->sector)>=sectors.Size())
        {
            Printf (PRINT_HIGH, "Sidedef %d has a bad sector\n", i);
            sd->sector = sec = NULL;
        }
        else
        {
            sd->sector = sec = &sectors[LittleShort(msd->sector)];
        }
        
        intmapsidedef_t imsd;
        imsd.toptexture.CopyCStrPart(msd->toptexture, 8);
        imsd.midtexture.CopyCStrPart(msd->midtexture, 8);
        imsd.bottomtexture.CopyCStrPart(msd->bottomtexture, 8);
        
        ProcessSideTextures(!map->HasBehavior, sd, sec, &imsd,
                              sidetemp[i].a.special, sidetemp[i].a.tag, &sidetemp[i].a.alpha, missingtex);
    }
}

