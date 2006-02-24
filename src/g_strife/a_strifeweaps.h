#include "a_pickups.h"

class AHEGrenadeRounds : public AAmmo
{
	DECLARE_ACTOR (AHEGrenadeRounds, AAmmo);
public:
	virtual const char *PickupMessage ();
};

class APhosphorusGrenadeRounds : public AAmmo
{
	DECLARE_ACTOR (APhosphorusGrenadeRounds, AAmmo);
public:
	virtual const char *PickupMessage ();
};

class AClipOfBullets : public AAmmo
{
	DECLARE_ACTOR (AClipOfBullets, AAmmo)
public:
	virtual const char *PickupMessage ();
};

class ABoxOfBullets : public AClipOfBullets
{
	DECLARE_ACTOR (ABoxOfBullets, AClipOfBullets)
public:
	virtual const char *PickupMessage ();
};

class AMiniMissiles : public AAmmo
{
	DECLARE_ACTOR (AMiniMissiles, AAmmo)
public:
	virtual const char *PickupMessage ();
};

class ACrateOfMissiles : public AMiniMissiles
{
	DECLARE_ACTOR (ACrateOfMissiles, AMiniMissiles)
public:
	virtual const char *PickupMessage ();
};

class AEnergyPod : public AAmmo
{
	DECLARE_ACTOR (AEnergyPod, AAmmo)
public:
	virtual const char *PickupMessage ();
};

class AEnergyPack : public AEnergyPod
{
	DECLARE_ACTOR (AEnergyPack, AEnergyPod)
public:
	virtual const char *PickupMessage ();
};

class APoisonBolts : public AAmmo
{
	DECLARE_ACTOR (APoisonBolts, AAmmo)
public:
	virtual const char *PickupMessage ();
};

class AElectricBolts : public AAmmo
{
	DECLARE_ACTOR (AElectricBolts, AAmmo)
public:
	virtual const char *PickupMessage ();
};
