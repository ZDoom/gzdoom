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
	ZDoomSoftware = 8,
	DoomSoftware = 16
};

// always use our own definition for consistency.
#ifdef M_PI
#undef M_PI
#endif

const double M_PI = 3.14159265358979323846;	// matches value in gcc v2 math.h

inline float DEG2RAD(float deg)
{
	return deg * float(M_PI / 180.0);
}

inline double DEG2RAD(double deg)
{
	return deg * (M_PI / 180.0);
}

inline float RAD2DEG(float deg)
{
	return deg * float(180. / M_PI);
}


// Auto-registration sections for GCC.
// Apparently, you cannot do string concatenation inside section attributes.
#ifdef __MACH__
#define SECTION_AREG "__DATA,areg"
#define SECTION_CREG "__DATA,creg"
#define SECTION_FREG "__DATA,freg"
#define SECTION_GREG "__DATA,greg"
#define SECTION_MREG "__DATA,mreg"
#define SECTION_YREG "__DATA,yreg"
#else
#define SECTION_AREG "areg"
#define SECTION_CREG "creg"
#define SECTION_FREG "freg"
#define SECTION_GREG "greg"
#define SECTION_MREG "mreg"
#define SECTION_YREG "yreg"
#endif

#endif
