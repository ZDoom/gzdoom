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
void AddStateDefines(const FStateLabels *list);
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
int ParseParameter(FScanner &sc, PClass *cls, char type, bool constant);


int ParseExpression (FScanner &sc, bool _not, PClass *cls);

bool IsExpressionConst(int id);
int EvalExpressionI (int id, AActor *self, const PClass *cls=NULL);
float EvalExpressionF (int id, AActor *self, const PClass *cls=NULL);


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
#define DECLARE_ACTION(name) void AF_##name(AActor *self, FState *, int);

// This distinction is here so that CALL_ACTION produces errors when trying to
// access a function that requires parameters.
#define DEFINE_ACTION_FUNCTION(cls, name) \
	void AF_##name (AActor *self, FState *, int); \
	AFuncDesc info_##cls##_##name = { #name, AF_##name }; \
	MSVC_ASEG AFuncDesc *infoptr_##cls##_##name GCC_ASEG = &info_##cls##_##name; \
	void AF_##name (AActor *self, FState *, int)

#define DEFINE_ACTION_FUNCTION_PARAMS(cls, name) \
	void AFP_##name (AActor *self, FState *CallingState, int ParameterIndex); \
	AFuncDesc info_##cls##_##name = { #name, AFP_##name }; \
	MSVC_ASEG AFuncDesc *infoptr_##cls##_##name GCC_ASEG = &info_##cls##_##name; \
	void AFP_##name (AActor *self, FState *CallingState, int ParameterIndex)

#define DECLARE_PARAMINFO FState *CallingState, int ParameterIndex
#define PUSH_PARAMINFO CallingState, ParameterIndex

#define CALL_ACTION(name,self) AF_##name(self, NULL, 0)

#define CheckIndex(count) (ParameterIndex-1)

#define ACTION_PARAM_START(count) \
	int ap_index_ = CheckIndex(count); \
	if (ap_index_ <= 0) return;

#define ACTION_PARAM_START_OPTIONAL(count) \
	int ap_index_ = CheckIndex(count); 

#define ACTION_PARAM_INT(var) \
	int var = EvalExpressionI(StateParameters[ap_index_++], self);
#define ACTION_PARAM_BOOL(var) \
	bool var = !!EvalExpressionI(StateParameters[ap_index_++], self);
#define ACTION_PARAM_FIXED(var) \
	fixed_t var = fixed_t(EvalExpressionF(StateParameters[ap_index_++], self)*65536.f);
#define ACTION_PARAM_FLOAT(var) \
	float var = EvalExpressionF(StateParameters[ap_index_++], self);
#define ACTION_PARAM_CLASS(var) \
	const PClass *var = PClass::FindClass(ENamedName(StateParameters[ap_index_++]));
#define ACTION_PARAM_STATE(var) \
	int var = StateParameters[ap_index_++];
#define ACTION_PARAM_SOUND(var) \
	FSoundID var = StateParameters[ap_index_++];
#define ACTION_PARAM_STRING(var) \
	const char *var = FName(ENamedName(StateParameters[ap_index_++]));

#endif
