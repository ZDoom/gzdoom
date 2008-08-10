#ifndef __THINGDEF_H
#define __THINGDEF_H

class FScanner;

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
int FinishStates (FScanner &sc, FActorInfo *actor, AActor *defaults, Baggage &bag);
int ParseStates(FScanner &sc, FActorInfo *actor, AActor *defaults, Baggage &bag);
FState *CheckState(FScanner &sc, PClass *type);


//==========================================================================
//
// Property parser
//
//==========================================================================

void ParseActorProperty(FScanner &sc, Baggage &bag);
void ParseActorFlag (FScanner &sc, Baggage &bag, int mod);
void FinishActor(FScanner &sc, FActorInfo *info, Baggage &bag);

void ParseConstant (FScanner &sc, PSymbolTable *symt, PClass *cls);
void ParseEnum (FScanner &sc, PSymbolTable *symt, PClass *cls);


int ParseExpression (FScanner &sc, bool _not, PClass *cls);

int EvalExpressionI (int id, AActor *self, const PClass *cls=NULL);
float EvalExpressionF (int id, AActor *self, const PClass *cls=NULL);
bool EvalExpressionN (int id, AActor *self, const PClass *cls=NULL);


// A truly awful hack to get to the state that called an action function
// without knowing whether it has been called from a weapon or actor.
extern FState * CallingState;
int CheckIndex(int paramsize, FState ** pcallstate=NULL);

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


// Types of old style decorations
enum EDefinitionType
{
	DEF_Decoration,
	DEF_BreakableDecoration,
	DEF_Pickup,
	DEF_Projectile,
};

#if defined(_MSC_VER)
#pragma data_seg(".areg$u")
#pragma data_seg()

#define MSVC_ASEG __declspec(allocate(".areg$u"))
#define GCC_ASEG
#else
#define MSVC_ASEG
#define GCC_ASEG __attribute__((section(AREG_SECTION)))
#endif


// Macros to handle action functions. These are here so that I don't have to
// change every single use in case the parameters change.
#define DECLARE_ACTION(name) void AF_##name(AActor *self);

#define DEFINE_ACTION_FUNCTION(cls, name) \
	void AF_##name (AActor *); \
	AFuncDesc info_##cls##_##name = { #name, AF_##name }; \
	MSVC_ASEG AFuncDesc *infoptr_##cls##_##name GCC_ASEG = &info_##cls##_##name; \
	void AF_##name (AActor *self)

#define CALL_ACTION(name,self) AF_##name(self)
#define GET_ACTION(name) AF_##name

#define ACTION_PARAM_START(count) \
	int index = CheckIndex(count); \
	if (index <= 0) return;

#define ACTION_PARAM_START_OPTIONAL(count) \
	int index = CheckIndex(count); 

#define ACTION_INT_PARAM(var) \
	int var = EvalExpressionI(StateParameters[index++], self);
#define ACTION_BOOL_PARAM(var) \
	bool var = !!EvalExpressionI(StateParameters[index++], self);
#define ACTION_NOT_BOOL_PARAM(var) \
	bool var = EvalExpressionN(StateParameters[index++], self);
#define ACTION_FIXED_PARAM(var) \
	fixed_t var = fixed_t(EvalExpressionF(StateParameters[index++], self)*65536.f);
#define ACTION_FLOAT_PARAM(var) \
	float var = EvalExpressionF(StateParameters[index++], self);
#define ACTION_CLASS_PARAM(var) \
	const PClass *var = PClass::FindClass(ENamedName(StateParameters[index++]));
#define ACTION_STATE_PARAM(var) \
	int var = StateParameters[index++];
#define ACTION_SOUND_PARAM(var) \
	FSoundID var = StateParameters[index++];
#define ACTION_STRING_PARAM(var) \
	const char *var = FName(ENamedName(StateParameters[index++]));


#endif
