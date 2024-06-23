//-----------------------------------------------------------------------------
//
// Copyright 1994-1996 Raven Software
// Copyright 1999-2016 Randy Heit
// Copyright 2002-2018 Christoph Oelckers
// Copyright 2005-2008 Martin Howe
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

extend class Actor
{
	// Blockmap, sector, and no interaction are the only relevant flags but the old ones are kept around
	// for legacy reasons.
	enum EPremorphProperty
	{
		MPROP_SOLID				= 1 << 1,
		MPROP_SHOOTABLE			= 1 << 2,
		MPROP_NO_BLOCKMAP		= 1 << 3,
		MPROP_NO_SECTOR			= 1 << 4,
		MPROP_NO_INTERACTION	= 1 << 5,
		MPROP_INVIS				= 1 << 6,
	}

	native int UnmorphTime;
	native int MorphFlags;
	native class<Actor> MorphExitFlash;
	native int PremorphProperties;

	// Players still track these separately for legacy reasons.
	void SetMorphStyle(EMorphFlags flags)
	{
		if (player)
			player.MorphStyle = flags;
		else
			MorphFlags = flags;
	}

	clearscope EMorphFlags GetMorphStyle() const
	{
		return player ? player.MorphStyle : MorphFlags;
	}

	void SetMorphExitFlash(class<Actor> flash)
	{
		if (player)
			player.MorphExitFlash = flash;
		else
			MorphExitFlash = flash;
	}

	clearscope class<Actor> GetMorphExitFlash() const
	{
		return player ? player.MorphExitFlash : MorphExitFlash;
	}

	void SetMorphTics(int dur)
	{
		if (player)
			player.MorphTics = dur;
		else
			UnmorphTime = Level.Time + dur;
	}

	clearscope int GetMorphTics() const
	{
		if (player)
			return player.MorphTics;

		if (UnmorphTime <= 0)
			return UnmorphTime;

		return UnmorphTime > Level.Time ? UnmorphTime - Level.Time : 0;
	}

	// This function doesn't return anything anymore since allowing so would be too volatile
	// for morphing management. Instead it's now a function that lets special actions occur
	// when a morphed Actor dies.
	virtual Actor, int, int MorphedDeath()
	{
		EMorphFlags mStyle = GetMorphStyle();
		if (mStyle & MRF_UNDOBYDEATH)
			Unmorph(self, force: mStyle & MRF_UNDOBYDEATHFORCED);

		return null, 0, 0;
	}

	// [MC] Called when an actor morphs, on both the previous form (!current) and present form (current).
	// Due to recent changes, these are now called internally instead of within the virtuals.
	virtual void PreMorph(Actor mo, bool current) {}
	virtual void PostMorph(Actor mo, bool current) {}
	virtual void PreUnmorph(Actor mo, bool current) {}
	virtual void PostUnmorph(Actor mo, bool current) {}
	
	//===========================================================================
	//
	// Main entry point
	//
	//===========================================================================

	virtual bool Morph(Actor activator, class<PlayerPawn> playerClass, class<Actor> monsterClass, int duration = 0, EMorphFlags style = 0, class<Actor> morphFlash = "TeleportFog", class<Actor> unmorphFlash = "TeleportFog")
	{
		if (player)
			return player.mo.MorphPlayer(activator ? activator.player : null, playerClass, duration, style, morphFlash, unmorphFlash);

		return MorphMonster(monsterClass, duration, style, morphFlash, unmorphFlash);
	}
	
	//===========================================================================
	//
	// Action function variant whose arguments differ from the generic one.
	//
	//===========================================================================

	bool A_Morph(class<Actor> type, int duration = 0, EMorphFlags style = 0, class<Actor> morphFlash = "TeleportFog", class<Actor> unmorphFlash = "TeleportFog")
	{
		return Morph(self, (class<PlayerPawn>)(type), type, duration, style, morphFlash, unmorphFlash);
	}

	//===========================================================================
	//
	// Main entry point
	//
	//===========================================================================

	virtual bool Unmorph(Actor activator, EMorphFlags flags = 0, bool force = false)
	{
		if (player)
			return player.mo.UndoPlayerMorph(activator ? activator.player : null, flags, force);

		return UndoMonsterMorph(force);
	}

	virtual bool CheckUnmorph()
	{
		return UnmorphTime && UnmorphTime <= Level.Time && Unmorph(self, MRF_UNDOBYTIMEOUT);
	}
	
	//---------------------------------------------------------------------------
	//
	// FUNC P_MorphMonster
	//
	// Returns true if the monster gets turned into a chicken/pig.
	//
	//---------------------------------------------------------------------------

	virtual bool MorphMonster(class<Actor> spawnType, int duration, EMorphFlags style, class<Actor> enterFlash = "TeleportFog", class<Actor> exitFlash = "TeleportFog")
	{
		if (player || !bIsMonster || !spawnType || bDontMorph || Health <= 0 || spawnType == GetClass())
			return false;

		Actor morphed = Spawn(spawnType, Pos, ALLOW_REPLACE);
		if (!MorphInto(morphed))
		{
			if (morphed)
			{
				morphed.ClearCounters();
				morphed.Destroy();
			}
			
			return false;
		}

		// [MC] Notify that we're just about to start the transfer.
		PreMorph(morphed, false);		// False: No longer the current.
		morphed.PreMorph(self, true);	// True: Becoming this actor.

		if ((style & MRF_TRANSFERTRANSLATION) && !morphed.bDontTranslate)
			morphed.Translation = Translation;

		morphed.Angle = Angle;
		morphed.Alpha = Alpha;
		morphed.RenderStyle = RenderStyle;
		morphed.bShadow |= bShadow;
		morphed.bGhost |= bGhost;
		morphed.Score = Score;
		morphed.Special = Special;
		for (int i; i < 5; ++i)
			morphed.Args[i] = Args[i];

		if (TID && (style & MRF_NEWTIDBEHAVIOUR))
		{
			morphed.ChangeTID(TID);
			ChangeTID(0);
		}

		morphed.Tracer = Tracer;
		morphed.Master = Master;
		morphed.CopyFriendliness(self, true);

		// Remove all armor.
		if (!(style & MRF_KEEPARMOR))
		{
			for (Inventory item = morphed.Inv; item;)
			{
				Inventory next = item.Inv;
				if (item is "Armor")
					item.DepleteOrDestroy();

				item = next;
			}
		}

		morphed.SetMorphTics((duration ? duration : DEFMORPHTICS) + Random[morphmonst]());
		morphed.SetMorphStyle(style);
		morphed.SetMorphExitFlash(exitFlash);
		morphed.PremorphProperties = (bSolid * MPROP_SOLID) | (bShootable * MPROP_SHOOTABLE)
										| (bNoBlockmap * MPROP_NO_BLOCKMAP) | (bNoSector * MPROP_NO_SECTOR)
										| (bNoInteraction * MPROP_NO_INTERACTION) | (bInvisible * MPROP_INVIS);

		// This is just here for backwards compatibility as MorphedMonster used to be required.
		let morphMon = MorphedMonster(morphed);
		if (morphMon)
		{
			morphMon.UnmorphedMe = morphMon.Alternative;
			morphMon.MorphStyle = morphMon.GetMorphStyle();
			morphMon.FlagsSave = morphMon.PremorphProperties;
		}

		Special = 0;
		bNoInteraction = true;
		A_ChangeLinkFlags(true, true);

		// Legacy
		bInvisible = true;
		bSolid = bShootable = false;

		PostMorph(morphed, false);
		morphed.PostMorph(self, true);
		
		if (enterFlash)
		{
			Actor fog = Spawn(enterFlash, morphed.Pos.PlusZ(GameInfo.TELEFOGHEIGHT), ALLOW_REPLACE);
			if (fog)
				fog.Target = morphed;
		}
		
		return true;
	}

	//----------------------------------------------------------------------------
	//
	// FUNC P_UndoMonsterMorph
	//
	// Returns true if the monster unmorphs.
	//
	//----------------------------------------------------------------------------

	virtual bool UndoMonsterMorph(bool force = false)
	{
		if (!Alternative || bStayMorphed || Alternative.bStayMorphed)
			return false;

		Actor alt = Alternative;
		alt.SetOrigin(Pos, false);
		if (!force && (PremorphProperties & MPROP_SOLID))
		{
			bool altSolid = alt.bSolid;
			bool isSolid = bSolid;
			bool isTouchy = bTouchy;

			alt.bSolid = true;
			bSolid = bTouchy = false;

			bool res = alt.TestMobjLocation();

			alt.bSolid = altSolid;
			bSolid = isSolid;
			bTouchy = isTouchy;

			// Didn't fit.
			if (!res)
			{
				SetMorphTics(5 * TICRATE);
				return false;
			}
		}

		if (!MorphInto(alt))
			return false;

		PreUnmorph(alt, false);
		alt.PreUnmorph(self, true);

		// Check to see if it had a powerup that caused it to morph.
		for (Inventory item = alt.Inv; item;)
		{
			Inventory next = item.Inv;
			if (item is "PowerMorph")
				item.Destroy();

			item = next;
		}

		alt.Angle = Angle;
		alt.bShadow = bShadow;
		alt.bGhost = bGhost;
		alt.bSolid = (PremorphProperties & MPROP_SOLID);
		alt.bShootable = (PremorphProperties & MPROP_SHOOTABLE);
		alt.bInvisible = (PremorphProperties & MPROP_INVIS);
		alt.Vel = Vel;
		alt.Score = Score;

		alt.bNoInteraction = (PremorphProperties & MPROP_NO_INTERACTION);
		alt.A_ChangeLinkFlags((PremorphProperties & MPROP_NO_BLOCKMAP), (PremorphProperties & MPROP_NO_SECTOR));

		EMorphFlags style = GetMorphStyle();
		if (TID && (style & MRF_NEWTIDBEHAVIOUR))
		{
			alt.ChangeTID(TID);
			ChangeTID(0);
		}

		alt.Special = Special;
		for (int i; i < 5; ++i)
			alt.Args[i] = Args[i];

		alt.Tracer = Tracer;
		alt.Master = Master;
		alt.CopyFriendliness(self, true, false);
		if (Health > 0 || (style & MRF_UNDOBYDEATHSAVES))
			alt.Health = alt.SpawnHealth();
		else
			alt.Health = Health;

		Special = 0;

		PostUnmorph(alt, false);		// From is false here: Leaving the caller's body.
		alt.PostUnmorph(self, true);	// True here: Entering this body from here.
		
		if (MorphExitFlash)
		{
			Actor fog = Spawn(MorphExitFlash, alt.Pos.PlusZ(GameInfo.TELEFOGHEIGHT), ALLOW_REPLACE);
			if (fog)
				fog.Target = alt;
		}

		ClearCounters();
		Destroy();
		return true;
	}
}

//===========================================================================
//
//
//
//===========================================================================

class MorphProjectile : Actor
{
	class<PlayerPawn> PlayerClass;
	class<Actor> MonsterClass, MorphFlash, UnmorphFlash;
	int Duration, MorphStyle;

	Default
	{
		Damage 1;
		Projectile;
		MorphProjectile.MorphFlash "TeleportFog";
		MorphProjectile.UnmorphFlash "TeleportFog";

		-ACTIVATEIMPACT
		-ACTIVATEPCROSS
	}
	
	override int DoSpecialDamage(Actor victim, int dmg, Name dmgType)
	{
		victim.Morph(Target, PlayerClass, MonsterClass, Duration, MorphStyle, MorphFlash, UnmorphFlash);
		return -1;
	}
}

//===========================================================================
//
// This class is redundant as it's no longer necessary for monsters to
// morph but is kept here for compatibility. Its previous fields either exist
// in the base Actor now or are just a shell for the actual fields
// which either already existed and weren't used for some reason or needed a
// better name.
//
//===========================================================================

class MorphedMonster : Actor
{
	Actor UnmorphedMe;
	EMorphFlags MorphStyle;
	EPremorphProperty FlagsSave;

	Default
	{
		Monster;

		+FLOORCLIP
		-COUNTKILL
	}

	// Make sure changes to these are propogated correctly when unmorphing. This is only
	// set for monsters since originally players and this class were the only ones
	// that were considered valid for morphing.
	override bool UndoMonsterMorph(bool force)
	{
		Alternative = UnmorphedMe;
		SetMorphStyle(MorphStyle);
		PremorphProperties = FlagsSave;
		return super.UndoMonsterMorph(force);
	}
}
