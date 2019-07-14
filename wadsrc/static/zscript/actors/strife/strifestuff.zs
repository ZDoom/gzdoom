// Notes so I don't forget them:
//
// When shooting missiles at something, if MF_SHADOW is set, the angle is adjusted with the formula:
//		angle += pr_spawnmissile.Random2() << 21
// When MF_STRIFEx4000000 is set, the angle is adjusted similarly:
//		angle += pr_spawnmissile.Random2() << 22
// Note that these numbers are different from those used by all the other Doom engine games.


// Tank 1 Huge ------------------------------------------------------------

class Tank1 : Actor
{
	Default
	{
		Radius 16;
		Height 192;
		+SOLID
	}
	States
	{
	Spawn:
		TNK1 A 15;
		TNK1 B 11;
		TNK1 C 40;
		Loop;
	}
}

// Tank 2 Huge ------------------------------------------------------------

class Tank2 : Actor
{
	Default
	{
		Radius 16;
		Height 192;
		+SOLID
	}
	States
	{
	Spawn:
		TNK2 A 15;
		TNK2 B 11;
		TNK2 C 40;
		Loop;
	}
}

// Tank 3 Huge ------------------------------------------------------------

class Tank3 : Actor
{
	Default
	{
		Radius 16;
		Height 192;
		+SOLID
	}
	States
	{
	Spawn:
		TNK3 A 15;
		TNK3 B 11;
		TNK3 C 40;
		Loop;
	}
}

// Tank 4 -------------------------------------------------------------------

class Tank4 : Actor
{
	Default
	{
		Radius 16;
		Height 56;
		+SOLID
	}
	States
	{
	Spawn:
		TNK4 A 15;
		TNK4 B 11;
		TNK4 C 40;
		Loop;
	}
}

// Tank 5 -------------------------------------------------------------------

class Tank5 : Actor
{
	Default
	{
		Radius 16;
		Height 56;
		+SOLID
	}
	States
	{
	Spawn:
		TNK5 A 15;
		TNK5 B 11;
		TNK5 C 40;
		Loop;
	}
}

// Tank 6 -------------------------------------------------------------------

class Tank6 : Actor
{
	Default
	{
		Radius 16;
		Height 56;
		+SOLID
	}
	States
	{
	Spawn:
		TNK6 A 15;
		TNK6 B 11;
		TNK6 C 40;
		Loop;
	}
}

// Water Bottle -------------------------------------------------------------

class WaterBottle : Actor
{
	States
	{
	Spawn:
		WATR A -1;
		Stop;
	}
}

// Mug ----------------------------------------------------------------------

class Mug : Actor
{
	States
	{
	Spawn:
		MUGG A -1;
		Stop;
	}
}

// Wooden Barrel ------------------------------------------------------------

class WoodenBarrel : Actor
{
	Default
	{
		Health 10;
		Radius 10;
		Height 32;
		+SOLID +SHOOTABLE +NOBLOOD
		+INCOMBAT
		DeathSound "woodenbarrel/death";
	}
	States
	{
	Spawn:
		BARW A -1;
		Stop;
	Death:
		BARW B 2 A_Scream;
		BARW C 2;
		BARW D 2 A_NoBlocking;
		BARW EFG 2;
		BARW H -1;
		Stop;
	}
}

// Strife's explosive barrel ------------------------------------------------

class ExplosiveBarrel2 : Actor
{
	Default
	{
		Health 30;
		Radius 10;
		Height 32;
		+SOLID +SHOOTABLE +NOBLOOD +OLDRADIUSDMG
		DeathSound "world/barrelx";
		+INCOMBAT
	}
	States
	{
	Spawn:
		BART A -1;
		Stop;
	Death:
		BART B 2 Bright A_Scream;
		BART CD 2 Bright;
		BART E 2 Bright A_NoBlocking;
		BART F 2 Bright A_Explode(64, 64, alert:true);
		BART GHIJ 2 Bright;
		BART K 3 Bright;
		BART L -1;
		Stop;
	}
}

// Light Silver, Fluorescent ----------------------------------------------

class LightSilverFluorescent : Actor
{
	Default
	{
		Radius 2.5;
		Height 16;
		+NOBLOCKMAP
		+FIXMAPTHINGPOS
	}
	States
	{
	Spawn:
		LITS A -1 Bright;
		Stop;
	}
}

// Light Brown, Fluorescent -----------------------------------------------

class LightBrownFluorescent : Actor
{
	Default
	{
		Radius 2.5;
		Height 16;
		+NOBLOCKMAP
		+FIXMAPTHINGPOS
	}
	States
	{
	Spawn:
		LITB A -1 Bright;
		Stop;
	}
}

// Light Gold, Fluorescent ------------------------------------------------

class LightGoldFluorescent : Actor
{
	Default
	{
		Radius 2.5;
		Height 16;
		+NOBLOCKMAP
		+FIXMAPTHINGPOS
	}
	States
	{
	Spawn:
		LITG A -1 Bright;
		Stop;
	}
}

// Light Globe --------------------------------------------------------------

class LightGlobe : Actor
{
	Default
	{
		Radius 16;
		Height 16;
		+SOLID
	}
	States
	{
	Spawn:
		LITE A -1 Bright;
		Stop;
	}
}

// Techno Pillar ------------------------------------------------------------

class PillarTechno : Actor
{
	Default
	{
		Radius 20;
		Height 128;
		+SOLID
	}
	States
	{
	Spawn:
		MONI A -1;
		Stop;
	}
}

// Aztec Pillar -------------------------------------------------------------

class PillarAztec : Actor
{
	Default
	{
		Radius 16;
		Height 128;
		+SOLID
	}
	States
	{
	Spawn:
		STEL A -1;
		Stop;
	}
}

// Damaged Aztec Pillar -----------------------------------------------------

class PillarAztecDamaged : Actor
{
	Default
	{
		Radius 16;
		Height 80;
		+SOLID
	}
	States
	{
	Spawn:
		STLA A -1;
		Stop;
	}
}

// Ruined Aztec Pillar ------------------------------------------------------

class PillarAztecRuined : Actor
{
	Default
	{
		Radius 16;
		Height 40;
		+SOLID
	}
	States
	{
	Spawn:
		STLE A -1;
		Stop;
	}
}

// Huge Tech Pillar ---------------------------------------------------------

class PillarHugeTech : Actor
{
	Default
	{
		Radius 24;
		Height 192;
		+SOLID
	}
	States
	{
	Spawn:
		HUGE ABCD 4;
		Loop;
	}
}

// Alien Power Crystal in a Pillar ------------------------------------------

class PillarAlienPower : Actor
{
	Default
	{
		Radius 24;
		Height 192;
		+SOLID
		ActiveSound "ambient/alien2";
	}
	States
	{
	Spawn:
		APOW A 4 A_LoopActiveSound;
		Loop;
	}
}

// SStalactiteBig -----------------------------------------------------------

class SStalactiteBig : Actor
{
	Default
	{
		Radius 16;
		Height 54;
		+SOLID +SPAWNCEILING +NOGRAVITY
	}
	States
	{
	Spawn:
		STLG C -1;
		Stop;
	}
}

// SStalactiteSmall ---------------------------------------------------------

class SStalactiteSmall : Actor
{
	Default
	{
		Radius 16;
		Height 40;
		+SOLID +SPAWNCEILING +NOGRAVITY
	}
	States
	{
	Spawn:
		STLG A -1;
		Stop;
	}
}

// SStalagmiteBig -----------------------------------------------------------

class SStalagmiteBig : Actor
{
	Default
	{
		Radius 16;
		Height 40;
		+SOLID
	}
	States
	{
	Spawn:
		STLG B -1;
		Stop;
	}
}

// Cave Pillar Top ----------------------------------------------------------

class CavePillarTop : Actor
{
	Default
	{
		Radius 16;
		Height 128;
		+SOLID +SPAWNCEILING +NOGRAVITY
	}
	States
	{
	Spawn:
		STLG D -1;
		Stop;
	}
}

// Cave Pillar Bottom -------------------------------------------------------

class CavePillarBottom : Actor
{
	Default
	{
		Radius 16;
		Height 128;
		+SOLID
	}
	States
	{
	Spawn:
		STLG E -1;
		Stop;
	}
}

// SStalagmiteSmall ---------------------------------------------------------

class SStalagmiteSmall : Actor
{
	Default
	{
		Radius 16;
		Height 25;
		+SOLID
	}
	States
	{
	Spawn:
		STLG F -1;
		Stop;
	}
}

// Candle -------------------------------------------------------------------

class Candle : Actor
{
	States
	{
	Spawn:
		KNDL A -1 Bright;
		Stop;
	}
}

// StrifeCandelabra ---------------------------------------------------------

class StrifeCandelabra : Actor
{
	Default
	{
		Radius 16;
		Height 40;
		+SOLID
	}
	States
	{
	Spawn:
		CLBR A -1;
		Stop;
	}
}

// Floor Water Drop ---------------------------------------------------------

class WaterDropOnFloor : Actor
{
	Default
	{
		+NOBLOCKMAP
		ActiveSound "world/waterdrip";
	}
	States
	{
	Spawn:
		DRIP A 6 A_FLoopActiveSound;
		DRIP BC 4;
		DRIP D 4 A_FLoopActiveSound;
		DRIP EF 4;
		DRIP G 4 A_FLoopActiveSound;
		DRIP H 4;
		Loop;
	}
}

// Waterfall Splash ---------------------------------------------------------

class WaterfallSplash : Actor
{
	Default
	{
		+NOBLOCKMAP
		ActiveSound "world/waterfall";
	}
	States
	{
	Spawn:
		SPLH ABCDEFG 4;
		SPLH H 4 A_LoopActiveSound;
		Loop;
	}
}

// Ceiling Water Drip -------------------------------------------------------

class WaterDrip : Actor
{
	Default
	{
		Height 1;
		+NOBLOCKMAP +SPAWNCEILING +NOGRAVITY
	}
	States
	{
	Spawn:
		CDRP A 10;
		CDRP BCD 8;
		Loop;
	}
}

// WaterFountain ------------------------------------------------------------

class WaterFountain : Actor
{
	Default
	{
		+NOBLOCKMAP
		ActiveSound "world/watersplash";
	}
	States
	{
	Spawn:
		WTFT ABC 4;
		WTFT D 4 A_LoopActiveSound;
		Loop;
	}
}

// Hearts in Tank -----------------------------------------------------------

class HeartsInTank : Actor
{
	Default
	{
		Radius 16;
		Height 56;
		+SOLID
	}
	States
	{
	Spawn:
		HERT ABC 4 Bright;
		Loop;
	}
}

// Teleport Swirl -----------------------------------------------------------

class TeleportSwirl : Actor
{
	Default
	{
		+NOBLOCKMAP
		+ZDOOMTRANS
		RenderStyle "Add";
		Alpha 0.25;
	}
	States
	{
	Spawn:
		TELP ABCD 3 Bright;
		Loop;
	}
}

// Dead Player --------------------------------------------------------------
// Strife's disappeared. This one doesn't.

class DeadStrifePlayer : Actor
{
	States
	{
	Spawn:
		PLAY P 700;
		RGIB H -1;
		Stop;
	}
}

// Dead Peasant -------------------------------------------------------------
// Unlike Strife's, this one does not turn into gibs and disappear.

class DeadPeasant : Actor
{
	States
	{
	Spawn:
		PEAS N -1;
		Stop;
	}
}

// Dead Acolyte -------------------------------------------------------------
// Unlike Strife's, this one does not turn into gibs and disappear.

class DeadAcolyte : Actor
{
	States
	{
	Spawn:
		AGRD N -1;
		Stop;
	}
}

// Dead Reaver --------------------------------------------------------------

class DeadReaver : Actor
{
	States
	{
	Spawn:
		ROB1 R -1;
		Stop;
	}
}

// Dead Rebel ---------------------------------------------------------------

class DeadRebel : Actor
{
	States
	{
	Spawn:
		HMN1 N -1;
		Stop;
	}
}

// Sacrificed Guy -----------------------------------------------------------

class SacrificedGuy : Actor
{
	States
	{
	Spawn:
		SACR A -1;
		Stop;
	}
}

// Pile of Guts -------------------------------------------------------------

class PileOfGuts : Actor
{
	// Strife used a doomednum, which is the same as the Aztec Pillar. Since
	// the pillar came first in the mobjinfo list, you could not spawn this
	// in a map. Pity.
	States
	{
	Spawn:
		DEAD A -1;
		Stop;
	}
}

// Burning Barrel -----------------------------------------------------------

class StrifeBurningBarrel : Actor
{
	Default
	{
		Radius 16;
		Height 48;
		+SOLID
	}
	States
	{
	Spawn:
		BBAR ABCD 4 Bright;
		Loop;
	}
}

// Burning Bowl -----------------------------------------------------------

class BurningBowl : Actor
{
	Default
	{
		Radius 16;
		Height 16;
		+SOLID
		ActiveSound "world/smallfire";
	}
	States
	{
	Spawn:
		BOWL ABCD 4 Bright;
		Loop;
	}
}

// Burning Brazier -----------------------------------------------------------

class BurningBrazier : Actor
{
	Default
	{
		Radius 10;
		Height 32;
		+SOLID
		ActiveSound "world/smallfire";
	}
	States
	{
	Spawn:
		BRAZ ABCD 4 Bright;
		Loop;
	}
}

// Small Torch Lit --------------------------------------------------------

class SmallTorchLit : Actor
{
	Default
	{
		Radius 2.5;
		Height 16;
		+NOBLOCKMAP
		+FIXMAPTHINGPOS

		// It doesn't have any action functions, so how does it use this sound?
		ActiveSound "world/smallfire";
	}
	States
	{
	Spawn:
		TRHL ABCD 4 Bright;
		Loop;
	}
}

// Small Torch Unlit --------------------------------------------------------

class SmallTorchUnlit : Actor
{
	Default
	{
		Radius 2.5;
		Height 16;
		+NOBLOCKMAP
		+FIXMAPTHINGPOS
	}
	States
	{
	Spawn:
		TRHO A -1;
		Stop;
	}
}

// Ceiling Chain ------------------------------------------------------------

class CeilingChain : Actor
{
	Default
	{
		Radius 20;
		Height 93;
		+NOBLOCKMAP +SPAWNCEILING +NOGRAVITY
	}
	States
	{
	Spawn:
		CHAN A -1;
		Stop;
	}
}

// Cage Light ---------------------------------------------------------------

class CageLight : Actor
{
	Default
	{
		// No, it's not bright even though it's a light.
		Height 3;
		+NOBLOCKMAP +SPAWNCEILING +NOGRAVITY
	}
	States
	{
	Spawn:
		CAGE A -1;
		Stop;
	}
}

// Statue -------------------------------------------------------------------

class Statue : Actor
{
	Default
	{
		Radius 20;
		Height 64;
		+SOLID
	}
	States
	{
	Spawn:
		STAT A -1;
		Stop;
	}
}

// Ruined Statue ------------------------------------------------------------

class StatueRuined : Actor
{
	Default
	{
		Radius 20;
		Height 56;
		+SOLID
	}
	States
	{
	Spawn:
		DSTA A -1;
		Stop;
	}
}

// Medium Torch -------------------------------------------------------------

class MediumTorch : Actor
{
	Default
	{
		Radius 4;
		Height 72;
		+SOLID
	}
	States
	{
	Spawn:
		LTRH ABCD 4;
		Loop;
	}
}

// Outside Lamp -------------------------------------------------------------

class OutsideLamp : Actor
{
	Default
	{
		// No, it's not bright.
		Radius 3;
		Height 80;
		+SOLID
	}
	States
	{
	Spawn:
		LAMP A -1;
		Stop;
	}
}

// Pole Lantern -------------------------------------------------------------

class PoleLantern : Actor
{
	Default
	{
		// No, it's not bright.
		Radius 3;
		Height 80;
		+SOLID
	}
	States
	{
	Spawn:
		LANT A -1;
		Stop;
	}
}

// Rock 1 -------------------------------------------------------------------

class SRock1 : Actor
{
	Default
	{
		+NOBLOCKMAP
	}
	States
	{
	Spawn:
		ROK1 A -1;
		Stop;
	}
}

// Rock 2 -------------------------------------------------------------------

class SRock2 : Actor
{
	Default
	{
		+NOBLOCKMAP
	}
	States
	{
	Spawn:
		ROK2 A -1;
		Stop;
	}
}

// Rock 3 -------------------------------------------------------------------

class SRock3 : Actor
{
	Default
	{
		+NOBLOCKMAP
	}
	States
	{
	Spawn:
		ROK3 A -1;
		Stop;
	}
}

// Rock 4 -------------------------------------------------------------------

class SRock4 : Actor
{
	Default
	{
		+NOBLOCKMAP
	}
	States
	{
	Spawn:
		ROK4 A -1;
		Stop;
	}
}

// Stick in Water -----------------------------------------------------------

class StickInWater : Actor
{
	Default
	{
		+NOBLOCKMAP
		+FLOORCLIP
		ActiveSound "world/river";
	}
	States
	{
	Spawn:
		LOGW ABCD 5 A_LoopActiveSound;
		Loop;
	}
}

// Rubble 1 -----------------------------------------------------------------

class Rubble1 : Actor
{
	Default
	{
		+NOBLOCKMAP +NOCLIP
	}
	States
	{
	Spawn:
		RUB1 A -1;
		Stop;
	}
}

// Rubble 2 -----------------------------------------------------------------

class Rubble2 : Actor
{
	Default
	{
		+NOBLOCKMAP +NOCLIP
	}
	States
	{
	Spawn:
		RUB2 A -1;
		Stop;
	}
}

// Rubble 3 -----------------------------------------------------------------

class Rubble3 : Actor
{
	Default
	{
		+NOBLOCKMAP +NOCLIP
	}
	States
	{
	Spawn:
		RUB3 A -1;
		Stop;
	}
}

// Rubble 4 -----------------------------------------------------------------

class Rubble4 : Actor
{
	Default
	{
		+NOBLOCKMAP +NOCLIP
	}
	States
	{
	Spawn:
		RUB4 A -1;
		Stop;
	}
}

// Rubble 5 -----------------------------------------------------------------

class Rubble5 : Actor
{
	Default
	{
		+NOBLOCKMAP +NOCLIP
	}
	States
	{
	Spawn:
		RUB5 A -1;
		Stop;
	}
}

// Rubble 6 -----------------------------------------------------------------

class Rubble6 : Actor
{
	Default
	{
		+NOBLOCKMAP +NOCLIP
	}
	States
	{
	Spawn:
		RUB6 A -1;
		Stop;
	}
}

// Rubble 7 -----------------------------------------------------------------

class Rubble7 : Actor
{
	Default
	{
		+NOBLOCKMAP +NOCLIP
	}
	States
	{
	Spawn:
		RUB7 A -1;
		Stop;
	}
}

// Rubble 8 -----------------------------------------------------------------

class Rubble8 : Actor
{
	Default
	{
		+NOBLOCKMAP +NOCLIP
	}
	States
	{
	Spawn:
		RUB8 A -1;
		Stop;
	}
}

// Surgery Crab -------------------------------------------------------------

class SurgeryCrab : Actor
{
	Default
	{
		+SOLID +SPAWNCEILING +NOGRAVITY
		Radius 20;
		Height 16;
	}
	States
	{
	Spawn:
		CRAB A -1;
		Stop;
	}
}

// Large Torch --------------------------------------------------------------

class LargeTorch : Actor
{
	Default
	{
		Radius 10;
		Height 72;
		+SOLID
		ActiveSound "world/smallfire";
	}
	States
	{
	Spawn:
		LMPC ABCD 4 Bright;
		Loop;
	}
}

// Huge Torch --------------------------------------------------------------

class HugeTorch : Actor
{
	Default
	{
		Radius 10;
		Height 80;
		+SOLID
		ActiveSound "world/smallfire";
	}
	States
	{
	Spawn:
		LOGS ABCD 4;
		Loop;
	}
}

// Palm Tree ----------------------------------------------------------------

class PalmTree : Actor
{
	Default
	{
		Radius 15;
		Height 109;
		+SOLID
	}
	States
	{
	Spawn:
		TREE A -1;
		Stop;
	}
}

// Big Tree ----------------------------------------------------------------

class BigTree2 : Actor
{
	Default
	{
		Radius 15;
		Height 109;
		+SOLID
	}
	States
	{
	Spawn:
		TREE B -1;
		Stop;
	}
}

// Potted Tree ----------------------------------------------------------------

class PottedTree : Actor
{
	Default
	{
		Radius 15;
		Height 64;
		+SOLID
	}
	States
	{
	Spawn:
		TREE C -1;
		Stop;
	}
}

// Tree Stub ----------------------------------------------------------------

class TreeStub : Actor
{
	Default
	{
		Radius 15;
		Height 80;
		+SOLID
	}
	States
	{
	Spawn:
		TRET A -1;
		Stop;
	}
}

// Short Bush ---------------------------------------------------------------

class ShortBush : Actor
{
	Default
	{
		Radius 15;
		Height 40;
		+SOLID
	}
	States
	{
	Spawn:
		BUSH A -1;
		Stop;
	}
}

// Tall Bush ---------------------------------------------------------------

class TallBush : Actor
{
	Default
	{
		Radius 20;
		Height 64;
		+SOLID
	}
	States
	{
	Spawn:
		SHRB A -1;
		Stop;
	}
}

// Chimney Stack ------------------------------------------------------------

class ChimneyStack : Actor
{
	Default
	{
		Radius 20;
		Height 64;	// This height does not fit the sprite
		+SOLID
	}
	States
	{
	Spawn:
		STAK A -1;
		Stop;
	}
}

// Barricade Column ---------------------------------------------------------

class BarricadeColumn : Actor
{
	Default
	{
		Radius 16;
		Height 128;
		+SOLID
	}
	States
	{
	Spawn:
		BARC A -1;
		Stop;
	}
}

// Pot ----------------------------------------------------------------------

class Pot : Actor
{
	Default
	{
		Radius 12;
		Height 24;
		+SOLID
	}
	States
	{
	Spawn:
		VAZE A -1;
		Stop;
	}
}

// Pitcher ------------------------------------------------------------------

class Pitcher : Actor
{
	Default
	{
		Radius 12;
		Height 32;
		+SOLID
	}
	States
	{
	Spawn:
		VAZE B -1;
		Stop;
	}
}

// Stool --------------------------------------------------------------------

class Stool : Actor
{
	Default
	{
		Radius 6;
		Height 24;
		+SOLID
	}
	States
	{
	Spawn:
		STOL A -1;
		Stop;
	}
}

// Metal Pot ----------------------------------------------------------------

class MetalPot : Actor
{
	Default
	{
		+NOBLOCKMAP
	}
	States
	{
	Spawn:
		MPOT A -1;
		Stop;
	}
}

// Tub ----------------------------------------------------------------------

class Tub : Actor
{
	Default
	{
		+NOBLOCKMAP
	}
	States
	{
	Spawn:
		TUB1 A -1;
		Stop;
	}
}

// Anvil --------------------------------------------------------------------

class Anvil : Actor
{
	Default
	{
		Radius 16;
		Height 32;
		+SOLID
	}
	States
	{
	Spawn:
		ANVL A -1;
		Stop;
	}
}

// Silver Tech Lamp ----------------------------------------------------------

class TechLampSilver : Actor
{
	Default
	{
		Radius 11;
		Height 64;
		+SOLID
	}
	States
	{
	Spawn:
		TECH A -1;
		Stop;
	}
}

// Brass Tech Lamp ----------------------------------------------------------

class TechLampBrass : Actor
{
	Default
	{
		Radius 8;
		Height 64;
		+SOLID
	}
	States
	{
	Spawn:
		TECH B -1;
		Stop;
	}
}

// Tray --------------------------------------------------------------------

class Tray : Actor
{
	Default
	{
		Radius 24;
		Height 40;
		+SOLID
	}
	States
	{
	Spawn:
		TRAY A -1;
		Stop;
	}
}

// AmmoFiller ---------------------------------------------------------------

class AmmoFiller : Actor
{
	Default
	{
		Radius 12;
		Height 24;
		+SOLID
	}
	States
	{
	Spawn:
		AFED A -1;
		Stop;
	}
}

// Sigil Banner -------------------------------------------------------------

class SigilBanner : Actor
{
	Default
	{
		Radius 24;
		Height 96;
		+NOBLOCKMAP	// I take it this was once solid, yes?
	}
	States
	{
	Spawn:
		SBAN A -1;
		Stop;
	}
}

// RebelBoots ---------------------------------------------------------------

class RebelBoots : Actor
{
	Default
	{
		+NOBLOCKMAP
	}
	States
	{
	Spawn:
		BOTR A -1;
		Stop;
	}
}

// RebelHelmet --------------------------------------------------------------

class RebelHelmet : Actor
{
	Default
	{
		+NOBLOCKMAP
	}
	States
	{
	Spawn:
		HATR A -1;
		Stop;
	}
}

// RebelShirt ---------------------------------------------------------------

class RebelShirt : Actor
{
	Default
	{
		+NOBLOCKMAP
	}
	States
	{
	Spawn:
		TOPR A -1;
		Stop;
	}
}

// Alien Bubble Column ------------------------------------------------------

class AlienBubbleColumn : Actor
{
	Default
	{
		Radius 16;
		Height 128;
		+SOLID
		ActiveSound "ambient/alien5";
	}
	States
	{
	Spawn:
		BUBB A 4 A_LoopActiveSound;
		Loop;
	}
}

// Alien Floor Bubble -------------------------------------------------------

class AlienFloorBubble : Actor
{
	Default
	{
		Radius 16;
		Height 72;
		+SOLID
		ActiveSound "ambient/alien6";
	}
	States
	{
	Spawn:
		BUBF A 4 A_LoopActiveSound;
		Loop;
	}
}

// Alien Ceiling Bubble -----------------------------------------------------

class AlienCeilingBubble : Actor
{
	Default
	{
		Radius 16;
		Height 72;
		+SOLID +SPAWNCEILING +NOGRAVITY
		ActiveSound "ambient/alien4";
	}
	States
	{
	Spawn:
		BUBC A 4 A_LoopActiveSound;
		Loop;
	}
}

// Alien Asp Climber --------------------------------------------------------

class AlienAspClimber : Actor
{
	Default
	{
		Radius 16;
		Height 128;
		+SOLID
		ActiveSound "ambient/alien3";
	}
	States
	{
	Spawn:
		ASPR A 4 A_LoopActiveSound;
		Loop;
	}
}

// Alien Spider Light -------------------------------------------------------

class AlienSpiderLight : Actor
{
	Default
	{
		Radius 32;
		Height 56;
		+SOLID
		ActiveSound "ambient/alien1";
	}
	States
	{
	Spawn:
		SPDL ABC 5 A_LoopActiveSound;
		Loop;
	}
}

// Target Practice -----------------------------------------------------------

class TargetPractice : Actor
{
	Default
	{
		Health 99999999;
		PainChance 255;
		Radius 10;
		Height 72;
		Mass 9999999;
		+SOLID +SHOOTABLE +NOBLOOD
		+INCOMBAT +NODAMAGE
		PainSound "misc/metalhit";
	}
	States
	{
	Spawn:
		HOGN A 2 A_CheckTerrain;
		Loop;
	Pain:
		HOGN B 1 A_CheckTerrain;
		HOGN C 1 A_Pain;
		Goto Spawn;
	}
}

// Force Field Guard --------------------------------------------------------

class ForceFieldGuard : Actor
{
	Default
	{
		Health 10;
		Radius 2;
		Height 1;
		Mass 10000;
		+SHOOTABLE
		+NOSECTOR
		+NOBLOOD
		+INCOMBAT
	}
	States
	{
	Spawn:
		TNT1 A -1;
		Stop;
	Death:
		TNT1 A 1 A_RemoveForceField;
		Stop;
	}
	
	override int TakeSpecialDamage (Actor inflictor, Actor source, int damage, Name damagetype)
	{
		if (inflictor == NULL || !(inflictor is "DegninOre"))
		{
			return -1;
		}
		return health;
	}
	
}

// Kneeling Guy -------------------------------------------------------------

class KneelingGuy : Actor
{
	Default
	{
		Health 51;
		Painchance 255;
		Radius 6;
		Height 17;
		Mass 50000;
		+SOLID
		+SHOOTABLE
		+NOBLOOD
		+ISMONSTER
		+INCOMBAT
		PainSound "misc/static";
		DeathSound "misc/static";
		ActiveSound "misc/chant";
	}
	States
	{
	Spawn:
	See:
		NEAL A 15 A_LoopActiveSound;
		NEAL B 40 A_LoopActiveSound;
		Loop;
	Pain:
		NEAL C 5 A_SetShadow;
		NEAL B 4 A_Pain;
		NEAL C 5 A_ClearShadow;
		Goto Spawn;
	Wound:
		NEAL B 6;
		NEAL C 13 A_GetHurt;
		Loop;
	Death:
		NEAL D 5;
		NEAL E 5 A_Scream;
		NEAL F 6;
		NEAL G 5 A_NoBlocking;
		NEAL H 5;
		NEAL I 6;
		NEAL J -1;
		Stop;
	}
}

// Power Coupling -----------------------------------------------------------

class PowerCoupling : Actor
{
	Default
	{
		Health 40;
		Radius 17;
		Height 64;
		Mass 999999;
		+SOLID
		+SHOOTABLE
		+DROPPED
		+NOBLOOD
		+NOTDMATCH
		+INCOMBAT
	}
	States
	{
	Spawn:
		COUP AB 5;
		Loop;
	}
	
	override void Die (Actor source, Actor inflictor, int dmgflags, Name MeansOfDeath)
	{
		Super.Die (source, inflictor, dmgflags, MeansOfDeath);

		int i;

		for (i = 0; i < MAXPLAYERS; ++i)
			if (playeringame[i] && players[i].health > 0)
				break;

		if (i == MAXPLAYERS)
			return;

		// [RH] In case the player broke it with the dagger, alert the guards now.
		if (LastHeard != source)
		{
			SoundAlert (source);
		}
		Door_Close(225, 16);
		Floor_LowerToHighestEE(44, 8);
		players[i].mo.GiveInventoryType ("QuestItem6");
		S_Sound ("svox/voc13", CHAN_VOICE);
		players[i].SetLogNumber (13);
		players[i].SetSubtitleNumber (13);
		A_DropItem ("BrokenPowerCoupling", -1, 256);
		Destroy ();
	}
	
}

// Gibs for things that bleed -----------------------------------------------

class Meat : Actor
{
	Default
	{
		+NOCLIP
	}
	States
	{
	Spawn:
		MEAT A 700;
		Stop;
		MEAT B 700;
		Stop;
		MEAT C 700;
		Stop;
		MEAT D 700;
		Stop;
		MEAT E 700;
		Stop;
		MEAT F 700;
		Stop;
		MEAT G 700;
		Stop;
		MEAT H 700;
		Stop;
		MEAT I 700;
		Stop;
		MEAT J 700;
		Stop;
		MEAT K 700;
		Stop;
		MEAT L 700;
		Stop;
		MEAT M 700;
		Stop;
		MEAT N 700;
		Stop;
		MEAT O 700;
		Stop;
		MEAT P 700;
		Stop;
		MEAT Q 700;
		Stop;
		MEAT R 700;
		Stop;
		MEAT S 700;
		Stop;
		MEAT T 700;
		Stop;
	}
	
	override void BeginPlay ()
	{
		// Strife used mod 19, but there are 20 states. Hmm.
		SetState (SpawnState + random[GibTosser](0, 19));
	}
	
}

// Gibs for things that don't bleed -----------------------------------------

class Junk : Meat
{
	States
	{
	Spawn:
		JUNK A 700;
		Stop;
		JUNK B 700;
		Stop;
		JUNK C 700;
		Stop;
		JUNK D 700;
		Stop;
		JUNK E 700;
		Stop;
		JUNK F 700;
		Stop;
		JUNK G 700;
		Stop;
		JUNK H 700;
		Stop;
		JUNK I 700;
		Stop;
		JUNK J 700;
		Stop;
		JUNK K 700;
		Stop;
		JUNK L 700;
		Stop;
		JUNK M 700;
		Stop;
		JUNK N 700;
		Stop;
		JUNK O 700;
		Stop;
		JUNK P 700;
		Stop;
		JUNK Q 700;
		Stop;
		JUNK R 700;
		Stop;
		JUNK S 700;
		Stop;
		JUNK T 700;
		Stop;
	}
}

