#ifndef __SC_MAN_H__
#define __SC_MAN_H__

void SC_Open (const char *name);
void SC_OpenLumpNum (int lump, const char *name);
void SC_Close (void);
BOOL SC_GetString (void);
void SC_MustGetString (void);
void SC_MustGetStringName (const char *name);
BOOL SC_GetNumber (void);
void SC_MustGetNumber (void);
void SC_UnGet (void);
//boolean SC_Check(void);
BOOL SC_Compare (const char *text);
int SC_MatchString (const char **strings);
int SC_MustMatchString (const char **strings);
void SC_ScriptError (const char *message);

extern char *sc_String;
extern int sc_Number;
extern int sc_Line;
extern BOOL sc_End;
extern BOOL sc_Crossed;
extern BOOL sc_FileScripts;
extern char *sc_ScriptsDir;

#endif //__SC_MAN_H__