#ifndef __A_ARTIFACTS_H__
#define __A_ARTIFACTS_H__

#include "a_pickups.h"

#define STREAM_ENUM(e) \
	inline FArchive &operator<< (FArchive &arc, e i) \
	{ \
		BYTE val = (BYTE)i; \
		arc << val; \
		i = (e)val; \
		return arc; \
	} \

typedef enum
{
	arti_none,
	arti_invulnerability,
	arti_invisibility,
	arti_health,
	arti_superhealth,
	arti_tomeofpower,
	arti_healingradius,
	arti_summon,
	arti_torch,
	arti_firebomb,
	arti_egg,
	arti_fly,
	arti_blastradius,
	arti_poisonbag,
	arti_teleportother,
	arti_speed,
	arti_boostmana,
	arti_boostarmor,
	arti_teleport,
	// Puzzle artifacts
	arti_firstpuzzitem,
	arti_puzzskull = arti_firstpuzzitem,
	arti_puzzgembig,
	arti_puzzgemred,
	arti_puzzgemgreen1,
	arti_puzzgemgreen2,
	arti_puzzgemblue1,
	arti_puzzgemblue2,
	arti_puzzbook1,
	arti_puzzbook2,
	arti_puzzskull2,
	arti_puzzfweapon,
	arti_puzzcweapon,
	arti_puzzmweapon,
	arti_puzzgear1,
	arti_puzzgear2,
	arti_puzzgear3,
	arti_puzzgear4,
	NUMARTIFACTS
} artitype_t;

STREAM_ENUM (artitype_t)

class player_s;
extern bool (*ArtiDispatch[NUMARTIFACTS]) (player_s *, artitype_t);
extern const char *ArtiPics[NUMARTIFACTS];

#define NUMINVENTORYSLOTS	NUMARTIFACTS

typedef enum
{
	pw_invulnerability,
	pw_strength,
	pw_invisibility,
	pw_ironfeet,
	pw_allmap,
	pw_infrared,

// Powerups added in Heretic
	pw_weaponlevel2,
	pw_flight,
	pw_shield,
	pw_health2,

// Powerups added in Hexen
	pw_speed,
	pw_minotaur,

	NUMPOWERS
	
} powertype_t;

STREAM_ENUM (powertype_t)

// An artifact is something the player can pickup and carry around
// in his/her inventory.
class AArtifact : public AInventory
{
	DECLARE_ACTOR (AArtifact, AInventory)
public:
	virtual void SetDormant ();
protected:
	virtual void SetHiddenState ();
	virtual void PlayPickupSound (AActor *toucher);
};

// A powerup is an artifact that can be made to activate immediately
// on pickup with the appropriate dmflags setting.
class APowerup : public AArtifact
{
	DECLARE_CLASS (APowerup, AArtifact)
};

class player_s;
void P_PlayerNextArtifact (player_s *player);
void P_PlayerRemoveArtifact (player_s *player, int slot);
void P_PlayerUseArtifact (player_s *player, artitype_t arti);

artitype_t P_NextInventory (player_s *player, artitype_t arti);
artitype_t P_PrevInventory (player_s *player, artitype_t arti);
artitype_t P_FindNamedInventory (const char *name);

bool P_UseArtifact (player_s *player, artitype_t arti);

// Helper macro to save me some typing --------------------------------------

#define BASIC_ARTI(name,type,msg) \
	class AArti##name : public AArtifact { \
		DECLARE_ACTOR (AArti##name, AArtifact) protected: \
		bool TryPickup (AActor *toucher) { \
			return P_GiveArtifact (toucher->player, type); } \
			const char *PickupMessage () { return msg; } 

// ^^^^^^^^^^^^^^^^^^ Notice ^^^^^^^^^^^^^^^^^^^ No closing };

#define POWER_ARTI(name,type,msg) \
	class AArti##name : public APowerup { \
		DECLARE_ACTOR (AArti##name, APowerup) protected: \
		bool TryPickup (AActor *toucher) { \
			return P_GiveArtifact (toucher->player, type); } \
			const char *PickupMessage () { return msg; } 

#endif //__A_ARTIFACTS_H__