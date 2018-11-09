//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1994-1996 Raven Software
// Copyright 1999-2016 Randy Heit
// Copyright 2002-2018 Christoph Oelckers
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
//        Map loader
//

#include "p_setup.h"
#include "maploader.h"
#include "mapdata.h"
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
#include "actor.h"
#include "gi.h"


//===========================================================================
//
//
//
//===========================================================================

void MapLoader::LoadReject (MapData * map)
{
    const int neededsize = (sectors.Size() * sectors.Size() + 7) >> 3;
    int rejectsize;
    
    if (!map->CheckName(ML_REJECT, "REJECT"))
    {
        rejectsize = 0;
    }
    else
    {
        rejectsize = map->Size(ML_REJECT);
    }
    
    if (rejectsize < neededsize)
    {
        if (rejectsize > 0)
        {
            Printf ("REJECT is %d byte%s too small.\n", neededsize - rejectsize,
                    neededsize-rejectsize==1?"":"s");
        }
        rejectmatrix.Reset();
    }
    else
    {
        // Check if the reject has some actual content. If not, free it.
        rejectsize = MIN (rejectsize, neededsize);
        rejectmatrix.Alloc(rejectsize);
        
        map->Read (ML_REJECT, &rejectmatrix[0], rejectsize);
        
        int qwords = rejectsize / 8;
        int i;
        
        if (qwords > 0)
        {
            const uint64_t *qreject = (const uint64_t *)&rejectmatrix[0];
            
            i = 0;
            do
            {
                if (qreject[i] != 0)
                    return;
            } while (++i < qwords);
        }
        rejectsize &= 7;
        qwords *= 8;
        for (i = 0; i < rejectsize; ++i)
        {
            if (rejectmatrix[qwords + i] != 0)
                return;
        }
        
        // Reject has no data, so pretend it isn't there.
        rejectmatrix.Reset();
    }
}

//===========================================================================
//
// Sound enviroment handling
//
//===========================================================================

void MapLoader::FloodZone (sector_t *sec, int zonenum)
{
    if (sec->ZoneNumber == zonenum)
        return;
    
    sec->ZoneNumber = zonenum;
    
    for (auto check : sec->Lines)
    {
        sector_t *other;
        
        if (check->sidedef[1] == nullptr || (check->flags & ML_ZONEBOUNDARY))
            continue;
        
        if (check->frontsector == sec)
        {
            assert(check->backsector != nullptr);
            other = check->backsector;
        }
        else
        {
            assert(check->frontsector != nullptr);
            other = check->frontsector;
        }
        
        if (other->ZoneNumber != zonenum)
            FloodZone (other, zonenum);
    }
}

void MapLoader::FloodZones ()
{
    int z = 0, i;
    ReverbContainer *reverb;
    
    for (auto &sec : sectors)
    {
        if (sec.ZoneNumber == 0xFFFF)
        {
            FloodZone (&sec, z++);
        }
    }
    Zones.Resize(z);
    reverb = S_FindEnvironment(level->DefaultEnvironment);
    if (reverb == NULL)
    {
        Printf("Sound environment %d, %d not found\n", level->DefaultEnvironment >> 8, level->DefaultEnvironment & 255);
        reverb = DefaultEnvironments[0];
    }
    for (i = 0; i < z; ++i)
    {
        Zones[i].Environment = reverb;
    }
}

//==========================================================================
//
// Keep both the original nodes from the WAD and the GL nodes created here.
// The original set is only being used to get the sector for in-game
// positioning of actors but not for rendering.
//
// This is necessary because ZDBSP is much more sensitive
// to sloppy mapping practices that produce overlapping sectors.
// The crane in P:AR E1M3 is a good example that would be broken if
// this wasn't done.
//
//==========================================================================


//==========================================================================
//
// PointOnLine
//
// Same as the one im the node builder, but not part of a specific class
//
//==========================================================================

static bool PointOnLine (int x, int y, int x1, int y1, int dx, int dy)
{
    const double SIDE_EPSILON = 6.5536;
    
    // For most cases, a simple dot product is enough.
    double d_dx = double(dx);
    double d_dy = double(dy);
    double d_x = double(x);
    double d_y = double(y);
    double d_x1 = double(x1);
    double d_y1 = double(y1);
    
    double s_num = (d_y1-d_y)*d_dx - (d_x1-d_x)*d_dy;
    
    if (fabs(s_num) < 17179869184.0)    // 4<<32
    {
        // Either the point is very near the line, or the segment defining
        // the line is very short: Do a more expensive test to determine
        // just how far from the line the point is.
        double l = g_sqrt(d_dx*d_dx+d_dy*d_dy);
        double dist = fabs(s_num)/l;
        if (dist < SIDE_EPSILON)
        {
            return true;
        }
    }
    return false;
}


//==========================================================================
//
// SetRenderSector
//
// Sets the render sector for each GL subsector so that the proper flat
// information can be retrieved
//
//==========================================================================

void MapLoader::SetRenderSector()
{
    int                 i;
    uint32_t                 j;
    TArray<subsector_t *> undetermined;
    subsector_t *        ss;
    
#if 0    // doesn't work as expected :(
    
    // hide all sectors on textured automap that only have hidden lines.
    bool *hidesec = new bool[numsectors];
    for(i = 0; i < numsectors; i++)
    {
        hidesec[i] = true;
    }
    for(i = 0; i < numlines; i++)
    {
        if (!(lines[i].flags & ML_DONTDRAW))
        {
            hidesec[lines[i].frontsector - sectors] = false;
            if (lines[i].backsector != NULL)
            {
                hidesec[lines[i].backsector - sectors] = false;
            }
        }
    }
    for(i = 0; i < numsectors; i++)
    {
        if (hidesec[i]) sectors[i].MoreFlags |= SECMF_HIDDEN;
    }
    delete [] hidesec;
#endif
    
    // Check for incorrect partner seg info so that the following code does not crash.
    
    for (auto &seg : segs)
    {
        auto p = seg.PartnerSeg;
        if (p != nullptr)
        {
            int partner = p->Index();
            
            if (partner < 0 || partner >= (int)segs.Size() || &segs[partner] != p)
            {
                seg.PartnerSeg = nullptr;
            }
            
            // glbsp creates such incorrect references for Strife.
            if (seg.linedef && seg.PartnerSeg != nullptr && !seg.PartnerSeg->linedef)
            {
                seg.PartnerSeg = seg.PartnerSeg->PartnerSeg = nullptr;
            }
        }
    }
    for (auto &seg : segs)
    {
        if (seg.PartnerSeg != nullptr && seg.PartnerSeg->PartnerSeg != &seg)
        {
            seg.PartnerSeg = nullptr;
        }
    }
    
    // look up sector number for each subsector
    for (auto &ss : subsectors)
    {
        // For rendering pick the sector from the first seg that is a sector boundary
        // this takes care of self-referencing sectors
        seg_t *seg = ss.firstline;
        
        // Check for one-dimensional subsectors. These should be ignored when
        // being processed for automap drawing etc.
        ss.flags |= SSECF_DEGENERATE;
        for(j=2; j<ss.numlines; j++)
        {
            if (!PointOnLine(seg[j].v1->fixX(), seg[j].v1->fixY(), seg->v1->fixX(), seg->v1->fixY(), seg->v2->fixX() -seg->v1->fixX(), seg->v2->fixY() -seg->v1->fixY()))
            {
                // Not on the same line
                ss.flags &= ~SSECF_DEGENERATE;
                break;
            }
        }
        
        seg = ss.firstline;
        for(j=0; j<ss.numlines; j++)
        {
            if(seg->sidedef && (seg->PartnerSeg == nullptr || (seg->PartnerSeg->sidedef != nullptr && seg->sidedef->sector!=seg->PartnerSeg->sidedef->sector)))
            {
                ss.render_sector = seg->sidedef->sector;
                break;
            }
            seg++;
        }
        if(ss.render_sector == NULL)
        {
            undetermined.Push(&ss);
        }
    }
    
    // assign a vaild render sector to all subsectors which haven't been processed yet.
    while (undetermined.Size())
    {
        bool deleted=false;
        for(i=undetermined.Size()-1;i>=0;i--)
        {
            ss=undetermined[i];
            seg_t * seg = ss->firstline;
            
            for(j=0; j<ss->numlines; j++)
            {
                if (seg->PartnerSeg != nullptr && seg->PartnerSeg->Subsector)
                {
                    sector_t * backsec = seg->PartnerSeg->Subsector->render_sector;
                    if (backsec)
                    {
                        ss->render_sector = backsec;
                        undetermined.Delete(i);
                        deleted = 1;
                        break;
                    }
                }
                seg++;
            }
        }
        // We still got some left but the loop above was unable to assign them.
        // This only happens when a subsector is off the map.
        // Don't bother and just assign the real sector for rendering
        if (!deleted && undetermined.Size())
        {
            for(i=undetermined.Size()-1;i>=0;i--)
            {
                ss=undetermined[i];
                ss->render_sector=ss->sector;
            }
            break;
        }
    }
}



//==========================================================================
//
// FixMinisegReferences
//
// Sometimes it can happen that two matching minisegs do not have their partner set.
// Fix that here.
//
//==========================================================================

void MapLoader::FixMinisegReferences()
{
    TArray<seg_t *> bogussegs;
    
    for (unsigned i = 0; i < segs.Size(); i++)
    {
        if (segs[i].sidedef == nullptr && segs[i].PartnerSeg == nullptr)
        {
            bogussegs.Push(&segs[i]);
        }
    }
    
    for (unsigned i = 0; i < bogussegs.Size(); i++)
    {
        auto seg1 = bogussegs[i];
        seg_t *pick = nullptr;
        unsigned int picki = -1;
        
        // Try to fix the reference: If there's exactly one other seg in the set which matches as a partner link those two segs together.
        for (unsigned j = i + 1; j < bogussegs.Size(); j++)
        {
            auto seg2 = bogussegs[j];
            if (seg1->v1 == seg2->v2 && seg2->v1 == seg1->v2 && seg1->Subsector->render_sector == seg2->Subsector->render_sector)
            {
                pick = seg2;
                picki = j;
                break;
            }
        }
        if (pick)
        {
            DPrintf(DMSG_NOTIFY, "Linking miniseg pair from (%2.3f, %2.3f) -> (%2.3f, %2.3f) in sector %d\n", pick->v2->fX(), pick->v2->fY(), pick->v1->fX(), pick->v1->fY(), pick->frontsector->Index());
            pick->PartnerSeg = seg1;
            seg1->PartnerSeg = pick;
            assert(seg1->v1 == pick->v2 && pick->v1 == seg1->v2);
            bogussegs.Delete(picki);
            bogussegs.Delete(i);
            i--;
        }
    }
}

//==========================================================================
//
// FixHoles
//
// ZDBSP can leave holes in the node tree on extremely detailed maps.
// To help out the triangulator these are filled with dummy subsectors
// so that it can process the area correctly.
//
//==========================================================================

void MapLoader::FixHoles()
{
    TArray<seg_t *> bogussegs;
    TArray<TArray<seg_t *>> segloops;
    
    for (unsigned i = 0; i < segs.Size(); i++)
    {
        if (segs[i].sidedef == nullptr && segs[i].PartnerSeg == nullptr)
        {
            bogussegs.Push(&segs[i]);
        }
    }
    
    while (bogussegs.Size() > 0)
    {
        segloops.Reserve(1);
        auto *segloop = &segloops.Last();
        
        seg_t *startseg;
        seg_t *checkseg;
        while (bogussegs.Size() > 0)
        {
            bool foundsome = false;
            if (segloop->Size() == 0)
            {
                bogussegs.Pop(startseg);
                segloop->Push(startseg);
                checkseg = startseg;
            }
            for (unsigned i = 0; i < bogussegs.Size(); i++)
            {
                auto seg1 = bogussegs[i];
                
                if (seg1->v1 == checkseg->v2 && seg1->Subsector->render_sector == checkseg->Subsector->render_sector)
                {
                    foundsome = true;
                    segloop->Push(seg1);
                    bogussegs.Delete(i);
                    i--;
                    checkseg = seg1;
                    
                    if (seg1->v2 == startseg->v1)
                    {
                        // The loop is complete. Start a new one
                        segloops.Reserve(1);
                        segloop = &segloops.Last();
                    }
                }
            }
            if (!foundsome)
            {
                if ((*segloop)[0]->v1 != segloop->Last()->v2)
                {
                    // There was no connected seg, leaving an unclosed loop.
                    // Clear this and continue looking.
                    segloop->Clear();
                }
            }
        }
        for (unsigned i = 0; i < segloops.Size(); i++)
        {
            if (segloops[i].Size() == 0)
            {
                segloops.Delete(i);
                i--;
            }
        }
        
        // Add dummy entries to the level's seg and subsector arrays
        if (segloops.Size() > 0)
        {
            // cound the number of segs to add.
            unsigned segcount = 0;
            for (auto &segloop : segloops)
                segcount += segloop.Size();
            
            seg_t *oldsegstartptr = &segs[0];
            subsector_t *oldssstartptr = &subsectors[0];
            
            unsigned newsegstart = segs.Reserve(segcount);
            unsigned newssstart = subsectors.Reserve(segloops.Size());
            
            seg_t *newsegstartptr = &segs[0];
            subsector_t *newssstartptr = &subsectors[0];
            
            // Now fix all references to these in the level data.
            // Note that the Index() method does not work here due to the reallocation.
            for (auto &seg : segs)
            {
                if (seg.PartnerSeg) seg.PartnerSeg = newsegstartptr + (seg.PartnerSeg - oldsegstartptr);
                seg.Subsector = newssstartptr + (seg.Subsector - oldssstartptr);
            }
            for (auto &sub : subsectors)
            {
                sub.firstline = newsegstartptr + (sub.firstline - oldsegstartptr);
            }
            for (auto &node : nodes)
            {
                // How hideous... :(
                for (auto & p : node.children)
                {
                    auto intp = (intptr_t)p;
                    if (intp & 1)
                    {
                        subsector_t *sp = (subsector_t*)(intp - 1);
                        sp = newssstartptr + (sp - oldssstartptr);
                        intp = intptr_t(sp) + 1;
                        p = (void*)intp;
                    }
                }
            }
            for (auto &segloop : segloops)
            {
                for (auto &seg : segloop)
                {
                    seg = newsegstartptr + (seg - oldsegstartptr);
                }
            }
            
            // The seg lists in the sidedefs and the subsector lists in the sectors are not set yet when this gets called.
            
            // Add the new data. This doesn't care about convexity. It is never directly used to generate a primitive.
            for (auto &segloop : segloops)
            {
                DPrintf(DMSG_NOTIFY, "Adding dummy subsector for sector %d\n", segloop[0]->Subsector->render_sector->Index());
                
                subsector_t &sub = subsectors[newssstart++];
                memset(&sub, 0, sizeof(sub));
                sub.sector = segloop[0]->frontsector;
                sub.render_sector = segloop[0]->Subsector->render_sector;
                sub.numlines = segloop.Size();
                sub.firstline = &segs[newsegstart];
                sub.flags = SSECF_HOLE;
                
                for (auto otherseg : segloop)
                {
                    DPrintf(DMSG_NOTIFY, "   Adding seg from (%2.3f, %2.3f) -> (%2.3f, %2.3f)\n", otherseg->v2->fX(), otherseg->v2->fY(), otherseg->v1->fX(), otherseg->v1->fY());
                    seg_t &seg = segs[newsegstart++];
                    memset(&seg, 0, sizeof(seg));
                    seg.v1 = otherseg->v2;
                    seg.v2 = otherseg->v1;
                    seg.frontsector = seg.backsector = otherseg->backsector = otherseg->frontsector;
                    seg.PartnerSeg = otherseg;
                    otherseg->PartnerSeg = &seg;
                    seg.Subsector = &sub;
                }
            }
        }
    }
}
