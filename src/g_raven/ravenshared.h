#ifndef __RAVENSHARED_H__
#define __RAVENSHARED_H__

class AActor;
class player_s;

bool P_MorphPlayer (player_s *player);
bool P_UndoPlayerMorph (player_s *player, bool force);

bool P_MorphMonster (AActor *actor, const TypeInfo *morphClass);
bool P_UpdateMorphedMonster (AActor *actor, int tics);

class AMinotaur : public AActor
{
	DECLARE_ACTOR (AMinotaur, AActor)
public:
	void NoBlockingSet ();
	int DoSpecialDamage (AActor *target, int damage);
	void BeginPlay ();
	void Serialize (FArchive &arc);

public:
	int StartTime;
	bool bIsFriend;
	bool IsOkayToAttack (AActor *target);
	void Die (AActor *source, AActor *inflictor);
	bool NewTarget (AActor *other);
	bool Slam (AActor *);
	void Tick ();
};

class AMinotaurFriend : public AMinotaur
{
	DECLARE_STATELESS_ACTOR (AMinotaurFriend, AMinotaur)
public:
	void BeginPlay ();
};

#endif //__RAVENSHARED_H__
