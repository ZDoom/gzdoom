
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
	native clearscope int Index() const;
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

	enum EPartFlags
	{
		NoGradient = 1,
		FlipGradient = 2,
		ClampGradient = 4,
		UseOwnSpecialColors = 8,
		UseOwnAdditiveColor = 16,
	};

	native readonly Sector sector;			// Sector the SideDef is facing.
	//DBaseDecal*	AttachedDecals;	// [RH] Decals bound to the wall
	native readonly Line linedef;
	native int16	Light;
	native uint16	Flags;

	native clearscope TextureID GetTexture(int which) const;
	native void SetTexture(int which, TextureID tex);
	native void SetTextureXOffset(int which, double offset);
	native clearscope double GetTextureXOffset(int which) const;
	native void AddTextureXOffset(int which, double delta);
	native void SetTextureYOffset(int which, double offset);
	native clearscope double GetTextureYOffset(int which) const;
	native void AddTextureYOffset(int which, double delta);
	native void SetTextureXScale(int which, double scale);
	native clearscope double GetTextureXScale(int which) const;
	native void MultiplyTextureXScale(int which, double delta);
	native void SetTextureYScale(int which, double scale);
	native clearscope double GetTextureYScale(int which) const;
	native void MultiplyTextureYScale(int which, double delta);
	native clearscope int GetTextureFlags(int tier) const;
	native void ChangeTextureFlags(int tier, int And, int Or);
	native void SetSpecialColor(int tier, int position, Color scolor, bool useowncolor = true);
	native clearscope Color GetAdditiveColor(int tier) const;
	native void SetAdditiveColor(int tier, Color color);
	native void EnableAdditiveColor(int tier, bool enable);
	native void SetColorization(int tier, Name cname);
	//native DInterpolation *SetInterpolation(int position);
	//native void StopInterpolation(int position);

	native clearscope Vertex V1() const;
	native clearscope Vertex V2() const;

	native clearscope int Index() const;
	
	clearscope int GetUDMFInt(Name nm) const
	{
		return Level.GetUDMFInt(LevelLocals.UDMF_Side, Index(), nm);
	}
	clearscope double GetUDMFFloat(Name nm) const
	{
		return Level.GetUDMFFloat(LevelLocals.UDMF_Side, Index(), nm);
	}
	clearscope String GetUDMFString(Name nm) const
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

	enum ELineFlags2
	{
		ML2_BLOCKLANDMONSTERS = 1,
	}

	native readonly vertex			v1, v2;		// vertices, from v1 to v2
	native readonly Vector2			delta;		// precalculated v2 - v1 for side checking
	native uint						flags;
	native uint						flags2;
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
	
	native clearscope bool isLinePortal() const;
	native clearscope bool isVisualPortal() const;
	native clearscope Line getPortalDestination() const;
	native clearscope int getPortalAlignment() const;
	native clearscope int Index() const;
	native bool Activate(Actor activator, int side, int type);
	native bool RemoteActivate(Actor activator, int side, int type, Vector3 pos);
	
	clearscope int GetUDMFInt(Name nm) const
	{
		return Level.GetUDMFInt(LevelLocals.UDMF_Line, Index(), nm);
	}
	clearscope double GetUDMFFloat(Name nm) const
	{
		return Level.GetUDMFFloat(LevelLocals.UDMF_Line, Index(), nm);
	}
	clearscope String GetUDMFString(Name nm) const
	{
		return Level.GetUDMFString(LevelLocals.UDMF_Line, Index(), nm);
	}

    native clearscope int GetHealth() const;
    native void SetHealth(int newhealth);
}

struct SecPlane native play
{
	native Vector3 Normal;
	native double D;
	native double negiC;
	
	native clearscope bool isSlope() const;
	native clearscope int PointOnSide(Vector3 pos) const;
	native clearscope double ZatPoint (Vector2 v) const;
	native clearscope double ZatPointDist(Vector2 v, double dist) const;
	native clearscope bool isEqual(Secplane other) const;
	native void ChangeHeight(double hdiff);
	native clearscope double GetChangedHeight(double hdiff) const;
	native clearscope double HeightDiff(double oldd, double newd = 1e37) const;
	native clearscope double PointToDist(Vector2 xy, double z) const;
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

	native clearscope TextureID GetTexture(int pos) const;
}

// This encapsulates all info Doom's original 'special' field contained - for saving and transferring.
struct SecSpecial play
{
	Name damagetype;
	int damageamount;
	int special;
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

	native int			special;
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

	native clearscope int Index() const;

	native clearscope double, Sector, F3DFloor NextHighestCeilingAt(double x, double y, double bottomz, double topz, int flags = 0) const;
	native clearscope double, Sector, F3DFloor NextLowestFloorAt(double x, double y, double z, int flags = 0, double steph = 0) const;

	native clearscope F3DFloor Get3DFloor(int index) const;
	native clearscope int Get3DFloorCount() const;
	native clearscope Sector GetAttached(int index) const;
	native clearscope int GetAttachedCount() const;

	native void RemoveForceField();
	deprecated("3.8", "Use Level.PointInSector instead") static clearscope Sector PointInSector(Vector2 pt)
	{
		return level.PointInSector(pt);
	}

	native clearscope bool PlaneMoving(int pos) const;
	native clearscope int GetFloorLight() const;
	native clearscope int GetCeilingLight() const;
	native clearscope Sector GetHeightSec() const;
	native void TransferSpecial(Sector model);
	native clearscope void GetSpecial(out SecSpecial spec) const;
	native void SetSpecial( SecSpecial spec);
	native clearscope int GetTerrain(int pos) const;
	native clearscope TerrainDef GetFloorTerrain(int pos) const;			// Gets the terraindef from floor/ceiling (see EPlane const).
	native void CheckPortalPlane(int plane);
	native clearscope double, Sector HighestCeilingAt(Vector2 a) const;
	native clearscope double, Sector LowestFloorAt(Vector2 a) const;
	native clearscope double, double GetFriction(int plane) const;

	native void SetXOffset(int pos, double o);
	native void AddXOffset(int pos, double o);
	native clearscope double GetXOffset(int pos) const;
	native void SetYOffset(int pos, double o);
	native void AddYOffset(int pos, double o);
	native clearscope double GetYOffset(int pos, bool addbase = true) const;
	native void SetXScale(int pos, double o);
	native clearscope double GetXScale(int pos) const;
	native void SetYScale(int pos, double o);
	native clearscope double GetYScale(int pos) const;
	native void SetAngle(int pos, double o);
	native clearscope double GetAngle(int pos, bool addbase = true) const;
	native void SetBase(int pos, double y, double o);
	native void SetAlpha(int pos, double o);
	native clearscope double GetAlpha(int pos) const;
	native clearscope int GetFlags(int pos) const;
	native clearscope int GetVisFlags(int pos) const;
	native void ChangeFlags(int pos, int And, int Or);
	native clearscope int GetPlaneLight(int pos) const;
	native void SetPlaneLight(int pos, int level);
	native void SetColor(color c, int desat = 0);
	native void SetFade(color c);
	native void SetFogDensity(int dens);
	native clearscope double GetGlowHeight(int pos) const;
	native clearscope color GetGlowColor(int pos) const;
	native void SetGlowHeight(int pos, double height);
	native void SetGlowColor(int pos, color color);
	native void SetSpecialColor(int pos, color color);
	native void SetAdditiveColor(int pos, Color color);
	native void SetColorization(int tier, Name cname);
	
	native clearscope TextureID GetTexture(int pos) const;
	native void SetTexture(int pos, TextureID tex, bool floorclip = true);
	native clearscope double GetPlaneTexZ(int pos) const;
	native void SetPlaneTexZ(int pos, double val, bool dirtify = false);	// This mainly gets used by init code. The only place where it must set the vertex to dirty is the interpolation code.
	native void ChangeLightLevel(int newval);
	native void SetLightLevel(int newval);
	native clearscope int GetLightLevel() const;
	native void AdjustFloorClip();
	native clearscope bool IsLinked(Sector other, bool ceiling) const;

	native clearscope bool PortalBlocksView(int plane) const;
	native clearscope bool PortalBlocksSight(int plane) const;
	native clearscope bool PortalBlocksMovement(int plane) const;
	native clearscope bool PortalBlocksSound(int plane) const;
	native clearscope bool PortalIsLinked(int plane) const;
	native void ClearPortal(int plane);
	native clearscope double GetPortalPlaneZ(int plane) const;
	native clearscope Vector2 GetPortalDisplacement(int plane) const;
	native clearscope int GetPortalType(int plane) const;
	native clearscope int GetOppositePortalGroup(int plane) const;
	native clearscope double CenterFloor() const;
	native clearscope double CenterCeiling() const;

	native int MoveFloor(double speed, double dest, int crush, int direction, bool hexencrush, bool instant = false);
	native int MoveCeiling(double speed, double dest, int crush, int direction, bool hexencrush);

	native clearscope Sector NextSpecialSector(int type, Sector prev) const;
	native clearscope double, Vertex FindLowestFloorSurrounding() const;
	native clearscope double, Vertex FindHighestFloorSurrounding() const;
	native clearscope double, Vertex FindNextHighestFloor() const;
	native clearscope double, Vertex FindNextLowestFloor() const;
	native clearscope double, Vertex FindLowestCeilingSurrounding() const;
	native clearscope double, Vertex FindHighestCeilingSurrounding() const;
	native clearscope double, Vertex FindNextLowestCeiling() const;	
	native clearscope double, Vertex FindNextHighestCeiling() const;

	native clearscope double FindShortestTextureAround() const;
	native clearscope double FindShortestUpperAround() const;
	native clearscope Sector FindModelFloorSector(double floordestheight) const;
	native clearscope Sector FindModelCeilingSector(double floordestheight) const;
	native clearscope int FindMinSurroundingLight(int max) const;
	native clearscope double, Vertex FindLowestCeilingPoint() const;
	native clearscope double, Vertex FindHighestFloorPoint() const;

	native void SetEnvironment(String env);
	native void SetEnvironmentID(int envnum);

	native SeqNode StartSoundSequenceID (int chan, int sequence, int type, int modenum, bool nostop = false);
	native SeqNode StartSoundSequence (int chan, Name seqname, int modenum);
	native clearscope SeqNode CheckSoundSequence (int chan) const;
	native void StopSoundSequence(int chan);
	native clearscope bool IsMakingLoopingSound () const;
	
	 clearscope bool isSecret() const
	 {
		 return !!(Flags & SECF_SECRET);
	 }

	 clearscope bool wasSecret() const
	 {
		 return !!(Flags & SECF_WASSECRET);
	 }

	 void ClearSecret()
	 {
		 Flags &= ~SECF_SECRET;
	 }
	 
	clearscope int GetUDMFInt(Name nm) const
	{
		return Level.GetUDMFInt(LevelLocals.UDMF_Sector, Index(), nm);
	}
	clearscope double GetUDMFFloat(Name nm) const
	{
		return Level.GetUDMFFloat(LevelLocals.UDMF_Sector, Index(), nm);
	}
	clearscope String GetUDMFString(Name nm) const
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

	
    native clearscope int GetHealth(SectorPart part) const;
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
