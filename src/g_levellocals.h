#pragma once

#include "g_level.h"
#include "r_defs.h"

struct FLevelLocals
{
	void Tick ();
	void AddScroller (int secnum);

	BYTE		md5[16];			// for savegame validation. If the MD5 does not match the savegame won't be loaded.
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
	EMapType	maptype;

	TStaticArray<vertex_t> vertexes;
	TStaticArray<sector_t> sectors;
	TStaticArray<line_t> lines;
	TStaticArray<side_t> sides;

	TArray<FSectorPortal> sectorPortals;


	DWORD		flags;
	DWORD		flags2;
	DWORD		flags3;

	DWORD		fadeto;					// The color the palette fades to (usually black)
	DWORD		outsidefog;				// The fog for sectors with sky ceilings

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

	SBYTE		WallVertLight;			// Light diffs for vert/horiz walls
	SBYTE		WallHorizLight;

	bool		FromSnapshot;			// The current map was restored from a snapshot

	double		teamdamage;

	bool		IsJumpingAllowed() const;
	bool		IsCrouchingAllowed() const;
	bool		IsFreelookAllowed() const;
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
