#ifndef __C_CVARS_H__
#define __C_CVARS_H__

#include "doomtype.h"

/*
==========================================================

CVARS (console variables)

==========================================================
*/

#define CVAR_ARCHIVE	1	// set to cause it to be saved to vars.rc
#define CVAR_USERINFO	2	// added to userinfo  when changed
#define CVAR_SERVERINFO	4	// added to serverinfo when changed
#define CVAR_NOSET		8	// don't allow change from console at all,
							// but can be set from the command line
#define CVAR_LATCH		16	// save changes until server restart
#define CVAR_UNSETTABLE	32	// can unset this var from console
#define CVAR_DEMOSAVE	64	// save the value of this cvar_t in a demo
#define CVAR_MODIFIED	128	// set each time the cvar_t is changed
#define CVAR_ISDEFAULT	256	// is cvar unchanged since creation?
#define CVAR_AUTO		512	// allocated, needs to be freed when destroyed

class cvar_t
{
public:
	cvar_t (const char *name, const char *def, unsigned int flags);
	cvar_t (const char *name, const char *def, unsigned int flags, void (*callback)(cvar_t &));
	~cvar_t ();

#if CVAR_IMPLEMENTOR
	char *name;
	char *string;
	float value;
	unsigned int flags;
#else
	const char *const name;
	const char *const string;
	const float value;
	const unsigned int flags;
#endif

	inline void Callback () { if (m_Callback) m_Callback (*this); }

	void SetDefault (const char *value);
	void Set (const char *value);
	void Set (float value);
	void ForceSet (const char *value);
	void ForceSet (float value);

	inline void Set (const byte *value) { Set ((const char *)value); }
	inline void ForceSet (const byte *value) { ForceSet ((const char *)value); }

	static void EnableNoSet ();		// enable the honoring of CVAR_NOSET
	static void EnableCallbacks ();

private:
	cvar_t (const cvar_t &var);
	cvar_t &operator= (const cvar_t &var);

	void InitSelf (const char *name, const char *def, unsigned int flags, void (*callback)(cvar_t &));
	void (*m_Callback)(cvar_t &);
	cvar_t *m_Next;
	char *m_LatchedString;
	char *m_Default;

	static bool m_UseCallback;
	static bool m_DoNoSet;

	friend class Cmd_cvarlist;

	// Writes all cvars that could effect demo sync to *demo_p. These are
	// cvars that have either CVAR_SERVERINFO or CVAR_DEMOSAVE set.
	friend void C_WriteCVars (byte **demo_p, DWORD filter, bool compact=false);

	// Read all cvars from *demo_p and set them appropriately.
	friend void C_ReadCVars (byte **demo_p);

	// Backup demo cvars. Called before a demo starts playing to save all
	// cvars the demo might change.
	friend void C_BackupCVars (void);

	// Finds a named cvar
	friend cvar_t *FindCVar (const char *var_name, cvar_t **prev);

	// Called from G_InitNew()
	friend void UnlatchCVars (void);

	// archive cvars to FILE f
	friend void C_ArchiveCVars (void *f);

	// initialize cvars to default values after they are created
	friend void C_SetCVarsToDefaults (void);

	friend BOOL SetServerVar (char *name, char *value);
};

// console variable interaction
cvar_t *cvar_set (const char *var_name, const char *value);
cvar_t *cvar_forceset (const char *var_name, const char *value);

inline cvar_t *cvar_set (const char *var_name, const byte *value) { return cvar_set (var_name, (const char *)value); }
inline cvar_t *cvar_forceset (const char *var_name, const byte *value) { return cvar_forceset (var_name, (const char *)value); }



// Maximum number of cvars that can be saved across a demo. If you need
// to save more, bump this up.
#define MAX_DEMOCVARS 32

// Restore demo cvars. Called after demo playback to restore all cvars
// that might possibly have been changed during the course of demo playback.
void C_RestoreCVars (void);


#define BEGIN_CUSTOM_CVAR(name,def,flags) \
	static void cvarfunc_##name(cvar_t &); \
	cvar_t name (#name, def, flags, cvarfunc_##name); \
	static void cvarfunc_##name(cvar_t &var)
	
#define END_CUSTOM_CVAR(name)

#define CVAR(name,def,flags) \
	cvar_t name (#name, def, flags);

#define EXTERN_CVAR(name) extern cvar_t name;


#endif //__C_CVARS_H__