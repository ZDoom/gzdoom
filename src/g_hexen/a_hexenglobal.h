#ifndef __A_HEXENGLOBAL_H__
#define __A_HEXENGLOBAL_H__

class ALightning : public AActor
{
	DECLARE_STATELESS_ACTOR (ALightning, AActor)
public:
	int SpecialMissileHit (AActor *victim);
};

class APoisonCloud : public AActor
{
	DECLARE_ACTOR (APoisonCloud, AActor)
public:
	void GetExplodeParms (int &damage, int &distance, bool &hurtSource);
	int DoSpecialDamage (AActor *target, int damage);
	void BeginPlay ();
};

class ABishop : public AActor
{
	DECLARE_ACTOR (ABishop, AActor)
public:
	void GetExplodeParms (int &damage, int &distance, bool &hurtSource);
	bool OkayToSwitchTarget (AActor *other);
};

class AHeresiarch : public AActor
{
	DECLARE_ACTOR (AHeresiarch, AActor)
public:
	const TypeInfo *StopBall;

	void Serialize (FArchive &arc);
	void Die (AActor *source, AActor *inflictor);
	bool AdjustReflectionAngle (AActor *thing, angle_t &angle);
	bool OkayToSwitchTarget (AActor *other);
};

class AHolySpirit : public AActor
{
	DECLARE_ACTOR (AHolySpirit, AActor)
public:
	bool Slam (AActor *thing);
	bool SpecialBlastHandling (AActor *source, fixed_t strength);
	bool IsOkayToAttack (AActor *link);
};

class AHammerPuff : public AActor
{
	DECLARE_ACTOR (AHammerPuff, AActor)
public:
	void BeginPlay ();
};

class ACentaur : public AActor
{
	DECLARE_ACTOR (ACentaur, AActor)
public:
	void Howl ();
	bool AdjustReflectionAngle (AActor *thing, angle_t &angle);
};

class AFourthWeaponPiece : public AInventory
{
	DECLARE_STATELESS_ACTOR (AFourthWeaponPiece, AInventory)
	HAS_OBJECT_POINTERS
public:
	void Serialize (FArchive &arc);
	bool TryPickup (AActor *toucher);
	const char *PickupMessage ();
	bool ShouldStay ();
	void PlayPickupSound (AActor *toucher);
protected:
	virtual bool MatchPlayerClass (AActor *toucher);
	virtual const char *PieceMessage ();
	const TypeInfo *FourthWeaponClass;
	int PieceValue;
	AInventory *TempFourthWeapon;
	bool PrivateShouldStay ();
};

class AFighterPlayer : public APlayerPawn
{
	DECLARE_ACTOR (AFighterPlayer, APlayerPawn)
public:
	void PlayAttacking2 ();
	void GiveDefaultInventory ();
	void TweakSpeeds (int &forward, int &side);
	const char *GetSoundClass ();
	fixed_t GetJumpZ ();
	int GetArmorMax ();
	int GetAutoArmorSave ();
	fixed_t GetArmorIncrement (int armortype);
	bool DoHealingRadius (APlayerPawn *other);
};

class AFighterWeapon : public AWeapon
{
	DECLARE_STATELESS_ACTOR (AFighterWeapon, AWeapon);
public:
	bool TryPickup (AActor *toucher);
};

class AClericPlayer : public APlayerPawn
{
	DECLARE_ACTOR (AClericPlayer, APlayerPawn)
public:
	void PlayAttacking2 ();
	void GiveDefaultInventory ();
	const char *GetSoundClass ();
	fixed_t GetJumpZ ();
	int GetArmorMax ();
	int GetAutoArmorSave ();
	fixed_t GetArmorIncrement (int armortype);
};

class AClericWeapon : public AWeapon
{
	DECLARE_STATELESS_ACTOR (AClericWeapon, AWeapon);
public:
	bool TryPickup (AActor *toucher);
};

class AMagePlayer : public APlayerPawn
{
	DECLARE_ACTOR (AMagePlayer, APlayerPawn)
public:
	void PlayAttacking2 ();
	void GiveDefaultInventory ();
	void TweakSpeeds (int &forward, int &side);
	const char *GetSoundClass ();
	fixed_t GetJumpZ ();
	int GetArmorMax ();
	int GetAutoArmorSave ();
	fixed_t GetArmorIncrement (int armortype);
	bool DoHealingRadius (APlayerPawn *other);
};

class AMageWeapon : public AWeapon
{
	DECLARE_STATELESS_ACTOR (AMageWeapon, AWeapon);
public:
	bool TryPickup (AActor *toucher);
};

#endif //__A_HEXENGLOBAL_H__
