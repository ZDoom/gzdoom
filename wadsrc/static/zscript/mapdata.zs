
struct SectorPortal native play
{
	enum EType
	{
		TYPE_SKYVIEWPOINT = 0,		// a regular skybox
		TYPE_STACKEDSECTORTHING,	// stacked sectors with the thing method
		TYPE_PORTAL,				// stacked sectors with Sector_SetPortal
		TYPE_LINKEDPORTAL,			// linked portal (interactive)
		TYPE_PLANE,					// EE-style plane portal (not implemented in SW renderer)
		TYPE_HORIZON,				// EE-style horizon portal (not implemented in SW renderer)
	};

	enum EFlags
	{
		FLAG_SKYFLATONLY = 1,			// portal is only active on skyflatnum
		FLAG_INSKYBOX = 2,				// to avoid recursion
	};

	native int mType;
	native int mFlags;
	native uint mPartner;
	native int mPlane;
	native Sector mOrigin;
	native Sector mDestination;
	native Vector2 mDisplacement;
	native double mPlaneZ;
	native Actor mSkybox;	
};


struct Vertex native play
{
	native readonly Vector2 p;
	native clearscope int Index();
}

struct Side native play
{
	enum ETexpart
	{
		top=0,
		mid=1,
		bottom=2
	};
	
	enum EColorPos
	{
		walltop = 0,
		wallbottom = 1
	}
	
	enum EWallFlags
	{
		WALLF_ABSLIGHTING	 = 1,	// Light is absolute instead of relative
		WALLF_NOAUTODECALS	 = 2,	// Do not attach impact decals to this wall
		WALLF_NOFAKECONTRAST = 4,	// Don't do fake contrast for this wall in side_t::GetLightLevel
		WALLF_SMOOTHLIGHTING = 8,   // Similar to autocontrast but applies to all angles.
		WALLF_CLIP_MIDTEX	 = 16,	// Like the line counterpart, but only for this side.
		WALLF_WRAP_MIDTEX	 = 32,	// Like the line counterpart, but only for this side.
		WALLF_POLYOBJ		 = 64,	// This wall belongs to a polyobject.
		WALLF_LIGHT_FOG      = 128,	// This wall's Light is used even in fog.
	};


	native readonly Sector sector;			// Sector the SideDef is facing.
	//DBaseDecal*	AttachedDecals;	// [RH] Decals bound to the wall
	native readonly Line linedef;
	native int16	Light;
	native uint16	Flags;

	native TextureID GetTexture(int which);
	native void SetTexture(int which, TextureID tex);
	native void SetTextureXOffset(int which, double offset);
	native double GetTextureXOffset(int which);
	native void AddTextureXOffset(int which, double delta);
	native void SetTextureYOffset(int which, double offset);
	native double GetTextureYOffset(int which);
	native void AddTextureYOffset(int which, double delta);
	native void SetTextureXScale(int which, double scale);
	native double GetTextureXScale(int which);
	native void MultiplyTextureXScale(int which, double delta);
	native void SetTextureYScale(int which, double scale);
	native double GetTextureYScale(int which);
	native void MultiplyTextureYScale(int which, double delta);
	native void SetSpecialColor(int tier, int position, Color scolor);
	native Color GetAdditiveColor(int tier);
	native void SetAdditiveColor(int tier, Color color);
	native void EnableAdditiveColor(int tier, bool enable);
	native void SetColorization(int tier, Name cname);
	//native DInterpolation *SetInterpolation(int position);
	//native void StopInterpolation(int position);

	native clearscope Vertex V1();
	native clearscope Vertex V2();

	native clearscope int Index();
	
	int GetUDMFInt(Name nm)
	{
		return Level.GetUDMFInt(LevelLocals.UDMF_Side, Index(), nm);
	}
	double GetUDMFFloat(Name nm)
	{
		return Level.GetUDMFFloat(LevelLocals.UDMF_Side, Index(), nm);
	}
	String GetUDMFString(Name nm)
	{
		return Level.GetUDMFString(LevelLocals.UDMF_Side, Index(), nm);
	}
	
};

struct Line native play
{
	enum ESide
	{
		front=0,
		back=1
	}

	enum ELineFlags
	{
		ML_BLOCKING					=0x00000001,	// solid, is an obstacle
		ML_BLOCKMONSTERS			=0x00000002,	// blocks monsters only
		ML_TWOSIDED					=0x00000004,	// backside will not be present at all if not two sided
		ML_DONTPEGTOP				= 0x00000008,	// upper texture unpegged
		ML_DONTPEGBOTTOM			= 0x00000010,	// lower texture unpegged
		ML_SECRET					= 0x00000020,	// don't map as two sided: IT'S A SECRET!
		ML_SOUNDBLOCK				= 0x00000040,	// don't let sound cross two of these
		ML_DONTDRAW 				= 0x00000080,	// don't draw on the automap
		ML_MAPPED					= 0x00000100,	// set if already drawn in automap
		ML_REPEAT_SPECIAL			= 0x00000200,	// special is repeatable
		ML_ADDTRANS					= 0x00000400,	// additive translucency (can only be set internally)

		// Extended flags
		ML_MONSTERSCANACTIVATE		= 0x00002000,	// [RH] Monsters (as well as players) can activate the line
		ML_BLOCK_PLAYERS			= 0x00004000,
		ML_BLOCKEVERYTHING			= 0x00008000,	// [RH] Line blocks everything
		ML_ZONEBOUNDARY				= 0x00010000,
		ML_RAILING					= 0x00020000,
		ML_BLOCK_FLOATERS			= 0x00040000,
		ML_CLIP_MIDTEX				= 0x00080000,	// Automatic for every Strife line
		ML_WRAP_MIDTEX				= 0x00100000,
		ML_3DMIDTEX					= 0x00200000,
		ML_CHECKSWITCHRANGE			= 0x00400000,
		ML_FIRSTSIDEONLY			= 0x00800000,	// activated only when crossed from front side
		ML_BLOCKPROJECTILE			= 0x01000000,
		ML_BLOCKUSE					= 0x02000000,	// blocks all use actions through this line
		ML_BLOCKSIGHT				= 0x04000000,	// blocks monster line of sight
		ML_BLOCKHITSCAN				= 0x08000000,	// blocks hitscan attacks
		ML_3DMIDTEX_IMPASS			= 0x10000000,	// [TP] if 3D midtex, behaves like a height-restricted ML_BLOCKING
	};


	native readonly vertex			v1, v2;		// vertices, from v1 to v2
	native readonly Vector2			delta;		// precalculated v2 - v1 for side checking
	native uint						flags;
	native uint						activation;	// activation type
	native int						special;
	native int						args[5];	// <--- hexen-style arguments (expanded to ZDoom's full width)
	native double					alpha;		// <--- translucency (0=invisible, 1.0=opaque)
	native readonly Side			sidedef[2];
	native readonly double			bbox[4];	// bounding box, for the extent of the LineDef.
	native readonly Sector			frontsector, backsector;
	native int 						validcount;	// if == validcount, already checked
	native int						locknumber;	// [Dusk] lock number for special
	native readonly uint			portalindex;
	native readonly uint			portaltransferred;

	native readonly int		health;
	native readonly int		healthgroup;
	
	native bool isLinePortal();
	native bool isVisualPortal();
	native Line getPortalDestination();
	native int getPortalAlignment();
	native clearscope int Index();
	native bool Activate(Actor activator, int side, int type);
	native bool RemoteActivate(Actor activator, int side, int type, Vector3 pos);
	
	int GetUDMFInt(Name nm)
	{
		return Level.GetUDMFInt(LevelLocals.UDMF_Line, Index(), nm);
	}
	double GetUDMFFloat(Name nm)
	{
		return Level.GetUDMFFloat(LevelLocals.UDMF_Line, Index(), nm);
	}
	String GetUDMFString(Name nm)
	{
		return Level.GetUDMFString(LevelLocals.UDMF_Line, Index(), nm);
	}

    native clearscope int GetHealth();
    native void SetHealth(int newhealth);
}

struct SecPlane native play
{
	native Vector3 Normal;
	native double D;
	native double negiC;
	
	native bool isSlope() const;
	native int PointOnSide(Vector3 pos) const;
	native clearscope double ZatPoint (Vector2 v) const;
	native double ZatPointDist(Vector2 v, double dist) const;
	native bool isEqual(Secplane other) const;
	native void ChangeHeight(double hdiff);
	native double GetChangedHeight(double hdiff) const;
	native double HeightDiff(double oldd, double newd = 1e37) const;
	native double PointToDist(Vector2 xy, double z) const;
}

struct F3DFloor native play
{
	enum EF3DFloorFlags
	{
		FF_EXISTS        = 0x1,    //MAKE SURE IT'S VALID
		FF_SOLID         = 0x2,    //Does it clip things?
		FF_RENDERSIDES   = 0x4,    //Render the sides?
		FF_RENDERPLANES  = 0x8,    //Render the floor/ceiling?
		FF_RENDERALL     = 0xC,    //Render everything?
		FF_SWIMMABLE     = 0x10,   //Can we swim?
		FF_NOSHADE       = 0x20,   //Does it mess with the lighting?
		FF_BOTHPLANES    = 0x200,  //Render both planes all the time?
		FF_TRANSLUCENT   = 0x800,  //See through!
		FF_FOG           = 0x1000, //Fog "brush"?
		FF_INVERTPLANES  = 0x2000, //Reverse the plane visibility rules?
		FF_ALLSIDES      = 0x4000, //Render inside and outside sides?
		FF_INVERTSIDES   = 0x8000, //Only render inside sides?
		FF_DOUBLESHADOW  = 0x10000,//Make two lightlist entries to reset light?
		FF_UPPERTEXTURE	 = 0x20000,
		FF_LOWERTEXTURE  = 0x40000,
		FF_THINFLOOR     = 0x80000,	// EDGE
		FF_SCROLLY       = 0x100000,  // old leftover definition
		FF_NODAMAGE      = 0x100000,  // no damage transfers
		FF_FIX           = 0x200000,  // use floor of model sector as floor and floor of real sector as ceiling
		FF_INVERTSECTOR  = 0x400000,	// swap meaning of sector planes
		FF_DYNAMIC       = 0x800000,	// created by partitioning another 3D-floor due to overlap
		FF_CLIPPED       = 0x1000000,	// split into several dynamic ffloors
		FF_SEETHROUGH    = 0x2000000,
		FF_SHOOTTHROUGH  = 0x4000000,
		FF_FADEWALLS     = 0x8000000,	// Applies real fog to walls and doesn't blend the view		
		FF_ADDITIVETRANS = 0x10000000, // Render this floor with additive translucency
		FF_FLOOD         = 0x20000000, // extends towards the next lowest flooding or solid 3D floor or the bottom of the sector
		FF_THISINSIDE    = 0x40000000, // hack for software 3D with FF_BOTHPLANES
		FF_RESET         = 0x80000000, // light effect is completely reset, once interrupted  
	};

	native readonly secplane bottom;
	native readonly secplane top;

	native readonly uint flags;
	native readonly Line master;

	native readonly Sector model;
	native readonly Sector target;

	native readonly int alpha;

	native TextureID GetTexture(int pos);
}

// This encapsulates all info Doom's original 'special' field contained - for saving and transferring.
struct SecSpecial play
{
	Name damagetype;
	int damageamount;
	short special;
	short damageinterval;
	short leakydamage;
	int Flags;
}

struct FColormap
{
	Color		LightColor;
	Color		FadeColor;
	uint8		Desaturation;
	uint8		BlendFactor;
	uint16		FogDensity;
}

struct Sector native play
{

	native readonly FColormap 	ColorMap;
	native readonly Color		SpecialColors[5];
	native readonly Color		AdditiveColors[5];

	native Actor 		SoundTarget;

	native int16 		special;
	native int16 		lightlevel;
	native int16		seqType;	

	native int			sky;
	native Name			SeqName;	

	native readonly Vector2		centerspot;	
	native int 			validcount;	
	native Actor		thinglist;	

	native double		friction, movefactor;
	native int			terrainnum[2];

	// thinker_t for reversable actions
	native SectorEffect floordata;
	native SectorEffect ceilingdata;
	native SectorEffect lightingdata;

	enum EPlane
	{
		floor,
		ceiling,
		// only used for specialcolors array
		walltop,
		wallbottom,
		sprites
	};
	
	enum EInterpolationType
	{
		CeilingMove,
		FloorMove,
		CeilingScroll,
		FloorScroll
	};
	//Interpolation interpolations[4];

	native uint8 		soundtraversed;
	native int8 		stairlock;
	native int 			prevsec;
	native int	 		nextsec;

	native readonly Array<Line> lines;

	native readonly @secplane floorplane;
	native readonly @secplane ceilingplane;
	
	native readonly Sector		heightsec;

	native uint			bottommap, midmap, topmap;

	//struct msecnode_t *touching_thinglist;
	//struct msecnode_t *sectorportal_thinglist;

	native double 		gravity;	
	native Name 		damagetype;
	native int 			damageamount;
	native int16 		damageinterval;
	native int16 		leakydamage;

	native readonly uint16	ZoneNumber;

	native readonly int healthceiling;
	native readonly int healthfloor;
	native readonly int healthceilinggroup;
	native readonly int healthfloorgroup;
	
	enum ESectorMoreFlags
	{
		SECMF_FAKEFLOORONLY	= 2,	// when used as heightsec in R_FakeFlat, only copies floor
		SECMF_CLIPFAKEPLANES = 4,	// as a heightsec, clip planes to target sector's planes
		SECMF_NOFAKELIGHT	= 8,	// heightsec does not change lighting
		SECMF_IGNOREHEIGHTSEC= 16,	// heightsec is only for triggering sector actions
		SECMF_UNDERWATER		= 32,	// sector is underwater
		SECMF_FORCEDUNDERWATER= 64,	// sector is forced to be underwater
		SECMF_UNDERWATERMASK	= 32+64,
		SECMF_DRAWN			= 128,	// sector has been drawn at least once
		SECMF_HIDDEN			= 256,	// Do not draw on textured automap
	}
	native uint16		MoreFlags;
	
	enum ESectorFlags
	{
		SECF_SILENT			= 1,	// actors in sector make no noise
		SECF_NOFALLINGDAMAGE= 2,	// No falling damage in this sector
		SECF_FLOORDROP		= 4,	// all actors standing on this floor will remain on it when it lowers very fast.
		SECF_NORESPAWN		= 8,	// players can not respawn in this sector
		SECF_FRICTION		= 16,	// sector has friction enabled
		SECF_PUSH			= 32,	// pushers enabled
		SECF_SILENTMOVE		= 64,	// Sector movement makes mo sound (Eternity got this so this may be useful for an extended cross-port standard.) 
		SECF_DMGTERRAINFX	= 128,	// spawns terrain splash when inflicting damage
		SECF_ENDGODMODE		= 256,	// getting damaged by this sector ends god mode
		SECF_ENDLEVEL		= 512,	// ends level when health goes below 10
		SECF_HAZARD			= 1024,	// Change to Strife's delayed damage handling.
		SECF_NOATTACK		= 2048,	// monsters cannot start attacks in this sector.

		SECF_WASSECRET		= 1 << 30,	// a secret that was discovered
		SECF_SECRET			= 1 << 31,	// a secret sector

		SECF_DAMAGEFLAGS = SECF_ENDGODMODE|SECF_ENDLEVEL|SECF_DMGTERRAINFX|SECF_HAZARD,
		SECF_NOMODIFY = SECF_SECRET|SECF_WASSECRET,	// not modifiable by Sector_ChangeFlags
		SECF_SPECIALFLAGS = SECF_DAMAGEFLAGS|SECF_FRICTION|SECF_PUSH,	// these flags originate from 'special and must be transferrable by floor thinkers
	}
	
	enum EMoveResult 
	{ 
		MOVE_OK, 
		MOVE_CRUSHED, 
		MOVE_PASTDEST
	};
	
	native uint			Flags;

	native SectorAction		SecActTarget;

	native internal uint		Portals[2];
	native readonly int			PortalGroup;

	native readonly int			sectornum;

	native clearscope int Index();

	native double, Sector, F3DFloor NextHighestCeilingAt(double x, double y, double bottomz, double topz, int flags = 0);
	native double, Sector, F3DFloor NextLowestFloorAt(double x, double y, double z, int flags = 0, double steph = 0);

	native F3DFloor Get3DFloor(int index);
	native int Get3DFloorCount();
	native Sector GetAttached(int index);
	native int GetAttachedCount();

	native void RemoveForceField();
	deprecated("3.8", "Use Level.PointInSector instead") static clearscope Sector PointInSector(Vector2 pt)
	{
		return level.PointInSector(pt);
	}

	native bool PlaneMoving(int pos);
	native int GetFloorLight();
	native int GetCeilingLight();
	native Sector GetHeightSec();
	native void TransferSpecial(Sector model);
	native void GetSpecial(out SecSpecial spec);
	native void SetSpecial( SecSpecial spec);
	native int GetTerrain(int pos);
	native void CheckPortalPlane(int plane);
	native double, Sector HighestCeilingAt(Vector2 a);
	native double, Sector LowestFloorAt(Vector2 a);
	native double, double GetFriction(int plane);

	native void SetXOffset(int pos, double o);
	native void AddXOffset(int pos, double o);
	native double GetXOffset(int pos);
	native void SetYOffset(int pos, double o);
	native void AddYOffset(int pos, double o);
	native double GetYOffset(int pos, bool addbase = true);
	native void SetXScale(int pos, double o);
	native double GetXScale(int pos);
	native void SetYScale(int pos, double o);
	native double GetYScale(int pos);
	native void SetAngle(int pos, double o);
	native double GetAngle(int pos, bool addbase = true);
	native void SetBase(int pos, double y, double o);
	native void SetAlpha(int pos, double o);
	native double GetAlpha(int pos);
	native int GetFlags(int pos);
	native int GetVisFlags(int pos);
	native void ChangeFlags(int pos, int And, int Or);
	native int GetPlaneLight(int pos);
	native void SetPlaneLight(int pos, int level);
	native void SetColor(color c, int desat = 0);
	native void SetFade(color c);
	native void SetFogDensity(int dens);
	native double GetGlowHeight(int pos);
	native color GetGlowColor(int pos);
	native void SetGlowHeight(int pos, double height);
	native void SetGlowColor(int pos, color color);
	native void SetSpecialColor(int pos, color color);
	native void SetAdditiveColor(int pos, Color color);
	native void SetColorization(int tier, Name cname);
	
	native TextureID GetTexture(int pos);
	native void SetTexture(int pos, TextureID tex, bool floorclip = true);
	native double GetPlaneTexZ(int pos);
	native void SetPlaneTexZ(int pos, double val, bool dirtify = false);	// This mainly gets used by init code. The only place where it must set the vertex to dirty is the interpolation code.
	native void ChangeLightLevel(int newval);
	native void SetLightLevel(int newval);
	native int GetLightLevel();
	native void AdjustFloorClip();
	native bool IsLinked(Sector other, bool ceiling);

	native bool PortalBlocksView(int plane);
	native bool PortalBlocksSight(int plane);
	native bool PortalBlocksMovement(int plane);
	native bool PortalBlocksSound(int plane);
	native bool PortalIsLinked(int plane);
	native void ClearPortal(int plane);
	native double GetPortalPlaneZ(int plane);
	native Vector2 GetPortalDisplacement(int plane);
	native int GetPortalType(int plane);
	native int GetOppositePortalGroup(int plane);
	native double CenterFloor();
	native double CenterCeiling();

	native int MoveFloor(double speed, double dest, int crush, int direction, bool hexencrush, bool instant = false);
	native int MoveCeiling(double speed, double dest, int crush, int direction, bool hexencrush);

	native Sector NextSpecialSector(int type, Sector prev);
	native double, Vertex FindLowestFloorSurrounding();
	native double, Vertex FindHighestFloorSurrounding();
	native double, Vertex FindNextHighestFloor();
	native double, Vertex FindNextLowestFloor();
	native double, Vertex FindLowestCeilingSurrounding();
	native double, Vertex FindHighestCeilingSurrounding();
	native double, Vertex FindNextLowestCeiling();	
	native double, Vertex FindNextHighestCeiling();

	native double FindShortestTextureAround();
	native double FindShortestUpperAround();
	native Sector FindModelFloorSector(double floordestheight);
	native Sector FindModelCeilingSector(double floordestheight);
	native int FindMinSurroundingLight(int max);
	native double, Vertex FindLowestCeilingPoint();
	native double, Vertex FindHighestFloorPoint();

	native void SetEnvironment(String env);
	native void SetEnvironmentID(int envnum);

	native SeqNode StartSoundSequenceID (int chan, int sequence, int type, int modenum, bool nostop = false);
	native SeqNode StartSoundSequence (int chan, Name seqname, int modenum);
	native SeqNode CheckSoundSequence (int chan);
	native void StopSoundSequence(int chan);
	native bool IsMakingLoopingSound ();
	
	 bool isSecret()
	 {
		 return !!(Flags & SECF_SECRET);
	 }

	 bool wasSecret()
	 {
		 return !!(Flags & SECF_WASSECRET);
	 }

	 void ClearSecret()
	 {
		 Flags &= ~SECF_SECRET;
	 }
	 
	int GetUDMFInt(Name nm)
	{
		return Level.GetUDMFInt(LevelLocals.UDMF_Sector, Index(), nm);
	}
	double GetUDMFFloat(Name nm)
	{
		return Level.GetUDMFFloat(LevelLocals.UDMF_Sector, Index(), nm);
	}
	String GetUDMFString(Name nm)
	{
		return Level.GetUDMFString(LevelLocals.UDMF_Sector, Index(), nm);
	}

	//===========================================================================
	//
	//
	//
	//===========================================================================

	bool TriggerSectorActions(Actor thing, int activation)
	{
		let act = SecActTarget;
		bool res = false;

		while (act != null)
		{
			Actor next = act.tracer;

			if (act.TriggerAction(thing, activation))
			{
				if (act.bStandStill)
				{
					act.Destroy();
				}
				res = true;
			}
			act = SectorAction(next);
		}
		return res;
	}

	
    native clearscope int GetHealth(SectorPart part);
    native void SetHealth(SectorPart part, int newhealth);
}

class SectorTagIterator : Object native
{
	deprecated("3.8", "Use Level.CreateSectorTagIterator() instead") static SectorTagIterator Create(int tag, line defline = null)
	{
		return level.CreateSectorTagIterator(tag, defline);
	}
	native int Next();
	native int NextCompat(bool compat, int secnum);
}

class LineIdIterator : Object native
{
	deprecated("3.8", "Use Level.CreateLineIdIterator() instead") static LineIdIterator Create(int tag)
	{
		return level.CreateLineIdIterator(tag);
	}
	native int Next();
}
