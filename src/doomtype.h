//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//              Simple basic typedefs, isolated here to make it easier
//               separating modules.
//        
//-----------------------------------------------------------------------------


#ifndef __DOOMTYPE__
#define __DOOMTYPE__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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

class PClassActor;
typedef TMap<int, PClassActor *> FClassMap;


#if defined(_MSC_VER)
#define NOVTABLE __declspec(novtable)
#else
#define NOVTABLE
#endif

#if defined(__GNUC__)
// With versions of GCC newer than 4.2, it appears it was determined that the
// cost of an unaligned pointer on PPC was high enough to add padding to the
// end of packed structs.  For whatever reason __packed__ and pragma pack are
// handled differently in this regard. Note that this only needs to be applied
// to types which are used in arrays or sizeof is needed. This also prevents
// code from taking references to the struct members.
#define FORCE_PACKED __attribute__((__packed__))
#else
#define FORCE_PACKED
#endif

#include "basictypes.h"

extern bool batchrun;

// Bounding box coordinate storage.
enum
{
	BOXTOP,
	BOXBOTTOM,
	BOXLEFT,
	BOXRIGHT
};		// bbox coordinates


// [RH] This gets used all over; define it here:
int Printf (int printlevel, const char *, ...) GCCPRINTF(2,3);
int Printf (const char *, ...) GCCPRINTF(1,2);

// [RH] Same here:
int DPrintf (int level, const char *, ...) GCCPRINTF(2,3);

extern "C" int mysnprintf(char *buffer, size_t count, const char *format, ...) GCCPRINTF(3,4);
extern "C" int myvsnprintf(char *buffer, size_t count, const char *format, va_list argptr) GCCFORMAT(3);


// game print flags
enum
{
	PRINT_LOW,		// pickup messages
	PRINT_MEDIUM,	// death messages
	PRINT_HIGH,		// critical messages
	PRINT_CHAT,		// chat messages
	PRINT_TEAMCHAT,	// chat messages from a teammate
	PRINT_LOG,		// only to logfile
	PRINT_BOLD = 200				// What Printf_Bold used
};

enum
{
	DMSG_OFF,		// no developer messages.
	DMSG_ERROR,		// general notification messages
	DMSG_WARNING,	// warnings
	DMSG_NOTIFY,	// general notification messages
	DMSG_SPAMMY,	// for those who want to see everything, regardless of its usefulness.
};

struct PalEntry
{
	PalEntry () {}
	PalEntry (uint32_t argb) { d = argb; }
	operator uint32_t () const { return d; }
	void SetRGB(PalEntry other)
	{
		d = other.d & 0xffffff;
	}
	PalEntry Modulate(PalEntry other) const
	{
		if (isWhite())
		{
			return other;
		}
		else if (other.isWhite())
		{
			return *this;
		}
		else
		{
			other.r = (r * other.r) / 255;
			other.g = (g * other.g) / 255;
			other.b = (b * other.b) / 255;
			return other;
		}
	}
	int Luminance() const 
	{
		return (r * 77 + g * 143 + b * 37) >> 8;
	}

	void Decolorize()	// this for 'nocoloredspritelighting' and not the same as desaturation. The normal formula results in a value that's too dark.
	{
		int v = (r + g + b);
		r = g = b = ((255*3) + v + v) / 9;
	}
	bool isBlack() const
	{
		return (d & 0xffffff) == 0;
	}
	bool isWhite() const
	{
		return (d & 0xffffff) == 0xffffff;
	}
	PalEntry &operator= (uint32_t other) { d = other; return *this; }
	PalEntry InverseColor() const { PalEntry nc; nc.a = a; nc.r = 255 - r; nc.g = 255 - g; nc.b = 255 - b; return nc; }
#ifdef __BIG_ENDIAN__
	PalEntry (uint8_t ir, uint8_t ig, uint8_t ib) : a(0), r(ir), g(ig), b(ib) {}
	PalEntry (uint8_t ia, uint8_t ir, uint8_t ig, uint8_t ib) : a(ia), r(ir), g(ig), b(ib) {}
	union
	{
		struct
		{
			uint8_t a,r,g,b;
		};
		uint32_t d;
	};
#else
	PalEntry (uint8_t ir, uint8_t ig, uint8_t ib) : b(ib), g(ig), r(ir), a(0) {}
	PalEntry (uint8_t ia, uint8_t ir, uint8_t ig, uint8_t ib) : b(ib), g(ig), r(ir), a(ia) {}
	union
	{
		struct
		{
			uint8_t b,g,r,a;
		};
		uint32_t d;
	};
#endif
};

inline int Luminance(int r, int g, int b)
{
	return (r * 77 + g * 143 + b * 37) >> 8;
}


class FTextureID
{
	friend class FTextureManager;
	friend void R_InitSpriteDefs();

public:
	FTextureID() throw() {}
	bool isNull() const { return texnum == 0; }
	bool isValid() const { return texnum > 0; }
	bool Exists() const { return texnum >= 0; }
	void SetInvalid() { texnum = -1; }
	void SetNull() { texnum = 0; }
	bool operator ==(const FTextureID &other) const { return texnum == other.texnum; }
	bool operator !=(const FTextureID &other) const { return texnum != other.texnum; }
	FTextureID operator +(int offset) throw();
	int GetIndex() const { return texnum; }	// Use this only if you absolutely need the index!

											// The switch list needs these to sort the switches by texture index
	int operator -(FTextureID other) const { return texnum - other.texnum; }
	bool operator < (FTextureID other) const { return texnum < other.texnum; }
	bool operator > (FTextureID other) const { return texnum > other.texnum; }

protected:
	FTextureID(int num) { texnum = num; }
private:
	int texnum;
};

// This is for the script interface which needs to do casts from int to texture.
class FSetTextureID : public FTextureID
{
public:
	FSetTextureID(int v) : FTextureID(v) {}
};


struct VersionInfo
{
	uint16_t major;
	uint16_t minor;
	uint32_t revision;

	bool operator <=(const VersionInfo &o) const
	{
		return o.major > this->major || (o.major == this->major && o.minor > this->minor) || (o.major == this->major && o.minor == this->minor && o.revision >= this->revision);
	}
	bool operator >=(const VersionInfo &o) const
	{
		return o.major < this->major || (o.major == this->major && o.minor < this->minor) || (o.major == this->major && o.minor == this->minor && o.revision <= this->revision);
	}
	bool operator > (const VersionInfo &o) const
	{
		return o.major < this->major || (o.major == this->major && o.minor < this->minor) || (o.major == this->major && o.minor == this->minor && o.revision < this->revision);
	}
	bool operator < (const VersionInfo &o) const
	{
		return o.major > this->major || (o.major == this->major && o.minor > this->minor) || (o.major == this->major && o.minor == this->minor && o.revision > this->revision);
	}
	void operator=(const char *string);
};

// Cannot be a constructor because Lemon would puke on it.
inline VersionInfo MakeVersion(unsigned int ma, unsigned int mi, unsigned int re = 0)
{
	return{ (uint16_t)ma, (uint16_t)mi, (uint32_t)re };
}



// Screenshot buffer image data types
enum ESSType
{
	SS_PAL,
	SS_RGB,
	SS_BGRA
};

// always use our own definition for consistency.
#ifdef M_PI
#undef M_PI
#endif

const double M_PI = 3.14159265358979323846;	// matches value in gcc v2 math.h

template <typename T, size_t N>
char ( &_ArraySizeHelper( T (&array)[N] ))[N];

#define countof( array ) (sizeof( _ArraySizeHelper( array ) ))

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
