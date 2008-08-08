/*
** a_mapmarker.cpp
** An actor that appears on the automap instead of in the 3D view.
**
**---------------------------------------------------------------------------
** Copyright 2006 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
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

#include "a_sharedglobal.h"
#include "statnums.h"

// Map Marker --------------------------------------------------------------
//
// This class uses the following argument:
//   args[0] == 0, shows the sprite at this actor
//           != 0, shows the sprite for all actors whose TIDs match instead
//
//   args[1] == 0, show the sprite always
//           == 1, show the sprite only after its sector has been drawn
//
// To enable display of the sprite, activate it. To turn off the sprite,
// deactivate it.
//
// All the code to display it is in am_map.cpp.
//
//--------------------------------------------------------------------------

IMPLEMENT_CLASS(AMapMarker)

void AMapMarker::BeginPlay ()
{
	ChangeStatNum (STAT_MAPMARKER);
}

void AMapMarker::Activate (AActor *activator)
{
	flags2 |= MF2_DORMANT;
}

void AMapMarker::Deactivate (AActor *activator)
{
	flags2 &= ~MF2_DORMANT;
}
