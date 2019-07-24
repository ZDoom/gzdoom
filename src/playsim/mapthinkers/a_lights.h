#pragma once

class DLighting : public DSectorEffect
{
	DECLARE_CLASS(DLighting, DSectorEffect)
public:
	static const int DEFAULT_STAT = STAT_LIGHT;
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
