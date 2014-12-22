/*
** c_cvars.h
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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
*/

#ifndef __C_CVARS_H__
#define __C_CVARS_H__

#include "doomtype.h"
#include "tarray.h"

/*
==========================================================

CVARS (console variables)

==========================================================
*/

enum
{
	CVAR_ARCHIVE		= 1,	// set to cause it to be saved to config
	CVAR_USERINFO		= 2,	// added to userinfo  when changed
	CVAR_SERVERINFO		= 4,	// added to serverinfo when changed
	CVAR_NOSET			= 8,	// don't allow change from console at all,
								// but can be set from the command line
	CVAR_LATCH			= 16,	// save changes until server restart
	CVAR_UNSETTABLE		= 32,	// can unset this var from console
	CVAR_DEMOSAVE		= 64,	// save the value of this cvar in a demo
	CVAR_ISDEFAULT		= 128,	// is cvar unchanged since creation?
	CVAR_AUTO			= 256,	// allocated; needs to be freed when destroyed
	CVAR_NOINITCALL		= 512,	// don't call callback at game start
	CVAR_GLOBALCONFIG	= 1024,	// cvar is saved to global config section
	CVAR_VIDEOCONFIG	= 2048, // cvar is saved to video config section (not implemented)
	CVAR_NOSAVE			= 4096, // when used with CVAR_SERVERINFO, do not save var to savegame
	CVAR_MOD			= 8192,	// cvar was defined by a mod
	CVAR_IGNORE			= 16384,// do not send cvar across the network/inaccesible from ACS (dummy mod cvar)
};

union UCVarValue
{
	bool Bool;
	int Int;
	float Float;
	const char *String;
	const GUID *pGUID;
};

enum ECVarType
{
	CVAR_Bool,
	CVAR_Int,
	CVAR_Float,
	CVAR_String,
	CVAR_Color,		// stored as CVAR_Int
	CVAR_Dummy,		// just redirects to another cvar
	CVAR_GUID
};

class FConfigFile;
class AActor;

class FBaseCVar
{
public:
	FBaseCVar (const char *name, uint32 flags, void (*callback)(FBaseCVar &));
	virtual ~FBaseCVar ();

	inline void Callback () { if (m_Callback) m_Callback (*this); }

	inline const char *GetName () const { return Name; }
	inline uint32 GetFlags () const { return Flags; }
	inline FBaseCVar *GetNext() const { return m_Next; }

	void CmdSet (const char *newval);
	void ForceSet (UCVarValue value, ECVarType type, bool nouserinfosend=false);
	void SetGenericRep (UCVarValue value, ECVarType type);
	void ResetToDefault ();
	void SetArchiveBit () { Flags |= CVAR_ARCHIVE; }

	virtual ECVarType GetRealType () const = 0;

	virtual UCVarValue GetGenericRep (ECVarType type) const = 0;
	virtual UCVarValue GetFavoriteRep (ECVarType *type) const = 0;

	virtual UCVarValue GetGenericRepDefault (ECVarType type) const = 0;
	virtual UCVarValue GetFavoriteRepDefault (ECVarType *type) const = 0;
	virtual void SetGenericRepDefault (UCVarValue value, ECVarType type) = 0;

	FBaseCVar &operator= (const FBaseCVar &var)
		{ UCVarValue val; ECVarType type; val = var.GetFavoriteRep (&type); SetGenericRep (val, type); return *this; }

	static void EnableNoSet ();		// enable the honoring of CVAR_NOSET
	static void EnableCallbacks ();
	static void DisableCallbacks ();
	static void ResetColors ();		// recalc color cvars' indices after screen change

	static void ListVars (const char *filter, bool plain);

protected:
	FBaseCVar () {}
	virtual void DoSet (UCVarValue value, ECVarType type) = 0;

	static bool ToBool (UCVarValue value, ECVarType type);
	static int ToInt (UCVarValue value, ECVarType type);
	static float ToFloat (UCVarValue value, ECVarType type);
	static const char *ToString (UCVarValue value, ECVarType type);
	static const GUID *ToGUID (UCVarValue value, ECVarType type);
	static UCVarValue FromBool (bool value, ECVarType type);
	static UCVarValue FromInt (int value, ECVarType type);
	static UCVarValue FromFloat (float value, ECVarType type);
	static UCVarValue FromString (const char *value, ECVarType type);
	static UCVarValue FromGUID (const GUID &value, ECVarType type);

	char *Name;
	uint32 Flags;

private:
	FBaseCVar (const FBaseCVar &var);
	FBaseCVar (const char *name, uint32 flags);

	void (*m_Callback)(FBaseCVar &);
	FBaseCVar *m_Next;

	static bool m_UseCallback;
	static bool m_DoNoSet;

	friend FString C_GetMassCVarString (uint32 filter, bool compact);
	friend void C_ReadCVars (BYTE **demo_p);
	friend void C_BackupCVars (void);
	friend FBaseCVar *FindCVar (const char *var_name, FBaseCVar **prev);
	friend FBaseCVar *FindCVarSub (const char *var_name, int namelen);
	friend void UnlatchCVars (void);
	friend void DestroyCVarsFlagged (DWORD flags);
	friend void C_ArchiveCVars (FConfigFile *f, uint32 filter);
	friend void C_SetCVarsToDefaults (void);
	friend void FilterCompactCVars (TArray<FBaseCVar *> &cvars, uint32 filter);
	friend void C_DeinitConsole();
};

// Returns a string with all cvars whose flags match filter. In compact mode,
// the cvar names are omitted to save space.
FString C_GetMassCVarString (uint32 filter, bool compact=false);

// Writes all cvars that could effect demo sync to *demo_p. These are
// cvars that have either CVAR_SERVERINFO or CVAR_DEMOSAVE set.
void C_WriteCVars (BYTE **demo_p, uint32 filter, bool compact=false);

// Read all cvars from *demo_p and set them appropriately.
void C_ReadCVars (BYTE **demo_p);

// Backup demo cvars. Called before a demo starts playing to save all
// cvars the demo might change.
void C_BackupCVars (void);

// Finds a named cvar
FBaseCVar *FindCVar (const char *var_name, FBaseCVar **prev);
FBaseCVar *FindCVarSub (const char *var_name, int namelen);

// Create a new cvar with the specified name and type
FBaseCVar *C_CreateCVar(const char *var_name, ECVarType var_type, DWORD flags);

// Called from G_InitNew()
void UnlatchCVars (void);

// Destroy CVars with the matching flags; called from CCMD(restart)
void DestroyCVarsFlagged (DWORD flags);

// archive cvars to FILE f
void C_ArchiveCVars (FConfigFile *f, uint32 filter);

// initialize cvars to default values after they are created
void C_SetCVarsToDefaults (void);

void FilterCompactCVars (TArray<FBaseCVar *> &cvars, uint32 filter);

void C_DeinitConsole();

class FBoolCVar : public FBaseCVar
{
public:
	FBoolCVar (const char *name, bool def, uint32 flags, void (*callback)(FBoolCVar &)=NULL);

	virtual ECVarType GetRealType () const;

	virtual UCVarValue GetGenericRep (ECVarType type) const;
	virtual UCVarValue GetFavoriteRep (ECVarType *type) const;
	virtual UCVarValue GetGenericRepDefault (ECVarType type) const;
	virtual UCVarValue GetFavoriteRepDefault (ECVarType *type) const;
	virtual void SetGenericRepDefault (UCVarValue value, ECVarType type);

	inline bool operator= (bool boolval)
		{ UCVarValue val; val.Bool = boolval; SetGenericRep (val, CVAR_Bool); return boolval; }
	inline operator bool () const { return Value; }
	inline bool operator *() const { return Value; }

protected:
	virtual void DoSet (UCVarValue value, ECVarType type);

	bool Value;
	bool DefaultValue;
};

class FIntCVar : public FBaseCVar
{
public:
	FIntCVar (const char *name, int def, uint32 flags, void (*callback)(FIntCVar &)=NULL);

	virtual ECVarType GetRealType () const;

	virtual UCVarValue GetGenericRep (ECVarType type) const;
	virtual UCVarValue GetFavoriteRep (ECVarType *type) const;
	virtual UCVarValue GetGenericRepDefault (ECVarType type) const;
	virtual UCVarValue GetFavoriteRepDefault (ECVarType *type) const;
	virtual void SetGenericRepDefault (UCVarValue value, ECVarType type);

	int operator= (int intval)
		{ UCVarValue val; val.Int = intval; SetGenericRep (val, CVAR_Int); return intval; }
	inline operator int () const { return Value; }
	inline int operator *() const { return Value; }

protected:
	virtual void DoSet (UCVarValue value, ECVarType type);

	int Value;
	int DefaultValue;

	friend class FFlagCVar;
};

class FFloatCVar : public FBaseCVar
{
public:
	FFloatCVar (const char *name, float def, uint32 flags, void (*callback)(FFloatCVar &)=NULL);

	virtual ECVarType GetRealType () const;

	virtual UCVarValue GetGenericRep (ECVarType type) const;
	virtual UCVarValue GetFavoriteRep (ECVarType *type) const;
	virtual UCVarValue GetGenericRepDefault (ECVarType type) const;
	virtual UCVarValue GetFavoriteRepDefault (ECVarType *type) const;
	virtual void SetGenericRepDefault (UCVarValue value, ECVarType type);

	float operator= (float floatval)
		{ UCVarValue val; val.Float = floatval; SetGenericRep (val, CVAR_Float); return floatval; }
	inline operator float () const { return Value; }
	inline float operator *() const { return Value; }

protected:
	virtual void DoSet (UCVarValue value, ECVarType type);

	float Value;
	float DefaultValue;
};

class FStringCVar : public FBaseCVar
{
public:
	FStringCVar (const char *name, const char *def, uint32 flags, void (*callback)(FStringCVar &)=NULL);
	~FStringCVar ();

	virtual ECVarType GetRealType () const;

	virtual UCVarValue GetGenericRep (ECVarType type) const;
	virtual UCVarValue GetFavoriteRep (ECVarType *type) const;
	virtual UCVarValue GetGenericRepDefault (ECVarType type) const;
	virtual UCVarValue GetFavoriteRepDefault (ECVarType *type) const;
	virtual void SetGenericRepDefault (UCVarValue value, ECVarType type);

	const char *operator= (const char *stringrep)
		{ UCVarValue val; val.String = const_cast<char *>(stringrep); SetGenericRep (val, CVAR_String); return stringrep; }
	inline operator const char * () const { return Value; }
	inline const char *operator *() const { return Value; }

protected:
	virtual void DoSet (UCVarValue value, ECVarType type);

	char *Value;
	char *DefaultValue;
};

class FColorCVar : public FIntCVar
{
public:
	FColorCVar (const char *name, int def, uint32 flags, void (*callback)(FColorCVar &)=NULL);

	virtual ECVarType GetRealType () const;

	virtual UCVarValue GetGenericRep (ECVarType type) const;
	virtual UCVarValue GetGenericRepDefault (ECVarType type) const;
	virtual void SetGenericRepDefault (UCVarValue value, ECVarType type);

	inline operator uint32 () const { return Value; }
	inline uint32 operator *() const { return Value; }
	inline int GetIndex () const { return Index; }

protected:
	virtual void DoSet (UCVarValue value, ECVarType type);
	
	static UCVarValue FromInt2 (int value, ECVarType type);
	static int ToInt2 (UCVarValue value, ECVarType type);

	int Index;
};

class FFlagCVar : public FBaseCVar
{
public:
	FFlagCVar (const char *name, FIntCVar &realvar, uint32 bitval);

	virtual ECVarType GetRealType () const;

	virtual UCVarValue GetGenericRep (ECVarType type) const;
	virtual UCVarValue GetFavoriteRep (ECVarType *type) const;
	virtual UCVarValue GetGenericRepDefault (ECVarType type) const;
	virtual UCVarValue GetFavoriteRepDefault (ECVarType *type) const;
	virtual void SetGenericRepDefault (UCVarValue value, ECVarType type);

	bool operator= (bool boolval)
		{ UCVarValue val; val.Bool = boolval; SetGenericRep (val, CVAR_Bool); return boolval; }
	bool operator= (FFlagCVar &flag)
		{ UCVarValue val; val.Bool = !!flag; SetGenericRep (val, CVAR_Bool); return val.Bool; }
	inline operator int () const { return (ValueVar & BitVal); }
	inline int operator *() const { return (ValueVar & BitVal); }

protected:
	virtual void DoSet (UCVarValue value, ECVarType type);

	FIntCVar &ValueVar;
	uint32 BitVal;
	int BitNum;
};

class FMaskCVar : public FBaseCVar
{
public:
	FMaskCVar (const char *name, FIntCVar &realvar, uint32 bitval);

	virtual ECVarType GetRealType () const;

	virtual UCVarValue GetGenericRep (ECVarType type) const;
	virtual UCVarValue GetFavoriteRep (ECVarType *type) const;
	virtual UCVarValue GetGenericRepDefault (ECVarType type) const;
	virtual UCVarValue GetFavoriteRepDefault (ECVarType *type) const;
	virtual void SetGenericRepDefault (UCVarValue value, ECVarType type);

	inline operator int () const { return (ValueVar & BitVal) >> BitNum; }
	inline int operator *() const { return (ValueVar & BitVal) >> BitNum; }

protected:
	virtual void DoSet (UCVarValue value, ECVarType type);

	FIntCVar &ValueVar;
	uint32 BitVal;
	int BitNum;
};

class FGUIDCVar : public FBaseCVar
{
public:
	FGUIDCVar (const char *name, const GUID *defguid, uint32 flags, void (*callback)(FGUIDCVar &)=NULL);

	virtual ECVarType GetRealType () const;

	virtual UCVarValue GetGenericRep (ECVarType type) const;
	virtual UCVarValue GetFavoriteRep (ECVarType *type) const;
	virtual UCVarValue GetGenericRepDefault (ECVarType type) const;
	virtual UCVarValue GetFavoriteRepDefault (ECVarType *type) const;
	virtual void SetGenericRepDefault (UCVarValue value, ECVarType type);

	const GUID &operator= (const GUID &guidval)
		{ UCVarValue val; val.pGUID = &guidval; SetGenericRep (val, CVAR_GUID); return guidval; }
	inline operator const GUID & () const { return Value; }
	inline const GUID &operator *() const { return Value; }

protected:
	virtual void DoSet (UCVarValue value, ECVarType type);

	GUID Value;
	GUID DefaultValue;
};

extern int cvar_defflags;

FBaseCVar *cvar_set (const char *var_name, const char *value);
FBaseCVar *cvar_forceset (const char *var_name, const char *value);

inline FBaseCVar *cvar_set (const char *var_name, const BYTE *value) { return cvar_set (var_name, (const char *)value); }
inline FBaseCVar *cvar_forceset (const char *var_name, const BYTE *value) { return cvar_forceset (var_name, (const char *)value); }



// Restore demo cvars. Called after demo playback to restore all cvars
// that might possibly have been changed during the course of demo playback.
void C_RestoreCVars (void);

void C_ForgetCVars (void);


#define CUSTOM_CVAR(type,name,def,flags) \
	static void cvarfunc_##name(F##type##CVar &); \
	F##type##CVar name (#name, def, flags, cvarfunc_##name); \
	static void cvarfunc_##name(F##type##CVar &self)

#define CVAR(type,name,def,flags) \
	F##type##CVar name (#name, def, flags);

#define EXTERN_CVAR(type,name) extern F##type##CVar name;

extern FBaseCVar *CVars;

#endif //__C_CVARS_H__
