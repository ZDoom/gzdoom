#ifndef __VERSION_H__
#define __VERSION_H__

// Lots of different representations for the version number
enum { VERSION = 122 };
#define VERSIONSTR "122"
#define CONFIGVERSIONSTR "122"
#define DOTVERSIONSTR "1.22"
#define GAMEVER (1*256+22)

// SAVESIG is the save game signature. It should be the minimum version
// whose savegames this version is compatible with, which could be
// earlier than this version.
#define SAVESIG "ZDOOMSAVE119    "	// Needs to be exactly 16 chars long

#endif //__VERSION_H__
