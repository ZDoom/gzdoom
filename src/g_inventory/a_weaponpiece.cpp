/*
** a_weaponpieces.cpp
** Implements generic weapon pieces
**
**---------------------------------------------------------------------------
** Copyright 2006-2016 Cheistoph Oelckers
** Copyright 2006-2016 Randy Heit
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

#include "a_pickups.h"
#include "a_weaponpiece.h"
#include "doomstat.h"
#include "serializer.h"

IMPLEMENT_CLASS(AWeaponHolder, false, false)

DEFINE_FIELD(AWeaponHolder, PieceMask);
DEFINE_FIELD(AWeaponHolder, PieceWeapon);

//===========================================================================
//
//
//
//===========================================================================

void AWeaponHolder::Serialize(FSerializer &arc)
{
	Super::Serialize(arc);
	arc("piecemask", PieceMask)
		("pieceweapon", PieceWeapon);
}

IMPLEMENT_CLASS(AWeaponPiece, false, true)

IMPLEMENT_POINTERS_START(AWeaponPiece)
	IMPLEMENT_POINTER(FullWeapon)
	IMPLEMENT_POINTER(WeaponClass)
IMPLEMENT_POINTERS_END

//===========================================================================
//
//
//
//===========================================================================

void AWeaponPiece::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	auto def = (AWeaponPiece*)GetDefault();
	arc("weaponclass", WeaponClass, def->WeaponClass)
		("fullweapon", FullWeapon)
		("piecevalue", PieceValue, def->PieceValue);
}

DEFINE_FIELD(AWeaponPiece, PieceValue);
DEFINE_FIELD(AWeaponPiece, WeaponClass);
DEFINE_FIELD(AWeaponPiece, FullWeapon);
