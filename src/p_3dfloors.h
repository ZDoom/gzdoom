#ifndef __SECTORE_H
#define __SECTORE_H


#define CenterSpot(sec) (vertex_t*)&(sec)->soundorg[0]

//#define _3DFLOORS

// 3D floor flags. Most are the same as in Legacy but I added some for EDGE's and Vavoom's features as well.
typedef enum
{
  FF_EXISTS            = 0x1,    //MAKE SURE IT'S VALID
  FF_SOLID             = 0x2,    //Does it clip things?
  FF_RENDERSIDES       = 0x4,    //Render the sides?
  FF_RENDERPLANES      = 0x8,    //Render the floor/ceiling?
  FF_RENDERALL         = 0xC,    //Render everything?
  FF_SWIMMABLE         = 0x10,   //Can we swim?
  FF_NOSHADE           = 0x20,   //Does it mess with the lighting?
  FF_BOTHPLANES        = 0x200,  //Render both planes all the time?
  FF_TRANSLUCENT       = 0x800,  //See through!
  FF_FOG               = 0x1000, //Fog "brush"?
  FF_INVERTPLANES      = 0x2000, //Reverse the plane visibility rules?
  FF_ALLSIDES          = 0x4000, //Render inside and outside sides?
  FF_INVERTSIDES       = 0x8000, //Only render inside sides?
  FF_DOUBLESHADOW      = 0x10000,//Make two lightlist entries to reset light?
  FF_UPPERTEXTURE	   = 0x20000,
  FF_LOWERTEXTURE      = 0x40000,
  FF_THINFLOOR		   = 0x80000,	// EDGE
  FF_SCROLLY           = 0x100000,  // EDGE - not yet implemented!!!
  FF_FIX			   = 0x200000,  // use floor of model sector as floor and floor of real sector as ceiling
  FF_INVERTSECTOR	   = 0x400000,	// swap meaning of sector planes
  FF_DYNAMIC		   = 0x800000,	// created by partitioning another 3D-floor due to overlap
  FF_CLIPPED		   = 0x1000000,	// split into several dynamic ffloors
  FF_SEETHROUGH        = 0x2000000,
  FF_SHOOTTHROUGH      = 0x4000000,
  FF_FADEWALLS         = 0x8000000,	// Applies real fog to walls and doesn't blend the view		
  FF_ADDITIVETRANS	   = 0x10000000, // Render this floor with additive translucency
  FF_FLOOD			   = 0x20000000, // extends towards the next lowest flooding or solid 3D floor or the bottom of the sector
  
} ffloortype_e;



#ifdef _3DFLOORS


struct secplane_t;

struct F3DFloor
{
	struct planeref
	{
		secplane_t *	plane;
		const FTextureID *	texture;
		const fixed_t *		texheight;
		sector_t *		model;
		bool			isceiling;
	};

	planeref			bottom;
	planeref			top;

	unsigned char		*toplightlevel;
	
	fixed_t				delta;
	
	int					flags;
	line_t*				master;
	
	sector_t *			model;
	sector_t *			target;
	
	int					lastlight;
	int					alpha;
	
};


struct FDynamicColormap;


struct lightlist_t
{
	secplane_t				plane;
	unsigned char *			p_lightlevel;
	FDynamicColormap **		p_extra_colormap;
	int						flags;
	F3DFloor*				caster;
};



class player_s;
void P_PlayerOnSpecial3DFloor(player_t* player);

void P_Get3DFloorAndCeiling(AActor * thing, sector_t * sector, fixed_t * floorz, fixed_t * ceilingz, int * floorpic);
bool P_CheckFor3DFloorHit(AActor * mo);
bool P_CheckFor3DCeilingHit(AActor * mo);
void P_Recalculate3DFloors(sector_t *);
void P_RecalculateAttached3DFloors(sector_t * sec);
lightlist_t * P_GetPlaneLight(sector_t * , secplane_t * plane, bool underside);
void P_Spawn3DFloors( void );

struct FLineOpening;

void P_LineOpening_XFloors (FLineOpening &open, AActor * thing, const line_t *linedef, 
							fixed_t x, fixed_t y, fixed_t refx, fixed_t refy);
							
#else

// Dummy definitions for disabled 3D floor code

struct F3DFloor
{
	int dummy;
};

struct lightlist_t
{
	int dummy;
};

class player_s;
inline void P_PlayerOnSpecial3DFloor(player_t* player) {}

inline void P_Get3DFloorAndCeiling(AActor * thing, sector_t * sector, fixed_t * floorz, fixed_t * ceilingz, int * floorpic) {}
inline bool P_CheckFor3DFloorHit(AActor * mo) { return false; }
inline bool P_CheckFor3DCeilingHit(AActor * mo) { return false; }
inline void P_Recalculate3DFloors(sector_t *) {}
inline void P_RecalculateAttached3DFloors(sector_t * sec) {}
inline lightlist_t * P_GetPlaneLight(sector_t * , secplane_t * plane, bool underside) { return NULL; }
inline void P_Spawn3DFloors( void ) {}

struct FLineOpening;

inline void P_LineOpening_XFloors (FLineOpening &open, AActor * thing, const line_t *linedef, 
							fixed_t x, fixed_t y, fixed_t refx, fixed_t refy) {}

#endif


#endif