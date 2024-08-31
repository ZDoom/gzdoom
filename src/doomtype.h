//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//              Simple basic typedefs, isolated here to make it easier
//               separating modules.
//        
//-----------------------------------------------------------------------------


#ifndef __DOOMTYPE__
#define __DOOMTYPE__

#ifdef _MSC_VER
// VC++ does not define PATH_MAX, but the Windows headers do define MAX_PATH.
// However, we want to avoid including the Windows headers in most of the
// source files, so we can't use it. So define PATH_MAX to be what MAX_PATH
// currently is:
#define PATH_MAX 260

// Disable warning about using unsized arrays in structs. It supports it just
// fine, and so do Clang and GCC, but the latter two don't warn about it.
#pragma warning(disable:4200)
#endif

#include <limits.h>
#include <tuple>
#include <algorithm>
#include "tarray.h"
#include "name.h"
#include "zstring.h"
#include "cmdlib.h"

class PClassActor;
typedef TMap<int, PClassActor *> FClassMap;

#include "basics.h"
#include "printf.h"

// Bounding box coordinate storage.
#include "palentry.h"
#include "textureid.h"

enum class ELightMode : int8_t
{
	NotSet = -1,
	LinearStandard = 0,
	DoomBright = 1,
	Doom = 2,
	DoomDark = 3,
	DoomLegacy = 4,
	Build = 5,
	ZDoomSoftware = 8,
	DoomSoftware = 16
};

#endif
