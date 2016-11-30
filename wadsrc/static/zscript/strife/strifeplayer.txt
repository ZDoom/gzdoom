// The player ---------------------------------------------------------------

class StrifePlayer : PlayerPawn
{
	Default
	{
		Health 100;
		Radius 18;
		Height 56;
		Mass 100;
		PainChance 255;
		Speed 1;
		MaxStepHeight 16;
		CrushPainSound "misc/pcrush";
		Player.DisplayName "Rebel";
		Player.StartItem "PunchDagger";
		Player.RunHealth 15;
		Player.WeaponSlot 1, "PunchDagger";
		Player.WeaponSlot 2, "StrifeCrossbow2", "StrifeCrossbow";
		Player.WeaponSlot 3, "AssaultGun";
		Player.WeaponSlot 4, "MiniMissileLauncher";
		Player.WeaponSlot 5, "StrifeGrenadeLauncher2", "StrifeGrenadeLauncher";
		Player.WeaponSlot 6, "FlameThrower";
		Player.WeaponSlot 7, "Mauler2", "Mauler";
		Player.WeaponSlot 8, "Sigil";

		Player.ColorRange 128, 143;
		Player.Colorset 0, "Brown",			0x80, 0x8F,	 0x82;
		Player.Colorset 1, "Red",			0x40, 0x4F,  0x42,  0x20, 0x3F, 0x00, 0x1F,  0xF1, 0xF6, 0xE0, 0xE5,  0xF7, 0xFB, 0xF1, 0xF5;
		Player.Colorset 2, "Rust",			0xB0, 0xBF,  0xB2,  0x20, 0x3F, 0x00, 0x1F;
		Player.Colorset 3, "Gray",			0x10, 0x1F,  0x12,  0x20, 0x2F, 0xD0, 0xDF,  0x30, 0x3F, 0xD0, 0xDF;
		Player.Colorset 4, "Dark Green",	0x30, 0x3F,  0x32,  0x20, 0x2F, 0xD0, 0xDF,  0x30, 0x3F, 0xD0, 0xDF;

		Player.Colorset 5, "Gold",			0x50, 0x5F,  0x52,	0x20, 0x3F, 0x00, 0x1F,  0x50, 0x5F, 0x80, 0x8F,  0xC0, 0xCF, 0xA0, 0xAF,                  0xD0, 0xDF, 0xB0, 0xBF;
		Player.Colorset 6, "Bright Green",	0x60, 0x6F,  0x62,	0x20, 0x3F, 0x00, 0x1F,  0x50, 0x5F, 0x10, 0x1F,  0xC0, 0xCF, 0x20, 0x2F,                  0xD0, 0xDF, 0x30, 0x3F;
		Player.Colorset 7, "Blue",			0x90, 0x9F,  0x92,	0x20, 0x3F, 0x00, 0x1F,  0x50, 0x5F, 0x40, 0x4F,  0xC1, 0xCF, 0x01, 0x0F,  0xC0,0xC0,1,1,  0xD0, 0xDF, 0x10, 0x1F;
	}

	States
	{
	Spawn:
		PLAY A -1;
		stop;
	See:
		PLAY ABCD 4;
		loop;
	Missile:
		PLAY E 12;
		goto Spawn;
	Melee:
		PLAY F 6;
		goto Missile;
	Pain:
		PLAY Q 4;
		PLAY Q 4 A_Pain;
		Goto Spawn;
	Death:
		PLAY H 3;
		PLAY I 3 A_PlayerScream;
		PLAY J 3 A_NoBlocking;
		PLAY KLMNO 4;
		PLAY P -1;
		Stop;
	XDeath:
		RGIB A 5 A_TossGib;
		RGIB B 5 A_XScream;
		RGIB C 5 A_NoBlocking;
		RGIB DEFG 5 A_TossGib;
		RGIB H -1 A_TossGib;
	Burn:
		BURN A 3 Bright A_ItBurnsItBurns;
		BURN B 3 Bright A_DropFire;
		BURN C 3 Bright A_Wander;
		BURN D 3 Bright A_NoBlocking;
		BURN E 5 Bright A_DropFire;
		BURN FGH 5 Bright A_Wander;
		BURN I 5 Bright A_DropFire;
		BURN JKL 5 Bright A_Wander;
		BURN M 5 Bright A_DropFire;
		BURN N 5 Bright A_CrispyPlayer;
		BURN OPQPQ 5 Bright;
		BURN RSTU 7 Bright;
		BURN V -1;
		Stop;
	Disintegrate:
		DISR A 5 A_PlaySound("misc/disruptordeath", CHAN_VOICE);
		DISR BC 5;
		DISR D 5 A_NoBlocking;
		DISR EF 5;
		DISR GHIJ 4;
		MEAT D -1;
		Stop;
	Firehands:
		WAVE ABCD 3;
		Loop;
	Firehandslower:
		WAVE ABCD 3 A_HandLower;
		Loop;
	}
	
	void A_ItBurnsItBurns()
	{
		A_PlaySound ("human/imonfire", CHAN_VOICE);

		if (player != null && player.mo == self)
		{
			player.SetPsprite(PSP_STRIFEHANDS, FindState("FireHands"));

			player.ReadyWeapon = null;
			player.PendingWeapon = WP_NOCHANGE;
			player.playerstate = PST_LIVE;
			player.extralight = 3;
		}
	}

	void A_CrispyPlayer()
	{
		if (player != null && player.mo == self)
		{
			PSprite psp = player.GetPSprite(PSP_STRIFEHANDS);

			State firehandslower = FindState("FireHandsLower");
			State firehands = FindState("FireHands");

			if (psp.CurState != null && firehandslower != null && firehands != null)
			{
				// Calculate state to go to.
				int dist = firehands.DistanceTo(psp.curState);
				if (dist > 0)
				{
					player.playerstate = PST_DEAD;
					psp.SetState(firehandslower + dist);
					return;
				}
			}
			player.playerstate = PST_DEAD;
			psp.SetState(null);
		}
	}

	void A_HandLower()
	{
		if (player != null)
		{
			PSprite psp = player.GetPSprite(PSP_STRIFEHANDS);

			if (psp.CurState == null)
			{
				psp.SetState(null);
				return;
			}

			psp.y += 9;
			if (psp.y > WEAPONBOTTOM*2)
			{
				psp.SetState(null);
			}

			if (player.extralight > 0) player.extralight--;
		}
		return;
	}
	
}
		
