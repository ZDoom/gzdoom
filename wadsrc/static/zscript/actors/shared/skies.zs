
/*
** a_skies.cpp
** Skybox-related actors
**
**---------------------------------------------------------------------------
** Copyright 1998-2016 Randy Heit
** Copyright 2006-2017 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, self list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, self list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from self software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

class SkyViewpoint : Actor
{
	default
	{
		+NOSECTOR
		+NOBLOCKMAP
		+NOGRAVITY
		+DONTSPLASH
	}
	
	// arg0 = Visibility*4 for self skybox

	// If self actor has no TID, make it the default sky box
	override void BeginPlay ()
	{
		Super.BeginPlay ();

		if (tid == 0 && level.sectorPortals[0].mSkybox == null)
		{
			level.sectorPortals[0].mSkybox = self;
			level.sectorPortals[0].mDestination = CurSector;
		}
	}

	override void OnDestroy ()
	{
		// remove all sector references to ourselves.
		for (int i = 0; i < level.sectorPortals.Size(); i++)
		{
			SectorPortal s = level.sectorPortals[i];
			if (s.mSkybox == self)
			{
				s.mSkybox = null;
				// This is necessary to entirely disable EE-style skyboxes
				// if their viewpoint gets deleted.
				s.mFlags |= SectorPortal.FLAG_SKYFLATONLY;
			}
		}

		Super.OnDestroy();
	}
	
}

//---------------------------------------------------------------------------

// arg0 = tid of matching SkyViewpoint
// A value of 0 means to use a regular stretched texture, in case
// there is a default SkyViewpoint in the level.
//
// arg1 = 0: set both floor and ceiling skybox
//		= 1: set only ceiling skybox
//		= 2: set only floor skybox

class SkyPicker : Actor
{
	default
	{
		+NOSECTOR
		+NOBLOCKMAP
		+NOGRAVITY
		+DONTSPLASH
	}
	
	override void PostBeginPlay ()
	{
		Actor box;
		Super.PostBeginPlay ();

		if (args[0] == 0)
		{
			box = null;
		}
		else
		{
			let it = Level.CreateActorIterator(args[0], "SkyViewpoint");
			box = it.Next ();
		}

		if (box == null && args[0] != 0)
		{
			A_Log(String.Format("Can't find SkyViewpoint %d for sector %d\n", args[0], CurSector.Index()));
		}
		else
		{
			int boxindex = level.GetSkyboxPortal(box);
			// Do not override special portal types, only regular skies.
			if (0 == (args[1] & 2))
			{
				if (CurSector.GetPortalType(sector.ceiling) == SectorPortal.TYPE_SKYVIEWPOINT)
					CurSector.Portals[sector.ceiling] = boxindex;
			}
			if (0 == (args[1] & 1))
			{
				if (CurSector.GetPortalType(sector.floor) == SectorPortal.TYPE_SKYVIEWPOINT)
					CurSector.Portals[sector.floor] = boxindex;
			}
		}
		Destroy ();
	}
	
}

class SkyCamCompat : SkyViewpoint
{
	override void BeginPlay ()
	{
		// Skip SkyViewpoint's initialization, Actor's is not needed here.
	}
}

class StackPoint : SkyViewpoint
{
	override void BeginPlay ()
	{
		// Skip SkyViewpoint's initialization, Actor's is not needed here.
	}

}

class UpperStackLookOnly : StackPoint
{
}

class LowerStackLookOnly : StackPoint
{
}


class SectorSilencer : Actor
{
	default
	{
		+NOBLOCKMAP
		+NOGRAVITY
		+DONTSPLASH
		RenderStyle "None";
	}
	
	override void BeginPlay ()
	{
		Super.BeginPlay ();
		CurSector.Flags |= Sector.SECF_SILENT;
	}

	override void OnDestroy ()
	{
		if (CurSector != null)
		{
			CurSector.Flags &= ~Sector.SECF_SILENT;
		}
		Super.OnDestroy();
	}
}
