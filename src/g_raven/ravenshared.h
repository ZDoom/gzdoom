#ifndef __RAVENSHARED_H__
#define __RAVENSHARED_H__

class AActor;

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

#endif //__RAVENSHARED_H__
