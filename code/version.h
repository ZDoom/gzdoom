#ifndef __VERSION_H__
#define __VERSION_H__

// Lots of different representations for the version number
enum { VERSION = 123 };
#define STRVERSION "123"
#define DOTVERSIONSTR "1.23 beta 24"
#define GAMEVER (1*256+23)

// SAVESIG is the save game signature. It should be the minimum version
// whose savegames this version is compatible with, which could be
// earlier than this version.
#define SAVESIG "ZDOOMSAVE123x   "	// Needs to be exactly 16 chars long

// The maximum length of one save game description for the menus.
#define SAVESTRINGSIZE		24

#endif //__VERSION_H__
