/*
** g_levellocals.h
** The static data for a level
**
**---------------------------------------------------------------------------
** Copyright 1998-2016 Randy Heit
** Copyright 2005-2017 Christoph Oelckers
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

#pragma once

#include "doomdata.h"
#include "g_level.h"
#include "r_defs.h"
#include "portal.h"
#include "p_blockmap.h"

struct FLevelLocals
{
	void Tick ();
	void AddScroller (int secnum);
	void SetInterMusic(const char *nextmap);
	void SetMusicVolume(float v);

	uint8_t		md5[16];			// for savegame validation. If the MD5 does not match the savegame won't be loaded.
	int			time;			// time in the hub
	int			maptime;		// time in the map
	int			totaltime;		// time in the game
	int			starttime;
	int			partime;
	int			sucktime;

	level_info_t *info;
	int			cluster;
	int			clusterflags;
	int			levelnum;
	int			lumpnum;
	FString		LevelName;
	FString		MapName;			// the lump name (E1M1, MAP01, etc)
	FString		NextMap;			// go here when using the regular exit
	FString		NextSecretMap;		// map to go to when used secret exit
	FString		F1Pic;
	EMapType	maptype;

	uint64_t	ShaderStartTime = 0;	// tell the shader system when we started the level (forces a timer restart)

	TArray<vertex_t> vertexes;
	TArray<sector_t> sectors;
	TArray<line_t*> linebuffer;	// contains the line lists for the sectors.
	TArray<line_t> lines;
	TArray<side_t> sides;
	TArray<seg_t> segs;
	TArray<subsector_t> subsectors;
	TArray<node_t> nodes;
	TArray<subsector_t> gamesubsectors;
	TArray<node_t> gamenodes;
	node_t *headgamenode;
	TArray<uint8_t> rejectmatrix;

	TArray<FSectorPortal> sectorPortals;
	TArray<FLinePortal> linePortals;

	// Portal information.
	FDisplacementTable Displacements;
	FPortalBlockmap PortalBlockmap;
	TArray<FLinePortal*> linkedPortals;	// only the linked portals, this is used to speed up looking for them in P_CollectConnectedGroups.
	TArray<FSectorPortalGroup *> portalGroups;	
	TArray<FLinePortalSpan> linePortalSpans;
	int NumMapSections;

	TArray<zone_t>	Zones;

	FBlockmap blockmap;

	// These are copies of the loaded map data that get used by the savegame code to skip unaltered fields
	// Without such a mechanism the savegame format would become too slow and large because more than 80-90% are normally still unaltered.
	TArray<sector_t>	loadsectors;
	TArray<line_t>	loadlines;
	TArray<side_t>	loadsides;

	// Maintain single and multi player starting spots.
	TArray<FPlayerStart> deathmatchstarts;
	FPlayerStart		playerstarts[MAXPLAYERS];
	TArray<FPlayerStart> AllPlayerStarts;


	uint32_t		flags;
	uint32_t		flags2;
	uint32_t		flags3;

	uint32_t		fadeto;					// The color the palette fades to (usually black)
	uint32_t		outsidefog;				// The fog for sectors with sky ceilings

	uint32_t		hazardcolor;			// what color strife hazard blends the screen color as
	uint32_t		hazardflash;			// what color strife hazard flashes the screen color as

	FString		Music;
	int			musicorder;
	int			cdtrack;
	unsigned int cdid;
	FTextureID	skytexture1;
	FTextureID	skytexture2;

	float		skyspeed1;				// Scrolling speed of sky textures, in pixels per ms
	float		skyspeed2;

	int			total_secrets;
	int			found_secrets;

	int			total_items;
	int			found_items;

	int			total_monsters;
	int			killed_monsters;

	double		gravity;
	double		aircontrol;
	double		airfriction;
	int			airsupply;
	int			DefaultEnvironment;		// Default sound environment.

	TArray<DVector2>	Scrolls;		// NULL if no DScrollers in this level

	int8_t		WallVertLight;			// Light diffs for vert/horiz walls
	int8_t		WallHorizLight;

	bool		FromSnapshot;			// The current map was restored from a snapshot
	bool		HasHeightSecs;			// true if some Transfer_Heights effects are present in the map. If this is false, some checks in the renderer can be shortcut.
	bool		HasDynamicLights;		// Another render optimization for maps with no lights at all.

	double		teamdamage;

	// former OpenGL-exclusive properties that should also be usable by the true color software renderer.
	int fogdensity;
	int outsidefogdensity;
	int skyfog;

	float		pixelstretch;
	float		MusicVolume;

	// Hardware render stuff that can either be set via CVAR or MAPINFO
	int			lightmode;
	bool		brightfog;
	bool		lightadditivesurfaces;
	bool		notexturefill;

	bool		IsJumpingAllowed() const;
	bool		IsCrouchingAllowed() const;
	bool		IsFreelookAllowed() const;

	node_t		*HeadNode() const
	{
		return nodes.Size() == 0? nullptr : &nodes[nodes.Size() - 1];
	}
	node_t		*HeadGamenode() const
	{
		return headgamenode;
	}

};

extern FLevelLocals level;

inline int vertex_t::Index() const
{
	return int(this - &level.vertexes[0]);
}

inline int side_t::Index() const
{
	return int(this - &level.sides[0]);
}

inline int line_t::Index() const
{
	return int(this - &level.lines[0]);
}

inline int seg_t::Index() const
{
	return int(this - &level.segs[0]);
}

inline int subsector_t::Index() const
{
	return int(this - &level.subsectors[0]);
}

inline int node_t::Index() const
{
	return int(this - &level.nodes[0]);
}

inline FSectorPortal *line_t::GetTransferredPortal()
{
	return portaltransferred >= level.sectorPortals.Size() ? (FSectorPortal*)nullptr : &level.sectorPortals[portaltransferred];
}

inline int sector_t::Index() const 
{ 
	return int(this - &level.sectors[0]); 
}

inline FSectorPortal *sector_t::GetPortal(int plane)
{
	return &level.sectorPortals[Portals[plane]];
}

inline double sector_t::GetPortalPlaneZ(int plane)
{
	return level.sectorPortals[Portals[plane]].mPlaneZ;
}

inline DVector2 sector_t::GetPortalDisplacement(int plane)
{
	return level.sectorPortals[Portals[plane]].mDisplacement;
}

inline int sector_t::GetPortalType(int plane)
{
	return level.sectorPortals[Portals[plane]].mType;
}

inline int sector_t::GetOppositePortalGroup(int plane)
{
	return level.sectorPortals[Portals[plane]].mDestination->PortalGroup;
}

inline bool sector_t::PortalBlocksView(int plane)
{
	if (GetPortalType(plane) != PORTS_LINKEDPORTAL) return false;
	return !!(planes[plane].Flags & (PLANEF_NORENDER | PLANEF_DISABLED | PLANEF_OBSTRUCTED));
}

inline bool sector_t::PortalBlocksSight(int plane)
{
	return PLANEF_LINKED != (planes[plane].Flags & (PLANEF_NORENDER | PLANEF_NOPASS | PLANEF_DISABLED | PLANEF_OBSTRUCTED | PLANEF_LINKED));
}

inline bool sector_t::PortalBlocksMovement(int plane)
{
	return PLANEF_LINKED != (planes[plane].Flags & (PLANEF_NOPASS | PLANEF_DISABLED | PLANEF_OBSTRUCTED | PLANEF_LINKED));
}

inline bool sector_t::PortalBlocksSound(int plane)
{
	return PLANEF_LINKED != (planes[plane].Flags & (PLANEF_BLOCKSOUND | PLANEF_DISABLED | PLANEF_OBSTRUCTED | PLANEF_LINKED));
}

inline bool sector_t::PortalIsLinked(int plane)
{
	return (GetPortalType(plane) == PORTS_LINKEDPORTAL);
}

inline FLinePortal *line_t::getPortal() const
{
	return portalindex >= level.linePortals.Size() ? (FLinePortal*)NULL : &level.linePortals[portalindex];
}

// returns true if the portal is crossable by actors
inline bool line_t::isLinePortal() const
{
	return portalindex >= level.linePortals.Size() ? false : !!(level.linePortals[portalindex].mFlags & PORTF_PASSABLE);
}

// returns true if the portal needs to be handled by the renderer
inline bool line_t::isVisualPortal() const
{
	return portalindex >= level.linePortals.Size() ? false : !!(level.linePortals[portalindex].mFlags & PORTF_VISIBLE);
}

inline line_t *line_t::getPortalDestination() const
{
	return portalindex >= level.linePortals.Size() ? (line_t*)NULL : level.linePortals[portalindex].mDestination;
}

inline int line_t::getPortalAlignment() const
{
	return portalindex >= level.linePortals.Size() ? 0 : level.linePortals[portalindex].mAlign;
}
