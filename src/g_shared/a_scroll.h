#pragma once

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

