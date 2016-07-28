#ifndef __A_MORPH__
#define __A_MORPH__

#define MORPHTICS (40*TICRATE)
#define MAXMORPHHEALTH 30

// Morph style states how morphing affects health and
// other effects in the game; only valid for players.
// Default should be the old Heretic/HeXen behaviour,
// so (int) value of MORPH_OLDEFFECTS *must* be zero.
enum
{
	MORPH_OLDEFFECTS		= 0x00000000,	// Default to old Heretic/HeXen behaviour unless flags given
	MORPH_ADDSTAMINA		= 0x00000001,	// Player has a "power" instead of a "curse" (add stamina instead of limiting to health)
	MORPH_FULLHEALTH		= 0x00000002,	// Player uses new health semantics (!POWER => MaxHealth of animal, POWER => Normal health behaviour)
	MORPH_UNDOBYTOMEOFPOWER	= 0x00000004,	// Player unmorphs upon activating a Tome of Power
	MORPH_UNDOBYCHAOSDEVICE	= 0x00000008,	// Player unmorphs upon activating a Chaos Device
	MORPH_FAILNOTELEFRAG	= 0x00000010,	// Player stays morphed if unmorph by Tome of Power fails
	MORPH_FAILNOLAUGH		= 0x00000020,	// Player doesn't laugh if unmorph by Chaos Device fails
	MORPH_WHENINVULNERABLE	= 0x00000040,	// Player can morph when invulnerable but ONLY if doing it to themselves
	MORPH_LOSEACTUALWEAPON	= 0x00000080,	// Player loses specified morph weapon only (not "whichever they have when unmorphing")
	MORPH_NEWTIDBEHAVIOUR	= 0x00000100,	// Actor TID is by default transferred from the old actor to the new actor
	MORPH_UNDOBYDEATH		= 0x00000200,	// Actor unmorphs when killed and (unless MORPH_UNDOBYDEATHSAVES) stays dead
	MORPH_UNDOBYDEATHFORCED	= 0x00000400,	// Actor (if unmorphed when killed) forces unmorph (not very useful with UNDOBYDEATHSAVES)
	MORPH_UNDOBYDEATHSAVES	= 0x00000800,	// Actor (if unmorphed when killed) regains their health and doesn't die
	MORPH_UNDOBYTIMEOUT		= 0x00001000,	// Player unmorphs once countdown expires
	MORPH_UNDOALWAYS		= 0x00002000,	// Powerups must always unmorph, no matter what.
	MORPH_TRANSFERTRANSLATION = 0x00004000,	// Transfer translation from the original actor to the morphed one

	MORPH_STANDARDUNDOING	= MORPH_UNDOBYTOMEOFPOWER | MORPH_UNDOBYCHAOSDEVICE | MORPH_UNDOBYTIMEOUT,
};

class PClass;
class AActor;
class player_t;
class AMorphedMonster;

bool P_MorphPlayer (player_t *activator, player_t *player, PClassPlayerPawn *morphclass, int duration = 0, int style = 0,
					PClassActor *enter_flash = NULL, PClassActor *exit_flash = NULL);
bool P_UndoPlayerMorph (player_t *activator, player_t *player, int unmorphflag = 0, bool force = false);
bool P_MorphMonster (AActor *actor, PClassActor *morphclass, int duration = 0, int style = 0,
					 PClassActor *enter_flash = NULL, PClassActor *exit_flash = NULL);
bool P_UndoMonsterMorph (AMorphedMonster *beast, bool force = false);
bool P_UpdateMorphedMonster (AActor *actor);
bool P_MorphedDeath(AActor *actor, AActor **morphed, int *morphedstyle, int *morphedhealth);

#endif //__A_MORPH__
