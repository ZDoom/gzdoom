#ifndef __RAVENSHARED_H__
#define __RAVENSHARED_H__

class AActor;
class player_s;

bool P_MorphPlayer (player_s *player);
bool P_UndoPlayerMorph (player_s *player, bool force);

bool P_MorphMonster (AActor *actor, const PClass *morphClass);
bool P_UpdateMorphedMonster (AActor *actor);

class AMinotaur : public AActor
{
	DECLARE_ACTOR (AMinotaur, AActor)
public:
	void NoBlockingSet ();
	int DoSpecialDamage (AActor *target, int damage);

public:
	bool Slam (AActor *);
	void Tick ();
};

class AMinotaurFriend : public AMinotaur
{
	DECLARE_STATELESS_ACTOR (AMinotaurFriend, AMinotaur)
public:
	int StartTime;

	void NoBlockingSet ();
	bool IsOkayToAttack (AActor *target);
	void Die (AActor *source, AActor *inflictor);
	bool OkayToSwitchTarget (AActor *other);
	void BeginPlay ();
	void Serialize (FArchive &arc);
};

class AEggFX : public AActor
{
	DECLARE_ACTOR (AEggFX, AActor)
public:
	int DoSpecialDamage (AActor *target, int damage);
	void Serialize (FArchive &arc);

	int PlayerClass, MonsterClass;		// actually names
};

class AMorphedMonster : public AActor
{
	DECLARE_ACTOR (AMorphedMonster, AActor)
	HAS_OBJECT_POINTERS
public:
	void Tick ();
	void Serialize (FArchive &arc);
	void Die (AActor *source, AActor *inflictor);
	void Destroy ();

	AActor *UnmorphedMe;
	int UnmorphTime;
	DWORD FlagsSave;
};

#endif //__RAVENSHARED_H__
