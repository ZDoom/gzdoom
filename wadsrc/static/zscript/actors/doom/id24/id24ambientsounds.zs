/*****************************************************************************
 * Copyright (C) 1993-2024 id Software LLC, a ZeniMax Media company.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 ****************************************************************************/

/*****************************************************************************
 * id1 - decohack - ambient sounds
 ****************************************************************************/

class ID24AmbientKlaxon : Actor
{
	Default
	{
		Radius 8;
		Height 16;
	}
	States
	{
	Spawn:
		TNT1 A 1 A_Look;
		Loop;
	See:
		TNT1 A 35 A_StartSound("ambient/klaxon");
		Loop;
	}
}

class ID24AmbientPortalOpen : ID24AmbientKlaxon
{
	States
	{
	See:
		TNT1 A 250 A_StartSound("ambient/portalopen", 1);
		Stop;
	}
}

class ID24AmbientPortalLoop : ID24AmbientKlaxon
{
	States
	{
	See:
		TNT1 A 139 A_StartSound("ambient/portalloop");
		Loop;
	}
}

class ID24AmbientPortalClose : ID24AmbientKlaxon
{
	States
	{
	See:
		TNT1 A 105 A_StartSound("ambient/portalclose", 1);
		Stop;
	}
}
