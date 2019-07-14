//-----------------------------------------------------------------------------
//
// Copyright 2018 Christoph Oelckers
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

#include "info.h"
#include "actor.h"
#include "vm.h"

bool P_MorphActor(AActor *activator, AActor *victim, PClassActor *ptype, PClassActor *mtype, int duration, int style, PClassActor *enter_flash, PClassActor *exit_flash)
{
	IFVIRTUALPTR(victim, AActor, Morph)
	{
		VMValue params[] = { victim, activator, ptype, mtype, duration, style, enter_flash, exit_flash };
		int retval;
		VMReturn ret(&retval);
		VMCall(func, params, countof(params), &ret, 1);
		return !!retval;
	}
	return false;
}

bool P_UnmorphActor(AActor *activator, AActor *morphed, int flags, bool force)
{
	IFVIRTUALPTR(morphed, AActor, UnMorph)
	{
		VMValue params[] = { morphed, activator, flags, force };
		int retval;
		VMReturn ret(&retval);
		VMCall(func, params, countof(params), &ret, 1);
		return !!retval;
	}
	return false;
}


