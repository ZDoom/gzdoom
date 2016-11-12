#ifndef __RAVENSHARED_H__
#define __RAVENSHARED_H__

class AActor;

class AMinotaur : public AActor
{
	DECLARE_CLASS (AMinotaur, AActor)
public:
	int DoSpecialDamage (AActor *target, int damage, FName damagetype);

public:
	bool Slam (AActor *);
	void Tick ();
};

class AMinotaurFriend : public AMinotaur
{
	DECLARE_CLASS (AMinotaurFriend, AMinotaur)
public:
	int StartTime;

	void Die (AActor *source, AActor *inflictor, int dmgflags);
	void BeginPlay ();
	
	void Serialize(FSerializer &arc);
};

#endif //__RAVENSHARED_H__
