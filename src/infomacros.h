/*
** infomacros.h
**
**---------------------------------------------------------------------------
** Copyright 1998-2001 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
** This file contains macros for building Actor defaults lists
** Defaults lists are byte-packed arrays of keys and values.
**
** For Visual C++, a simple byte array will do, with one
** restriction: Any strings must be at the end of the array.
**
** For GCC, we need to get more complicated: Everything needs
** to be a separate variable. Fortunately, GCC byte-aligns
** char variables, so as long they appear in the right order
** (which they do in all version I tried), this will work.
*/

#ifndef __INFOMACROS_H__
#define __INFOMACROS_H__

#ifndef __INFO_H__
#error infomacros.h is meant to be included by info.h
#endif

#ifdef WORDS_BIGENDIAN
#define BREAK_WORD(x) ((unsigned)(x)>>8), (x)&255
#define BREAK_LONG(x) ((unsigned)(x)>>24), ((x)>>16)&255, ((x)>>8)&255, (x)&255
#else
#define BREAK_WORD(x) (x)&255, ((unsigned)(x))>>8
#define BREAK_LONG(x) (x)&255, ((x)>>8)&255, ((x)>>16)&255, ((unsigned)(x))>>24
#endif

typedef void (*voidfunc_)();


#if defined(_MSC_VER)

/************************************************
************** Visual C++ macros ****************
************************************************/

#pragma data_seg(".areg$u")		// ActorInfo initializer list
#pragma data_seg(".greg$u")		// AT_GAME_SET list
#pragma data_seg(".sreg$u")		// AT_SPEED_SET list
#pragma data_seg()

#define DOOMEDNUMOF(actor) actor##Defaults_.DoomEdNum

#define BEGIN_DEFAULTS_PRE(actor) \
	extern FActorInfo actor##Defaults_; \
	__declspec(allocate(".areg$u")) FActorInfo *actor##DefaultsReg = &actor##Defaults_; \
	FActorInfo actor##Defaults_ = {

#define BEGIN_DEFAULTS_POST(actor,game,ednum,id) \
	GAME_##game, id, ednum,

#ifdef WORDS_BIGENDIAN
#define END_DEFAULTS "\xED\x5E" };
#else
#define END_DEFAULTS "\x5E\xED" };
#endif

#define ADD_BYTE_PROP(prop,val) BREAK_WORD(prop|ADEFTYPE_Byte), val,
#define ADD_FIXD_PROP(prop,val) BREAK_WORD(prop|ADEFTYPE_FixedMul), val,
#define ADD_WORD_PROP(prop,val) BREAK_WORD(prop|ADEFTYPE_Word), BREAK_WORD(val),
#define ADD_LONG_PROP(prop,val) BREAK_WORD(prop|ADEFTYPE_Long), BREAK_LONG(val),
#ifdef WORDS_BIGENDIAN
#define ADD_STRING_PROP(prop1,prop2,val) "\0" prop2 val "\0"
#else
#define ADD_STRING_PROP(prop1,prop2,val) prop2 "\0" val "\0"
#endif

// Define a function that gets called when the game is started.
#define AT_GAME_SET(ns) \
	extern void ns##_gs(); \
	__declspec(allocate(".greg$u")) voidfunc_ ns##_gsr = ns##_gs; \
	void ns##_gs ()

// Define a function that gets called when the speed is changed.
#define AT_SPEED_SET(ns,varname) \
	extern void ns##_ss(EGameSpeed); \
	__declspec(allocate(".sreg$u")) void (*ns##_gsr)(EGameSpeed) = ns##_ss; \
	void ns##_ss (EGameSpeed varname)

#elif defined(__GNUC__)

/************************************************
****************** GCC macros *******************
************************************************/

#define DOOMEDNUMOF(actor) actor##Defaults_::TheInfo.DoomEdNum

// All variables used for the default list must be declared extern to
// ensure that GCC actually generates them in the object file.
#define BEGIN_DEFAULTS_PRE(actor) \
	namespace actor##Defaults_ { \
	extern FActorInfo *DefaultsReg; \
	extern FActorInfo TheInfo; \
	FActorInfo *DefaultsReg __attribute__((section("areg"))) = &TheInfo; \
	FActorInfo TheInfo = {

#define BEGIN_DEFAULTS_POST(actor,game,ednum,id) \
	GAME_##game, id, ednum };

// Be very careful! If any of these arrays ends up being all zero, later GCCs will move the array into the BSS
// section, which we do not want.
#define END_DEFAULTS extern BYTE endoflist[2]; BYTE endoflist[2] = { BREAK_WORD(ADEF_EOL) }; }

#define ADD_BYTE_PROP(prop,val) extern BYTE prop##_1[3]; BYTE prop##_1[3] = { BREAK_WORD(prop|ADEFTYPE_Byte), val };
#define ADD_FIXD_PROP(prop,val) extern BYTE prop##_1[3]; BYTE prop##_1[3] = { BREAK_WORD(prop|ADEFTYPE_FixedMul), val };
#define ADD_WORD_PROP(prop,val) extern BYTE prop##_1[4]; BYTE prop##_1[4] = { BREAK_WORD(prop|ADEFTYPE_Word), BREAK_WORD(val) };
#define ADD_LONG_PROP(prop,val) extern BYTE prop##_1[6]; BYTE prop##_1[6] = { BREAK_WORD(prop|ADEFTYPE_Long), BREAK_LONG(val) };
#define ADD_STRING_PROP(prop1,prop2,val) struct prop1##_s { BYTE label[2]; BYTE content[sizeof(val)]; }; extern prop1##_s prop1##_1; \
								prop1##_s prop1##_1 = { BREAK_WORD(prop1), val };
	
#define AT_GAME_SET(ns) \
	extern void ns##_gs(); \
	voidfunc_ ns##_gsr __attribute__((section("greg"))) = ns##_gs; \
	void ns##_gs ()

//typedef void (*speedfunc)(EGameSpeed);

#define AT_SPEED_SET(ns,varname) \
	extern void ns##_ss(EGameSpeed); \
	void (*ns##_gsr)(EGameSpeed) __attribute__((section("sreg"))) = ns##_ss; \
	void ns##_ss (EGameSpeed varname)

#else

#error Actor default lists are only implemented for Visual C++ and GCC

#endif

// Common macros

#define SimpleSpeedSetter(T,normalSpeed,fastSpeed,speed) \
	{ GetDefault<T>()->Speed = (speed == SPEED_Fast) ? fastSpeed : normalSpeed; }

#define AT_GAME_SET_FRIEND(ns)	friend void ns##_gs();

#define DECLARE_STATELESS_ACTOR(cls,parent) \
	DECLARE_CLASS(cls,parent) \
protected: \
	cls () throw() {} \
public:

#define DECLARE_ACTOR(cls,parent) \
	DECLARE_STATELESS_ACTOR(cls,parent) \
	static FState States[];

#define BEGIN_DEFAULTS(actor,game,ednum,spawnid) \
	BEGIN_DEFAULTS_PRE(actor) \
	RUNTIME_CLASS(actor), &actor::States[0], NULL, sizeof(actor::States)/sizeof(actor::States[0]), \
	BEGIN_DEFAULTS_POST(actor,game,ednum,spawnid)

#define BEGIN_STATELESS_DEFAULTS(actor,game,ednum,spawnid) \
	BEGIN_DEFAULTS_PRE(actor) \
	RUNTIME_CLASS(actor), NULL, NULL, 0, \
	BEGIN_DEFAULTS_POST(actor,game,ednum,spawnid)

// IMPLEMENT_ACTOR combines IMPLEMENT_CLASS and BEGIN_DEFAULTS
#define IMPLEMENT_ACTOR(actor,game,ednum,spawnid) \
	IMPLEMENT_CLASS(actor) BEGIN_DEFAULTS(actor,game,ednum,spawnid)

#define IMPLEMENT_STATELESS_ACTOR(actor,game,ednum,spawnid) \
	IMPLEMENT_CLASS(actor) BEGIN_STATELESS_DEFAULTS(actor,game,ednum,spawnid)

#define IMPLEMENT_ABSTRACT_ACTOR(actor) \
	IMPLEMENT_STATELESS_ACTOR(actor,Any,-1,0) END_DEFAULTS

#define PROP_SeeSound(x)				ADD_STRING_PROP(ADEF_SeeSound,"\1",x)
#define PROP_AttackSound(x)				ADD_STRING_PROP(ADEF_AttackSound,"\2",x)
#define PROP_PainSound(x)				ADD_STRING_PROP(ADEF_PainSound,"\3",x)
#define PROP_DeathSound(x)				ADD_STRING_PROP(ADEF_DeathSound,"\4",x)
#define PROP_ActiveSound(x)				ADD_STRING_PROP(ADEF_ActiveSound,"\5",x)
#define PROP_UseSound(x)				ADD_STRING_PROP(ADEF_UseSound,"\6",x)
#define PROP_Weapon_UpSound(x)			ADD_STRING_PROP(ADEF_Weapon_UpSound,"\7",x)
#define PROP_Weapon_ReadySound(x)		ADD_STRING_PROP(ADEF_Weapon_ReadySound,"\10",x)
#define PROP_Tag(x)						ADD_STRING_PROP(ADEF_Tag,"\11",x)
#define PROP_Weapon_AmmoType1(x)		ADD_STRING_PROP(ADEF_Weapon_AmmoType1,"\12",x)
#define PROP_Weapon_AmmoType2(x)		ADD_STRING_PROP(ADEF_Weapon_AmmoType2,"\13",x)
#define PROP_Weapon_SisterType(x)		ADD_STRING_PROP(ADEF_Weapon_SisterType,"\14",x)
#define PROP_Weapon_ProjectileType(x)	ADD_STRING_PROP(ADEF_Weapon_ProjectileType,"\15",x)
#define PROP_PowerupGiver_Powerup(x)	ADD_STRING_PROP(ADEF_PowerupGiver_Powerup,"\16",x)
#define PROP_Inventory_Icon(x)			ADD_STRING_PROP(ADEF_Inventory_Icon,"\17",x)

#define PROP_XScale(x)					ADD_BYTE_PROP(ADEF_XScale,x)
#define PROP_YScale(x)					ADD_BYTE_PROP(ADEF_YScale,x)
#define PROP_SpawnHealth(x)				ADD_WORD_PROP(ADEF_SpawnHealth,x)
#define PROP_SpawnHealthLong(x)			ADD_LONG_PROP(ADEF_SpawnHealth,x)
#define PROP_ReactionTime(x)			ADD_BYTE_PROP(ADEF_ReactionTime,x)
#define PROP_PainChance(x)				ADD_BYTE_PROP(ADEF_PainChance,x)
#define PROP_MaxPainChance				ADD_WORD_PROP(ADEF_PainChance,256)
#define PROP_SpeedFixed(x)				ADD_FIXD_PROP(ADEF_Speed,x)
#define PROP_SpeedLong(x)				ADD_LONG_PROP(ADEF_Speed,x)
#define PROP_Radius(x)					ADD_LONG_PROP(ADEF_Radius,x)
#define PROP_RadiusFixed(x)				ADD_FIXD_PROP(ADEF_Radius,x)
#define PROP_Height(x)					ADD_LONG_PROP(ADEF_Height,x)
#define PROP_HeightFixed(x)				ADD_FIXD_PROP(ADEF_Height,x)
#define PROP_Mass(x)					ADD_WORD_PROP(ADEF_Mass,x)
#define PROP_MassLong(x)				ADD_LONG_PROP(ADEF_Mass,x)
#define PROP_Damage(x)					ADD_BYTE_PROP(ADEF_Damage,x)
#define PROP_DamageLong(x)				ADD_LONG_PROP(ADEF_Damage,x)
#define PROP_DamageType(x)				ADD_BYTE_PROP(ADEF_DamageType,x)
#define PROP_Flags(x)					ADD_LONG_PROP(ADEF_Flags,x)
#define PROP_Flags2(x)					ADD_LONG_PROP(ADEF_Flags2,x)
#define PROP_Flags3(x)					ADD_LONG_PROP(ADEF_Flags3,x)
#define PROP_Flags4(x)					ADD_LONG_PROP(ADEF_Flags4,x)
#define PROP_FlagsSet(x)				ADD_LONG_PROP(ADEF_FlagsSet,x)
#define PROP_Flags2Set(x)				ADD_LONG_PROP(ADEF_Flags2Set,x)
#define PROP_Flags3Set(x)				ADD_LONG_PROP(ADEF_Flags3Set,x)
#define PROP_Flags4Set(x)				ADD_LONG_PROP(ADEF_Flags4Set,x)
#define PROP_FlagsClear(x)				ADD_LONG_PROP(ADEF_FlagsClear,x)
#define PROP_Flags2Clear(x)				ADD_LONG_PROP(ADEF_Flags2Clear,x)
#define PROP_Flags3Clear(x)				ADD_LONG_PROP(ADEF_Flags3Clear,x)
#define PROP_Flags4Clear(x)				ADD_LONG_PROP(ADEF_Flags4Clear,x)
#define PROP_Alpha(x)					ADD_LONG_PROP(ADEF_Alpha,x)
#define PROP_RenderStyle(x)				ADD_BYTE_PROP(ADEF_RenderStyle,x)
#define PROP_RenderFlags(x)				ADD_WORD_PROP(ADEF_RenderFlags,x)
#define PROP_Translation(x,y)			ADD_WORD_PROP(ADEF_Translation,((x)<<8)|(y))
#define PROP_MinMissileChance(x)		ADD_BYTE_PROP(ADEF_MinMissileChance,x)

#define PROP_SpawnState(x)				ADD_BYTE_PROP(ADEF_SpawnState,x)
#define PROP_SeeState(x)				ADD_BYTE_PROP(ADEF_SeeState,x)
#define PROP_PainState(x)				ADD_BYTE_PROP(ADEF_PainState,x)
#define PROP_MeleeState(x)				ADD_BYTE_PROP(ADEF_MeleeState,x)
#define PROP_MissileState(x)			ADD_BYTE_PROP(ADEF_MissileState,x)
#define PROP_CrashState(x)				ADD_BYTE_PROP(ADEF_CrashState,x)
#define PROP_DeathState(x)				ADD_BYTE_PROP(ADEF_DeathState,x)
#define PROP_XDeathState(x)				ADD_BYTE_PROP(ADEF_XDeathState,x)
#define PROP_BDeathState(x)				ADD_BYTE_PROP(ADEF_BDeathState,x)
#define PROP_IDeathState(x)				ADD_BYTE_PROP(ADEF_IDeathState,x)
#define PROP_EDeathState(x)				ADD_BYTE_PROP(ADEF_EDeathState,x)
#define PROP_RaiseState(x)				ADD_BYTE_PROP(ADEF_RaiseState,x)
#define PROP_WoundState(x)				ADD_BYTE_PROP(ADEF_WoundState,x)

#define PROP_StrifeType(x)				ADD_WORD_PROP(ADEF_StrifeType,x)
#define PROP_StrifeTeaserType(x)		ADD_WORD_PROP(ADEF_StrifeTeaserType,x)
#define PROP_StrifeTeaserType2(x)		ADD_WORD_PROP(ADEF_StrifeTeaserType2,x)

#define PROP_Inventory_Amount(x)		ADD_BYTE_PROP(ADEF_Inventory_Amount,x)
#define PROP_Inventory_AmountWord(x)	ADD_WORD_PROP(ADEF_Inventory_Amount,x)
#define PROP_Inventory_MaxAmount(x)		ADD_WORD_PROP(ADEF_Inventory_MaxAmount,x)
#define PROP_Inventory_MaxAmountLong(x)	ADD_LONG_PROP(ADEF_Inventory_MaxAmount,x)
#define PROP_Inventory_DefMaxAmount		ADD_BYTE_PROP(ADEF_Inventory_DefMaxAmount,0)
#define PROP_Inventory_RespawnTics(x)	ADD_WORD_PROP(ADEF_Inventory_RespawnTics,x)
#define PROP_Inventory_FlagsSet(x)		ADD_LONG_PROP(ADEF_Inventory_FlagsSet,x)
#define PROP_Inventory_FlagsClear(x)	ADD_LONG_PROP(ADEF_Inventory_FlagsClear,x)

#define PROP_PuzzleItem_Number(x)		ADD_BYTE_PROP(ADEF_PuzzleItem_Number,x)

#define PROP_BasicArmorPickup_SavePercent(x)	ADD_LONG_PROP(ADEF_BasicArmorPickup_SavePercent,x)
#define PROP_BasicArmorPickup_SaveAmount(x)		ADD_BYTE_PROP(ADEF_BasicArmorPickup_SaveAmount,x)
#define PROP_BasicArmorBonus_SavePercent(x)		ADD_LONG_PROP(ADEF_BasicArmorBonus_SavePercent,x)
#define PROP_BasicArmorBonus_SaveAmount(x)		ADD_BYTE_PROP(ADEF_BasicArmorBonus_SaveAmount,x)
#define PROP_BasicArmorBonus_MaxSaveAmount(x)	ADD_BYTE_PROP(ADEF_BasicArmorBonus_MaxSaveAmount,x)
#define PROP_HexenArmor_ArmorAmount(x)	ADD_BYTE_PROP(ADEF_HexenArmor_ArmorAmount,x)

#define PROP_PowerupGiver_EffectTics(x)	ADD_LONG_PROP(ADEF_PowerupGiver_EffectTics,x)
#define PROP_Powerup_EffectTics(x)		ADD_LONG_PROP(ADEF_Powerup_EffectTics,x)
#define PROP_Powerup_Color(a,r,g,b)		ADD_LONG_PROP(ADEF_Powerup_Color,((a)<<24)|((r)<<16)|((g)<<8)|(b))

#define PROP_Key_KeyNumber(x)			ADD_BYTE_PROP(ADEF_Key_KeyNumber,x)
#define PROP_Key_AltKeyNumber(x)		ADD_BYTE_PROP(ADEF_Key_AltKeyNumber,x)

#define PROP_Ammo_BackpackAmount(x)		ADD_WORD_PROP(ADEF_Ammo_BackpackAmount,x)
#define PROP_Ammo_BackpackMaxAmount(x)	ADD_WORD_PROP(ADEF_Ammo_BackpackMaxAmount,x)

#define PROP_Weapon_Flags(x)			ADD_LONG_PROP(ADEF_Weapon_Flags,x)
#define PROP_Weapon_AmmoGive1(x)		ADD_BYTE_PROP(ADEF_Weapon_AmmoGive1,x)
#define PROP_Weapon_AmmoGive2(x)		ADD_BYTE_PROP(ADEF_Weapon_AmmoGive2,x)
#define PROP_Weapon_AmmoUse1(x)			ADD_BYTE_PROP(ADEF_Weapon_AmmoUse1,x)
#define PROP_Weapon_AmmoUse2(x)			ADD_BYTE_PROP(ADEF_Weapon_AmmoUse2,x)
#define PROP_Weapon_Kickback(x)			ADD_BYTE_PROP(ADEF_Weapon_Kickback,x)
#define PROP_Weapon_YAdjust(x)			ADD_FIXD_PROP(ADEF_Weapon_YAdjust,x)
#define PROP_Weapon_SelectionOrder(x)	ADD_WORD_PROP(ADEF_Weapon_SelectionOrder,x)
#define PROP_Weapon_MoveCombatDist(x)	ADD_LONG_PROP(ADEF_Weapon_MoveCombatDist,x)
#define PROP_Weapon_UpState(x)			ADD_BYTE_PROP(ADEF_Weapon_UpState,x)
#define PROP_Weapon_DownState(x)		ADD_BYTE_PROP(ADEF_Weapon_DownState,x)
#define PROP_Weapon_ReadyState(x)		ADD_BYTE_PROP(ADEF_Weapon_ReadyState,x)
#define PROP_Weapon_AtkState(x)			ADD_BYTE_PROP(ADEF_Weapon_AtkState,x)
#define PROP_Weapon_HoldAtkState(x)		ADD_BYTE_PROP(ADEF_Weapon_HoldAtkState,x)
#define PROP_Weapon_AltAtkState(x)		ADD_BYTE_PROP(ADEF_Weapon_AltAtkState,x)
#define PROP_Weapon_AltHoldAtkState(x)	ADD_BYTE_PROP(ADEF_Weapon_AltHoldAtkState,x)
#define PROP_Weapon_FlashState(x)		ADD_BYTE_PROP(ADEF_Weapon_FlashState,x)
#define PROP_Sigil_NumPieces(x)			ADD_BYTE_PROP(ADEF_Sigil_NumPieces,x)

#define PROP_SKIP_SUPER					ADD_BYTE_PROP(ADEF_SkipSuper,0)

#endif //__INFOMACROS_H__
