#ifndef __THINGDEF_H
#define __THINGDEF_H

//==========================================================================
//
// This class is for storing a name inside a const PClass* field without
// generating compiler warnings. It does not manipulate data in any other
// way.
//
//==========================================================================
class fuglyname : public FName
{
public:
	fuglyname() : FName() {}
	fuglyname(const char *foo) : FName(foo) {}
	operator const PClass *()
	{
		return reinterpret_cast<const PClass *>(size_t(int(*this)));
	}
	fuglyname &operator= (const PClass *foo)
	{
		FName *p = this;
		*p = ENamedName(reinterpret_cast<size_t>(foo));
		return *this;
	}
};


//==========================================================================
//
// Dropitem list
//
//==========================================================================

struct FDropItem 
{
	FName Name;
	int probability;
	int amount;
	FDropItem * Next;
};

FDropItem *GetDropItems(const PClass * cls);

//==========================================================================
//
// Extra info maintained while defining an actor.
//
//==========================================================================

struct Baggage
{
	FActorInfo *Info;
	bool DropItemSet;
	bool StateSet;
	int CurrentState;

	FDropItem *DropItemList;
};

inline void ResetBaggage (Baggage *bag)
{
	bag->DropItemList = NULL;
	bag->DropItemSet = false;
	bag->CurrentState = 0;
	bag->StateSet = false;
}


//==========================================================================
//
// Action function lookup
//
//==========================================================================

struct AFuncDesc
{
	const char *Name;
	actionf_p Function;
};

AFuncDesc * FindFunction(const char * string);


//==========================================================================
//
// State parser
//
//==========================================================================

extern TArray<int> StateParameters;
extern TArray<FName> JumpParameters;

void ClearStateLabels();
void AddState (const char * statename, FState * state);
FState * FindState(AActor * actor, const PClass * type, const char * name);
void InstallStates(FActorInfo *info, AActor *defaults);
void MakeStateDefines(const FStateLabels *list);
FState *P_GetState(AActor *self, FState *CallingState, int offset);
int FinishStates (FActorInfo *actor, AActor *defaults, Baggage &bag);
int ParseStates(FActorInfo * actor, AActor * defaults, Baggage &bag);
FState *CheckState(PClass *type);


//==========================================================================
//
// Property parser
//
//==========================================================================

void ParseActorProperty(Baggage &bag);
void ParseActorFlag (Baggage &bag, int mod);
void FinishActor(FActorInfo *info, Baggage &bag);

void ParseConstant (PSymbolTable * symt, PClass *cls);
void ParseEnum (PSymbolTable * symt, PClass *cls);


int ParseExpression (bool _not, PClass *cls);

int EvalExpressionI (int id, AActor *self, const PClass *cls=NULL);
float EvalExpressionF (int id, AActor *self, const PClass *cls=NULL);
bool EvalExpressionN (int id, AActor *self, const PClass *cls=NULL);


// A truly awful hack to get to the state that called an action function
// without knowing whether it has been called from a weapon or actor.
extern FState * CallingState;
int CheckIndex(int paramsize, FState ** pcallstate=NULL);

void A_ExplodeParms(AActor * self);

enum
{
	ACMETA_BASE				= 0x83000,
	ACMETA_DropItems,		// Int (index into DropItemList)
	ACMETA_ExplosionDamage,
	ACMETA_ExplosionRadius,
	ACMETA_DontHurtShooter,
	ACMETA_MeleeSound,
	ACMETA_MeleeDamage,
	ACMETA_MissileName,
	ACMETA_MissileHeight,
};


// The contents of string must be lowercase (for now, anyway)
int FindLineSpecialEx (const char *string, int *minargs, int *maxargs);


// Types of old style decorations
enum EDefinitionType
{
	DEF_Decoration,
	DEF_BreakableDecoration,
	DEF_Pickup,
	DEF_Projectile,
};



#endif
