#include "dsectoreffect.h"

#include "p_local.h"

IMPLEMENT_SERIAL (DSectorEffect, DThinker)

DSectorEffect::DSectorEffect ()
{
	m_Sector = NULL;
}

DSectorEffect::~DSectorEffect ()
{
	if (m_Sector)
	{
		if (m_Sector->floordata == this)
			m_Sector->floordata = NULL;
		if (m_Sector->ceilingdata == this)
			m_Sector->ceilingdata = NULL;
		if (m_Sector->lightingdata == this)
			m_Sector->lightingdata = NULL;
	}
}

DSectorEffect::DSectorEffect (sector_t *sector)
{
	m_Sector = sector;
}

void DSectorEffect::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	if (arc.IsStoring ())
		arc << m_Sector;
	else
		arc >> m_Sector;

}

IMPLEMENT_SERIAL (DMover, DSectorEffect)

DMover::DMover ()
{
}

DMover::DMover (sector_t *sector)
	: DSectorEffect (sector)
{
}

void DMover::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
}

IMPLEMENT_SERIAL (DMovingFloor, DMover)

DMovingFloor::DMovingFloor ()
{
}

DMovingFloor::DMovingFloor (sector_t *sector)
	: DMover (sector)
{
	sector->floordata = this;
}

void DMovingFloor::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
}

IMPLEMENT_SERIAL (DMovingCeiling, DMover)

DMovingCeiling::DMovingCeiling ()
{
}

DMovingCeiling::DMovingCeiling (sector_t *sector)
	: DMover (sector)
{
	sector->ceilingdata = this;
}

void DMovingCeiling::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
}

//
// Move a plane (floor or ceiling) and check for crushing
// [RH] Crush specifies the actual amount of crushing damage inflictable.
//		(Use -1 to prevent it from trying to crush)
//
DMover::EResult DMover::MovePlane (fixed_t speed, fixed_t dest, int crush,
								   int floorOrCeiling, int direction)
{
	bool	 	flag;
	fixed_t 	lastpos;
	fixed_t		destheight;	//jff 02/04/98 used to keep floors/ceilings
							// from moving thru each other

	switch (floorOrCeiling)
	{
	case 0:
		// FLOOR
		switch (direction)
		{
		case -1:
			// DOWN
			if (m_Sector->floorheight - speed < dest)
			{
				lastpos = m_Sector->floorheight;
				m_Sector->floorheight = dest;
				flag = P_ChangeSector (m_Sector, crush);
				if (flag == true)
				{
					m_Sector->floorheight = lastpos;
					P_ChangeSector (m_Sector, crush);
					//return crushed;
				}
				return pastdest;
			}
			else
			{
				lastpos = m_Sector->floorheight;
				m_Sector->floorheight -= speed;
				flag = P_ChangeSector (m_Sector, crush);
				if (flag == true)
				{
					m_Sector->floorheight = lastpos;
					P_ChangeSector (m_Sector, crush);
					return crushed;
				}
			}
			break;
												
		case 1:
			// UP
			// jff 02/04/98 keep floor from moving thru ceilings
			destheight = (dest < m_Sector->ceilingheight) ? dest : m_Sector->ceilingheight;
			if (m_Sector->floorheight + speed > destheight)
			{
				lastpos = m_Sector->floorheight;
				m_Sector->floorheight = destheight;
				flag = P_ChangeSector (m_Sector, crush);
				if (flag == true)
				{
					m_Sector->floorheight = lastpos;
					P_ChangeSector (m_Sector, crush);
					//return crushed;
				}
				return pastdest;
			}
			else
			{
				// COULD GET CRUSHED
				lastpos = m_Sector->floorheight;
				m_Sector->floorheight += speed;
				flag = P_ChangeSector (m_Sector, crush);
				if (flag == true)
				{
					if (crush)
						return crushed;
					m_Sector->floorheight = lastpos;
					P_ChangeSector (m_Sector, crush);
					return crushed;
				}
			}
			break;
		}
		break;
																		
	  case 1:
		// CEILING
		switch(direction)
		{
		case -1:
			// DOWN
			// jff 02/04/98 keep ceiling from moving thru floors
			destheight = (dest > m_Sector->floorheight) ? dest : m_Sector->floorheight;
			if (m_Sector->ceilingheight - speed < destheight)
			{
				lastpos = m_Sector->ceilingheight;
				m_Sector->ceilingheight = destheight;
				flag = P_ChangeSector (m_Sector, crush);

				if (flag == true)
				{
					m_Sector->ceilingheight = lastpos;
					P_ChangeSector (m_Sector, crush);
					//return crushed;
				}
				return pastdest;
			}
			else
			{
				// COULD GET CRUSHED
				lastpos = m_Sector->ceilingheight;
				m_Sector->ceilingheight -= speed;
				flag = P_ChangeSector (m_Sector, crush);

				if (flag == true)
				{
					if (crush)
						return crushed;
					m_Sector->ceilingheight = lastpos;
					P_ChangeSector (m_Sector, crush);
					return crushed;
				}
			}
			break;
												
		case 1:
			// UP
			if (m_Sector->ceilingheight + speed > dest)
			{
				lastpos = m_Sector->ceilingheight;
				m_Sector->ceilingheight = dest;
				flag = P_ChangeSector (m_Sector, crush);
				if (flag == true)
				{
					m_Sector->ceilingheight = lastpos;
					P_ChangeSector (m_Sector, crush);
					//return crushed;
				}
				return pastdest;
			}
			else
			{
				lastpos = m_Sector->ceilingheight;
				m_Sector->ceilingheight += speed;
				flag = P_ChangeSector (m_Sector, crush);
// UNUSED
#if 0
				if (flag == true)
				{
					m_Sector->ceilingheight = lastpos;
					P_ChangeSector (m_Sector, crush);
					return crushed;
				}
#endif
			}
			break;
		}
		break;
				
	}
	return ok;
}
