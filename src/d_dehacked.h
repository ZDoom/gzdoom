/*
** d_dehacked.h
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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

#ifndef __D_DEHACK_H__
#define __D_DEHACK_H__

#include "a_pickups.h"

class ADehackedPickup : public AInventory
{
	DECLARE_CLASS (ADehackedPickup, AInventory)
	HAS_OBJECT_POINTERS
public:
	void Destroy ();
	const char *PickupMessage ();
	bool ShouldRespawn ();
	bool ShouldStay ();
	bool TryPickup (AActor *&toucher);
	void PlayPickupSound (AActor *toucher);
	void DoPickupSpecial (AActor *toucher);
	void Serialize(FArchive &arc);
private:
	const PClass *DetermineType ();
	AInventory *RealPickup;
public:
	bool droppedbymonster;
};

int D_LoadDehLumps();
bool D_LoadDehLump(int lumpnum);
bool D_LoadDehFile(const char *filename);
void FinishDehPatch ();

#endif //__D_DEHACK_H__
