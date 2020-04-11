//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1999-2016 Randy Heit
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------
//

#include <fnmatch.h>

#ifdef __APPLE__
#include <AvailabilityMacros.h>
#endif // __APPLE__

#include "cmdlib.h"
#include "d_protocol.h"
#include "i_system.h"
#include "gameconfigfile.h"
#include "x86.h"


void I_Tactile(int /*on*/, int /*off*/, int /*total*/)
{
}

static ticcmd_t emptycmd;

ticcmd_t *I_BaseTiccmd()
{
	return &emptycmd;
}

//
// I_Init
//
void I_Init()
{
	extern void CalculateCPUSpeed();

	CheckCPUID(&CPU);
	CalculateCPUSpeed();
	DumpCPUInfo(&CPU);
}


bool I_WriteIniFailed()
{
	printf("The config file %s could not be saved:\n%s\n", GameConfig->GetPathName(), strerror(errno));
	return false; // return true to retry
}

TArray<FString> I_GetGogPaths()
{
	// GOG's Doom games are Windows only at the moment
	return TArray<FString>();
}
