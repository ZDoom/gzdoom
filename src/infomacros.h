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

#ifdef __BIG_ENDIAN__
#define BREAK_WORD(x) ((unsigned)(x)>>8), (x)&255
#define BREAK_LONG(x) ((unsigned)(x)>>24), ((x)>>16)&255, ((x)>>8)&255, (x)&255
#else
#define BREAK_WORD(x) (x)&255, ((unsigned)(x))>>8
#define BREAK_LONG(x) (x)&255, ((x)>>8)&255, ((x)>>16)&255, ((unsigned)(x))>>24
#endif

typedef void (*voidfunc_)();


#if defined(_MSC_VER)

// Visual C++ macros
#pragma data_seg(".areg$u")		// ActorInfo initializer list
#pragma data_seg(".greg$u")		// AT_GAME_SET list
#pragma data_seg(".sreg$u")		// AT_SPEED_SET list
#pragma data_seg(".wreg$u")		// WEAPON1/WEAPON2 list
#pragma data_seg()

#define DOOMEDNUMOF(actor) actor##Defaults_.DoomEdNum

#define BEGIN_DEFAULTS_PRE(actor) \
	extern FActorInfo actor##Defaults_; \
	__declspec(allocate(".areg$u")) FActorInfo *actor##DefaultsReg = &actor##Defaults_; \
	FActorInfo actor##Defaults_ = {

#define BEGIN_DEFAULTS_POST(actor,game,ednum,id) \
	GAME_##game, id, ednum,

#define END_DEFAULTS "" };

#define ADD_BYTE_PROP(prop,val) prop|ADEFTYPE_Byte, val,
#define ADD_FIXD_PROP(prop,val) prop|ADEFTYPE_FixedMul, val,
#define ADD_WORD_PROP(prop,val) prop|ADEFTYPE_Word, BREAK_WORD(val),
#define ADD_LONG_PROP(prop,val) prop|ADEFTYPE_Long, BREAK_LONG(val),
#define ADD_STRING_PROP(prop1,prop2,val) prop2 val "\0"

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

#define ADDWPINF(wp) \
	extern const FWeaponInfoInit *wpinit##wp##ptr; \
	__declspec(allocate(".wreg$u")) const FWeaponInfoInit *wpinit##wp##ptr = &wpinit##wp;

#define WEAPON1(wp,inf) \
static const FWeaponInfoInit wpinit##wp = { wp, &inf::WeaponInfo, &inf::WeaponInfo }; \
	ADDWPINF(wp)

#define WEAPON2(wp,inf) \
static const FWeaponInfoInit wpinit##wp = { wp, &inf::WeaponInfo1, &inf::WeaponInfo2 }; \
	ADDWPINF(wp)


#elif defined(__GNUC__)

// GCC macros

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

#define END_DEFAULTS BYTE endoflist = 0; }

#define ADD_BYTE_PROP(prop,val) extern BYTE prop##_1, prop##_2; BYTE prop##_1 = prop|ADEFTYPE_Byte; BYTE prop##_2 = val;
#define ADD_FIXD_PROP(prop,val) extern BYTE prop##_1, prop##_2; BYTE prop##_1 = prop|ADEFTYPE_FixedMul; BYTE prop##_2 = val;
#define ADD_WORD_PROP(prop,val) extern BYTE prop##_1, prop##_2[2]; BYTE prop##_1 = prop|ADEFTYPE_Word; BYTE prop##_2[2] = { BREAK_WORD(val) };
#define ADD_LONG_PROP(prop,val) extern BYTE prop##_1, prop##_2[4]; BYTE prop##_1 = prop|ADEFTYPE_Long; BYTE prop##_2[4] = { BREAK_LONG(val) };
#define ADD_STRING_PROP(prop1,prop2,val) extern BYTE prop1##_1, prop1##_2[]; BYTE prop1##_1 = prop1; BYTE prop1##_2[] = val;
	
#define AT_GAME_SET(ns) \
	extern void ns##_gs(); \
	voidfunc_ ns##_gsr __attribute__((section("greg"))) = ns##_gs; \
	void ns##_gs ()

//typedef void (*speedfunc)(EGameSpeed);

#define AT_SPEED_SET(ns,varname) \
	extern void ns##_ss(EGameSpeed); \
	void (*ns##_gsr)(EGameSpeed) __attribute__((section("sreg"))) = ns##_ss; \
	void ns##_ss (EGameSpeed varname)

#define ADDWPINF(wp) \
	extern const FWeaponInfoInit *wpinit##wp##ptr; \
	const FWeaponInfoInit *__attribute__((section("wreg"))) wpinit##wp##ptr = &wpinit##wp;

#define WEAPON1(wp,inf) \
static const FWeaponInfoInit wpinit##wp = { wp, &inf::WeaponInfo, &inf::WeaponInfo }; \
	ADDWPINF(wp)

#define WEAPON2(wp,inf) \
static const FWeaponInfoInit wpinit##wp = { wp, &inf::WeaponInfo1, &inf::WeaponInfo2 }; \
	ADDWPINF(wp)

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

#define PROP_SeeSound(x)		ADD_STRING_PROP(ADEF_SeeSound,"\1",x)
#define PROP_AttackSound(x)		ADD_STRING_PROP(ADEF_AttackSound,"\2",x)
#define PROP_PainSound(x)		ADD_STRING_PROP(ADEF_PainSound,"\3",x)
#define PROP_DeathSound(x)		ADD_STRING_PROP(ADEF_DeathSound,"\4",x)
#define PROP_ActiveSound(x)		ADD_STRING_PROP(ADEF_ActiveSound,"\5",x)

#define PROP_XScale(x)			ADD_BYTE_PROP(ADEF_XScale,x)
#define PROP_YScale(x)			ADD_BYTE_PROP(ADEF_YScale,x)
#define PROP_SpawnHealth(x)		ADD_WORD_PROP(ADEF_SpawnHealth,x)
#define PROP_ReactionTime(x)	ADD_BYTE_PROP(ADEF_ReactionTime,x)
#define PROP_PainChance(x)		ADD_BYTE_PROP(ADEF_PainChance,x)
#define PROP_MaxPainChance		ADD_WORD_PROP(ADEF_PainChance,256)
#define PROP_SpeedLong(x)		ADD_LONG_PROP(ADEF_Speed,x)
#define PROP_SpeedFixed(x)		ADD_FIXD_PROP(ADEF_Speed,x)
#define PROP_Radius(x)			ADD_LONG_PROP(ADEF_Radius,x)
#define PROP_RadiusFixed(x)		ADD_FIXD_PROP(ADEF_Radius,x)
#define PROP_Height(x)			ADD_LONG_PROP(ADEF_Height,x)
#define PROP_HeightFixed(x)		ADD_FIXD_PROP(ADEF_Height,x)
#define PROP_Mass(x)			ADD_WORD_PROP(ADEF_Mass,x)
#define PROP_MassLong(x)		ADD_LONG_PROP(ADEF_Mass,x)
#define PROP_Damage(x)			ADD_BYTE_PROP(ADEF_Damage,x)
#define PROP_DamageLong(x)		ADD_LONG_PROP(ADEF_Damage,x)
#define PROP_Flags(x)			ADD_LONG_PROP(ADEF_Flags,x)
#define PROP_Flags2(x)			ADD_LONG_PROP(ADEF_Flags2,x)
#define PROP_Flags3(x)			ADD_LONG_PROP(ADEF_Flags3,x)
#define PROP_FlagsSet(x)		ADD_LONG_PROP(ADEF_FlagsSet,x)
#define PROP_Flags2Set(x)		ADD_LONG_PROP(ADEF_Flags2Set,x)
#define PROP_Flags3Set(x)		ADD_LONG_PROP(ADEF_Flags3Set,x)
#define PROP_FlagsClear(x)		ADD_LONG_PROP(ADEF_FlagsClear,x)
#define PROP_Flags2Clear(x)		ADD_LONG_PROP(ADEF_Flags2Clear,x)
#define PROP_Flags3Clear(x)		ADD_LONG_PROP(ADEF_Flags3Clear,x)
#define PROP_Alpha(x)			ADD_LONG_PROP(ADEF_Alpha,x)
#define PROP_RenderStyle(x)		ADD_BYTE_PROP(ADEF_RenderStyle,x)
#define PROP_RenderFlags(x)		ADD_WORD_PROP(ADEF_RenderFlags,x)
#define PROP_Translation(x,y)	ADD_WORD_PROP(ADEF_Translation,((x)<<8)|(y))

#define PROP_SpawnState(x)		ADD_BYTE_PROP(ADEF_SpawnState,x)
#define PROP_SeeState(x)		ADD_BYTE_PROP(ADEF_SeeState,x)
#define PROP_PainState(x)		ADD_BYTE_PROP(ADEF_PainState,x)
#define PROP_MeleeState(x)		ADD_BYTE_PROP(ADEF_MeleeState,x)
#define PROP_MissileState(x)	ADD_BYTE_PROP(ADEF_MissileState,x)
#define PROP_CrashState(x)		ADD_BYTE_PROP(ADEF_CrashState,x)
#define PROP_DeathState(x)		ADD_BYTE_PROP(ADEF_DeathState,x)
#define PROP_XDeathState(x)		ADD_BYTE_PROP(ADEF_XDeathState,x)
#define PROP_BDeathState(x)		ADD_BYTE_PROP(ADEF_BDeathState,x)
#define PROP_IDeathState(x)		ADD_BYTE_PROP(ADEF_IDeathState,x)
#define PROP_RaiseState(x)		ADD_BYTE_PROP(ADEF_RaiseState,x)

#define PROP_SKIP_SUPER			ADD_BYTE_PROP(ADEF_SkipSuper,0)
#if 0
#ifndef __GNUC__
#if _MSC_VER < 1300
#define PROP_STATE_BASE(x)		ADD_LONG_PROP(ADEF_StateBase,((int)RUNTIME_CLASS(x)))
#else
#define PROP_STATE_BASE(x)		ADD_LONG_PROP(ADEF_StateBase,(1))
#endif
#else
#define PROP_STATE_BASE(x)		ADD_STRING_PROP(ADEF_StateBase,"\x29",#x)
#endif
#endif
#endif //__INFOMACROS_H__
