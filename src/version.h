/*
** version.h
**
**---------------------------------------------------------------------------
** Copyright 1998-2007 Randy Heit
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

#ifndef __VERSION_H__
#define __VERSION_H__

// The svnrevision.h is automatically updated to grab the revision of
// of the current source tree so that it can be included with version numbers.
#include "svnrevision.h"

/** Lots of different version numbers **/

#define DOTVERSIONSTR_NOREV "2.6.0"

// The version string the user actually sees.
#define DOTVERSIONSTR DOTVERSIONSTR_NOREV " (r" SVN_REVISION_STRING ")"

// The version as seen in the Windows resource
#define RC_FILEVERSION 2,6,0,SVN_REVISION_NUMBER
#define RC_PRODUCTVERSION 2,6,0,0
#define RC_FILEVERSION2 DOTVERSIONSTR
#define RC_PRODUCTVERSION2 "2.6"

// Version identifier for network games.
// Bump it every time you do a release unless you're certain you
// didn't change anything that will affect sync.
#define NETGAMEVERSION 228

// Version stored in the ini's [LastRun] section.
// Bump it if you made some configuration change that you want to
// be able to migrate in FGameConfigFile::DoGlobalSetup().
#define LASTRUNVERSION "210"

// Protocol version used in demos.
// Bump it if you change existing DEM_ commands or add new ones.
// Otherwise, it should be safe to leave it alone.
#define DEMOGAMEVERSION 0x219

// Minimum demo version we can play.
// Bump it whenever you change or remove existing DEM_ commands.
#define MINDEMOVERSION 0x215

// SAVEVER is the version of the information stored in level snapshots.
// Note that SAVEVER is not directly comparable to VERSION.
// SAVESIG should match SAVEVER.

// MINSAVEVER is the minimum level snapshot version that can be loaded.
#define MINSAVEVER 3100

#if SVN_REVISION_NUMBER < MINSAVEVER
// If we don't know the current revision write something very high to ensure that
// the reesulting executable can read its own savegames but no regular engine can.
#define SAVEVER			999999
#define SAVESIG			MakeSaveSig()
static inline const char *MakeSaveSig()
{
	static char foo[] = { 'Z','D','O','O','M','S','A','V','E',
#if SAVEVER > 99999
		'0' + (SAVEVER / 100000),
#endif
#if SAVEVER > 9999
		'0' + ((SAVEVER / 10000) % 10),
#endif
#if SAVEVER > 999
		'0' + ((SAVEVER / 1000) % 10),
#endif
		'0' + ((SAVEVER / 100) % 10),
		'0' + ((SAVEVER / 10) % 10),
		'0' + (SAVEVER % 10),
		'\0'
	};
	return foo;
}
#else
#define SAVEVER			SVN_REVISION_NUMBER
#define SAVESIG			"ZDOOMSAVE"SVN_REVISION_STRING
#endif

// This is so that derivates can use the same savegame versions without worrying about engine compatibility
#define GAMESIG "ZDOOM"
#define BASEWAD "zdoom.pk3"

// More stuff that needs to be different for derivatives.
#define GAMENAME "ZDoom"
#define FORUM_URL "http://forum.zdoom.org"
#define BUGS_FORUM_URL	"http://forum.zdoom.org/index.php?c=3"

#ifdef unix
#define GAME_DIR ".zdoom"
#elif defined(__APPLE__)
#define GAME_DIR GAMENAME
#else
#define CDROM_DIR "C:\\ZDOOMDAT"
#endif


// The maximum length of one save game description for the menus.
#define SAVESTRINGSIZE		24

#endif //__VERSION_H__
