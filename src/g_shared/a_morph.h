#ifndef __A_MORPH__
#define __A_MORPH__

#define MORPHTICS (40*TICRATE)
#define MAXMORPHHEALTH	30

// Morph style states how morphing affects health and
// other effects in the game; only valid for players.
// Default should be the old Heretic/HeXen behaviour,
// so the (int) value of MORPH_RAVEN *must* be zero.
enum
{
	MORPH_OLDEFFECTS = 0,	// Default to old Heretic/HeXen behaviour unless flags given
	MORPH_ADDSTAMINA = 1,	// Power instead of curse (add stamina instead of limiting to health)
	MORPH_FULLHEALTH = 2,	// New health semantics (!POWER => MaxHealth of animal, POWER => Normal health behaviour)
	MORPH_UNDOBYTOMEOFPOWER = 4,
	MORPH_UNDOBYCHAOSDEVICE = 8,
};

struct PClass;
class AActor;
class player_s;

//
// A_MORPH
//
bool P_MorphPlayer (player_s *player, const PClass *morphclass, int duration = 0, int style = 0,
					const PClass *enter_flash = NULL, const PClass *exit_flash = NULL);
bool P_UndoPlayerMorph (player_s *player, bool force = false);
bool P_MorphMonster (AActor *actor, const PClass *morphclass, int duration = 0, int style = 0,
					 const PClass *enter_flash = NULL, const PClass *exit_flash = NULL);
bool P_UpdateMorphedMonster (AActor *actor);



#endif //__A_MORPH__
