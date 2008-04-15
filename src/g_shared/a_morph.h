#ifndef __A_MORPH__
#define __A_MORPH__

#define MORPHTICS (40*TICRATE)
#define MAXMORPHHEALTH	30

// Morph style states how morphing affects health and
// other effects in the game; only valid for players.
// Default should be the old Heretic/HeXen behaviour,
// so (int) value of MORPH_OLDEFFECTS *must* be zero.
enum
{
	MORPH_OLDEFFECTS		= 0x00000000,	// Default to old Heretic/HeXen behaviour unless flags given
	MORPH_ADDSTAMINA		= 0x00000001,	// Power instead of curse (add stamina instead of limiting to health)
	MORPH_FULLHEALTH		= 0x00000002,	// New health semantics (!POWER => MaxHealth of animal, POWER => Normal health behaviour)
	MORPH_UNDOBYTOMEOFPOWER	= 0x00000004,	// Player unmorphs upon activating a Tome of Power
	MORPH_UNDOBYCHAOSDEVICE	= 0x00000008,	// Player unmorphs upon activating a Chaos Device
	MORPH_FAILNOTELEFRAG	= 0x00000010,	// Player stays morphed if unmorph by Tome of Power fails
	MORPH_FAILNOLAUGH		= 0x00000020,	// Player doesn't laugh if unmorph by Chaos Device fails
	MORPH_WHENINVULNERABLE	= 0x00000040,	// Player can morph when invulnerable but ONLY if doing it to themselves
	MORPH_LOSEACTUALWEAPON	= 0X00000080	// Player loses specified morph weapon only (not "whichever they have when unmorphing")
};

struct PClass;
class AActor;
class player_s;

bool P_MorphPlayer (player_s *activator, player_s *player, const PClass *morphclass, int duration = 0, int style = 0,
					const PClass *enter_flash = NULL, const PClass *exit_flash = NULL);
bool P_UndoPlayerMorph (player_s *player, bool force = false);
bool P_MorphMonster (AActor *actor, const PClass *morphclass, int duration = 0, int style = 0,
					 const PClass *enter_flash = NULL, const PClass *exit_flash = NULL);
bool P_UpdateMorphedMonster (AActor *actor);

#endif //__A_MORPH__
