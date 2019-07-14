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
	virtual Actor, int, int MorphedDeath()
	{
		return null, 0, 0;
	}

	
	//===========================================================================
	//
	// Main entry point
	//
	//===========================================================================

	virtual bool Morph(Actor activator, class<PlayerPawn> playerclass, class<MorphedMonster> monsterclass, int duration = 0, int style = 0, class<Actor> morphflash = null, class<Actor>unmorphflash = null)
	{
		if (player != null && player.mo != null && playerclass != null)
		{
			return player.mo.MorphPlayer(activator? activator.player : null, playerclass, duration, style, morphflash, unmorphflash);
		}
		else
		{
			return MorphMonster(monsterclass, duration, style, morphflash, unmorphflash);
		}
	}
	
	//===========================================================================
	//
	// Action function variant whose arguments differ from the generic one.
	//
	//===========================================================================

	bool A_Morph(class<Actor> type, int duration = 0, int style = 0, class<Actor> morphflash = null, class<Actor>unmorphflash = null)
	{
		if (self.player != null)
		{
			let playerclass = (class<PlayerPawn>)(type);
			if (playerclass && self.player.mo != null) return player.mo.MorphPlayer(self.player, playerclass, duration, style, morphflash, unmorphflash);
		}
		else
		{
			return MorphMonster(type, duration, style, morphflash, unmorphflash);
		}
		return false;
	}

	//===========================================================================
	//
	// Main entry point
	//
	//===========================================================================

	virtual bool UnMorph(Actor activator, int flags, bool force)
	{
		if (player)
		{
			return player.mo.UndoPlayerMorph(activator? activator.player : null, flags, force);
		}
		else 
		{
			let morphed = MorphedMonster(self);
			if (morphed)
				return morphed.UndoMonsterMorph(force);
		}
		return false;
	}

	
	//---------------------------------------------------------------------------
	//
	// FUNC P_MorphMonster
	//
	// Returns true if the monster gets turned into a chicken/pig.
	//
	//---------------------------------------------------------------------------

	virtual bool MorphMonster (Class<Actor> spawntype, int duration, int style, Class<Actor> enter_flash, Class<Actor> exit_flash)
	{
		if (player || spawntype == NULL || bDontMorph || !bIsMonster || !(spawntype is 'MorphedMonster'))
		{
			return false;
		}

		let morphed = MorphedMonster(Spawn (spawntype, Pos, NO_REPLACE));
		Substitute (morphed);
		if ((style & MRF_TRANSFERTRANSLATION) && !morphed.bDontTranslate)
		{
			morphed.Translation = Translation;
		}
		morphed.ChangeTid(tid);
		ChangeTid(0);
		morphed.Angle = Angle;
		morphed.UnmorphedMe = self;
		morphed.Alpha = Alpha;
		morphed.RenderStyle = RenderStyle;
		morphed.Score = Score;

		morphed.UnmorphTime = level.time + ((duration) ? duration : DEFMORPHTICS) + random[morphmonst]();
		morphed.MorphStyle = style;
		morphed.MorphExitFlash = (exit_flash) ? exit_flash : (class<Actor>)("TeleportFog");
		morphed.FlagsSave = bSolid * 2 + bShootable * 4 + bInvisible * 0x40;	// The factors are for savegame compatibility
		
		morphed.special = special;
		morphed.args[0] = args[0];
		morphed.args[1] = args[1];
		morphed.args[2] = args[2];
		morphed.args[3] = args[3];
		morphed.args[4] = args[4];
		morphed.CopyFriendliness (self, true);
		morphed.bShadow |= bShadow;
		morphed.bGhost |= bGhost;
		special = 0;
		bSolid = false;
		bShootable = false;
		bUnmorphed = true;
		bInvisible = true;
		let eflash = Spawn(enter_flash ? enter_flash : (class<Actor>)("TeleportFog"), Pos + (0, 0, gameinfo.TELEFOGHEIGHT), ALLOW_REPLACE);
		if (eflash)
			eflash.target = morphed;
		return true;
	}
}

