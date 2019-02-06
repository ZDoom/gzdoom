#pragma once

//
// [RH]
// P_PILLAR
//

class DPillar : public DMover
{
	DECLARE_CLASS (DPillar, DMover)
	HAS_OBJECT_POINTERS
public:
	enum EPillar
	{
		pillarBuild,
		pillarOpen

	};

	void Construct (sector_t *sector, EPillar type, double speed, double height, double height2, int crush, bool hexencrush);

	void Serialize(FSerializer &arc);
	void Tick ();
	void OnDestroy() override;

protected:
	EPillar		m_Type;
	double		m_FloorSpeed;
	double		m_CeilingSpeed;
	double		m_FloorTarget;
	double		m_CeilingTarget;
	int			m_Crush;
	bool		m_Hexencrush;
	TObjPtr<DInterpolation*> m_Interp_Ceiling;
	TObjPtr<DInterpolation*> m_Interp_Floor;
};

