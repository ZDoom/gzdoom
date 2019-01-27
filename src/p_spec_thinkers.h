#pragma once

#include "p_spec.h"
#include "r_defs.h"

class DLightTransfer : public DThinker
{
	DECLARE_CLASS (DLightTransfer, DThinker)

public:
	static const int DEFAULT_STAT = STAT_LIGHTTRANSFER;
	void Construct(sector_t *srcSec, int target, bool copyFloor);
	void Serialize(FSerializer &arc);
	void Tick ();

protected:
	void DoTransfer (int level, int target, bool floor);

	sector_t *Source;
	int TargetTag;
	bool CopyFloor;
	short LastLight;
};

class DWallLightTransfer : public DThinker
{
	enum
	{
		WLF_SIDE1=1,
		WLF_SIDE2=2,
		WLF_NOFAKECONTRAST=4
	};

	DECLARE_CLASS (DWallLightTransfer, DThinker)
public:
	static const int DEFAULT_STAT = STAT_LIGHTTRANSFER;
	void Construct(sector_t *srcSec, int target, uint8_t flags);
	void Serialize(FSerializer &arc);
	void Tick ();

protected:
	void DoTransfer (short level, int target, uint8_t flags);

	sector_t *Source;
	int TargetID;
	short LastLight;
	uint8_t Flags;
};


class DFireFlicker : public DLighting
{
	DECLARE_CLASS(DFireFlicker, DLighting)
public:
	void Construct(sector_t *sector);
	void Construct(sector_t *sector, int upper, int lower);
	void		Serialize(FSerializer &arc);
	void		Tick();
protected:
	int 		m_Count;
	int 		m_MaxLight;
	int 		m_MinLight;
};

class DFlicker : public DLighting
{
	DECLARE_CLASS(DFlicker, DLighting)
public:
	void Construct(sector_t *sector, int upper, int lower);
	void		Serialize(FSerializer &arc);
	void		Tick();
protected:
	int 		m_Count;
	int 		m_MaxLight;
	int 		m_MinLight;
};

class DLightFlash : public DLighting
{
	DECLARE_CLASS(DLightFlash, DLighting)
public:
	void Construct(sector_t *sector);
	void Construct(sector_t *sector, int min, int max);
	void		Serialize(FSerializer &arc);
	void		Tick();
protected:
	int 		m_Count;
	int 		m_MaxLight;
	int 		m_MinLight;
	int 		m_MaxTime;
	int 		m_MinTime;
};

class DStrobe : public DLighting
{
	DECLARE_CLASS(DStrobe, DLighting)
public:
	void Construct(sector_t *sector, int utics, int ltics, bool inSync);
	void Construct(sector_t *sector, int upper, int lower, int utics, int ltics);
	void		Serialize(FSerializer &arc);
	void		Tick();
protected:
	int 		m_Count;
	int 		m_MinLight;
	int 		m_MaxLight;
	int 		m_DarkTime;
	int 		m_BrightTime;
};

class DGlow : public DLighting
{
	DECLARE_CLASS(DGlow, DLighting)
public:
	void Construct(sector_t *sector);
	void		Serialize(FSerializer &arc);
	void		Tick();
protected:
	int 		m_MinLight;
	int 		m_MaxLight;
	int 		m_Direction;
};

// [RH] Glow from Light_Glow and Light_Fade specials
class DGlow2 : public DLighting
{
	DECLARE_CLASS(DGlow2, DLighting)
public:
	void Construct(sector_t *sector, int start, int end, int tics, bool oneshot);
	void		Serialize(FSerializer &arc);
	void		Tick();
protected:
	int			m_Start;
	int			m_End;
	int			m_MaxTics;
	int			m_Tics;
	bool		m_OneShot;
};

// [RH] Phased light thinker
class DPhased : public DLighting
{
	DECLARE_CLASS(DPhased, DLighting)
public:
	void Construct(sector_t *sector, int baselevel = 0, int phase = 0);
	// These are for internal use only but the Create template needs access to them.
	void Construct();
	void Propagate();

	void		Serialize(FSerializer &arc);
	void		Tick();
protected:
	uint8_t		m_BaseLevel;
	uint8_t		m_Phase;
private:
	int PhaseHelper(sector_t *sector, int index, int light, sector_t *prev);
};

// phares 3/20/98: added new model of Pushers for push/pull effects

class DPusher : public DThinker
{
	DECLARE_CLASS (DPusher, DThinker)
	HAS_OBJECT_POINTERS
public:
	enum EPusher
	{
		p_push,
		p_pull,
		p_wind,
		p_current
	};

	void Construct(EPusher type, line_t *l, int magnitude, int angle, AActor *source, int affectee);
	void Serialize(FSerializer &arc);
	int CheckForSectorMatch (EPusher type, int tag);
	void ChangeValues (int magnitude, int angle)
	{
		DAngle ang = angle * (360. / 256.);
		m_PushVec = ang.ToVector(magnitude);
		m_Magnitude = magnitude;
	}

	void Tick ();

protected:
	EPusher m_Type;
	TObjPtr<AActor*> m_Source;// Point source if point pusher
	DVector2 m_PushVec;
	double m_Magnitude;		// Vector strength for point pusher
	double m_Radius;		// Effective radius for point pusher
	int m_Affectee;			// Number of affected sector

	friend bool PIT_PushThing (AActor *thing);
};

//-----------------------------------------------------------------------------
//
// killough 3/7/98: Add generalized scroll effects
//
//-----------------------------------------------------------------------------

class DScroller : public DThinker
{
	DECLARE_CLASS (DScroller, DThinker)
	HAS_OBJECT_POINTERS
public:
	static const int DEFAULT_STAT = STAT_SCROLLER;

	void Construct(EScroll type, double dx, double dy, sector_t *control, sector_t *sec, side_t *side, int accel, EScrollPos scrollpos = EScrollPos::scw_all);
	void Construct(double dx, double dy, const line_t *l, sector_t *control, int accel, EScrollPos scrollpos = EScrollPos::scw_all);
	void OnDestroy() override;

	void Serialize(FSerializer &arc);
	void Tick ();

	bool AffectsWall (side_t * wall) const { return m_Side == wall; }
	side_t *GetWall () const { return m_Side; }
	sector_t *GetSector() const { return m_Sector; }
	void SetRate (double dx, double dy) { m_dx = dx; m_dy = dy; }
	bool IsType (EScroll type) const { return type == m_Type; }
	EScrollPos GetScrollParts() const { return m_Parts; }

protected:
	EScroll m_Type;		// Type of scroll effect
	double m_dx, m_dy;		// (dx,dy) scroll speeds
	sector_t *m_Sector;		// Affected sector
	side_t *m_Side;			// ... or side
	sector_t *m_Controller;	// Control sector (nullptr if none) used to control scrolling
	double m_LastHeight;	// Last known height of control sector
	double m_vdx, m_vdy;	// Accumulated velocity if accelerative
	int m_Accel;			// Whether it's accelerative
	EScrollPos m_Parts;			// Which parts of a sidedef are being scrolled?
	TObjPtr<DInterpolation*> m_Interpolations[3];
};

