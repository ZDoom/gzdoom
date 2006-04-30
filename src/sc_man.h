#ifndef __SC_MAN_H__
#define __SC_MAN_H__

void SC_Open (const char *name);
void SC_OpenFile (const char *name);
void SC_OpenMem (const char *name, char *buffer, int size);
void SC_OpenLumpNum (int lump, const char *name);
void SC_Close (void);
void SC_SetCMode (bool cmode);
void SC_SetEscape (bool esc);
void SC_SavePos (void);
void SC_RestorePos (void);
BOOL SC_GetString (void);
void SC_MustGetString (void);
void SC_MustGetStringName (const char *name);
bool SC_CheckString (const char *name);
BOOL SC_GetNumber (void);
void SC_MustGetNumber (void);
BOOL SC_CheckNumber (void);
BOOL SC_CheckFloat (void);
BOOL SC_GetFloat (void);
void SC_MustGetFloat (void);
void SC_UnGet (void);
//boolean SC_Check(void);
BOOL SC_Compare (const char *text);
int SC_MatchString (const char **strings);
int SC_MustMatchString (const char **strings);
void STACK_ARGS SC_ScriptError (const char *message, ...);
void SC_SaveScriptState();
void SC_RestoreScriptState();	

extern char *sc_String;
extern int sc_StringLen;
extern int sc_Number;
extern float sc_Float;
extern int sc_Line;
extern BOOL sc_End;
extern BOOL sc_Crossed;
extern BOOL sc_FileScripts;
extern char *sc_ScriptsDir;

#endif //__SC_MAN_H__
