/*
** p_acs.cpp
** General BEHAVIOR management and ACS execution environment
**
**---------------------------------------------------------------------------
** Copyright 1998-2012 Randy Heit
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
** This code at one time made lots of little-endian assumptions.
** I think it should be fine on big-endian machines now, but I have no
** real way to test it.
*/

#include <assert.h>

#include "templates.h"
#include "doomdef.h"
#include "p_local.h"
#include "d_player.h"
#include "p_spec.h"
#include "g_level.h"
#include "s_sound.h"
#include "p_acs.h"
#include "p_saveg.h"
#include "p_lnspec.h"
#include "p_enemy.h"
#include "m_random.h"
#include "doomstat.h"
#include "c_console.h"
#include "c_dispatch.h"
#include "s_sndseq.h"
#include "i_system.h"
#include "i_movie.h"
#include "sbar.h"
#include "m_swap.h"
#include "a_sharedglobal.h"
#include "a_doomglobal.h"
#include "a_strifeglobal.h"
#include "v_video.h"
#include "w_wad.h"
#include "r_sky.h"
#include "gstrings.h"
#include "gi.h"
#include "sc_man.h"
#include "c_bind.h"
#include "info.h"
#include "r_data/r_translate.h"
#include "cmdlib.h"
#include "m_png.h"
#include "p_setup.h"
#include "po_man.h"
#include "actorptrselect.h"
#include "farchive.h"
#include "decallib.h"
#include "p_terrain.h"
#include "version.h"
#include "p_effect.h"
#include "r_utility.h"
#include "a_morph.h"
#include "i_music.h"

#include "g_shared/a_pickups.h"

extern FILE *Logfile;

FRandom pr_acs ("ACS");

// I imagine this much stack space is probably overkill, but it could
// potentially get used with recursive functions.
#define STACK_SIZE 4096

#define CLAMPCOLOR(c)		(EColorRange)((unsigned)(c) >= NUM_TEXT_COLORS ? CR_UNTRANSLATED : (c))
#define LANGREGIONMASK		MAKE_ID(0,0,0xff,0xff)

// HUD message flags
#define HUDMSG_LOG					(0x80000000)
#define HUDMSG_COLORSTRING			(0x40000000)
#define HUDMSG_ADDBLEND				(0x20000000)
#define HUDMSG_ALPHA				(0x10000000)
#define HUDMSG_NOWRAP				(0x08000000)

// HUD message layers; these are not flags
#define HUDMSG_LAYER_SHIFT			12
#define HUDMSG_LAYER_MASK			(0x0000F000)
// See HUDMSGLayer enumerations in sbar.h

// HUD message visibility flags
#define HUDMSG_VISIBILITY_SHIFT		16
#define HUDMSG_VISIBILITY_MASK		(0x00070000)
// See HUDMSG visibility enumerations in sbar.h

// Flags for ReplaceTextures
#define NOT_BOTTOM			1
#define NOT_MIDDLE			2
#define NOT_TOP				4
#define NOT_FLOOR			8
#define NOT_CEILING			16

// LineAttack flags
#define FHF_NORANDOMPUFFZ	1
#define FHF_NOIMPACTDECAL	2

// SpawnDecal flags
#define SDF_ABSANGLE		1
#define SDF_PERMANENT		2

// GetArmorInfo
enum
{
	ARMORINFO_CLASSNAME,
	ARMORINFO_SAVEAMOUNT,
	ARMORINFO_SAVEPERCENT,
	ARMORINFO_MAXABSORB,
	ARMORINFO_MAXFULLABSORB,
	ARMORINFO_ACTUALSAVEAMOUNT,
};

// PickActor
// [JP] I've renamed these flags to something else to avoid confusion with the other PAF_ flags
enum
{
//	PAF_FORCETID,
//	PAF_RETURNTID
	PICKAF_FORCETID = 1,
	PICKAF_RETURNTID = 2,
};

// ACS specific conversion functions to and from fixed point.
// These should be used to convert from and to sctipt variables
// so that there is a clear distinction between leftover fixed point code
// and genuinely needed conversions.

inline double ACSToDouble(int acsval)
{
	return acsval / 65536.;
}

inline float ACSToFloat(int acsval)
{
	return acsval / 65536.f;
}

inline int DoubleToACS(double val)
{
	return xs_Fix<16>::ToFix(val);
}

inline DAngle ACSToAngle(int acsval)
{
	return acsval * (360. / 65536.);
}

inline int AngleToACS(DAngle ang)
{
	return ang.BAMs() >> 16;
}

inline int PitchToACS(DAngle ang)
{
	return int(ang.Normalized180().Degrees * (65536. / 360));
}

struct CallReturn
{
	CallReturn(int pc, ScriptFunction *func, FBehavior *module, SDWORD *locals, ACSLocalArrays *arrays, bool discard, unsigned int runaway)
		: ReturnFunction(func),
		  ReturnModule(module),
		  ReturnLocals(locals),
		  ReturnArrays(arrays),
		  ReturnAddress(pc),
		  bDiscardResult(discard),
		  EntryInstrCount(runaway)
	{}

	ScriptFunction *ReturnFunction;
	FBehavior *ReturnModule;
	SDWORD *ReturnLocals;
	ACSLocalArrays *ReturnArrays;
	int ReturnAddress;
	int bDiscardResult;
	unsigned int EntryInstrCount;
};

static DLevelScript *P_GetScriptGoing (AActor *who, line_t *where, int num, const ScriptPtr *code, FBehavior *module,
	const int *args, int argcount, int flags);


struct FBehavior::ArrayInfo
{
	DWORD ArraySize;
	SDWORD *Elements;
};

TArray<FBehavior *> FBehavior::StaticModules;
TArray<FString> ACS_StringBuilderStack;

#define STRINGBUILDER_START(Builder) if (Builder.IsNotEmpty() || ACS_StringBuilderStack.Size()) { ACS_StringBuilderStack.Push(Builder); Builder = ""; }
#define STRINGBUILDER_FINISH(Builder) if (!ACS_StringBuilderStack.Pop(Builder)) { Builder = ""; }

//============================================================================
//
// uallong
//
// Read a possibly unaligned four-byte little endian integer from memory.
//
//============================================================================

#if defined(_M_IX86) || defined(_M_X64) || defined(__i386__)
inline int uallong(const int &foo)
{
	return foo;
}
#else
inline int uallong(const int &foo)
{
	const unsigned char *bar = (const unsigned char *)&foo;
	return bar[0] | (bar[1] << 8) | (bar[2] << 16) | (bar[3] << 24);
}
#endif

//============================================================================
//
// Global and world variables
//
//============================================================================

// ACS variables with world scope
SDWORD ACS_WorldVars[NUM_WORLDVARS];
FWorldGlobalArray ACS_WorldArrays[NUM_WORLDVARS];

// ACS variables with global scope
SDWORD ACS_GlobalVars[NUM_GLOBALVARS];
FWorldGlobalArray ACS_GlobalArrays[NUM_GLOBALVARS];

//----------------------------------------------------------------------------
//
// ACS stack manager
//
// This is needed so that the garbage collector has access to all active
// script stacks
//
//----------------------------------------------------------------------------

struct FACSStack
{
	SDWORD buffer[STACK_SIZE];
	int sp;
	FACSStack *next;
	FACSStack *prev;
	static FACSStack *head;

	FACSStack();
	~FACSStack();
};

FACSStack *FACSStack::head;

FACSStack::FACSStack()
{
	sp = 0;
	next = head;
	prev = NULL;
	head = this;
}

FACSStack::~FACSStack()
{
	if (next != NULL) next->prev = prev;
	if (prev == NULL)
	{
		head = next;
	}
	else
	{
		prev->next = next;
	}
}

//----------------------------------------------------------------------------
//
// Global ACS strings (Formerly known as On the fly strings)
//
// This special string table is part of the global state. Programmatically
// generated strings (e.g. those returned by strparam) are stored here.
// PCD_TAGSTRING also now stores strings in this table instead of simply
// tagging strings with their library ID.
//
// Identical strings map to identical string identifiers.
//
// When the string table needs to grow to hold more strings, a garbage
// collection is first attempted to see if more room can be made to store
// strings without growing. A string is considered in use if any value
// in any of these variable blocks contains a valid ID in the global string
// table:
//   * The active area of the ACS stack
//   * All running scripts' local variables
//   * All map variables
//   * All world variables
//   * All global variables
// It's not important whether or not they are really used as strings, only
// that they might be. A string is also considered in use if its lock count
// is non-zero, even if none of the above variable blocks referenced it.
//
// To keep track of local and map variables for nonresident maps in a hub,
// when a map's state is archived, all strings found in its local and map
// variables are locked. When a map is revisited in a hub, all strings found
// in its local and map variables are unlocked. Locking and unlocking are
// cumulative operations.
//
// What this all means is that:
//   * Strings returned by strparam last indefinitely. No longer do they
//     disappear at the end of the tic they were generated.
//   * You can pass library strings around freely without having to worry
//     about always having the same libraries loaded in the same order on
//     every map that needs to use those strings.
//
//----------------------------------------------------------------------------

ACSStringPool GlobalACSStrings;

ACSStringPool::ACSStringPool()
{
	memset(PoolBuckets, 0xFF, sizeof(PoolBuckets));
	FirstFreeEntry = 0;
}

//============================================================================
//
// ACSStringPool :: Clear
//
// Remove all strings from the pool.
//
//============================================================================

void ACSStringPool::Clear()
{
	Pool.Clear();
	memset(PoolBuckets, 0xFF, sizeof(PoolBuckets));
	FirstFreeEntry = 0;
}

//============================================================================
//
// ACSStringPool :: AddString
//
// Returns a valid string identifier (including library ID) or -1 if we ran
// out of room. Identical strings will return identical values.
//
//============================================================================

int ACSStringPool::AddString(const char *str)
{
	size_t len = strlen(str);
	unsigned int h = SuperFastHash(str, len);
	unsigned int bucketnum = h % NUM_BUCKETS;
	int i = FindString(str, len, h, bucketnum);
	if (i >= 0)
	{
		return i | STRPOOL_LIBRARYID_OR;
	}
	FString fstr(str);
	return InsertString(fstr, h, bucketnum);
}

int ACSStringPool::AddString(FString &str)
{
	unsigned int h = SuperFastHash(str.GetChars(), str.Len());
	unsigned int bucketnum = h % NUM_BUCKETS;
	int i = FindString(str, str.Len(), h, bucketnum);
	if (i >= 0)
	{
		return i | STRPOOL_LIBRARYID_OR;
	}
	return InsertString(str, h, bucketnum);
}

//============================================================================
//
// ACSStringPool :: GetString
//
//============================================================================

const char *ACSStringPool::GetString(int strnum)
{
	assert((strnum & LIBRARYID_MASK) == STRPOOL_LIBRARYID_OR);
	strnum &= ~LIBRARYID_MASK;
	if ((unsigned)strnum < Pool.Size() && Pool[strnum].Next != FREE_ENTRY)
	{
		return Pool[strnum].Str;
	}
	return NULL;
}

//============================================================================
//
// ACSStringPool :: LockString
//
// Prevents this string from being purged.
//
//============================================================================

void ACSStringPool::LockString(int strnum)
{
	assert((strnum & LIBRARYID_MASK) == STRPOOL_LIBRARYID_OR);
	strnum &= ~LIBRARYID_MASK;
	assert((unsigned)strnum < Pool.Size());
	Pool[strnum].LockCount++;
}

//============================================================================
//
// ACSStringPool :: UnlockString
//
// When equally mated with LockString, allows this string to be purged.
//
//============================================================================

void ACSStringPool::UnlockString(int strnum)
{
	assert((strnum & LIBRARYID_MASK) == STRPOOL_LIBRARYID_OR);
	strnum &= ~LIBRARYID_MASK;
	assert((unsigned)strnum < Pool.Size());
	assert(Pool[strnum].LockCount > 0);
	Pool[strnum].LockCount--;
}

//============================================================================
//
// ACSStringPool :: MarkString
//
// Prevent this string from being purged during the next call to PurgeStrings.
// This does not carry over to subsequent calls of PurgeStrings.
//
//============================================================================

void ACSStringPool::MarkString(int strnum)
{
	assert((strnum & LIBRARYID_MASK) == STRPOOL_LIBRARYID_OR);
	strnum &= ~LIBRARYID_MASK;
	assert((unsigned)strnum < Pool.Size());
	Pool[strnum].LockCount |= 0x80000000;
}

//============================================================================
//
// ACSStringPool :: LockStringArray
//
// Prevents several strings from being purged. Entries not in this pool will
// be silently ignored. The idea here is to pass this function a block of
// ACS variables. Everything that looks like it might be a string in the pool
// is locked, even if it's not actually used as such. It's better to keep
// more strings than we need than to throw away ones we do need.
//
//============================================================================

void ACSStringPool::LockStringArray(const int *strnum, unsigned int count)
{
	for (unsigned int i = 0; i < count; ++i)
	{
		int num = strnum[i];
		if ((num & LIBRARYID_MASK) == STRPOOL_LIBRARYID_OR)
		{
			num &= ~LIBRARYID_MASK;
			if ((unsigned)num < Pool.Size())
			{
				Pool[num].LockCount++;
			}
		}
	}
}

//============================================================================
//
// ACSStringPool :: UnlockStringArray
//
// Reverse of LockStringArray.
//
//============================================================================

void ACSStringPool::UnlockStringArray(const int *strnum, unsigned int count)
{
	for (unsigned int i = 0; i < count; ++i)
	{
		int num = strnum[i];
		if ((num & LIBRARYID_MASK) == STRPOOL_LIBRARYID_OR)
		{
			num &= ~LIBRARYID_MASK;
			if ((unsigned)num < Pool.Size())
			{
				assert(Pool[num].LockCount > 0);
				Pool[num].LockCount--;
			}
		}
	}
}

//============================================================================
//
// ACSStringPool :: MarkStringArray
//
// Array version of MarkString.
//
//============================================================================

void ACSStringPool::MarkStringArray(const int *strnum, unsigned int count)
{
	for (unsigned int i = 0; i < count; ++i)
	{
		int num = strnum[i];
		if ((num & LIBRARYID_MASK) == STRPOOL_LIBRARYID_OR)
		{
			num &= ~LIBRARYID_MASK;
			if ((unsigned)num < Pool.Size())
			{
				Pool[num].LockCount |= 0x80000000;
			}
		}
	}
}

//============================================================================
//
// ACSStringPool :: MarkStringMap
//
// World/global variables version of MarkString.
//
//============================================================================

void ACSStringPool::MarkStringMap(const FWorldGlobalArray &aray)
{
	FWorldGlobalArray::ConstIterator it(aray);
	FWorldGlobalArray::ConstPair *pair;

	while (it.NextPair(pair))
	{
		int num = pair->Value;
		if ((num & LIBRARYID_MASK) == STRPOOL_LIBRARYID_OR)
		{
			num &= ~LIBRARYID_MASK;
			if ((unsigned)num < Pool.Size())
			{
				Pool[num].LockCount |= 0x80000000;
			}
		}
	}
}

//============================================================================
//
// ACSStringPool :: UnlockAll
//
// Resets every entry's lock count to 0. Used when doing a partial reset of
// ACS state such as travelling to a new hub.
//
//============================================================================

void ACSStringPool::UnlockAll()
{
	for (unsigned int i = 0; i < Pool.Size(); ++i)
	{
		Pool[i].LockCount = 0;
	}
}

//============================================================================
//
// ACSStringPool :: PurgeStrings
//
// Remove all unlocked strings from the pool.
//
//============================================================================

void ACSStringPool::PurgeStrings()
{
	// Clear the hash buckets. We'll rebuild them as we decide what strings
	// to keep and which to toss.
	memset(PoolBuckets, 0xFF, sizeof(PoolBuckets));
	size_t usedcount = 0, freedcount = 0;
	for (unsigned int i = 0; i < Pool.Size(); ++i)
	{
		PoolEntry *entry = &Pool[i];
		if (entry->Next != FREE_ENTRY)
		{
			if (entry->LockCount == 0)
			{
				freedcount++;
				// Mark this entry as free.
				entry->Next = FREE_ENTRY;
				if (i < FirstFreeEntry)
				{
					FirstFreeEntry = i;
				}
				// And free the string.
				entry->Str = "";
			}
			else
			{
				usedcount++;
				// Rehash this entry.
				unsigned int h = entry->Hash % NUM_BUCKETS;
				entry->Next = PoolBuckets[h];
				PoolBuckets[h] = i;
				// Remove MarkString's mark.
				entry->LockCount &= 0x7FFFFFFF;
			}
		}
	}
}

//============================================================================
//
// ACSStringPool :: FindString
//
// Finds a string in the pool. Does not include the library ID in the returned
// value. Returns -1 if the string does not exist in the pool.
//
//============================================================================

int ACSStringPool::FindString(const char *str, size_t len, unsigned int h, unsigned int bucketnum)
{
	unsigned int i = PoolBuckets[bucketnum];
	while (i != NO_ENTRY)
	{
		PoolEntry *entry = &Pool[i];
		assert(entry->Next != FREE_ENTRY);
		if (entry->Hash == h && entry->Str.Len() == len &&
			memcmp(entry->Str.GetChars(), str, len) == 0)
		{
			return i;
		}
		i = entry->Next;
	}
	return -1;
}

//============================================================================
//
// ACSStringPool :: InsertString
//
// Inserts a new string into the pool.
//
//============================================================================

int ACSStringPool::InsertString(FString &str, unsigned int h, unsigned int bucketnum)
{
	unsigned int index = FirstFreeEntry;
	if (index >= MIN_GC_SIZE && index == Pool.Max())
	{ // We will need to grow the array. Try a garbage collection first.
		P_CollectACSGlobalStrings();
		index = FirstFreeEntry;
	}
	if (FirstFreeEntry >= STRPOOL_LIBRARYID_OR)
	{ // If we go any higher, we'll collide with the library ID marker.
		return -1;
	}
	if (index == Pool.Size())
	{ // There were no free entries; make a new one.
		Pool.Reserve(1);
		FirstFreeEntry++;
	}
	else
	{ // Scan for the next free entry
		FindFirstFreeEntry(FirstFreeEntry + 1);
	}
	PoolEntry *entry = &Pool[index];
	entry->Str = str;
	entry->Hash = h;
	entry->Next = PoolBuckets[bucketnum];
	entry->LockCount = 0;
	PoolBuckets[bucketnum] = index;
	return index | STRPOOL_LIBRARYID_OR;
}

//============================================================================
//
// ACSStringPool :: FindFirstFreeEntry
//
// Finds the first free entry, starting at base.
//
//============================================================================

void ACSStringPool::FindFirstFreeEntry(unsigned base)
{
	while (base < Pool.Size() && Pool[base].Next != FREE_ENTRY)
	{
		base++;
	}
	FirstFreeEntry = base;
}

//============================================================================
//
// ACSStringPool :: ReadStrings
//
// Reads strings from a PNG chunk.
//
//============================================================================

void ACSStringPool::ReadStrings(PNGHandle *png, DWORD id)
{
	Clear();

	size_t len = M_FindPNGChunk(png, id);
	if (len != 0)
	{
		FPNGChunkArchive arc(png->File->GetFile(), id, len);
		int32_t i, j, poolsize;
		unsigned int h, bucketnum;
		char *str = NULL;

		arc << poolsize;

		Pool.Resize(poolsize);
		i = 0;
		j = arc.ReadCount();
		while (j >= 0)
		{
			// Mark skipped entries as free
			for (; i < j; ++i)
			{
				Pool[i].Next = FREE_ENTRY;
				Pool[i].LockCount = 0;
			}
			arc << str;
			h = SuperFastHash(str, strlen(str));
			bucketnum = h % NUM_BUCKETS;
			Pool[i].Str = str;
			Pool[i].Hash = h;
			Pool[i].LockCount = arc.ReadCount();
			Pool[i].Next = PoolBuckets[bucketnum];
			PoolBuckets[bucketnum] = i;
			i++;
			j = arc.ReadCount();
		}
		if (str != NULL)
		{
			delete[] str;
		}
		FindFirstFreeEntry(0);
	}
}

//============================================================================
//
// ACSStringPool :: WriteStrings
//
// Writes strings to a PNG chunk.
//
//============================================================================

void ACSStringPool::WriteStrings(FILE *file, DWORD id) const
{
	int32_t i, poolsize = (int32_t)Pool.Size();
	
	if (poolsize == 0)
	{ // No need to write if we don't have anything.
		return;
	}
	FPNGChunkArchive arc(file, id);

	arc << poolsize;
	for (i = 0; i < poolsize; ++i)
	{
		PoolEntry *entry = &Pool[i];
		if (entry->Next != FREE_ENTRY)
		{
			arc.WriteCount(i);
			arc.WriteString(entry->Str);
			arc.WriteCount(entry->LockCount);
		}
	}
	arc.WriteCount(-1);
}

//============================================================================
//
// ACSStringPool :: Dump
//
// Lists all strings in the pool.
//
//============================================================================

void ACSStringPool::Dump() const
{
	for (unsigned int i = 0; i < Pool.Size(); ++i)
	{
		if (Pool[i].Next != FREE_ENTRY)
		{
			Printf("%4u. (%2d) \"%s\"\n", i, Pool[i].LockCount, Pool[i].Str.GetChars());
		}
	}
	Printf("First free %u\n", FirstFreeEntry);
}

//============================================================================
//
// P_MarkWorldVarStrings
//
//============================================================================

void P_MarkWorldVarStrings()
{
	GlobalACSStrings.MarkStringArray(ACS_WorldVars, countof(ACS_WorldVars));
	for (size_t i = 0; i < countof(ACS_WorldArrays); ++i)
	{
		GlobalACSStrings.MarkStringMap(ACS_WorldArrays[i]);
	}
}

//============================================================================
//
// P_MarkGlobalVarStrings
//
//============================================================================

void P_MarkGlobalVarStrings()
{
	GlobalACSStrings.MarkStringArray(ACS_GlobalVars, countof(ACS_GlobalVars));
	for (size_t i = 0; i < countof(ACS_GlobalArrays); ++i)
	{
		GlobalACSStrings.MarkStringMap(ACS_GlobalArrays[i]);
	}
}

//============================================================================
//
// P_CollectACSGlobalStrings
//
// Garbage collect ACS global strings.
//
//============================================================================

void P_CollectACSGlobalStrings()
{
	for (FACSStack *stack = FACSStack::head; stack != NULL; stack = stack->next)
	{
		GlobalACSStrings.MarkStringArray(stack->buffer, stack->sp);
	}
	FBehavior::StaticMarkLevelVarStrings();
	P_MarkWorldVarStrings();
	P_MarkGlobalVarStrings();
	GlobalACSStrings.PurgeStrings();
}

#ifdef _DEBUG
CCMD(acsgc)
{
	P_CollectACSGlobalStrings();
}
CCMD(globstr)
{
	GlobalACSStrings.Dump();
}
#endif

//============================================================================
//
// ScriptPresentation
//
// Returns a presentable version of the script number.
//
//============================================================================

static FString ScriptPresentation(int script)
{
	FString out = "script ";

	if (script < 0)
	{
		FName scrname = FName(ENamedName(-script));
		if (scrname.IsValidName())
		{
			out << '"' << scrname.GetChars() << '"';
			return out;
		}
	}
	out.AppendFormat("%d", script);
	return out;
}

//============================================================================
//
// P_ClearACSVars
//
//============================================================================

void P_ClearACSVars(bool alsoglobal)
{
	int i;

	memset (ACS_WorldVars, 0, sizeof(ACS_WorldVars));
	for (i = 0; i < NUM_WORLDVARS; ++i)
	{
		ACS_WorldArrays[i].Clear ();
	}
	if (alsoglobal)
	{
		memset (ACS_GlobalVars, 0, sizeof(ACS_GlobalVars));
		for (i = 0; i < NUM_GLOBALVARS; ++i)
		{
			ACS_GlobalArrays[i].Clear ();
		}
		// Since we cleared all ACS variables, we know nothing refers to them
		// anymore.
		GlobalACSStrings.Clear();
	}
	else
	{
		// Purge any strings that aren't referenced by global variables, since
		// they're the only possible references left.
		P_MarkGlobalVarStrings();
		GlobalACSStrings.PurgeStrings();
	}
}

//============================================================================
//
// WriteVars
//
//============================================================================

static void WriteVars (FILE *file, SDWORD *vars, size_t count, DWORD id)
{
	size_t i, j;

	for (i = 0; i < count; ++i)
	{
		if (vars[i] != 0)
			break;
	}
	if (i < count)
	{
		// Find last non-zero var. Anything beyond the last stored variable
		// will be zeroed at load time.
		for (j = count-1; j > i; --j)
		{
			if (vars[j] != 0)
				break;
		}
		FPNGChunkArchive arc (file, id);
		for (i = 0; i <= j; ++i)
		{
			DWORD var = vars[i];
			arc << var;
		}
	}
}

//============================================================================
//
//
//
//============================================================================

static void ReadVars (PNGHandle *png, SDWORD *vars, size_t count, DWORD id)
{
	size_t len = M_FindPNGChunk (png, id);
	size_t used = 0;

	if (len != 0)
	{
		DWORD var;
		size_t i;
		FPNGChunkArchive arc (png->File->GetFile(), id, len);
		used = len / 4;

		for (i = 0; i < used; ++i)
		{
			arc << var;
			vars[i] = var;
		}
		png->File->ResetFilePtr();
	}
	if (used < count)
	{
		memset (&vars[used], 0, (count-used)*4);
	}
}

//============================================================================
//
//
//
//============================================================================

static void WriteArrayVars (FILE *file, FWorldGlobalArray *vars, unsigned int count, DWORD id)
{
	unsigned int i, j;

	// Find the first non-empty array.
	for (i = 0; i < count; ++i)
	{
		if (vars[i].CountUsed() != 0)
			break;
	}
	if (i < count)
	{
		// Find last non-empty array. Anything beyond the last stored array
		// will be emptied at load time.
		for (j = count-1; j > i; --j)
		{
			if (vars[j].CountUsed() != 0)
				break;
		}
		FPNGChunkArchive arc (file, id);
		arc.WriteCount (i);
		arc.WriteCount (j);
		for (; i <= j; ++i)
		{
			arc.WriteCount (vars[i].CountUsed());

			FWorldGlobalArray::ConstIterator it(vars[i]);
			const FWorldGlobalArray::Pair *pair;

			while (it.NextPair (pair))
			{
				arc.WriteCount (pair->Key);
				arc.WriteCount (pair->Value);
			}
		}
	}
}

//============================================================================
//
//
//
//============================================================================

static void ReadArrayVars (PNGHandle *png, FWorldGlobalArray *vars, size_t count, DWORD id)
{
	size_t len = M_FindPNGChunk (png, id);
	unsigned int i, k;

	for (i = 0; i < count; ++i)
	{
		vars[i].Clear ();
	}

	if (len != 0)
	{
		DWORD max, size;
		FPNGChunkArchive arc (png->File->GetFile(), id, len);

		i = arc.ReadCount ();
		max = arc.ReadCount ();

		for (; i <= max; ++i)
		{
			size = arc.ReadCount ();
			for (k = 0; k < size; ++k)
			{
				SDWORD key, val;
				key = arc.ReadCount();

				val = arc.ReadCount();
				vars[i].Insert (key, val);
			}
		}
		png->File->ResetFilePtr();
	}
}

//============================================================================
//
//
//
//============================================================================

void P_ReadACSVars(PNGHandle *png)
{
	ReadVars (png, ACS_WorldVars, NUM_WORLDVARS, MAKE_ID('w','v','A','r'));
	ReadVars (png, ACS_GlobalVars, NUM_GLOBALVARS, MAKE_ID('g','v','A','r'));
	ReadArrayVars (png, ACS_WorldArrays, NUM_WORLDVARS, MAKE_ID('w','a','R','r'));
	ReadArrayVars (png, ACS_GlobalArrays, NUM_GLOBALVARS, MAKE_ID('g','a','R','r'));
	GlobalACSStrings.ReadStrings(png, MAKE_ID('a','s','T','r'));
}

//============================================================================
//
//
//
//============================================================================

void P_WriteACSVars(FILE *stdfile)
{
	WriteVars (stdfile, ACS_WorldVars, NUM_WORLDVARS, MAKE_ID('w','v','A','r'));
	WriteVars (stdfile, ACS_GlobalVars, NUM_GLOBALVARS, MAKE_ID('g','v','A','r'));
	WriteArrayVars (stdfile, ACS_WorldArrays, NUM_WORLDVARS, MAKE_ID('w','a','R','r'));
	WriteArrayVars (stdfile, ACS_GlobalArrays, NUM_GLOBALVARS, MAKE_ID('g','a','R','r'));
	GlobalACSStrings.WriteStrings(stdfile, MAKE_ID('a','s','T','r'));
}

//---- Inventory functions --------------------------------------//
//

//============================================================================
//
// ClearInventory
//
// Clears the inventory for one or more actors.
//
//============================================================================

static void ClearInventory (AActor *activator)
{
	if (activator == NULL)
	{
		for (int i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i])
				players[i].mo->ClearInventory();
		}
	}
	else
	{
		activator->ClearInventory();
	}
}

//============================================================================
//
// GiveInventory
//
// Gives an item to one or more actors.
//
//============================================================================

static void GiveInventory (AActor *activator, const char *type, int amount)
{
	PClassActor *info;

	if (amount <= 0 || type == NULL)
	{
		return;
	}
	if (stricmp (type, "Armor") == 0)
	{
		type = "BasicArmorPickup";
	}
	info = PClass::FindActor(type);
	if (info == NULL)
	{
		Printf ("ACS: I don't know what %s is.\n", type);
	}
	else if (!info->IsDescendantOf (RUNTIME_CLASS(AInventory)))
	{
		Printf ("ACS: %s is not an inventory item.\n", type);
	}
	else if (activator == NULL)
	{
		for (int i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i])
				players[i].mo->GiveInventory(static_cast<PClassInventory *>(info), amount);
		}
	}
	else
	{
		activator->GiveInventory(static_cast<PClassInventory *>(info), amount);
	}
}

//============================================================================
//
// TakeInventory
//
// Takes an item from one or more actors.
//
//============================================================================

static void TakeInventory (AActor *activator, const char *type, int amount)
{
	PClassActor *info;

	if (type == NULL)
	{
		return;
	}
	if (strcmp (type, "Armor") == 0)
	{
		type = "BasicArmor";
	}
	if (amount <= 0)
	{
		return;
	}
	info = PClass::FindActor (type);
	if (info == NULL)
	{
		return;
	}
	if (activator == NULL)
	{
		for (int i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i])
				players[i].mo->TakeInventory(info, amount);
		}
	}
	else
	{
		activator->TakeInventory(info, amount);
	}
}

//============================================================================
//
// DoUseInv
//
// Makes a single actor use an inventory item
//
//============================================================================

static bool DoUseInv (AActor *actor, PClassActor *info)
{
	AInventory *item = actor->FindInventory (info);
	if (item != NULL)
	{
		if (actor->player == NULL)
		{
			return actor->UseInventory(item);
		}
		else
		{
			int cheats;
			bool res;

			// Bypass CF_TOTALLYFROZEN
			cheats = actor->player->cheats;
			actor->player->cheats &= ~CF_TOTALLYFROZEN;
			res = actor->UseInventory(item);
			actor->player->cheats |= (cheats & CF_TOTALLYFROZEN);
			return res;
		}
	}
	return false;
}

//============================================================================
//
// UseInventory
//
// makes one or more actors use an inventory item.
//
//============================================================================

static int UseInventory (AActor *activator, const char *type)
{
	PClassActor *info;
	int ret = 0;

	if (type == NULL)
	{
		return 0;
	}
	info = PClass::FindActor (type);
	if (info == NULL)
	{
		return 0;
	}
	if (activator == NULL)
	{
		for (int i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i])
				ret += DoUseInv (players[i].mo, info);
		}
	}
	else
	{
		ret = DoUseInv (activator, info);
	}
	return ret;
}

//============================================================================
//
// CheckInventory
//
// Returns how much of a particular item an actor has.
//
//============================================================================

static int CheckInventory (AActor *activator, const char *type, bool max)
{
	if (activator == NULL || type == NULL)
		return 0;

	if (stricmp (type, "Armor") == 0)
	{
		type = "BasicArmor";
	}
	else if (stricmp (type, "Health") == 0)
	{
		if (max)
		{
			if (activator->IsKindOf (RUNTIME_CLASS (APlayerPawn)))
				return static_cast<APlayerPawn *>(activator)->MaxHealth;
			else
				return activator->SpawnHealth();
		}
		return activator->health;
	}

	PClassActor *info = PClass::FindActor (type);

	if (info == NULL)
	{
		Printf ("ACS: I don't know what '%s' is.\n", type);
		return 0;
	}
	else if (!info->IsDescendantOf(RUNTIME_CLASS(AInventory)))
	{
		Printf ("ACS: '%s' is not an inventory item.\n", type);
		return 0;
	}

	AInventory *item = activator->FindInventory (info);

	if (max)
	{
		if (item)
		{
			return item->MaxAmount;
		}
		else if (info != nullptr && info->IsDescendantOf(RUNTIME_CLASS(AInventory)))
		{
			return ((AInventory *)GetDefaultByType(info))->MaxAmount;
		}
	}
	return item ? item->Amount : 0;
}

//---- Plane watchers ----//

class DPlaneWatcher : public DThinker
{
	DECLARE_CLASS (DPlaneWatcher, DThinker)
	HAS_OBJECT_POINTERS
public:
	DPlaneWatcher (AActor *it, line_t *line, int lineSide, bool ceiling,
		int tag, int height, int special,
		int arg0, int arg1, int arg2, int arg3, int arg4);
	void Tick ();
	void Serialize (FArchive &arc);
private:
	sector_t *Sector;
	double WatchD, LastD;
	int Special, Arg0, Arg1, Arg2, Arg3, Arg4;
	TObjPtr<AActor> Activator;
	line_t *Line;
	bool LineSide;
	bool bCeiling;

	DPlaneWatcher() {}
};

IMPLEMENT_POINTY_CLASS (DPlaneWatcher)
 DECLARE_POINTER (Activator)
END_POINTERS

DPlaneWatcher::DPlaneWatcher (AActor *it, line_t *line, int lineSide, bool ceiling,
	int tag, int height, int special,
	int arg0, int arg1, int arg2, int arg3, int arg4)
	: Special (special), Arg0 (arg0), Arg1 (arg1), Arg2 (arg2), Arg3 (arg3), Arg4 (arg4),
	  Activator (it), Line (line), LineSide (!!lineSide), bCeiling (ceiling)
{
	int secnum;

	secnum = P_FindFirstSectorFromTag (tag);
	if (secnum >= 0)
	{
		secplane_t plane;

		Sector = &sectors[secnum];
		if (bCeiling)
		{
			plane = Sector->ceilingplane;
		}
		else
		{
			plane = Sector->floorplane;
		}
		LastD = plane.fD();
		plane.ChangeHeight (height);
		WatchD = plane.fD();
	}
	else
	{
		Sector = NULL;
		WatchD = LastD = 0;
	}
}

void DPlaneWatcher::Serialize (FArchive &arc)
{
	Super::Serialize (arc);

	arc << Special << Arg0 << Arg1 << Arg2 << Arg3 << Arg4
		<< Sector << bCeiling << WatchD << LastD << Activator
		<< Line << LineSide << bCeiling;
}

void DPlaneWatcher::Tick ()
{
	if (Sector == NULL)
	{
		Destroy ();
		return;
	}

	double newd;

	if (bCeiling)
	{
		newd = Sector->ceilingplane.fD();
	}
	else
	{
		newd = Sector->floorplane.fD();
	}

	if ((LastD < WatchD && newd >= WatchD) ||
		(LastD > WatchD && newd <= WatchD))
	{
		P_ExecuteSpecial(Special, Line, Activator, LineSide, Arg0, Arg1, Arg2, Arg3, Arg4);
		Destroy ();
	}

}

//---- ACS lump manager ----//

// Load user-specified default modules. This must be called after the level's
// own behavior is loaded (if it has one).
void FBehavior::StaticLoadDefaultModules ()
{
	// Scan each LOADACS lump and load the specified modules in order
	int lump, lastlump = 0;

	while ((lump = Wads.FindLump ("LOADACS", &lastlump)) != -1)
	{
		FScanner sc(lump);
		while (sc.GetString())
		{
			int acslump = Wads.CheckNumForName (sc.String, ns_acslibrary);
			if (acslump >= 0)
			{
				StaticLoadModule (acslump);
			}
			else
			{
				Printf (TEXTCOLOR_RED "Could not find autoloaded ACS library %s\n", sc.String);
			}
		}
	}
}

FBehavior *FBehavior::StaticLoadModule (int lumpnum, FileReader *fr, int len)
{
	if (lumpnum == -1 && fr == NULL) return NULL;

	for (unsigned int i = 0; i < StaticModules.Size(); ++i)
	{
		if (StaticModules[i]->LumpNum == lumpnum)
		{
			return StaticModules[i];
		}
	}

	FBehavior * behavior = new FBehavior ();
	if (behavior->Init(lumpnum, fr, len))
	{
		return behavior;
	}
	else
	{
		delete behavior;
		Printf(TEXTCOLOR_RED "%s: invalid ACS module\n", Wads.GetLumpFullName(lumpnum));
		return NULL;
	}
}

bool FBehavior::StaticCheckAllGood ()
{
	for (unsigned int i = 0; i < StaticModules.Size(); ++i)
	{
		if (!StaticModules[i]->IsGood())
		{
			return false;
		}
	}
	return true;
}

void FBehavior::StaticUnloadModules ()
{
	for (unsigned int i = StaticModules.Size(); i-- > 0; )
	{
		delete StaticModules[i];
	}
	StaticModules.Clear ();
}

FBehavior *FBehavior::StaticGetModule (int lib)
{
	if ((size_t)lib >= StaticModules.Size())
	{
		return NULL;
	}
	return StaticModules[lib];
}

void FBehavior::StaticMarkLevelVarStrings()
{
	// Mark map variables.
	for (DWORD modnum = 0; modnum < StaticModules.Size(); ++modnum)
	{
		StaticModules[modnum]->MarkMapVarStrings();
	}
	// Mark running scripts' local variables.
	if (DACSThinker::ActiveThinker != NULL)
	{
		for (DLevelScript *script = DACSThinker::ActiveThinker->Scripts; script != NULL; script = script->GetNext())
		{
			script->MarkLocalVarStrings();
		}
	}
}

void FBehavior::StaticLockLevelVarStrings()
{
	// Lock map variables.
	for (DWORD modnum = 0; modnum < StaticModules.Size(); ++modnum)
	{
		StaticModules[modnum]->LockMapVarStrings();
	}
	// Lock running scripts' local variables.
	if (DACSThinker::ActiveThinker != NULL)
	{
		for (DLevelScript *script = DACSThinker::ActiveThinker->Scripts; script != NULL; script = script->GetNext())
		{
			script->LockLocalVarStrings();
		}
	}
}

void FBehavior::StaticUnlockLevelVarStrings()
{
	// Unlock map variables.
	for (DWORD modnum = 0; modnum < StaticModules.Size(); ++modnum)
	{
		StaticModules[modnum]->UnlockMapVarStrings();
	}
	// Unlock running scripts' local variables.
	if (DACSThinker::ActiveThinker != NULL)
	{
		for (DLevelScript *script = DACSThinker::ActiveThinker->Scripts; script != NULL; script = script->GetNext())
		{
			script->UnlockLocalVarStrings();
		}
	}
}

void FBehavior::MarkMapVarStrings() const
{
	GlobalACSStrings.MarkStringArray(MapVarStore, NUM_MAPVARS);
	for (int i = 0; i < NumArrays; ++i)
	{
		GlobalACSStrings.MarkStringArray(ArrayStore[i].Elements, ArrayStore[i].ArraySize);
	}
}

void FBehavior::LockMapVarStrings() const
{
	GlobalACSStrings.LockStringArray(MapVarStore, NUM_MAPVARS);
	for (int i = 0; i < NumArrays; ++i)
	{
		GlobalACSStrings.LockStringArray(ArrayStore[i].Elements, ArrayStore[i].ArraySize);
	}
}

void FBehavior::UnlockMapVarStrings() const
{
	GlobalACSStrings.UnlockStringArray(MapVarStore, NUM_MAPVARS);
	for (int i = 0; i < NumArrays; ++i)
	{
		GlobalACSStrings.UnlockStringArray(ArrayStore[i].Elements, ArrayStore[i].ArraySize);
	}
}

void FBehavior::StaticSerializeModuleStates (FArchive &arc)
{
	DWORD modnum;

	modnum = StaticModules.Size();
	arc << modnum;

	if (modnum != StaticModules.Size())
	{
		I_Error("Level was saved with a different number of ACS modules. (Have %d, save has %d)", StaticModules.Size(), modnum);
	}

	for (modnum = 0; modnum < StaticModules.Size(); ++modnum)
	{
		FBehavior *module = StaticModules[modnum];
		int ModSize = module->GetDataSize();

		if (arc.IsStoring())
		{
			arc.WriteString (module->ModuleName);
			arc << ModSize;
		}
		else
		{
			char *modname = NULL;
			arc << modname;
			arc << ModSize;
			if (stricmp (modname, module->ModuleName) != 0)
			{
				delete[] modname;
				I_Error("Level was saved with a different set or order of ACS modules. (Have %s, save has %s)", module->ModuleName, modname);
			}
			else if (ModSize != module->GetDataSize())
			{
				delete[] modname;
				I_Error("ACS module %s has changed from what was saved. (Have %d bytes, save has %d bytes)", module->ModuleName, module->GetDataSize(), ModSize);
			}
			delete[] modname;
		}
		module->SerializeVars (arc);
	}
}

void FBehavior::SerializeVars (FArchive &arc)
{
	SerializeVarSet (arc, MapVarStore, NUM_MAPVARS);
	for (int i = 0; i < NumArrays; ++i)
	{
		SerializeVarSet (arc, ArrayStore[i].Elements, ArrayStore[i].ArraySize);
	}
}

void FBehavior::SerializeVarSet (FArchive &arc, SDWORD *vars, int max)
{
	SDWORD arcval;
	SDWORD first, last;

	if (arc.IsStoring ())
	{
		// Find first non-zero variable
		for (first = 0; first < max; ++first)
		{
			if (vars[first] != 0)
			{
				break;
			}
		}

		// Find last non-zero variable
		for (last = max - 1; last >= first; --last)
		{
			if (vars[last] != 0)
			{
				break;
			}
		}

		if (last < first)
		{ // no non-zero variables
			arcval = 0;
			arc << arcval;
			return;
		}

		arcval = last - first + 1;
		arc << arcval;
		arcval = first;
		arc << arcval;

		while (first <= last)
		{
			arc << vars[first];
			++first;
		}
	}
	else
	{
		SDWORD truelast;

		memset (vars, 0, max*sizeof(*vars));

		arc << last;
		if (last == 0)
		{
			return;
		}
		arc << first;
		last += first;
		truelast = last;

		if (last > max)
		{
			last = max;
		}

		while (first < last)
		{
			arc << vars[first];
			++first;
		}
		while (first < truelast)
		{
			arc << arcval;
			++first;
		}
	}
}

static int ParseLocalArrayChunk(void *chunk, ACSLocalArrays *arrays, int offset)
{
	unsigned count = (LittleShort(static_cast<unsigned short>(((unsigned *)chunk)[1]) - 2)) / 4;
	int *sizes = (int *)((BYTE *)chunk + 10);
	arrays->Count = count;
	if (count > 0)
	{
		ACSLocalArrayInfo *info = new ACSLocalArrayInfo[count];
		arrays->Info = info;
		for (unsigned i = 0; i < count; ++i)
		{
			info[i].Size = LittleLong(sizes[i]);
			info[i].Offset = offset;
			offset += info[i].Size;
		}
	}
	// Return the new local variable size, with space for the arrays
	return offset;
}

FBehavior::FBehavior()
{
	NumScripts = 0;
	NumFunctions = 0;
	NumArrays = 0;
	NumTotalArrays = 0;
	Scripts = NULL;
	Functions = NULL;
	Arrays = NULL;
	ArrayStore = NULL;
	Chunks = NULL;
	Data = NULL;
	Format = ACS_Unknown;
	LumpNum = -1;
	memset (MapVarStore, 0, sizeof(MapVarStore));
	ModuleName[0] = 0;
	FunctionProfileData = NULL;

}
	
	
bool FBehavior::Init(int lumpnum, FileReader * fr, int len)
{
	BYTE *object;
	int i;

	LumpNum = lumpnum;

	// Now that everything is set up, record this module as being among the loaded modules.
	// We need to do this before resolving any imports, because an import might (indirectly)
	// need to resolve exports in this module. The only things that can be exported are
	// functions and map variables, which must already be present if they're exported, so
	// this is okay.

	// This must be done first for 2 reasons:
	// 1. If not, corrupt modules cause memory leaks
	// 2. Corrupt modules won't be reported when a level is being loaded if this function quits before
	//    adding it to the list.

	if (fr == NULL) len = Wads.LumpLength (lumpnum);



	// Any behaviors smaller than 32 bytes cannot possibly contain anything useful.
	// (16 bytes for a completely empty behavior + 12 bytes for one script header
	//  + 4 bytes for PCD_TERMINATE for an old-style object. A new-style object
	// has 24 bytes if it is completely empty. An empty SPTR chunk adds 8 bytes.)
	if (len < 32)
	{
		return false;
	}

	object = new BYTE[len];
	if (fr == NULL)
	{
		Wads.ReadLump (lumpnum, object);
	}
	else
	{
		fr->Read (object, len);
	}

	if (object[0] != 'A' || object[1] != 'C' || object[2] != 'S')
	{
		delete[] object;
		return false;
	}

	switch (object[3])
	{
	case 0:
		Format = ACS_Old;
		break;
	case 'E':
		Format = ACS_Enhanced;
		break;
	case 'e':
		Format = ACS_LittleEnhanced;
		break;
	default:
		delete[] object;
		return false;
	}
    LibraryID = StaticModules.Push (this) << LIBRARYID_SHIFT;

	if (fr == NULL)
	{
		Wads.GetLumpName (ModuleName, lumpnum);
		ModuleName[8] = 0;
	}
	else
	{
		strcpy(ModuleName, "BEHAVIOR");
	}

	Data = object;
	DataSize = len;

	if (Format == ACS_Old)
	{
		DWORD dirofs = LittleLong(((DWORD *)object)[1]);
		DWORD pretag = ((DWORD *)(object + dirofs))[-1];

		Chunks = object + len;
		// Check for redesigned ACSE/ACSe
		if (dirofs >= 6*4 &&
			(pretag == MAKE_ID('A','C','S','e') ||
			 pretag == MAKE_ID('A','C','S','E')))
		{
			Format = (pretag == MAKE_ID('A','C','S','e')) ? ACS_LittleEnhanced : ACS_Enhanced;
			Chunks = object + LittleLong(((DWORD *)(object + dirofs))[-2]);
			// Forget about the compatibility cruft at the end of the lump
			DataSize = LittleLong(((DWORD *)object)[1]) - 8;
		}
	}
	else
	{
		Chunks = object + LittleLong(((DWORD *)object)[1]);
	}

	LoadScriptsDirectory ();

	if (Format == ACS_Old)
	{
		StringTable = LittleLong(((DWORD *)Data)[1]);
		StringTable += LittleLong(((DWORD *)(Data + StringTable))[0]) * 12 + 4;
		UnescapeStringTable(Data + StringTable, Data, false);
	}
	else
	{
		UnencryptStrings ();
		BYTE *strings = FindChunk (MAKE_ID('S','T','R','L'));
		if (strings != NULL)
		{
			StringTable = DWORD(strings - Data + 8);
			UnescapeStringTable(strings + 8, NULL, true);
		}
		else
		{
			StringTable = 0;
		}
	}

	if (Format == ACS_Old)
	{
		// Do initialization for old-style behavior lumps
		for (i = 0; i < NUM_MAPVARS; ++i)
		{
			MapVars[i] = &MapVarStore[i];
		}
		//LibraryID = StaticModules.Push (this) << LIBRARYID_SHIFT;
	}
	else
	{
		DWORD *chunk;

		// Load functions
		BYTE *funcs;
		Functions = NULL;
		funcs = FindChunk (MAKE_ID('F','U','N','C'));
		if (funcs != NULL)
		{
			NumFunctions = LittleLong(((DWORD *)funcs)[1]) / 8;
			funcs += 8;
			FunctionProfileData = new ACSProfileInfo[NumFunctions];
			Functions = new ScriptFunction[NumFunctions];
			for (i = 0; i < NumFunctions; ++i)
			{
				ScriptFunctionInFile *funcf = &((ScriptFunctionInFile *)funcs)[i];
				ScriptFunction *funcm = &Functions[i];
				funcm->ArgCount = funcf->ArgCount;
				funcm->HasReturnValue = funcf->HasReturnValue;
				funcm->ImportNum = funcf->ImportNum;
				funcm->LocalCount = funcf->LocalCount;
				funcm->Address = LittleLong(funcf->Address);
			}
		}

		// Load local arrays for functions
		if (NumFunctions > 0)
		{
			for (chunk = (DWORD *)FindChunk(MAKE_ID('F','A','R','Y')); chunk != NULL; chunk = (DWORD *)NextChunk((BYTE *)chunk))
			{
				int size = LittleLong(chunk[1]);
				if (size >= 6)
				{
					unsigned int func_num = LittleShort(((WORD *)chunk)[4]);
					if (func_num < (unsigned int)NumFunctions)
					{
						ScriptFunction *func = &Functions[func_num];
						// Unlike scripts, functions do not include their arg count in their local count.
						func->LocalCount = ParseLocalArrayChunk(chunk, &func->LocalArrays, func->LocalCount + func->ArgCount) - func->ArgCount;
					}
				}
			}
		}

		// Load JUMP points
		chunk = (DWORD *)FindChunk (MAKE_ID('J','U','M','P'));
		if (chunk != NULL)
		{
			for (i = 0;i < (int)LittleLong(chunk[1]);i += 4)
				JumpPoints.Push(LittleLong(chunk[2 + i/4]));
		}

		// Initialize this object's map variables
		memset (MapVarStore, 0, sizeof(MapVarStore));
		chunk = (DWORD *)FindChunk (MAKE_ID('M','I','N','I'));
		while (chunk != NULL)
		{
			int numvars = LittleLong(chunk[1])/4 - 1;
			int firstvar = LittleLong(chunk[2]);
			for (i = 0; i < numvars; ++i)
			{
				MapVarStore[i+firstvar] = LittleLong(chunk[3+i]);
			}
			chunk = (DWORD *)NextChunk ((BYTE *)chunk);
		}

		// Initialize this object's map variable pointers to defaults. They can be changed
		// later once the imported modules are loaded.
		for (i = 0; i < NUM_MAPVARS; ++i)
		{
			MapVars[i] = &MapVarStore[i];
		}

		// Create arrays for this module
		chunk = (DWORD *)FindChunk (MAKE_ID('A','R','A','Y'));
		if (chunk != NULL)
		{
			NumArrays = LittleLong(chunk[1])/8;
			ArrayStore = new ArrayInfo[NumArrays];
			memset (ArrayStore, 0, sizeof(*Arrays)*NumArrays);
			for (i = 0; i < NumArrays; ++i)
			{
				MapVarStore[LittleLong(chunk[2+i*2])] = i;
				ArrayStore[i].ArraySize = LittleLong(chunk[3+i*2]);
				ArrayStore[i].Elements = new SDWORD[ArrayStore[i].ArraySize];
				memset(ArrayStore[i].Elements, 0, ArrayStore[i].ArraySize*sizeof(DWORD));
			}
		}

		// Initialize arrays for this module
		chunk = (DWORD *)FindChunk (MAKE_ID('A','I','N','I'));
		while (chunk != NULL)
		{
			int arraynum = MapVarStore[LittleLong(chunk[2])];
			if ((unsigned)arraynum < (unsigned)NumArrays)
			{
				// Use unsigned iterator here to avoid issue with GCC 4.9/5.x
				// optimizer. Might be some undefined behavior in this code,
				// but I don't know what it is.
				unsigned int initsize = MIN<unsigned int> (ArrayStore[arraynum].ArraySize, (LittleLong(chunk[1])-4)/4);
				SDWORD *elems = ArrayStore[arraynum].Elements;
				for (unsigned int j = 0; j < initsize; ++j)
				{
					elems[j] = LittleLong(chunk[3+j]);
				}
			}
			chunk = (DWORD *)NextChunk((BYTE *)chunk);
		}

		// Start setting up array pointers
		NumTotalArrays = NumArrays;
		chunk = (DWORD *)FindChunk (MAKE_ID('A','I','M','P'));
		if (chunk != NULL)
		{
			NumTotalArrays += LittleLong(chunk[2]);
		}
		if (NumTotalArrays != 0)
		{
			Arrays = new ArrayInfo *[NumTotalArrays];
			for (i = 0; i < NumArrays; ++i)
			{
				Arrays[i] = &ArrayStore[i];
			}
		}

		// Tag the library ID to any map variables that are initialized with strings
		if (LibraryID != 0)
		{
			chunk = (DWORD *)FindChunk (MAKE_ID('M','S','T','R'));
			if (chunk != NULL)
			{
				for (DWORD i = 0; i < LittleLong(chunk[1])/4; ++i)
				{
					const char *str = LookupString(MapVarStore[LittleLong(chunk[i+2])]);
					if (str != NULL)
					{
						MapVarStore[LittleLong(chunk[i+2])] = GlobalACSStrings.AddString(str);
					}
				}
			}

			chunk = (DWORD *)FindChunk (MAKE_ID('A','S','T','R'));
			if (chunk != NULL)
			{
				for (DWORD i = 0; i < LittleLong(chunk[1])/4; ++i)
				{
					int arraynum = MapVarStore[LittleLong(chunk[i+2])];
					if ((unsigned)arraynum < (unsigned)NumArrays)
					{
						SDWORD *elems = ArrayStore[arraynum].Elements;
						for (int j = ArrayStore[arraynum].ArraySize; j > 0; --j, ++elems)
						{
//							*elems |= LibraryID;
							const char *str = LookupString(*elems);
							if (str != NULL)
							{
								*elems = GlobalACSStrings.AddString(str);
							}
						}
					}
				}
			}

			// [BL] Newer version of ASTR for structure aware compilers although we only have one array per chunk
			chunk = (DWORD *)FindChunk (MAKE_ID('A','T','A','G'));
			while (chunk != NULL)
			{
				const BYTE* chunkData = (const BYTE*)(chunk + 2);
				// First byte is version, it should be 0
				if(*chunkData++ == 0)
				{
					int arraynum = MapVarStore[uallong(LittleLong(*(const int*)(chunkData)))];
					chunkData += 4;
					if ((unsigned)arraynum < (unsigned)NumArrays)
					{
						SDWORD *elems = ArrayStore[arraynum].Elements;
						// Ending zeros may be left out.
						for (int j = MIN(LittleLong(chunk[1])-5, ArrayStore[arraynum].ArraySize); j > 0; --j, ++elems, ++chunkData)
						{
							// For ATAG, a value of 0 = Integer, 1 = String, 2 = FunctionPtr
							// Our implementation uses the same tags for both String and FunctionPtr
							if (*chunkData == 2)
							{
								*elems |= LibraryID;
							}
							else if (*chunkData == 1)
							{
								const char *str = LookupString(*elems);
								if (str != NULL)
								{
									*elems = GlobalACSStrings.AddString(str);
								}
							}
						}
					}
				}

				chunk = (DWORD *)NextChunk ((BYTE *)chunk);
			}
		}

		// Load required libraries.
		if (NULL != (chunk = (DWORD *)FindChunk (MAKE_ID('L','O','A','D'))))
		{
			const char *const parse = (char *)&chunk[2];
			DWORD i;

			for (i = 0; i < LittleLong(chunk[1]); )
			{
				if (parse[i])
				{
					FBehavior *module = NULL;
					int lump = Wads.CheckNumForName (&parse[i], ns_acslibrary);
					if (lump < 0)
					{
						Printf (TEXTCOLOR_RED "Could not find ACS library %s.\n", &parse[i]);
					}
					else
					{
						module = StaticLoadModule (lump);
					}
					if (module != NULL) Imports.Push (module);
					do {;} while (parse[++i]);
				}
				++i;
			}

			// Go through each imported module in order and resolve all imported functions
			// and map variables.
			for (i = 0; i < Imports.Size(); ++i)
			{
				FBehavior *lib = Imports[i];
				int j;

				if (lib == NULL)
					continue;

				// Resolve functions
				chunk = (DWORD *)FindChunk(MAKE_ID('F','N','A','M'));
				for (j = 0; j < NumFunctions; ++j)
				{
					ScriptFunction *func = &((ScriptFunction *)Functions)[j];
					if (func->Address == 0 && func->ImportNum == 0)
					{
						int libfunc = lib->FindFunctionName ((char *)(chunk + 2) + LittleLong(chunk[3+j]));
						if (libfunc >= 0)
						{
							ScriptFunction *realfunc = &((ScriptFunction *)lib->Functions)[libfunc];
							// Make sure that the library really defines this function. It might simply
							// be importing it itself.
							if (realfunc->Address != 0 && realfunc->ImportNum == 0)
							{
								func->Address = libfunc;
								func->ImportNum = i+1;
								if (realfunc->ArgCount != func->ArgCount)
								{
									Printf (TEXTCOLOR_ORANGE "Function %s in %s has %d arguments. %s expects it to have %d.\n",
										(char *)(chunk + 2) + LittleLong(chunk[3+j]), lib->ModuleName, realfunc->ArgCount,
										ModuleName, func->ArgCount);
									Format = ACS_Unknown;
								}
								// The next two properties do not affect code compatibility, so it is
								// okay for them to be different in the imported module than they are
								// in this one, as long as we make sure to use the real values.
								func->LocalCount = LittleLong(realfunc->LocalCount);
								func->HasReturnValue = realfunc->HasReturnValue;
							}
						}
					}
				}

				// Resolve map variables
				chunk = (DWORD *)FindChunk(MAKE_ID('M','I','M','P'));
				if (chunk != NULL)
				{
					char *parse = (char *)&chunk[2];
					for (DWORD j = 0; j < LittleLong(chunk[1]); )
					{
						DWORD varNum = LittleLong(*(DWORD *)&parse[j]);
						j += 4;
						int impNum = lib->FindMapVarName (&parse[j]);
						if (impNum >= 0)
						{
							MapVars[varNum] = &lib->MapVarStore[impNum];
						}
						do {;} while (parse[++j]);
						++j;
					}
				}

				// Resolve arrays
				if (NumTotalArrays > NumArrays)
				{
					chunk = (DWORD *)FindChunk(MAKE_ID('A','I','M','P'));
					char *parse = (char *)&chunk[3];
					for (DWORD j = 0; j < LittleLong(chunk[2]); ++j)
					{
						DWORD varNum = LittleLong(*(DWORD *)parse);
						parse += 4;
						DWORD expectedSize = LittleLong(*(DWORD *)parse);
						parse += 4;
						int impNum = lib->FindMapArray (parse);
						if (impNum >= 0)
						{
							Arrays[NumArrays + j] = &lib->ArrayStore[impNum];
							MapVarStore[varNum] = NumArrays + j;
							if (lib->ArrayStore[impNum].ArraySize != expectedSize)
							{
								Format = ACS_Unknown;
								Printf (TEXTCOLOR_ORANGE "The array %s in %s has %u elements, but %s expects it to only have %u.\n",
									parse, lib->ModuleName, lib->ArrayStore[impNum].ArraySize,
									ModuleName, expectedSize);
							}
						}
						do {;} while (*++parse);
						++parse;
					}
				}
			}
		}
	}

	DPrintf ("Loaded %d scripts, %d functions\n", NumScripts, NumFunctions);
	return true;
}

FBehavior::~FBehavior ()
{
	if (Scripts != NULL)
	{
		delete[] Scripts;
		Scripts = NULL;
	}
	if (Arrays != NULL)
	{
		delete[] Arrays;
		Arrays = NULL;
	}
	if (ArrayStore != NULL)
	{
		for (int i = 0; i < NumArrays; ++i)
		{
			if (ArrayStore[i].Elements != NULL)
			{
				delete[] ArrayStore[i].Elements;
				ArrayStore[i].Elements = NULL;
			}
		}
		delete[] ArrayStore;
		ArrayStore = NULL;
	}
	if (Functions != NULL)
	{
		delete[] Functions;
		Functions = NULL;
	}
	if (FunctionProfileData != NULL)
	{
		delete[] FunctionProfileData;
		FunctionProfileData = NULL;
	}
	if (Data != NULL)
	{
		delete[] Data;
		Data = NULL;
	}
}

void FBehavior::LoadScriptsDirectory ()
{
	union
	{
		BYTE *b;
		DWORD *dw;
		WORD *w;
		SWORD *sw;
		ScriptPtr2 *po;		// Old
		ScriptPtr1 *pi;		// Intermediate
		ScriptPtr3 *pe;		// LittleEnhanced
	} scripts;
	int i, max;

	NumScripts = 0;
	Scripts = NULL;

	// Load the main script directory
	switch (Format)
	{
	case ACS_Old:
		scripts.dw = (DWORD *)(Data + LittleLong(((DWORD *)Data)[1]));
		NumScripts = LittleLong(scripts.dw[0]);
		if (NumScripts != 0)
		{
			scripts.dw++;

			Scripts = new ScriptPtr[NumScripts];

			for (i = 0; i < NumScripts; ++i)
			{
				ScriptPtr2 *ptr1 = &scripts.po[i];
				ScriptPtr  *ptr2 = &Scripts[i];

				ptr2->Number = LittleLong(ptr1->Number) % 1000;
				ptr2->Type = LittleLong(ptr1->Number) / 1000;
				ptr2->ArgCount = LittleLong(ptr1->ArgCount);
				ptr2->Address = LittleLong(ptr1->Address);
			}
		}
		break;

	case ACS_Enhanced:
	case ACS_LittleEnhanced:
		scripts.b = FindChunk (MAKE_ID('S','P','T','R'));
		if (scripts.b == NULL)
		{
			// There are no scripts!
		}
		else if (*(DWORD *)Data != MAKE_ID('A','C','S',0))
		{
			NumScripts = LittleLong(scripts.dw[1]) / 12;
			Scripts = new ScriptPtr[NumScripts];
			scripts.dw += 2;

			for (i = 0; i < NumScripts; ++i)
			{
				ScriptPtr1 *ptr1 = &scripts.pi[i];
				ScriptPtr  *ptr2 = &Scripts[i];

				ptr2->Number = LittleShort(ptr1->Number);
				ptr2->Type = BYTE(LittleShort(ptr1->Type));
				ptr2->ArgCount = LittleLong(ptr1->ArgCount);
				ptr2->Address = LittleLong(ptr1->Address);
			}
		}
		else
		{
			NumScripts = LittleLong(scripts.dw[1]) / 8;
			Scripts = new ScriptPtr[NumScripts];
			scripts.dw += 2;

			for (i = 0; i < NumScripts; ++i)
			{
				ScriptPtr3 *ptr1 = &scripts.pe[i];
				ScriptPtr  *ptr2 = &Scripts[i];

				ptr2->Number = LittleShort(ptr1->Number);
				ptr2->Type = ptr1->Type;
				ptr2->ArgCount = ptr1->ArgCount;
				ptr2->Address = LittleLong(ptr1->Address);
			}
		}
		break;

	default:
		break;
	}

// [EP] Clang 3.5.0 optimizer miscompiles this function and causes random
// crashes in the program. This is fixed in 3.5.1 onwards.
#if defined(__clang__) && __clang_major__ == 3 && __clang_minor__ == 5 && __clang_patchlevel__ == 0
	asm("" : "+g" (NumScripts));
#endif
	for (i = 0; i < NumScripts; ++i)
	{
		Scripts[i].Flags = 0;
		Scripts[i].VarCount = LOCAL_SIZE;
	}

	// Sort scripts, so we can use a binary search to find them
	if (NumScripts > 1)
	{
		qsort (Scripts, NumScripts, sizeof(ScriptPtr), SortScripts);
		// Check for duplicates because ACC originally did not enforce
		// script number uniqueness across different script types. We
		// only need to do this for old format lumps, because the ACCs
		// that produce new format lumps won't let you do this.
		if (Format == ACS_Old)
		{
			for (i = 0; i < NumScripts - 1; ++i)
			{
				if (Scripts[i].Number == Scripts[i+1].Number)
				{
					Printf(TEXTCOLOR_ORANGE "%s appears more than once.\n",
						ScriptPresentation(Scripts[i].Number).GetChars());
					// Make the closed version the first one.
					if (Scripts[i+1].Type == SCRIPT_Closed)
					{
						swapvalues(Scripts[i], Scripts[i+1]);
					}
				}
			}
		}
	}

	if (Format == ACS_Old)
		return;

	// Load script flags
	scripts.b = FindChunk (MAKE_ID('S','F','L','G'));
	if (scripts.dw != NULL)
	{
		max = LittleLong(scripts.dw[1]) / 4;
		scripts.dw += 2;
		for (i = max; i > 0; --i, scripts.w += 2)
		{
			ScriptPtr *ptr = const_cast<ScriptPtr *>(FindScript (LittleShort(scripts.sw[0])));
			if (ptr != NULL)
			{
				ptr->Flags = LittleShort(scripts.w[1]);
			}
		}
	}

	// Load script var counts. (Only recorded for scripts that use more than LOCAL_SIZE variables.)
	scripts.b = FindChunk (MAKE_ID('S','V','C','T'));
	if (scripts.dw != NULL)
	{
		max = LittleLong(scripts.dw[1]) / 4;
		scripts.dw += 2;
		for (i = max; i > 0; --i, scripts.w += 2)
		{
			ScriptPtr *ptr = const_cast<ScriptPtr *>(FindScript (LittleShort(scripts.sw[0])));
			if (ptr != NULL)
			{
				ptr->VarCount = LittleShort(scripts.w[1]);
			}
		}
	}

	// Load script array sizes. (One chunk per script that uses arrays.)
	for (scripts.b = FindChunk(MAKE_ID('S','A','R','Y')); scripts.dw != NULL; scripts.b = NextChunk(scripts.b))
	{
		int size = LittleLong(scripts.dw[1]);
		if (size >= 6)
		{
			int script_num = LittleShort(scripts.sw[4]);
			ScriptPtr *ptr = const_cast<ScriptPtr *>(FindScript(script_num));
			if (ptr != NULL)
			{
				ptr->VarCount = ParseLocalArrayChunk(scripts.b, &ptr->LocalArrays, ptr->VarCount);
			}
		}
	}

	// Load script names (if any)
	scripts.b = FindChunk(MAKE_ID('S','N','A','M'));
	if (scripts.dw != NULL)
	{
		UnescapeStringTable(scripts.b + 8, NULL, false);
		for (i = 0; i < NumScripts; ++i)
		{
			// ACC stores script names as an index into the SNAM chunk, with the first index as
			// -1 and counting down from there. We convert this from an index into SNAM into
			// a negative index into the global name table.
			if (Scripts[i].Number < 0)
			{
				const char *str = (const char *)(scripts.b + 8 + scripts.dw[3 + (-Scripts[i].Number - 1)]);
				FName name(str);
				Scripts[i].Number = -name;
			}
		}
		// We need to resort scripts, because the new numbers for named scripts likely
		// do not match the order they were originally in.
		qsort (Scripts, NumScripts, sizeof(ScriptPtr), SortScripts);
	}
}

int FBehavior::SortScripts (const void *a, const void *b)
{
	ScriptPtr *ptr1 = (ScriptPtr *)a;
	ScriptPtr *ptr2 = (ScriptPtr *)b;
	return ptr1->Number - ptr2->Number;
}

//============================================================================
//
// FBehavior :: UnencryptStrings
//
// Descrambles strings in a STRE chunk to transform it into a STRL chunk.
//
//============================================================================

void FBehavior::UnencryptStrings ()
{
	DWORD *prevchunk = NULL;
	DWORD *chunk = (DWORD *)FindChunk(MAKE_ID('S','T','R','E'));
	while (chunk != NULL)
	{
		for (DWORD strnum = 0; strnum < LittleLong(chunk[3]); ++strnum)
		{
			int ofs = LittleLong(chunk[5+strnum]);
			BYTE *data = (BYTE *)chunk + ofs + 8, last;
			int p = (BYTE)(ofs*157135);
			int i = 0;
			do
			{
				last = (data[i] ^= (BYTE)(p+(i>>1)));
				++i;
			} while (last != 0);
		}
		prevchunk = chunk;
		chunk = (DWORD *)NextChunk ((BYTE *)chunk);
		*prevchunk = MAKE_ID('S','T','R','L');
	}
	if (prevchunk != NULL)
	{
		*prevchunk = MAKE_ID('S','T','R','L');
	}
}

//============================================================================
//
// FBehavior :: UnescapeStringTable
//
// Processes escape sequences for every string in a string table.
// Chunkstart points to the string table. Datastart points to the base address
// for offsets in the string table; if NULL, it will use chunkstart. If
// has_padding is true, then this is a STRL chunk with four bytes of padding
// on either side of the string count.
//
//============================================================================

void FBehavior::UnescapeStringTable(BYTE *chunkstart, BYTE *datastart, bool has_padding)
{
	assert(chunkstart != NULL);

	DWORD *chunk = (DWORD *)chunkstart;

	if (datastart == NULL)
	{
		datastart = chunkstart;
	}
	if (!has_padding)
	{
		chunk[0] = LittleLong(chunk[0]);
		for (DWORD strnum = 0; strnum < chunk[0]; ++strnum)
		{
			int ofs = LittleLong(chunk[1 + strnum]);	// Byte swap offset, if needed.
			chunk[1 + strnum] = ofs;
			strbin((char *)datastart + ofs);
		}
	}
	else
	{
		chunk[1] = LittleLong(chunk[1]);
		for (DWORD strnum = 0; strnum < chunk[1]; ++strnum)
		{
			int ofs = LittleLong(chunk[3 + strnum]);	// Byte swap offset, if needed.
			chunk[3 + strnum] = ofs;
			strbin((char *)datastart + ofs);
		}
	}
}

//============================================================================
//
// FBehavior :: IsGood
//
//============================================================================

bool FBehavior::IsGood ()
{
	bool bad;
	int i;

	// Check that the data format was understood
	if (Format == ACS_Unknown)
	{
		return false;
	}

	// Check that all functions are resolved
	bad = false;
	for (i = 0; i < NumFunctions; ++i)
	{
		ScriptFunction *funcdef = (ScriptFunction *)Functions + i;
		if (funcdef->Address == 0 && funcdef->ImportNum == 0)
		{
			DWORD *chunk = (DWORD *)FindChunk (MAKE_ID('F','N','A','M'));
			Printf (TEXTCOLOR_RED "Could not find ACS function %s for use in %s.\n",
				(char *)(chunk + 2) + chunk[3+i], ModuleName);
			bad = true;
		}
	}

	// Check that all imported modules were loaded
	for (i = Imports.Size() - 1; i >= 0; --i)
	{
		if (Imports[i] == NULL)
		{
			Printf (TEXTCOLOR_RED "Not all the libraries used by %s could be found.\n", ModuleName);
			return false;
		}
	}

	return !bad;
}

const ScriptPtr *FBehavior::FindScript (int script) const
{
	const ScriptPtr *ptr = BinarySearch<ScriptPtr, int>
		((ScriptPtr *)Scripts, NumScripts, &ScriptPtr::Number, script);

	// If the preceding script has the same number, return it instead.
	// See the note by the script sorting above for why.
	if (ptr > Scripts)
	{
		if (ptr[-1].Number == script)
		{
			ptr--;
		}
	}
	return ptr;
}

const ScriptPtr *FBehavior::StaticFindScript (int script, FBehavior *&module)
{
	for (DWORD i = 0; i < StaticModules.Size(); ++i)
	{
		const ScriptPtr *code = StaticModules[i]->FindScript (script);
		if (code != NULL)
		{
			module = StaticModules[i];
			return code;
		}
	}
	return NULL;
}

ScriptFunction *FBehavior::GetFunction (int funcnum, FBehavior *&module) const
{
	if ((unsigned)funcnum >= (unsigned)NumFunctions)
	{
		return NULL;
	}
	ScriptFunction *funcdef = (ScriptFunction *)Functions + funcnum;
	if (funcdef->ImportNum)
	{
		return Imports[funcdef->ImportNum - 1]->GetFunction (funcdef->Address, module);
	}
	// Should I just un-const this function instead of using a const_cast?
	module = const_cast<FBehavior *>(this);
	return funcdef;
}

int FBehavior::FindFunctionName (const char *funcname) const
{
	return FindStringInChunk ((DWORD *)FindChunk (MAKE_ID('F','N','A','M')), funcname);
}

int FBehavior::FindMapVarName (const char *varname) const
{
	return FindStringInChunk ((DWORD *)FindChunk (MAKE_ID('M','E','X','P')), varname);
}

int FBehavior::FindMapArray (const char *arrayname) const
{
	int var = FindMapVarName (arrayname);
	if (var >= 0)
	{
		return MapVarStore[var];
	}
	return -1;
}

int FBehavior::FindStringInChunk (DWORD *names, const char *varname) const
{
	if (names != NULL)
	{
		DWORD i;

		for (i = 0; i < LittleLong(names[2]); ++i)
		{
			if (stricmp (varname, (char *)(names + 2) + LittleLong(names[3+i])) == 0)
			{
				return (int)i;
			}
		}
	}
	return -1;
}

int FBehavior::GetArrayVal (int arraynum, int index) const
{
	if ((unsigned)arraynum >= (unsigned)NumTotalArrays)
		return 0;
	const ArrayInfo *array = Arrays[arraynum];
	if ((unsigned)index >= (unsigned)array->ArraySize)
		return 0;
	return array->Elements[index];
}

void FBehavior::SetArrayVal (int arraynum, int index, int value)
{
	if ((unsigned)arraynum >= (unsigned)NumTotalArrays)
		return;
	const ArrayInfo *array = Arrays[arraynum];
	if ((unsigned)index >= (unsigned)array->ArraySize)
		return;
	array->Elements[index] = value;
}

inline bool FBehavior::CopyStringToArray(int arraynum, int index, int maxLength, const char *string)
{
	 // false if the operation was incomplete or unsuccessful

	if ((unsigned)arraynum >= (unsigned)NumTotalArrays || index < 0)
		return false;
	const ArrayInfo *array = Arrays[arraynum];
	
	if ((signed)array->ArraySize - index < maxLength) maxLength = (signed)array->ArraySize - index;

	while (maxLength-- > 0)
	{
		array->Elements[index++] = *string;
		if (!(*string)) return true; // written terminating 0
		string++;
	}
	return !(*string); // return true if only terminating 0 was not written
}

BYTE *FBehavior::FindChunk (DWORD id) const
{
	BYTE *chunk = Chunks;

	while (chunk != NULL && chunk < Data + DataSize)
	{
		if (((DWORD *)chunk)[0] == id)
		{
			return chunk;
		}
		chunk += LittleLong(((DWORD *)chunk)[1]) + 8;
	}
	return NULL;
}

BYTE *FBehavior::NextChunk (BYTE *chunk) const
{
	DWORD id = *(DWORD *)chunk;
	chunk += LittleLong(((DWORD *)chunk)[1]) + 8;
	while (chunk != NULL && chunk < Data + DataSize)
	{
		if (((DWORD *)chunk)[0] == id)
		{
			return chunk;
		}
		chunk += LittleLong(((DWORD *)chunk)[1]) + 8;
	}
	return NULL;
}

const char *FBehavior::StaticLookupString (DWORD index)
{
	DWORD lib = index >> LIBRARYID_SHIFT;

	if (lib == STRPOOL_LIBRARYID)
	{
		return GlobalACSStrings.GetString(index);
	}
	if (lib >= (DWORD)StaticModules.Size())
	{
		return NULL;
	}
	return StaticModules[lib]->LookupString (index & 0xffff);
}

const char *FBehavior::LookupString (DWORD index) const
{
	if (StringTable == 0)
	{
		return NULL;
	}
	if (Format == ACS_Old)
	{
		DWORD *list = (DWORD *)(Data + StringTable);

		if (index >= list[0])
			return NULL;	// Out of range for this list;
		return (const char *)(Data + list[1+index]);
	}
	else
	{
		DWORD *list = (DWORD *)(Data + StringTable);

		if (index >= list[1])
			return NULL;	// Out of range for this list
		return (const char *)(Data + StringTable + list[3+index]);
	}
}

void FBehavior::StaticStartTypedScripts (WORD type, AActor *activator, bool always, int arg1, bool runNow)
{
	static const char *const TypeNames[] =
	{
		"Closed",
		"Open",
		"Respawn",
		"Death",
		"Enter",
		"Pickup",
		"BlueReturn",
		"RedReturn",
		"WhiteReturn",
		"Unknown", "Unknown", "Unknown",
		"Lightning",
		"Unloading",
		"Disconnect",
		"Return"
	};
	DPrintf("Starting all scripts of type %d (%s)\n", type,
		type < countof(TypeNames) ? TypeNames[type] : TypeNames[SCRIPT_Lightning - 1]);
	for (unsigned int i = 0; i < StaticModules.Size(); ++i)
	{
		StaticModules[i]->StartTypedScripts (type, activator, always, arg1, runNow);
	}
}

void FBehavior::StartTypedScripts (WORD type, AActor *activator, bool always, int arg1, bool runNow)
{
	const ScriptPtr *ptr;
	int i;

	for (i = 0; i < NumScripts; ++i)
	{
		ptr = &Scripts[i];
		if (ptr->Type == type)
		{
			DLevelScript *runningScript = P_GetScriptGoing (activator, NULL, ptr->Number,
				ptr, this, &arg1, 1, always ? ACS_ALWAYS : 0);
			if (runNow)
			{
				runningScript->RunScript ();
			}
		}
	}
}

// FBehavior :: StaticStopMyScripts
//
// Stops any scripts started by the specified actor. Used by the net code
// when a player disconnects. Should this be used in general whenever an
// actor is destroyed?

void FBehavior::StaticStopMyScripts (AActor *actor)
{
	DACSThinker *controller = DACSThinker::ActiveThinker;

	if (controller != NULL)
	{
		controller->StopScriptsFor (actor);
	}
}

//==========================================================================
//
// P_SerializeACSScriptNumber
//
// Serializes a script number. If it's negative, it's really a name, so
// that will get serialized after it.
//
//==========================================================================

void P_SerializeACSScriptNumber(FArchive &arc, int &scriptnum, bool was2byte)
{
	arc << scriptnum;
	// If the script number is negative, then it's really a name.
	// So read/store the name after it.
	if (scriptnum < 0)
	{
		if (arc.IsStoring())
		{
			arc.WriteName(FName(ENamedName(-scriptnum)).GetChars());
		}
		else
		{
			const char *nam = arc.ReadName();
			scriptnum = -FName(nam);
		}
	}
}

//---- The ACS Interpreter ----//

IMPLEMENT_POINTY_CLASS (DACSThinker)
 DECLARE_POINTER(LastScript)
 DECLARE_POINTER(Scripts)
END_POINTERS

TObjPtr<DACSThinker> DACSThinker::ActiveThinker;

DACSThinker::DACSThinker ()
: DThinker(STAT_SCRIPTS)
{
	if (ActiveThinker)
	{
		I_Error ("Only one ACSThinker is allowed to exist at a time.\nCheck your code.");
	}
	else
	{
		ActiveThinker = this;
		Scripts = NULL;
		LastScript = NULL;
		RunningScripts.Clear();
	}
}

DACSThinker::~DACSThinker ()
{
	Scripts = NULL;
	ActiveThinker = NULL;
}

void DACSThinker::Serialize (FArchive &arc)
{
	int scriptnum;
	int scriptcount = 0;

	Super::Serialize (arc);
	if (arc.IsStoring())
	{
		DLevelScript *script;
		script = Scripts;
		while (script)
		{
			scriptcount++;

			// We want to store this list backwards, so we can't loose the last pointer
			if (script->next == NULL)
				break;
			script = script->next;
		}
		arc << scriptcount;

		while (script)
		{
			arc << script;
			script = script->prev;
		}
	}
	else
	{
		// We are running through this list backwards, so the next entry is the last processed
		DLevelScript *next = NULL;
		arc << scriptcount;
		Scripts = NULL;
		LastScript = NULL;
		for (int i = 0; i < scriptcount; i++)
		{
			arc << Scripts;

			Scripts->next = next;
			Scripts->prev = NULL;
			if (next != NULL)
				next->prev = Scripts;

			next = Scripts;

			if (i == 0)
				LastScript = Scripts;
		}
	}
	if (arc.IsStoring ())
	{
		ScriptMap::Iterator it(RunningScripts);
		ScriptMap::Pair *pair;

		while (it.NextPair(pair))
		{
			assert(pair->Value != NULL);
			arc << pair->Value;
			scriptnum = pair->Key;
			P_SerializeACSScriptNumber(arc, scriptnum, true);
		}
		DLevelScript *nilptr = NULL;
		arc << nilptr;
	}
	else // Loading
	{
		DLevelScript *script = NULL;
		RunningScripts.Clear();

		arc << script;
		while (script)
		{
			P_SerializeACSScriptNumber(arc, scriptnum, true);
			RunningScripts[scriptnum] = script;
			arc << script;
		}
	}
}

void DACSThinker::Tick ()
{
	DLevelScript *script = Scripts;

	while (script)
	{
		DLevelScript *next = script->next;
		script->RunScript ();
		script = next;
	}

//	GlobalACSStrings.Clear();

	if (ACS_StringBuilderStack.Size())
	{
		int size = ACS_StringBuilderStack.Size();
		ACS_StringBuilderStack.Clear();
		I_Error("Error: %d garbage entries on ACS string builder stack.", size);
	}
}

void DACSThinker::StopScriptsFor (AActor *actor)
{
	DLevelScript *script = Scripts;

	while (script != NULL)
	{
		DLevelScript *next = script->next;
		if (script->activator == actor)
		{
			script->SetState (DLevelScript::SCRIPT_PleaseRemove);
		}
		script = next;
	}
}

IMPLEMENT_POINTY_CLASS (DLevelScript)
 DECLARE_POINTER(next)
 DECLARE_POINTER(prev)
 DECLARE_POINTER(activator)
END_POINTERS

inline FArchive &operator<< (FArchive &arc, DLevelScript::EScriptState &state)
{
	BYTE val = (BYTE)state;
	arc << val;
	state = (DLevelScript::EScriptState)val;
	return arc;
}

void DLevelScript::Serialize (FArchive &arc)
{
	DWORD i;

	Super::Serialize (arc);

	P_SerializeACSScriptNumber(arc, script, false);

	arc	<< state
		<< statedata
		<< activator
		<< activationline
		<< backSide
		<< numlocalvars;

	if (arc.IsLoading())
	{
		localvars = new SDWORD[numlocalvars];
	}
	for (i = 0; i < (DWORD)numlocalvars; i++)
	{
		arc << localvars[i];
	}

	if (arc.IsStoring ())
	{
		WORD lib = activeBehavior->GetLibraryID() >> LIBRARYID_SHIFT;
		arc << lib;
		i = activeBehavior->PC2Ofs (pc);
		arc << i;
	}
	else
	{
		WORD lib;
		arc << lib << i;
		activeBehavior = FBehavior::StaticGetModule (lib);
		pc = activeBehavior->Ofs2PC (i);
	}

	arc << activefont
		<< hudwidth << hudheight;
	arc << ClipRectLeft << ClipRectTop << ClipRectWidth << ClipRectHeight
		<< WrapWidth;
	arc << InModuleScriptNumber;
}

DLevelScript::DLevelScript ()
{
	next = prev = NULL;
	if (DACSThinker::ActiveThinker == NULL)
		new DACSThinker;
	activefont = SmallFont;
	localvars = NULL;
}

DLevelScript::~DLevelScript ()
{
	if (localvars != NULL)
		delete[] localvars;
	localvars = NULL;
}

void DLevelScript::Unlink ()
{
	DACSThinker *controller = DACSThinker::ActiveThinker;

	if (controller->LastScript == this)
	{
		controller->LastScript = prev;
		GC::WriteBarrier(controller, prev);
	}
	if (controller->Scripts == this)
	{
		controller->Scripts = next;
		GC::WriteBarrier(controller, next);
	}
	if (prev)
	{
		prev->next = next;
		GC::WriteBarrier(prev, next);
	}
	if (next)
	{
		next->prev = prev;
		GC::WriteBarrier(next, prev);
	}
}

void DLevelScript::Link ()
{
	DACSThinker *controller = DACSThinker::ActiveThinker;

	next = controller->Scripts;
	GC::WriteBarrier(this, next);
	if (controller->Scripts)
	{
		controller->Scripts->prev = this;
		GC::WriteBarrier(controller->Scripts, this);
	}
	prev = NULL;
	controller->Scripts = this;
	GC::WriteBarrier(controller, this);
	if (controller->LastScript == NULL)
	{
		controller->LastScript = this;
	}
}

void DLevelScript::PutLast ()
{
	DACSThinker *controller = DACSThinker::ActiveThinker;

	if (controller->LastScript == this)
		return;

	Unlink ();
	if (controller->Scripts == NULL)
	{
		Link ();
	}
	else
	{
		if (controller->LastScript)
			controller->LastScript->next = this;
		prev = controller->LastScript;
		next = NULL;
		controller->LastScript = this;
	}
}

void DLevelScript::PutFirst ()
{
	DACSThinker *controller = DACSThinker::ActiveThinker;

	if (controller->Scripts == this)
		return;

	Unlink ();
	Link ();
}

int DLevelScript::Random (int min, int max)
{
	if (max < min)
	{
		swapvalues (max, min);
	}

	return min + pr_acs(max - min + 1);
}

int DLevelScript::ThingCount (int type, int stringid, int tid, int tag)
{
	AActor *actor;
	PClassActor *kind;
	int count = 0;
	bool replacemented = false;

	if (type > 0)
	{
		kind = P_GetSpawnableType(type);
		if (kind == NULL)
			return 0;
	}
	else if (stringid >= 0)
	{
		const char *type_name = FBehavior::StaticLookupString (stringid);
		if (type_name == NULL)
			return 0;

		kind = PClass::FindActor(type_name);
		if (kind == NULL)
			return 0;
	}
	else
	{
		kind = NULL;
	}

do_count:
	if (tid)
	{
		FActorIterator iterator (tid);
		while ( (actor = iterator.Next ()) )
		{
			if (actor->health > 0 &&
				(kind == NULL || actor->IsA (kind)))
			{
				if (tag == -1 || tagManager.SectorHasTag(actor->Sector, tag))
				{
					// Don't count items in somebody's inventory
					if (!actor->IsKindOf (RUNTIME_CLASS(AInventory)) ||
						static_cast<AInventory *>(actor)->Owner == NULL)
					{
						count++;
					}
				}
			}
		}
	}
	else
	{
		TThinkerIterator<AActor> iterator;
		while ( (actor = iterator.Next ()) )
		{
			if (actor->health > 0 &&
				(kind == NULL || actor->IsA (kind)))
			{
				if (tag == -1 || tagManager.SectorHasTag(actor->Sector, tag))
				{
					// Don't count items in somebody's inventory
					if (!actor->IsKindOf (RUNTIME_CLASS(AInventory)) ||
						static_cast<AInventory *>(actor)->Owner == NULL)
					{
						count++;
					}
				}
			}
		}
	}
	if (!replacemented && kind != NULL)
	{
		// Again, with decorate replacements
		replacemented = true;
		PClassActor *newkind = kind->GetReplacement();
		if (newkind != kind)
		{
			kind = newkind;
			goto do_count;
		}
	}
	return count;
}

void DLevelScript::ChangeFlat (int tag, int name, bool floorOrCeiling)
{
	FTextureID flat;
	int secnum = -1;
	const char *flatname = FBehavior::StaticLookupString (name);

	if (flatname == NULL)
		return;

	flat = TexMan.GetTexture (flatname, FTexture::TEX_Flat, FTextureManager::TEXMAN_Overridable);

	FSectorTagIterator it(tag);
	while ((secnum = it.Next()) >= 0)
	{
		int pos = floorOrCeiling? sector_t::ceiling : sector_t::floor;
		sectors[secnum].SetTexture(pos, flat);
	}
}

int DLevelScript::CountPlayers ()
{
	int count = 0, i;

	for (i = 0; i < MAXPLAYERS; i++)
		if (playeringame[i])
			count++;

	return count;
}

void DLevelScript::SetLineTexture (int lineid, int side, int position, int name)
{
	FTextureID texture;
	int linenum = -1;
	const char *texname = FBehavior::StaticLookupString (name);

	if (texname == NULL)
		return;

	side = !!side;

	texture = TexMan.GetTexture (texname, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable);

	FLineIdIterator itr(lineid);
	while ((linenum = itr.Next()) >= 0)
	{
		side_t *sidedef;

		sidedef = lines[linenum].sidedef[side];
		if (sidedef == NULL)
			continue;

		switch (position)
		{
		case TEXTURE_TOP:
			sidedef->SetTexture(side_t::top, texture);
			break;
		case TEXTURE_MIDDLE:
			sidedef->SetTexture(side_t::mid, texture);
			break;
		case TEXTURE_BOTTOM:
			sidedef->SetTexture(side_t::bottom, texture);
			break;
		default:
			break;
		}

	}
}

void DLevelScript::ReplaceTextures (int fromnamei, int tonamei, int flags)
{
	const char *fromname = FBehavior::StaticLookupString (fromnamei);
	const char *toname = FBehavior::StaticLookupString (tonamei);
	FTextureID picnum1, picnum2;

	if (fromname == NULL)
		return;

	if ((flags ^ (NOT_BOTTOM | NOT_MIDDLE | NOT_TOP)) != 0)
	{
		picnum1 = TexMan.GetTexture (fromname, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable);
		picnum2 = TexMan.GetTexture (toname, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable);

		for (int i = 0; i < numsides; ++i)
		{
			side_t *wal = &sides[i];

			for(int j=0;j<3;j++)
			{
				static BYTE bits[]={NOT_TOP, NOT_MIDDLE, NOT_BOTTOM};
				if (!(flags & bits[j]) && wal->GetTexture(j) == picnum1)
				{
					wal->SetTexture(j, picnum2);
				}
			}
		}
	}
	if ((flags ^ (NOT_FLOOR | NOT_CEILING)) != 0)
	{
		picnum1 = TexMan.GetTexture (fromname, FTexture::TEX_Flat, FTextureManager::TEXMAN_Overridable);
		picnum2 = TexMan.GetTexture (toname, FTexture::TEX_Flat, FTextureManager::TEXMAN_Overridable);

		for (int i = 0; i < numsectors; ++i)
		{
			sector_t *sec = &sectors[i];

			if (!(flags & NOT_FLOOR) && sec->GetTexture(sector_t::floor) == picnum1) 
				sec->SetTexture(sector_t::floor, picnum2);
			if (!(flags & NOT_CEILING) && sec->GetTexture(sector_t::ceiling) == picnum1)	
				sec->SetTexture(sector_t::ceiling, picnum2);
		}
	}
}

int DLevelScript::DoSpawn (int type, const DVector3 &pos, int tid, DAngle angle, bool force)
{
	PClassActor *info = PClass::FindActor(FBehavior::StaticLookupString (type));
	AActor *actor = NULL;
	int spawncount = 0;

	if (info != NULL)
	{
		info = info->GetReplacement ();

		if ((GetDefaultByType (info)->flags3 & MF3_ISMONSTER) &&
			((dmflags & DF_NO_MONSTERS) || (level.flags2 & LEVEL2_NOMONSTERS)))
		{
			return 0;
		}

		actor = Spawn (info, pos, ALLOW_REPLACE);
		if (actor != NULL)
		{
			ActorFlags2 oldFlags2 = actor->flags2;
			actor->flags2 |= MF2_PASSMOBJ;
			if (force || P_TestMobjLocation (actor))
			{
				actor->Angles.Yaw = angle;
				actor->tid = tid;
				actor->AddToHash ();
				if (actor->flags & MF_SPECIAL)
					actor->flags |= MF_DROPPED;  // Don't respawn
				actor->flags2 = oldFlags2;
				spawncount++;
			}
			else
			{
				// If this is a monster, subtract it from the total monster
				// count, because it already added to it during spawning.
				actor->ClearCounters();
				actor->Destroy ();
				actor = NULL;
			}
		}
	}
	return spawncount;
}

int DLevelScript::DoSpawn(int type, int x, int y, int z, int tid, int angle, bool force)
{
	return DoSpawn(type, DVector3(ACSToDouble(x), ACSToDouble(y), ACSToDouble(z)), tid, angle * (360. / 256), force);
}


int DLevelScript::DoSpawnSpot (int type, int spot, int tid, int angle, bool force)
{
	int spawned = 0;

	if (spot != 0)
	{
		FActorIterator iterator (spot);
		AActor *aspot;

		while ( (aspot = iterator.Next ()) )
		{
			spawned += DoSpawn (type, aspot->Pos(), tid, angle * (360. / 256), force);
		}
	}
	else if (activator != NULL)
	{
		spawned += DoSpawn (type, activator->Pos(), tid, angle * (360. / 256), force);
	}
	return spawned;
}

int DLevelScript::DoSpawnSpotFacing (int type, int spot, int tid, bool force)
{
	int spawned = 0;

	if (spot != 0)
	{
		FActorIterator iterator (spot);
		AActor *aspot;

		while ( (aspot = iterator.Next ()) )
		{
			spawned += DoSpawn (type, aspot->Pos(), tid, aspot->Angles.Yaw, force);
		}
	}
	else if (activator != NULL)
	{
			spawned += DoSpawn (type, activator->Pos(), tid, activator->Angles.Yaw, force);
	}
	return spawned;
}

void DLevelScript::DoFadeTo (int r, int g, int b, int a, int time)
{
	DoFadeRange (0, 0, 0, -1, clamp(r, 0, 255), clamp(g, 0, 255), clamp(b, 0, 255), clamp(a, 0, 65536), time);
}

void DLevelScript::DoFadeRange (int r1, int g1, int b1, int a1,
								int r2, int g2, int b2, int a2, int time)
{
	player_t *viewer;
	float ftime = (float)time / 65536.f;
	bool fadingFrom = a1 >= 0;
	float fr1 = 0, fg1 = 0, fb1 = 0, fa1 = 0;
	float fr2, fg2, fb2, fa2;
	int i;

	fr2 = (float)r2 / 255.f;
	fg2 = (float)g2 / 255.f;
	fb2 = (float)b2 / 255.f;
	fa2 = (float)a2 / 65536.f;

	if (fadingFrom)
	{
		fr1 = (float)r1 / 255.f;
		fg1 = (float)g1 / 255.f;
		fb1 = (float)b1 / 255.f;
		fa1 = (float)a1 / 65536.f;
	}

	if (activator != NULL)
	{
		viewer = activator->player;
		if (viewer == NULL)
			return;
		i = MAXPLAYERS;
		goto showme;
	}
	else
	{
		for (i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i])
			{
				viewer = &players[i];
showme:
				if (ftime <= 0.f)
				{
					viewer->BlendR = fr2;
					viewer->BlendG = fg2;
					viewer->BlendB = fb2;
					viewer->BlendA = fa2;
				}
				else
				{
					if (!fadingFrom)
					{
						if (viewer->BlendA <= 0.f)
						{
							fr1 = fr2;
							fg1 = fg2;
							fb1 = fb2;
							fa1 = 0.f;
						}
						else
						{
							fr1 = viewer->BlendR;
							fg1 = viewer->BlendG;
							fb1 = viewer->BlendB;
							fa1 = viewer->BlendA;
						}
					}
					new DFlashFader (fr1, fg1, fb1, fa1, fr2, fg2, fb2, fa2, ftime, viewer->mo);
				}
			}
		}
	}
}

void DLevelScript::DoSetFont (int fontnum)
{
	const char *fontname = FBehavior::StaticLookupString (fontnum);
	activefont = V_GetFont (fontname);
	if (activefont == NULL)
	{
		activefont = SmallFont;
	}
}

int DoSetMaster (AActor *self, AActor *master)
{
    AActor *defs;
    if (self->flags3&MF3_ISMONSTER)
    {
        if (master)
        {
            if (master->flags3&MF3_ISMONSTER)
            {
                self->FriendPlayer = 0;
                self->master = master;
                level.total_monsters -= self->CountsAsKill();
                self->flags = (self->flags & ~MF_FRIENDLY) | (master->flags & MF_FRIENDLY);
                level.total_monsters += self->CountsAsKill();
                // Don't attack your new master
                if (self->target == self->master) self->target = NULL;
                if (self->lastenemy == self->master) self->lastenemy = NULL;
                if (self->LastHeard == self->master) self->LastHeard = NULL;
                return 1;
            }
            else if (master->player)
            {
                // [KS] Be friendly to this player
                self->master = NULL;
                level.total_monsters -= self->CountsAsKill();
                self->flags|=MF_FRIENDLY;
                self->SetFriendPlayer(master->player);

                AActor * attacker=master->player->attacker;
                if (attacker)
                {
                    if (!(attacker->flags&MF_FRIENDLY) || 
                        (deathmatch && attacker->FriendPlayer!=0 && attacker->FriendPlayer!=self->FriendPlayer))
                    {
                        self->LastHeard = self->target = attacker;
                    }
                }
                // And stop attacking him if necessary.
                if (self->target == master) self->target = NULL;
                if (self->lastenemy == master) self->lastenemy = NULL;
                if (self->LastHeard == master) self->LastHeard = NULL;
                return 1;
            }
        }
        else
        {
            self->master = NULL;
            self->FriendPlayer = 0;
            // Go back to whatever friendliness we usually have...
            defs = self->GetDefault();
            level.total_monsters -= self->CountsAsKill();
            self->flags = (self->flags & ~MF_FRIENDLY) | (defs->flags & MF_FRIENDLY);
            level.total_monsters += self->CountsAsKill();
            // ...And re-side with our friends.
            if (self->target && !self->IsHostile (self->target)) self->target = NULL;
            if (self->lastenemy && !self->IsHostile (self->lastenemy)) self->lastenemy = NULL;
            if (self->LastHeard && !self->IsHostile (self->LastHeard)) self->LastHeard = NULL;
            return 1;
        }
    }
    return 0;
}

int DoGetMasterTID (AActor *self)
{
	if (self->master) return self->master->tid;
	else if (self->FriendPlayer)
	{
		player_t *player = &players[(self->FriendPlayer)-1];
		return player->mo->tid;
	}
	else return 0;
}

AActor *SingleActorFromTID (int tid, AActor *defactor)
{
	if (tid == 0)
	{
		return defactor;
	}
	else
	{
		FActorIterator iterator (tid);
		return iterator.Next();
	}
}

enum
{
	APROP_Health		= 0,
	APROP_Speed			= 1,
	APROP_Damage		= 2,
	APROP_Alpha			= 3,
	APROP_RenderStyle	= 4,
	APROP_SeeSound		= 5,	// Sounds can only be set, not gotten
	APROP_AttackSound	= 6,
	APROP_PainSound		= 7,
	APROP_DeathSound	= 8,
	APROP_ActiveSound	= 9,
	APROP_Ambush		= 10,
	APROP_Invulnerable	= 11,
	APROP_JumpZ			= 12,	// [GRB]
	APROP_ChaseGoal		= 13,
	APROP_Frightened	= 14,
	APROP_Gravity		= 15,
	APROP_Friendly		= 16,
	APROP_SpawnHealth   = 17,
	APROP_Dropped		= 18,
	APROP_Notarget		= 19,
	APROP_Species		= 20,
	APROP_NameTag		= 21,
	APROP_Score			= 22,
	APROP_Notrigger		= 23,
	APROP_DamageFactor	= 24,
	APROP_MasterTID     = 25,
	APROP_TargetTID		= 26,
	APROP_TracerTID		= 27,
	APROP_WaterLevel	= 28,
	APROP_ScaleX        = 29,
	APROP_ScaleY        = 30,
	APROP_Dormant		= 31,
	APROP_Mass			= 32,
	APROP_Accuracy      = 33,
	APROP_Stamina       = 34,
	APROP_Height		= 35,
	APROP_Radius		= 36,
	APROP_ReactionTime  = 37,
	APROP_MeleeRange	= 38,
	APROP_ViewHeight	= 39,
	APROP_AttackZOffset	= 40,
	APROP_StencilColor	= 41,
	APROP_Friction		= 42,
	APROP_DamageMultiplier=43,
	APROP_MaxStepHeight	= 44,
	APROP_MaxDropOffHeight= 45,
	APROP_DamageType	= 46,
};

// These are needed for ACS's APROP_RenderStyle
static const int LegacyRenderStyleIndices[] =
{
	0,	// STYLE_None,
	1,  // STYLE_Normal,
	2,  // STYLE_Fuzzy,
	3,	// STYLE_SoulTrans,
	4,	// STYLE_OptFuzzy,
	5,	// STYLE_Stencil,
	64,	// STYLE_Translucent
	65,	// STYLE_Add,
	66,	// STYLE_Shaded,
	67,	// STYLE_TranslucentStencil,
	68,	// STYLE_Shadow,
	69,	// STYLE_Subtract,
	6,	// STYLE_AddStencil
	7,	// STYLE_AddShaded
	-1
};

void DLevelScript::SetActorProperty (int tid, int property, int value)
{
	if (tid == 0)
	{
		DoSetActorProperty (activator, property, value);
	}
	else
	{
		AActor *actor;
		FActorIterator iterator (tid);

		while ((actor = iterator.Next()) != NULL)
		{
			DoSetActorProperty (actor, property, value);
		}
	}
}

void DLevelScript::DoSetActorProperty (AActor *actor, int property, int value)
{
	if (actor == NULL)
	{
		return;
	}
	switch (property)
	{
	case APROP_Health:
		// Don't alter the health of dead things.
		if (actor->health <= 0 || (actor->player != NULL && actor->player->playerstate == PST_DEAD))
		{
			break;
		}
		actor->health = value;
		if (actor->player != NULL)
		{
			actor->player->health = value;
		}
		// If the health is set to a non-positive value, properly kill the actor.
		if (value <= 0)
		{
			actor->Die(activator, activator);
		}
		break;

	case APROP_Speed:
		actor->Speed = ACSToDouble(value);
		break;

	case APROP_Damage:
		actor->Damage = CreateDamageFunction(value);
		break;

	case APROP_Alpha:
		actor->Alpha = ACSToDouble(value);
		break;

	case APROP_RenderStyle:
		for(int i=0; LegacyRenderStyleIndices[i] >= 0; i++)
		{
			if (LegacyRenderStyleIndices[i] == value)
			{
				actor->RenderStyle = ERenderStyle(i);
				break;
			}
		}
		break;

	case APROP_Ambush:
		if (value) actor->flags |= MF_AMBUSH; else actor->flags &= ~MF_AMBUSH;
		break;

	case APROP_Dropped:
		if (value) actor->flags |= MF_DROPPED; else actor->flags &= ~MF_DROPPED;
		break;

	case APROP_Invulnerable:
		if (value) actor->flags2 |= MF2_INVULNERABLE; else actor->flags2 &= ~MF2_INVULNERABLE;
		break;

	case APROP_Notarget:
		if (value) actor->flags3 |= MF3_NOTARGET; else actor->flags3 &= ~MF3_NOTARGET;
		break;

	case APROP_Notrigger:
		if (value) actor->flags6 |= MF6_NOTRIGGER; else actor->flags6 &= ~MF6_NOTRIGGER;
		break;

	case APROP_JumpZ:
		if (actor->IsKindOf (RUNTIME_CLASS (APlayerPawn)))
			static_cast<APlayerPawn *>(actor)->JumpZ = ACSToDouble(value);
		break; 	// [GRB]

	case APROP_ChaseGoal:
		if (value)
			actor->flags5 |= MF5_CHASEGOAL;
		else
			actor->flags5 &= ~MF5_CHASEGOAL;
		break;

	case APROP_Frightened:
		if (value)
			actor->flags4 |= MF4_FRIGHTENED;
		else
			actor->flags4 &= ~MF4_FRIGHTENED;
		break;

	case APROP_Friendly:
		if (actor->CountsAsKill()) level.total_monsters--;
		if (value)
		{
			actor->flags |= MF_FRIENDLY;
		}
		else
		{
			actor->flags &= ~MF_FRIENDLY;
		}
		if (actor->CountsAsKill()) level.total_monsters++;
		break;


	case APROP_SpawnHealth:
		if (actor->IsKindOf (RUNTIME_CLASS (APlayerPawn)))
		{
			static_cast<APlayerPawn *>(actor)->MaxHealth = value;
		}
		break;

	case APROP_Gravity:
		actor->Gravity = ACSToDouble(value);
		break;

	case APROP_SeeSound:
		actor->SeeSound = FBehavior::StaticLookupString(value);
		break;

	case APROP_AttackSound:
		actor->AttackSound = FBehavior::StaticLookupString(value);
		break;

	case APROP_PainSound:
		actor->PainSound = FBehavior::StaticLookupString(value);
		break;

	case APROP_DeathSound:
		actor->DeathSound = FBehavior::StaticLookupString(value);
		break;

	case APROP_ActiveSound:
		actor->ActiveSound = FBehavior::StaticLookupString(value);
		break;

	case APROP_Species:
		actor->Species = FBehavior::StaticLookupString(value);
		break;

	case APROP_Score:
		actor->Score = value;
		break;

	case APROP_NameTag:
		actor->SetTag(FBehavior::StaticLookupString(value));
		break;

	case APROP_DamageFactor:
		actor->DamageFactor = ACSToDouble(value);
		break;

	case APROP_DamageMultiplier:
		actor->DamageMultiply = ACSToDouble(value);
		break;

	case APROP_MasterTID:
		AActor *other;
		other = SingleActorFromTID (value, NULL);
		DoSetMaster (actor, other);
		break;

	case APROP_ScaleX:
		actor->Scale.X = ACSToDouble(value);
		break;

	case APROP_ScaleY:
		actor->Scale.Y = ACSToDouble(value);
		break;

	case APROP_Mass:
		actor->Mass = value;
		break;

	case APROP_Accuracy:
		actor->accuracy = value;
		break;

	case APROP_Stamina:
		actor->stamina = value;
		break;

	case APROP_ReactionTime:
		actor->reactiontime = value;
		break;

	case APROP_MeleeRange:
		actor->meleerange = ACSToDouble(value);
		break;

	case APROP_ViewHeight:
		if (actor->IsKindOf (RUNTIME_CLASS (APlayerPawn)))
		{
			static_cast<APlayerPawn *>(actor)->ViewHeight = ACSToDouble(value);
			if (actor->player != NULL)
			{
				actor->player->viewheight = ACSToDouble(value);
			}
		}
		break;

	case APROP_AttackZOffset:
		if (actor->IsKindOf (RUNTIME_CLASS (APlayerPawn)))
			static_cast<APlayerPawn *>(actor)->AttackZOffset = ACSToDouble(value);
		break;

	case APROP_StencilColor:
		actor->SetShade(value);
		break;

	case APROP_Friction:
		actor->Friction = ACSToDouble(value);
		break;

	case APROP_MaxStepHeight:
		actor->MaxStepHeight = ACSToDouble(value);
		break;

	case APROP_MaxDropOffHeight:
		actor->MaxDropOffHeight = ACSToDouble(value);
		break;

	case APROP_DamageType:
		actor->DamageType = FBehavior::StaticLookupString(value);
		break;

	default:
		// do nothing.
		break;
	}
}

int DLevelScript::GetActorProperty (int tid, int property)
{
	AActor *actor = SingleActorFromTID (tid, activator);

	if (actor == NULL)
	{
		return 0;
	}
	switch (property)
	{
	case APROP_Health:		return actor->health;
	case APROP_Speed:		return DoubleToACS(actor->Speed);
	case APROP_Damage:		return actor->GetMissileDamage(0,1);
	case APROP_DamageFactor:return DoubleToACS(actor->DamageFactor);
	case APROP_DamageMultiplier: return DoubleToACS(actor->DamageMultiply);
	case APROP_Alpha:		return DoubleToACS(actor->Alpha);
	case APROP_RenderStyle:	for (int style = STYLE_None; style < STYLE_Count; ++style)
							{ // Check for a legacy render style that matches.
								if (LegacyRenderStyles[style] == actor->RenderStyle)
								{
									return LegacyRenderStyleIndices[style];
								}
							}
							// The current render style isn't expressable as a legacy style,
							// so pretends it's normal.
							return STYLE_Normal;
	case APROP_Gravity:		return DoubleToACS(actor->Gravity);
	case APROP_Invulnerable:return !!(actor->flags2 & MF2_INVULNERABLE);
	case APROP_Ambush:		return !!(actor->flags & MF_AMBUSH);
	case APROP_Dropped:		return !!(actor->flags & MF_DROPPED);
	case APROP_ChaseGoal:	return !!(actor->flags5 & MF5_CHASEGOAL);
	case APROP_Frightened:	return !!(actor->flags4 & MF4_FRIGHTENED);
	case APROP_Friendly:	return !!(actor->flags & MF_FRIENDLY);
	case APROP_Notarget:	return !!(actor->flags3 & MF3_NOTARGET);
	case APROP_Notrigger:	return !!(actor->flags6 & MF6_NOTRIGGER);
	case APROP_Dormant:		return !!(actor->flags2 & MF2_DORMANT);
	case APROP_SpawnHealth: if (actor->IsKindOf (RUNTIME_CLASS (APlayerPawn)))
							{
								return static_cast<APlayerPawn *>(actor)->MaxHealth;
							}
							else
							{
								return actor->SpawnHealth();
							}

	case APROP_JumpZ:		if (actor->IsKindOf (RUNTIME_CLASS (APlayerPawn)))
							{
								return DoubleToACS(static_cast<APlayerPawn *>(actor)->JumpZ);	// [GRB]
							}
							else
							{
								return 0;
							}
	case APROP_Score:		return actor->Score;
	case APROP_MasterTID:	return DoGetMasterTID (actor);
	case APROP_TargetTID:	return (actor->target != NULL)? actor->target->tid : 0;
	case APROP_TracerTID:	return (actor->tracer != NULL)? actor->tracer->tid : 0;
	case APROP_WaterLevel:	return actor->waterlevel;
	case APROP_ScaleX: 		return DoubleToACS(actor->Scale.X);
	case APROP_ScaleY: 		return DoubleToACS(actor->Scale.Y);
	case APROP_Mass: 		return actor->Mass;
	case APROP_Accuracy:    return actor->accuracy;
	case APROP_Stamina:     return actor->stamina;
	case APROP_Height:		return DoubleToACS(actor->Height);
	case APROP_Radius:		return DoubleToACS(actor->radius);
	case APROP_ReactionTime:return actor->reactiontime;
	case APROP_MeleeRange:	return DoubleToACS(actor->meleerange);
	case APROP_ViewHeight:	if (actor->IsKindOf (RUNTIME_CLASS (APlayerPawn)))
							{
								return DoubleToACS(static_cast<APlayerPawn *>(actor)->ViewHeight);
							}
							else
							{
								return 0;
							}
	case APROP_AttackZOffset:
							if (actor->IsKindOf (RUNTIME_CLASS (APlayerPawn)))
							{
								return DoubleToACS(static_cast<APlayerPawn *>(actor)->AttackZOffset);
							}
							else
							{
								return 0;
							}

	case APROP_SeeSound:	return GlobalACSStrings.AddString(actor->SeeSound);
	case APROP_AttackSound:	return GlobalACSStrings.AddString(actor->AttackSound);
	case APROP_PainSound:	return GlobalACSStrings.AddString(actor->PainSound);
	case APROP_DeathSound:	return GlobalACSStrings.AddString(actor->DeathSound);
	case APROP_ActiveSound:	return GlobalACSStrings.AddString(actor->ActiveSound);
	case APROP_Species:		return GlobalACSStrings.AddString(actor->GetSpecies());
	case APROP_NameTag:		return GlobalACSStrings.AddString(actor->GetTag());
	case APROP_StencilColor:return actor->fillcolor;
	case APROP_Friction:	return DoubleToACS(actor->Friction);
	case APROP_MaxStepHeight: return DoubleToACS(actor->MaxStepHeight);
	case APROP_MaxDropOffHeight: return DoubleToACS(actor->MaxDropOffHeight);
	case APROP_DamageType:	return GlobalACSStrings.AddString(actor->DamageType);

	default:				return 0;
	}
}



int DLevelScript::CheckActorProperty (int tid, int property, int value)
{
	AActor *actor = SingleActorFromTID (tid, activator);
	const char *string = NULL;
	if (actor == NULL)
	{
		return 0;
	}
	switch (property)
	{
		// Default
		default:				return 0;

		// Straightforward integer values:
		case APROP_Health:
		case APROP_Speed:
		case APROP_Damage:
		case APROP_DamageFactor:
		case APROP_Alpha:
		case APROP_RenderStyle:
		case APROP_Gravity:
		case APROP_SpawnHealth:
		case APROP_JumpZ:
		case APROP_Score:
		case APROP_MasterTID:
		case APROP_TargetTID:
		case APROP_TracerTID:
		case APROP_WaterLevel:
		case APROP_ScaleX:
		case APROP_ScaleY:
		case APROP_Mass:
		case APROP_Accuracy:
		case APROP_Stamina:
		case APROP_Height:
		case APROP_Radius:
		case APROP_ReactionTime:
		case APROP_MeleeRange:
		case APROP_ViewHeight:
		case APROP_AttackZOffset:
		case APROP_MaxStepHeight:
		case APROP_MaxDropOffHeight:
		case APROP_StencilColor:
			return (GetActorProperty(tid, property) == value);

		// Boolean values need to compare to a binary version of value
		case APROP_Ambush:
		case APROP_Invulnerable:
		case APROP_Dropped:
		case APROP_ChaseGoal:
		case APROP_Frightened:
		case APROP_Friendly:
		case APROP_Notarget:
		case APROP_Notrigger:
		case APROP_Dormant:
			return (GetActorProperty(tid, property) == (!!value));

		// Strings are covered by GetActorProperty, but they're fairly
		// heavy-duty, so make the check here.
		case APROP_SeeSound:	string = actor->SeeSound; break;
		case APROP_AttackSound:	string = actor->AttackSound; break;
		case APROP_PainSound:	string = actor->PainSound; break;
		case APROP_DeathSound:	string = actor->DeathSound; break;
		case APROP_ActiveSound:	string = actor->ActiveSound; break; 
		case APROP_Species:		string = actor->GetSpecies(); break;
		case APROP_NameTag:		string = actor->GetTag(); break;
		case APROP_DamageType:	string = actor->DamageType; break;
	}
	if (string == NULL) string = "";
	return (!stricmp(string, FBehavior::StaticLookupString(value)));
}

bool DLevelScript::DoCheckActorTexture(int tid, AActor *activator, int string, bool floor)
{
	AActor *actor = SingleActorFromTID(tid, activator);
	if (actor == NULL)
	{
		return 0;
	}
	FTexture *tex = TexMan.FindTexture(FBehavior::StaticLookupString(string), FTexture::TEX_Flat,
			FTextureManager::TEXMAN_Overridable|FTextureManager::TEXMAN_TryAny|FTextureManager::TEXMAN_DontCreate);

	if (tex == NULL)
	{ // If the texture we want to check against doesn't exist, then
	  // they're obviously not the same.
		return 0;
	}
	FTextureID secpic;
	sector_t *resultsec;
	F3DFloor *resffloor;

	if (floor)
	{
		actor->Sector->NextLowestFloorAt(actor->X(), actor->Y(), actor->Z(), 0, actor->MaxStepHeight, &resultsec, &resffloor);
		secpic = resffloor ? *resffloor->top.texture : resultsec->planes[sector_t::floor].Texture;
	}
	else
	{
		actor->Sector->NextHighestCeilingAt(actor->X(), actor->Y(), actor->Z(), actor->Top(), 0, &resultsec, &resffloor);
		secpic = resffloor ? *resffloor->bottom.texture : resultsec->planes[sector_t::ceiling].Texture;
	}
	return tex == TexMan[secpic];
}


int DLevelScript::GetPlayerInput(int playernum, int inputnum)
{
	player_t *p;

	if (playernum < 0)
	{
		if (activator == NULL)
		{
			return 0;
		}
		p = activator->player;
	}
	else if (playernum >= MAXPLAYERS || !playeringame[playernum])
	{
		return 0;
	}
	else
	{
		p = &players[playernum];
	}
	if (p == NULL)
	{
		return 0;
	}

	return P_Thing_CheckInputNum(p, inputnum);
}

enum
{
	ACTOR_NONE				= 0x00000000,
	ACTOR_WORLD				= 0x00000001,
	ACTOR_PLAYER			= 0x00000002,
	ACTOR_BOT				= 0x00000004,
	ACTOR_VOODOODOLL		= 0x00000008,
	ACTOR_MONSTER			= 0x00000010,
	ACTOR_ALIVE				= 0x00000020,
	ACTOR_DEAD				= 0x00000040,
	ACTOR_MISSILE			= 0x00000080,
	ACTOR_GENERIC			= 0x00000100
};

int DLevelScript::DoClassifyActor(int tid)
{
	AActor *actor;
	int classify;

	if (tid == 0)
	{
		actor = activator;
		if (actor == NULL)
		{
			return ACTOR_WORLD;
		}
	}
	else
	{
		FActorIterator it(tid);
		actor = it.Next();
	}
	if (actor == NULL)
	{
		return ACTOR_NONE;
	}

	classify = 0;
	if (actor->player != NULL)
	{
		classify |= ACTOR_PLAYER;
		if (actor->player->playerstate == PST_DEAD)
		{
			classify |= ACTOR_DEAD;
		}
		else
		{
			classify |= ACTOR_ALIVE;
		}
		if (actor->player->mo != actor)
		{
			classify |= ACTOR_VOODOODOLL;
		}
		if (actor->player->Bot != NULL)
		{
			classify |= ACTOR_BOT;
		}
	}
	else if (actor->flags3 & MF3_ISMONSTER)
	{
		classify |= ACTOR_MONSTER;
		if (actor->health <= 0)
		{
			classify |= ACTOR_DEAD;
		}
		else
		{
			classify |= ACTOR_ALIVE;
		}
	}
	else if (actor->flags & MF_MISSILE)
	{
		classify |= ACTOR_MISSILE;
	}
	else
	{
		classify |= ACTOR_GENERIC;
	}
	return classify;
}

enum
{
	SOUND_See,
	SOUND_Attack,
	SOUND_Pain,
	SOUND_Death,
	SOUND_Active,
	SOUND_Use,
	SOUND_Bounce,
	SOUND_WallBounce,
	SOUND_CrushPain,
	SOUND_Howl,
};

static FSoundID GetActorSound(const AActor *actor, int soundtype)
{
	switch (soundtype)
	{
	case SOUND_See:			return actor->SeeSound;
	case SOUND_Attack:		return actor->AttackSound;
	case SOUND_Pain:		return actor->PainSound;
	case SOUND_Death:		return actor->DeathSound;
	case SOUND_Active:		return actor->ActiveSound;
	case SOUND_Use:			return actor->UseSound;
	case SOUND_Bounce:		return actor->BounceSound;
	case SOUND_WallBounce:	return actor->WallBounceSound;
	case SOUND_CrushPain:	return actor->CrushPainSound;
	case SOUND_Howl:		return actor->GetClass()->HowlSound;
	default:				return 0;
	}
}

enum EACSFunctions
{
	ACSF_GetLineUDMFInt=1,
	ACSF_GetLineUDMFFixed,
	ACSF_GetThingUDMFInt,
	ACSF_GetThingUDMFFixed,
	ACSF_GetSectorUDMFInt,
	ACSF_GetSectorUDMFFixed,
	ACSF_GetSideUDMFInt,
	ACSF_GetSideUDMFFixed,
	ACSF_GetActorVelX,
	ACSF_GetActorVelY,
	ACSF_GetActorVelZ,
	ACSF_SetActivator,
	ACSF_SetActivatorToTarget,
	ACSF_GetActorViewHeight,
	ACSF_GetChar,
	ACSF_GetAirSupply,
	ACSF_SetAirSupply,
	ACSF_SetSkyScrollSpeed,
	ACSF_GetArmorType,
	ACSF_SpawnSpotForced,
	ACSF_SpawnSpotFacingForced,
	ACSF_CheckActorProperty,
    ACSF_SetActorVelocity,
	ACSF_SetUserVariable,
	ACSF_GetUserVariable,
	ACSF_Radius_Quake2,
	ACSF_CheckActorClass,
	ACSF_SetUserArray,
	ACSF_GetUserArray,
	ACSF_SoundSequenceOnActor,
	ACSF_SoundSequenceOnSector,
	ACSF_SoundSequenceOnPolyobj,
	ACSF_GetPolyobjX,
	ACSF_GetPolyobjY,
    ACSF_CheckSight,
	ACSF_SpawnForced,
	ACSF_AnnouncerSound,	// Skulltag
	ACSF_SetPointer,
	ACSF_ACS_NamedExecute,
	ACSF_ACS_NamedSuspend,
	ACSF_ACS_NamedTerminate,
	ACSF_ACS_NamedLockedExecute,
	ACSF_ACS_NamedLockedExecuteDoor,
	ACSF_ACS_NamedExecuteWithResult,
	ACSF_ACS_NamedExecuteAlways,
	ACSF_UniqueTID,
	ACSF_IsTIDUsed,
	ACSF_Sqrt,
	ACSF_FixedSqrt,
	ACSF_VectorLength,
	ACSF_SetHUDClipRect,
	ACSF_SetHUDWrapWidth,
	ACSF_SetCVar,
	ACSF_GetUserCVar,
	ACSF_SetUserCVar,
	ACSF_GetCVarString,
	ACSF_SetCVarString,
	ACSF_GetUserCVarString,
	ACSF_SetUserCVarString,
	ACSF_LineAttack,
	ACSF_PlaySound,
	ACSF_StopSound,
	ACSF_strcmp,
	ACSF_stricmp,
	ACSF_StrLeft,
	ACSF_StrRight,
	ACSF_StrMid,
	ACSF_GetActorClass,
	ACSF_GetWeapon,
	ACSF_SoundVolume,
	ACSF_PlayActorSound,
	ACSF_SpawnDecal,
	ACSF_CheckFont,
	ACSF_DropItem,
	ACSF_CheckFlag,
	ACSF_SetLineActivation,
	ACSF_GetLineActivation,
	ACSF_GetActorPowerupTics,
	ACSF_ChangeActorAngle,
	ACSF_ChangeActorPitch,		// 80
	ACSF_GetArmorInfo,
	ACSF_DropInventory,
	ACSF_PickActor,
	ACSF_IsPointerEqual,
	ACSF_CanRaiseActor,
	ACSF_SetActorTeleFog,		// 86
	ACSF_SwapActorTeleFog,
	ACSF_SetActorRoll,
	ACSF_ChangeActorRoll,
	ACSF_GetActorRoll,
	ACSF_QuakeEx,
	ACSF_Warp,					// 92
	ACSF_GetMaxInventory,
	ACSF_SetSectorDamage,
	ACSF_SetSectorTerrain,
	ACSF_SpawnParticle,
	ACSF_SetMusicVolume,
	ACSF_CheckProximity,
	// 1 more left...
	
	/* Zandronum's - these must be skipped when we reach 99!
	-100:ResetMap(0),
	-101 : PlayerIsSpectator(1),
	-102 : ConsolePlayerNumber(0),
	-103 : GetTeamProperty(2),
	-104 : GetPlayerLivesLeft(1),
	-105 : SetPlayerLivesLeft(2),
	-106 : KickFromGame(2),
	*/

	// ZDaemon
	ACSF_GetTeamScore = 19620,	// (int team)
	ACSF_SetTeamScore,			// (int team, int value)
};

int DLevelScript::SideFromID(int id, int side)
{
	if (side != 0 && side != 1) return -1;
	
	if (id == 0)
	{
		if (activationline == NULL) return -1;
		if (activationline->sidedef[side] == NULL) return -1;
		return activationline->sidedef[side]->Index;
	}
	else
	{
		int line = P_FindFirstLineFromID(id);
		if (line == -1) return -1;
		if (lines[line].sidedef[side] == NULL) return -1;
		return lines[line].sidedef[side]->Index;
	}
}

int DLevelScript::LineFromID(int id)
{
	if (id == 0)
	{
		if (activationline == NULL) return -1;
		return int(activationline - lines);
	}
	else
	{
		return P_FindFirstLineFromID(id);
	}
}

bool GetVarAddrType(AActor *self, FName varname, int index, void *&addr, PType *&type, bool readonly)
{
	PField *var = dyn_cast<PField>(self->GetClass()->Symbols.FindSymbol(varname, true));
	PArray *arraytype;

	if (var == NULL || (!readonly && (var->Flags & VARF_Native)))
	{
		return false;
	}
	type = var->Type;
	BYTE *baddr = reinterpret_cast<BYTE *>(self) + var->Offset;
	arraytype = dyn_cast<PArray>(type);
	if (arraytype != NULL)
	{
		// unwrap contained type
		type = arraytype->ElementType;
		// offset by index (if in bounds)
		if ((unsigned)index >= arraytype->ElementCount)
		{ // out of bounds
			return false;
		}
		baddr += arraytype->ElementSize * index;
	}
	else if (index != 0)
	{ // ignore attempts to set indexed values on non-arrays
		return false;
	}
	addr = baddr;
	// We don't want Int subclasses like Name or Color to be accessible,
	// but we do want to support Float subclasses like Fixed.
	if (!type->IsA(RUNTIME_CLASS(PInt)) && !type->IsKindOf(RUNTIME_CLASS(PFloat)))
	{
		// For reading, we also support Name and String types.
		if (readonly && (type->IsA(RUNTIME_CLASS(PName)) || type->IsA(RUNTIME_CLASS(PString))))
		{
			return true;
		}
		return false;
	}
	return true;
}

static void SetUserVariable(AActor *self, FName varname, int index, int value)
{
	void *addr;
	PType *type;

	if (GetVarAddrType(self, varname, index, addr, type, false))
	{
		if (!type->IsKindOf(RUNTIME_CLASS(PFloat)))
		{
			type->SetValue(addr, value);
		}
		else
		{
			type->SetValue(addr, ACSToDouble(value));
		}
	}
}

static int GetUserVariable(AActor *self, FName varname, int index)
{
	void *addr;
	PType *type;

	if (GetVarAddrType(self, varname, index, addr, type, true))
	{
		if (type->IsKindOf(RUNTIME_CLASS(PFloat)))
		{
			return DoubleToACS(type->GetValueFloat(addr));
		}
		else if (type->IsA(RUNTIME_CLASS(PName)))
		{
			return GlobalACSStrings.AddString(FName(ENamedName(type->GetValueInt(addr))).GetChars());
		}
		else if (type->IsA(RUNTIME_CLASS(PString)))
		{
			return GlobalACSStrings.AddString(*(FString *)addr);
		}
		else
		{
			return type->GetValueInt(addr);
		}
	}
	return 0;
}

// Converts fixed- to floating-point as required.
static void DoSetCVar(FBaseCVar *cvar, int value, bool is_string, bool force=false)
{
	UCVarValue val;
	ECVarType type;

	// For serverinfo variables, only the arbitrator should set it.
	// The actual change to this cvar will not show up until it's
	// been replicated to all peers.
	if ((cvar->GetFlags() & CVAR_SERVERINFO) && consoleplayer != Net_Arbitrator)
	{
		return;
	}
	if (is_string)
	{
		val.String = FBehavior::StaticLookupString(value);
		type = CVAR_String;
	}
	else if (cvar->GetRealType() == CVAR_Float)
	{
		val.Float = ACSToFloat(value);
		type = CVAR_Float;
	}
	else
	{
		val.Int = value;
		type = CVAR_Int;
	}
	if (force)
	{
		cvar->ForceSet(val, type, true);
	}
	else
	{
		cvar->SetGenericRep(val, type);
	}
}

// Converts floating- to fixed-point as required.
static int DoGetCVar(FBaseCVar *cvar, bool is_string)
{
	UCVarValue val;

	if (cvar == nullptr)
	{
		return 0;
	}
	else if (is_string)
	{
		val = cvar->GetGenericRep(CVAR_String);
		return GlobalACSStrings.AddString(val.String);
	}
	else if (cvar->GetRealType() == CVAR_Float)
	{
		val = cvar->GetGenericRep(CVAR_Float);
		return DoubleToACS(val.Float);
	}
	else
	{
		val = cvar->GetGenericRep(CVAR_Int);
		return val.Int;
	}
}

static int SetUserCVar(int playernum, const char *cvarname, int value, bool is_string)
{
	if ((unsigned)playernum >= MAXPLAYERS || !playeringame[playernum])
	{
		return 0;
	}
	FBaseCVar **cvar_p = players[playernum].userinfo.CheckKey(FName(cvarname, true));
	FBaseCVar *cvar;
	// Only mod-created cvars may be set.
	if (cvar_p == NULL || (cvar = *cvar_p) == NULL || (cvar->GetFlags() & CVAR_IGNORE) || !(cvar->GetFlags() & CVAR_MOD))
	{
		return 0;
	}
	DoSetCVar(cvar, value, is_string);

	// If we are this player, then also reflect this change in the local version of this cvar.
	if (playernum == consoleplayer)
	{
		FBaseCVar *cvar = FindCVar(cvarname, NULL);
		// If we can find it in the userinfo, then we should also be able to find it in the normal cvar list,
		// but check just to be safe.
		if (cvar != NULL)
		{
			DoSetCVar(cvar, value, is_string, true);
		}
	}

	return 1;
}

static int SetCVar(AActor *activator, const char *cvarname, int value, bool is_string)
{
	FBaseCVar *cvar = FindCVar(cvarname, NULL);
	// Only mod-created cvars may be set.
	if (cvar == NULL || (cvar->GetFlags() & (CVAR_IGNORE|CVAR_NOSET)) || !(cvar->GetFlags() & CVAR_MOD))
	{
		return 0;
	}
	// For userinfo cvars, redirect to SetUserCVar
	if (cvar->GetFlags() & CVAR_USERINFO)
	{
		if (activator == NULL || activator->player == NULL)
		{
			return 0;
		}
		return SetUserCVar(int(activator->player - players), cvarname, value, is_string);
	}
	DoSetCVar(cvar, value, is_string);
	return 1;
}

static bool DoSpawnDecal(AActor *actor, const FDecalTemplate *tpl, int flags, DAngle angle, double zofs, double distance)
{
	if (!(flags & SDF_ABSANGLE))
	{
		angle += actor->Angles.Yaw;
	}
	return NULL != ShootDecal(tpl, actor, actor->Sector, actor->X(), actor->Y(),
		actor->Center() - actor->Floorclip + actor->GetBobOffset() + zofs,
		angle, distance, !!(flags & SDF_PERMANENT));
}

static void SetActorAngle(AActor *activator, int tid, int angle, bool interpolate)
{
	DAngle an = ACSToAngle(angle);
	if (tid == 0)
	{
		if (activator != NULL)
		{
			activator->SetAngle(an, interpolate);
		}
	}
	else
	{
		FActorIterator iterator(tid);
		AActor *actor;

		while ((actor = iterator.Next()))
		{
			actor->SetAngle(an, interpolate);
		}
	}
}

static void SetActorPitch(AActor *activator, int tid, int angle, bool interpolate)
{
	DAngle an = ACSToAngle(angle);
	if (tid == 0)
	{
		if (activator != NULL)
		{
			activator->SetPitch(an, interpolate);
		}
	}
	else
	{
		FActorIterator iterator(tid);
		AActor *actor;

		while ((actor = iterator.Next()))
		{
			actor->SetPitch(an, interpolate);
		}
	}
}

static void SetActorRoll(AActor *activator, int tid, int angle, bool interpolate)
{
	DAngle an = ACSToAngle(angle);
	if (tid == 0)
	{
		if (activator != NULL)
		{
			activator->SetRoll(an, interpolate);
		}
	}
	else
	{
		FActorIterator iterator(tid);
		AActor *actor;

		while ((actor = iterator.Next()))
		{
			actor->SetRoll(an, interpolate);
		}
	}
}

static void SetActorTeleFog(AActor *activator, int tid, FString telefogsrc, FString telefogdest)
{
	// Set the actor's telefog to the specified actor. Handle "" as "don't
	// change" since "None" should work just fine for disabling the fog (given
	// that it will resolve to NAME_None which is not a valid actor name).
	if (tid == 0)
	{
		if (activator != NULL)
		{
			if (telefogsrc.IsNotEmpty())
				activator->TeleFogSourceType = PClass::FindActor(telefogsrc);
			if (telefogdest.IsNotEmpty())
				activator->TeleFogDestType = PClass::FindActor(telefogdest);
		}
	}
	else
	{
		FActorIterator iterator(tid);
		AActor *actor;

		PClassActor * src = PClass::FindActor(telefogsrc);
		PClassActor * dest = PClass::FindActor(telefogdest);
		while ((actor = iterator.Next()))
		{
			if (telefogsrc.IsNotEmpty())
				actor->TeleFogSourceType = src;
			if (telefogdest.IsNotEmpty())
				actor->TeleFogDestType = dest;
		}
	}
}

static int SwapActorTeleFog(AActor *activator, int tid)
{
	int count = 0;
	if (tid == 0)
	{
		if ((activator == NULL) || (activator->TeleFogSourceType == activator->TeleFogDestType)) 
			return 0; //Does nothing if they're the same.

		swapvalues (activator->TeleFogSourceType, activator->TeleFogDestType);
		return 1;
	}
	else
	{
		FActorIterator iterator(tid);
		AActor *actor;
		
		while ((actor = iterator.Next()))
		{
			if (actor->TeleFogSourceType == actor->TeleFogDestType) 
				continue; //They're the same. Save the effort.

			swapvalues (actor->TeleFogSourceType, actor->TeleFogDestType);
			count++;
		}
	}
	return count;
}



int DLevelScript::CallFunction(int argCount, int funcIndex, SDWORD *args)
{
	AActor *actor;
	switch(funcIndex)
	{
		case ACSF_GetLineUDMFInt:
			return GetUDMFInt(UDMF_Line, LineFromID(args[0]), FBehavior::StaticLookupString(args[1]));

		case ACSF_GetLineUDMFFixed:
			return DoubleToACS(GetUDMFFloat(UDMF_Line, LineFromID(args[0]), FBehavior::StaticLookupString(args[1])));

		case ACSF_GetThingUDMFInt:
		case ACSF_GetThingUDMFFixed:
			return 0;	// Not implemented yet

		case ACSF_GetSectorUDMFInt:
			return GetUDMFInt(UDMF_Sector, P_FindFirstSectorFromTag(args[0]), FBehavior::StaticLookupString(args[1]));

		case ACSF_GetSectorUDMFFixed:
			return DoubleToACS(GetUDMFFloat(UDMF_Sector, P_FindFirstSectorFromTag(args[0]), FBehavior::StaticLookupString(args[1])));

		case ACSF_GetSideUDMFInt:
			return GetUDMFInt(UDMF_Side, SideFromID(args[0], args[1]), FBehavior::StaticLookupString(args[2]));

		case ACSF_GetSideUDMFFixed:
			return DoubleToACS(GetUDMFFloat(UDMF_Side, SideFromID(args[0], args[1]), FBehavior::StaticLookupString(args[2])));

		case ACSF_GetActorVelX:
			actor = SingleActorFromTID(args[0], activator);
			return actor != NULL? DoubleToACS(actor->Vel.X) : 0;

		case ACSF_GetActorVelY:
			actor = SingleActorFromTID(args[0], activator);
			return actor != NULL? DoubleToACS(actor->Vel.Y) : 0;

		case ACSF_GetActorVelZ:
			actor = SingleActorFromTID(args[0], activator);
			return actor != NULL? DoubleToACS(actor->Vel.Z) : 0;

		case ACSF_SetPointer:
			if (activator)
			{
				AActor *ptr = SingleActorFromTID(args[1], activator);
				if (argCount > 2)
				{
					ptr = COPY_AAPTR(ptr, args[2]);
				}
				if (ptr == activator) ptr = NULL;
				ASSIGN_AAPTR(activator, args[0], ptr, (argCount > 3) ? args[3] : 0);
				return ptr != NULL;
			}
			return 0;

		case ACSF_SetActivator:
			if (argCount > 1 && args[1] != AAPTR_DEFAULT) // condition (x != AAPTR_DEFAULT) is essentially condition (x).
			{
				activator = COPY_AAPTR(SingleActorFromTID(args[0], activator), args[1]);
			}
			else
			{
				activator = SingleActorFromTID(args[0], NULL);
			}
			return activator != NULL;
		
		case ACSF_SetActivatorToTarget:
			// [KS] I revised this a little bit
			actor = SingleActorFromTID(args[0], activator);
			if (actor != NULL)
			{
				if (actor->player != NULL && actor->player->playerstate == PST_LIVE)
				{
					FTranslatedLineTarget t;
					P_BulletSlope(actor, &t, ALF_PORTALRESTRICT);
					actor = t.linetarget;
				}
				else
				{
					actor = actor->target;
				}
				if (actor != NULL) // [FDARI] moved this (actor != NULL)-branch inside the other, so that it is only tried when it can be true
				{
					activator = actor;
					return 1;
				}
			}
			return 0;

		case ACSF_GetActorViewHeight:
			actor = SingleActorFromTID(args[0], activator);
			if (actor != NULL)
			{
				if (actor->player != NULL)
				{
					return DoubleToACS(actor->player->mo->ViewHeight + actor->player->crouchviewdelta);
				}
				else
				{
					return DoubleToACS(actor->GetCameraHeight());
				}
			}
			else return 0;

		case ACSF_GetChar:
		{
			const char *p = FBehavior::StaticLookupString(args[0]);
			if (p != NULL && args[1] >= 0 && args[1] < int(strlen(p)))
			{
				return p[args[1]];
			}
			else 
			{
				return 0;
			}
		}

		case ACSF_GetAirSupply:
		{
			if (args[0] < 0 || args[0] >= MAXPLAYERS || !playeringame[args[0]])
			{
				return 0;
			}
			else
			{
				return players[args[0]].air_finished - level.time;
			}
		}

		case ACSF_SetAirSupply:
		{
			if (args[0] < 0 || args[0] >= MAXPLAYERS || !playeringame[args[0]])
			{
				return 0;
			}
			else
			{
				players[args[0]].air_finished = args[1] + level.time;
				return 1;
			}
		}

		case ACSF_SetSkyScrollSpeed:
		{
			if (args[0] == 1) level.skyspeed1 = ACSToFloat(args[1]);
			else if (args[0] == 2) level.skyspeed2 = ACSToFloat(args[1]);
			return 1;
		}

		case ACSF_GetArmorType:
		{
			if (args[1] < 0 || args[1] >= MAXPLAYERS || !playeringame[args[1]])
			{
				return 0;
			}
			else
			{
				FName p(FBehavior::StaticLookupString(args[0]));
				ABasicArmor * armor = (ABasicArmor *) players[args[1]].mo->FindInventory(NAME_BasicArmor);
				if (armor && armor->ArmorType == p) return armor->Amount;
			}
			return 0;
		}

		case ACSF_GetArmorInfo:
		{
			if (activator == NULL || activator->player == NULL) return 0;

			ABasicArmor * equippedarmor = (ABasicArmor *) activator->FindInventory(NAME_BasicArmor);

			if (equippedarmor && equippedarmor->Amount != 0)
			{
				switch(args[0])
				{
					case ARMORINFO_CLASSNAME:
						return GlobalACSStrings.AddString(equippedarmor->ArmorType.GetChars());

					case ARMORINFO_SAVEAMOUNT:
						return equippedarmor->MaxAmount;

					case ARMORINFO_SAVEPERCENT:
						return DoubleToACS(equippedarmor->SavePercent);

					case ARMORINFO_MAXABSORB:
						return equippedarmor->MaxAbsorb;

					case ARMORINFO_MAXFULLABSORB:
						return equippedarmor->MaxFullAbsorb;

					case ARMORINFO_ACTUALSAVEAMOUNT:
						return equippedarmor->ActualSaveAmount;

					default:
						return 0;
				}
			}
			return args[0] == ARMORINFO_CLASSNAME ? GlobalACSStrings.AddString("None") : 0;
		}

		case ACSF_SpawnSpotForced:
			return DoSpawnSpot(args[0], args[1], args[2], args[3], true);

		case ACSF_SpawnSpotFacingForced:
			return DoSpawnSpotFacing(args[0], args[1], args[2], true);

		case ACSF_CheckActorProperty:
			return (CheckActorProperty(args[0], args[1], args[2]));
        
        case ACSF_SetActorVelocity:
		{
			DVector3 vel(ACSToDouble(args[1]), ACSToDouble(args[2]), ACSToDouble(args[3]));
			if (args[0] == 0)
			{
				P_Thing_SetVelocity(activator, vel, !!args[4], !!args[5]);
			}
			else
			{
				TActorIterator<AActor> iterator(args[0]);

				while ((actor = iterator.Next()))
				{
					P_Thing_SetVelocity(actor, vel, !!args[4], !!args[5]);
				}
			}
			return 0; 
		}

		case ACSF_SetUserVariable:
		{
			int cnt = 0;
			FName varname(FBehavior::StaticLookupString(args[1]), true);
			if (varname != NAME_None)
			{
				if (args[0] == 0)
				{
					if (activator != NULL)
					{
						SetUserVariable(activator, varname, 0, args[2]);
					}
					cnt++;
				}
				else
				{
					TActorIterator<AActor> iterator(args[0]);
	                
					while ( (actor = iterator.Next()) )
					{
						SetUserVariable(actor, varname, 0, args[2]);
						cnt++;
					}
				}
			}
			return cnt;
		}
		
		case ACSF_GetUserVariable:
		{
			FName varname(FBehavior::StaticLookupString(args[1]), true);
			if (varname != NAME_None)
			{
				AActor *a = SingleActorFromTID(args[0], activator); 
				return a != NULL ? GetUserVariable(a, varname, 0) : 0;
			}
			return 0;
		}

		case ACSF_SetUserArray:
		{
			int cnt = 0;
			FName varname(FBehavior::StaticLookupString(args[1]), true);
			if (varname != NAME_None)
			{
				if (args[0] == 0)
				{
					if (activator != NULL)
					{
						SetUserVariable(activator, varname, args[2], args[3]);
					}
					cnt++;
				}
				else
				{
					TActorIterator<AActor> iterator(args[0]);
	                
					while ( (actor = iterator.Next()) )
					{
						SetUserVariable(actor, varname, args[2], args[3]);
						cnt++;
					}
				}
			}
			return cnt;
		}
		
		case ACSF_GetUserArray:
		{
			FName varname(FBehavior::StaticLookupString(args[1]), true);
			if (varname != NAME_None)
			{
				AActor *a = SingleActorFromTID(args[0], activator); 
				return a != NULL ? GetUserVariable(a, varname, args[2]) : 0;
			}
			return 0;
		}

		case ACSF_Radius_Quake2:
			P_StartQuake(activator, args[0], args[1], args[2], args[3], args[4], FBehavior::StaticLookupString(args[5]));
			break;

		case ACSF_CheckActorClass:
		{
			AActor *a = SingleActorFromTID(args[0], activator);
			return a == NULL ? false : a->GetClass()->TypeName == FName(FBehavior::StaticLookupString(args[1]));
		}

		case ACSF_GetActorClass:
		{
			AActor *a = SingleActorFromTID(args[0], activator);
			return GlobalACSStrings.AddString(a == NULL ? "None" : a->GetClass()->TypeName.GetChars());
		}

		case ACSF_SoundSequenceOnActor:
			{
				const char *seqname = FBehavior::StaticLookupString(args[1]);
				if (seqname != NULL)
				{
					if (args[0] == 0)
					{
						if (activator != NULL)
						{
							SN_StartSequence(activator, seqname, 0);
						}
					}
					else
					{
						FActorIterator it(args[0]);
						AActor *actor;

						while ( (actor = it.Next()) )
						{
							SN_StartSequence(actor, seqname, 0);
						}
					}
				}
			}
			break;

		case ACSF_SoundSequenceOnSector:
			{
				const char *seqname = FBehavior::StaticLookupString(args[1]);
				int space = args[2] < CHAN_FLOOR || args[2] > CHAN_INTERIOR ? CHAN_FULLHEIGHT : args[2];
				if (seqname != NULL)
				{
					FSectorTagIterator it(args[0]);
					int s;
					while ((s = it.Next()) >= 0)
					{
						SN_StartSequence(&sectors[s], args[2], seqname, 0);
					}
				}
			}
			break;

		case ACSF_SoundSequenceOnPolyobj:
			{
				const char *seqname = FBehavior::StaticLookupString(args[1]);
				if (seqname != NULL)
				{
					FPolyObj *poly = PO_GetPolyobj(args[0]);
					if (poly != NULL)
					{
						SN_StartSequence(poly, seqname, 0);
					}
				}
			}
			break;

		case ACSF_GetPolyobjX:
			{
				FPolyObj *poly = PO_GetPolyobj(args[0]);
				if (poly != NULL)
				{
					return DoubleToACS(poly->StartSpot.pos.X);
				}
			}
			return FIXED_MAX;

		case ACSF_GetPolyobjY:
			{
				FPolyObj *poly = PO_GetPolyobj(args[0]);
				if (poly != NULL)
				{
					return DoubleToACS(poly->StartSpot.pos.Y);
				}
			}
			return FIXED_MAX;
        
        case ACSF_CheckSight:
        {
			AActor *source;
			AActor *dest;

			int flags = SF_IGNOREVISIBILITY;

			if (args[2] & 1) flags |= SF_IGNOREWATERBOUNDARY;
			if (args[2] & 2) flags |= SF_SEEPASTBLOCKEVERYTHING | SF_SEEPASTSHOOTABLELINES;

			if (args[0] == 0) 
			{
				source = (AActor *) activator;

				if (args[1] == 0) return 1; // [KS] I'm sure the activator can see itself.

				TActorIterator<AActor> dstiter (args[1]);

				while ( (dest = dstiter.Next ()) )
				{
					if (P_CheckSight(source, dest, flags)) return 1;
				}
			}
			else
			{
				TActorIterator<AActor> srciter (args[0]);

				while ( (source = srciter.Next ()) )
				{
					if (args[1] != 0)
					{
						TActorIterator<AActor> dstiter (args[1]);
						while ( (dest = dstiter.Next ()) )
						{
							if (P_CheckSight(source, dest, flags)) return 1;
						}
					}
					else
					{
						if (P_CheckSight(source, activator, flags)) return 1;
					}
				}
			}
            return 0;
        }

		case ACSF_SpawnForced:
			return DoSpawn(args[0], args[1], args[2], args[3], args[4], args[5], true);

		case ACSF_ACS_NamedExecute:
		case ACSF_ACS_NamedSuspend:
		case ACSF_ACS_NamedTerminate:
		case ACSF_ACS_NamedLockedExecute:
		case ACSF_ACS_NamedLockedExecuteDoor:
		case ACSF_ACS_NamedExecuteWithResult:
		case ACSF_ACS_NamedExecuteAlways:
			{
				int scriptnum = -FName(FBehavior::StaticLookupString(args[0]));
				int arg1 = argCount > 1 ? args[1] : 0;
				int arg2 = argCount > 2 ? args[2] : 0;
				int arg3 = argCount > 3 ? args[3] : 0;
				int arg4 = argCount > 4 ? args[4] : 0;
				return P_ExecuteSpecial(NamedACSToNormalACS[funcIndex - ACSF_ACS_NamedExecute],
					activationline, activator, backSide,
					scriptnum, arg1, arg2, arg3, arg4);
			}
			break;

		case ACSF_UniqueTID:
			return P_FindUniqueTID(argCount > 0 ? args[0] : 0, (argCount > 1 && args[1] >= 0) ? args[1] : 0);

		case ACSF_IsTIDUsed:
			return P_IsTIDUsed(args[0]);

		case ACSF_Sqrt:
			return xs_FloorToInt(g_sqrt(double(args[0])));

		case ACSF_FixedSqrt:
			return DoubleToACS(g_sqrt(ACSToDouble(args[0])));

		case ACSF_VectorLength:
			return DoubleToACS(DVector2(ACSToDouble(args[0]), ACSToDouble(args[1])).Length());

		case ACSF_SetHUDClipRect:
			ClipRectLeft = argCount > 0 ? args[0] : 0;
			ClipRectTop = argCount > 1 ? args[1] : 0;
			ClipRectWidth = argCount > 2 ? args[2] : 0;
			ClipRectHeight = argCount > 3 ? args[3] : 0;
			WrapWidth = argCount > 4 ? args[4] : 0;
			HandleAspect = argCount > 5 ? !!args[5] : true;
			break;

		case ACSF_SetHUDWrapWidth:
			WrapWidth = argCount > 0 ? args[0] : 0;
			break;

		case ACSF_GetCVarString:
			if (argCount == 1)
			{
				return DoGetCVar(GetCVar(activator, FBehavior::StaticLookupString(args[0])), true);
			}
			break;

		case ACSF_SetCVar:
			if (argCount == 2)
			{
				return SetCVar(activator, FBehavior::StaticLookupString(args[0]), args[1], false);
			}
			break;

		case ACSF_SetCVarString:
			if (argCount == 2)
			{
				return SetCVar(activator, FBehavior::StaticLookupString(args[0]), args[1], true);
			}
			break;

		case ACSF_GetUserCVar:
			if (argCount == 2)
			{
				return DoGetCVar(GetUserCVar(args[0], FBehavior::StaticLookupString(args[1])), false);
			}
			break;

		case ACSF_GetUserCVarString:
			if (argCount == 2)
			{
				return DoGetCVar(GetUserCVar(args[0], FBehavior::StaticLookupString(args[1])), true);
			}
			break;

		case ACSF_SetUserCVar:
			if (argCount == 3)
			{
				return SetUserCVar(args[0], FBehavior::StaticLookupString(args[1]), args[2], false);
			}
			break;

		case ACSF_SetUserCVarString:
			if (argCount == 3)
			{
				return SetUserCVar(args[0], FBehavior::StaticLookupString(args[1]), args[2], true);
			}
			break;

		//[RC] A bullet firing function for ACS. Thanks to DavidPH.
		case ACSF_LineAttack:
			{
				DAngle angle		= ACSToAngle(args[1]);
				DAngle pitch		= ACSToAngle(args[2]);
				int	damage			= args[3];
				FName pufftype		= argCount > 4 && args[4]? FName(FBehavior::StaticLookupString(args[4])) : NAME_BulletPuff;
				FName damagetype	= argCount > 5 && args[5]? FName(FBehavior::StaticLookupString(args[5])) : NAME_None;
				double range		= argCount > 6 && args[6]? ACSToDouble(args[6]) : MISSILERANGE;
				int flags			= argCount > 7 && args[7]? args[7] : 0;
				int pufftid			= argCount > 8 && args[8]? args[8] : 0;

				int fhflags = 0;
				if (flags & FHF_NORANDOMPUFFZ) fhflags |= LAF_NORANDOMPUFFZ;
				if (flags & FHF_NOIMPACTDECAL) fhflags |= LAF_NOIMPACTDECAL;

				if (args[0] == 0)
				{
					AActor *puff = P_LineAttack(activator, angle, range, pitch, damage, damagetype, pufftype, fhflags);
					if (puff != NULL && pufftid != 0)
					{
						puff->tid = pufftid;
						puff->AddToHash();
					}
				}
				else
				{
					AActor *source;
					FActorIterator it(args[0]);

					while ((source = it.Next()) != NULL)
					{
						AActor *puff = P_LineAttack(source, angle, range, pitch, damage, damagetype, pufftype, fhflags);
						if (puff != NULL && pufftid != 0)
						{
							puff->tid = pufftid;
							puff->AddToHash();
						}
					}
				}
			}
			break;

		case ACSF_PlaySound:
		case ACSF_PlayActorSound:
			// PlaySound(tid, "SoundName", channel, volume, looping, attenuation)
			{
				FSoundID sid;

				if (funcIndex == ACSF_PlaySound)
				{
					const char *lookup = FBehavior::StaticLookupString(args[1]);
					if (lookup != NULL)
					{
						sid = lookup;
					}
				}
				if (sid != 0 || funcIndex == ACSF_PlayActorSound)
				{
					FActorIterator it(args[0]);
					AActor *spot;

					int chan = argCount > 2 ? args[2] : CHAN_BODY;
					float vol = argCount > 3 ? ACSToFloat(args[3]) : 1.f;
					INTBOOL looping = argCount > 4 ? args[4] : false;
					float atten = argCount > 5 ? ACSToFloat(args[5]) : ATTN_NORM;

					if (args[0] == 0)
					{
						spot = activator;
						goto doplaysound;
					}
					while ((spot = it.Next()) != NULL)
					{
doplaysound:			if (funcIndex == ACSF_PlayActorSound)
						{
							sid = GetActorSound(spot, args[1]);
						}
						if (sid != 0)
						{
							if (!looping)
							{
								S_Sound(spot, chan, sid, vol, atten);
							}
							else if (!S_IsActorPlayingSomething(spot, chan & 7, sid))
							{
								S_Sound(spot, chan | CHAN_LOOP, sid, vol, atten);
							}
						}
					}
				}
			}
			break;

		case ACSF_StopSound:
			{
				int chan = argCount > 1 ? args[1] : CHAN_BODY;

				if (args[0] == 0)
				{
					S_StopSound(activator, chan);
				}
				else
				{
					FActorIterator it(args[0]);
					AActor *spot;

					while ((spot = it.Next()) != NULL)
					{
						S_StopSound(spot, chan);
					}
				}
			}
			break;

		case ACSF_SoundVolume:
			// SoundVolume(int tid, int channel, fixed volume)
			{
				int chan = args[1];
				float volume = ACSToFloat(args[2]);

				if (args[0] == 0)
				{
					S_ChangeSoundVolume(activator, chan, volume);
				}
				else
				{
					FActorIterator it(args[0]);
					AActor *spot;

					while ((spot = it.Next()) != NULL)
					{
						S_ChangeSoundVolume(spot, chan, volume);
					}
				}
			}
			break;

		case ACSF_strcmp:
		case ACSF_stricmp:
			if (argCount >= 2)
			{
				const char *a, *b;
				// If the string indicies are the same, then they are the same string.
				if (args[0] == args[1])
				{
					return 0;
				}
				a = FBehavior::StaticLookupString(args[0]);
				b = FBehavior::StaticLookupString(args[1]);

				// Don't crash on invalid strings.
				if (a == NULL) a = "";
				if (b == NULL) b = "";

				if (argCount > 2)
				{
					int n = args[2];
					return (funcIndex == ACSF_strcmp) ? strncmp(a, b, n) : strnicmp(a, b, n);
				}
				else
				{
					return (funcIndex == ACSF_strcmp) ? strcmp(a, b) : stricmp(a, b);
				}
			}
			break;

		case ACSF_StrLeft:
		case ACSF_StrRight:
			if (argCount >= 2)
			{
				const char *oldstr = FBehavior::StaticLookupString(args[0]);
				if (oldstr == NULL || *oldstr == '\0')
				{
					return GlobalACSStrings.AddString("");
				}
				size_t oldlen = strlen(oldstr);
				size_t newlen = args[1];

				if (oldlen < newlen)
				{
					newlen = oldlen;
				}
				FString newstr(funcIndex == ACSF_StrLeft ? oldstr : oldstr + oldlen - newlen, newlen);
				return GlobalACSStrings.AddString(newstr);
			}
			break;

		case ACSF_StrMid:
			if (argCount >= 3)
			{
				const char *oldstr = FBehavior::StaticLookupString(args[0]);
				if (oldstr == NULL || *oldstr == '\0')
				{
					return GlobalACSStrings.AddString("");
				}
				size_t oldlen = strlen(oldstr);
				size_t pos = args[1];
				size_t newlen = args[2];

				if (pos >= oldlen)
				{
					return GlobalACSStrings.AddString("");
				}
				if (pos + newlen > oldlen || pos + newlen < pos)
				{
					newlen = oldlen - pos;
				}
				return GlobalACSStrings.AddString(FString(oldstr + pos, newlen));
			}
			break;

		case ACSF_GetWeapon:
            if (activator == NULL || activator->player == NULL || // Non-players do not have weapons
                activator->player->ReadyWeapon == NULL)
            {
                return GlobalACSStrings.AddString("None");
            }
            else
            {
				return GlobalACSStrings.AddString(activator->player->ReadyWeapon->GetClass()->TypeName.GetChars());
            }

		case ACSF_SpawnDecal:
			// int SpawnDecal(int tid, str decalname, int flags, fixed angle, int zoffset, int distance)
			// Returns number of decals spawned (not including spreading)
			{
				int count = 0;
				const FDecalTemplate *tpl = DecalLibrary.GetDecalByName(FBehavior::StaticLookupString(args[1]));
				if (tpl != NULL)
				{
					int flags = (argCount > 2) ? args[2] : 0;
					DAngle angle = ACSToAngle((argCount > 3) ? args[3] : 0);
					int zoffset = (argCount > 4) ? args[4]: 0;
					int distance = (argCount > 5) ? args[5] : 64;

					if (args[0] == 0)
					{
						if (activator != NULL)
						{
							count += DoSpawnDecal(activator, tpl, flags, angle, zoffset, distance);
						}
					}
					else
					{
						FActorIterator it(args[0]);
						AActor *actor;

						while ((actor = it.Next()) != NULL)
						{
							count += DoSpawnDecal(actor, tpl, flags, angle, zoffset, distance);
						}
					}
				}
				return count;
			}
			break;

		case ACSF_CheckFont:
			// bool CheckFont(str fontname)
			return V_GetFont(FBehavior::StaticLookupString(args[0])) != NULL;

		case ACSF_DropItem:
		{
			const char *type = FBehavior::StaticLookupString(args[1]);
			int amount = argCount >= 3? args[2] : -1;
			int chance = argCount >= 4? args[3] : 256;
			PClassActor *cls = PClass::FindActor(type);
			int cnt = 0;
			if (cls != NULL)
			{
				if (args[0] == 0)
				{
					if (activator != NULL)
					{
						P_DropItem(activator, cls, amount, chance);
						cnt++;
					}
				}
				else
				{
					FActorIterator it(args[0]);
					AActor *actor;

					while ((actor = it.Next()) != NULL)
					{
						P_DropItem(actor, cls, amount, chance);
						cnt++;
					}
				}
				return cnt;
			}
			break;
		}

		case ACSF_DropInventory:
		{
			const char *type = FBehavior::StaticLookupString(args[1]);
			AInventory *inv;
			
			if (type != NULL)
			{
				if (args[0] == 0)
				{
					if (activator != NULL)
					{
						inv = activator->FindInventory(type);
						if (inv)
						{
							activator->DropInventory(inv);
						}
					}
				}
				else
				{
					FActorIterator it(args[0]);
					AActor *actor;
					
					while ((actor = it.Next()) != NULL)
					{
						inv = actor->FindInventory(type);
						if (inv)
						{
							actor->DropInventory(inv);
						}
					}
				}
			}
		break;
		}

		case ACSF_CheckFlag:
		{
			AActor *actor = SingleActorFromTID(args[0], activator);
			if (actor != NULL)
			{
				return !!CheckActorFlag(actor, FBehavior::StaticLookupString(args[1]));
			}
			break;
		}

		case ACSF_QuakeEx:
		{
			return P_StartQuakeXYZ(activator, args[0], args[1], args[2], args[3], args[4], args[5], args[6], FBehavior::StaticLookupString(args[7]), 
				argCount > 8 ? args[8] : 0,
				argCount > 9 ? ACSToDouble(args[9]) : 1.0,
				argCount > 10 ? ACSToDouble(args[10]) : 1.0,
				argCount > 11 ? ACSToDouble(args[11]) : 1.0,
				argCount > 12 ? args[12] : 0,
				argCount > 13 ? args[13] : 0,
				argCount > 14 ? ACSToDouble(args[14]) : 0,
				argCount > 15 ? ACSToDouble(args[15]) : 0);
		}

		case ACSF_SetLineActivation:
			if (argCount >= 2)
			{
				int line;
				FLineIdIterator itr(args[0]);
				while ((line = itr.Next()) >= 0)
				{
					lines[line].activation = args[1];
				}
			}
			break;

		case ACSF_GetLineActivation:
			if (argCount > 0)
			{
				int line = P_FindFirstLineFromID(args[0]);
				return line >= 0 ? lines[line].activation : 0;
			}
			break;

		case ACSF_GetActorPowerupTics:
			if (argCount >= 2)
			{
				PClassActor *powerupclass = PClass::FindActor(FBehavior::StaticLookupString(args[1]));
				if (powerupclass == NULL || !RUNTIME_CLASS(APowerup)->IsAncestorOf(powerupclass))
				{
					Printf("'%s' is not a type of Powerup.\n", FBehavior::StaticLookupString(args[1]));
					return 0;
				}

				AActor *actor = SingleActorFromTID(args[0], activator);
				if (actor != NULL)
				{
					APowerup* powerup = (APowerup*)actor->FindInventory(powerupclass);
					if (powerup != NULL)
						return powerup->EffectTics;
				}
				return 0;
			}
			break;

		case ACSF_ChangeActorAngle:
			if (argCount >= 2)
			{
				SetActorAngle(activator, args[0], args[1], argCount > 2 ? !!args[2] : false);
			}
			break;

		case ACSF_ChangeActorPitch:
			if (argCount >= 2)
			{
				SetActorPitch(activator, args[0], args[1], argCount > 2 ? !!args[2] : false);
			}
			break;
		case ACSF_SetActorTeleFog:
			if (argCount >= 3)
			{
				SetActorTeleFog(activator, args[0], FBehavior::StaticLookupString(args[1]), FBehavior::StaticLookupString(args[2]));
			}
			break;
		case ACSF_SwapActorTeleFog:
			if (argCount >= 1)
			{
				return SwapActorTeleFog(activator, args[0]);
			}
			break;
		case ACSF_PickActor:
			if (argCount >= 5)
			{
				actor = SingleActorFromTID(args[0], activator);
				if (actor == NULL)
				{
					return 0;
				}

				ActorFlags actorMask = MF_SHOOTABLE;
				if (argCount >= 6) {
					actorMask = ActorFlags::FromInt(args[5]);
				}

				DWORD wallMask = ML_BLOCKEVERYTHING | ML_BLOCKHITSCAN;
				if (argCount >= 7) {
					wallMask = args[6];
				}

				int flags = 0;
				if (argCount >= 8)
				{
					flags = args[7];
				}

				AActor* pickedActor = P_LinePickActor(actor, ACSToAngle(args[1]), ACSToDouble(args[3]), ACSToAngle(args[2]), actorMask, wallMask);
				if (pickedActor == NULL) {
					return 0;
				}

				if (!(flags & PICKAF_FORCETID) && (args[4] == 0) && (pickedActor->tid == 0))
					return 0;

				if ((pickedActor->tid == 0) || (flags & PICKAF_FORCETID))
				{
					pickedActor->RemoveFromHash();
					pickedActor->tid = args[4];
					pickedActor->AddToHash();
				}
				if (flags & PICKAF_RETURNTID)
				{
					return pickedActor->tid;
				}
				return 1;
			}
			break;

		case ACSF_IsPointerEqual:
			{
				int tid1 = 0, tid2 = 0;
				switch (argCount)
				{
				case 4: tid2 = args[3];
				case 3: tid1 = args[2];
				}

				actor = SingleActorFromTID(tid1, activator);
				AActor * actor2 = tid2 == tid1 ? actor : SingleActorFromTID(tid2, activator);

				return COPY_AAPTR(actor, args[0]) == COPY_AAPTR(actor2, args[1]);
			}
			break;

		case ACSF_CanRaiseActor:
			if (argCount >= 1) {
				if (args[0] == 0) {
					actor = SingleActorFromTID(args[0], activator);
					if (actor != NULL) {
						return P_Thing_CanRaise(actor);
					}
				}

				FActorIterator iterator(args[0]);
				bool canraiseall = true;
				while ((actor = iterator.Next()))
				{
					canraiseall = P_Thing_CanRaise(actor) & canraiseall;
				}
				
				return canraiseall;
			}
			break;

		// [Nash] Actor roll functions. Let's roll!
		case ACSF_SetActorRoll:
			SetActorRoll(activator, args[0], args[1], false);
			return 0;

		case ACSF_ChangeActorRoll:
			if (argCount >= 2)
			{
				SetActorRoll(activator, args[0], args[1], argCount > 2 ? !!args[2] : false);
			}
			break;

		case ACSF_GetActorRoll:
			actor = SingleActorFromTID(args[0], activator);
			return actor != NULL? AngleToACS(actor->Angles.Roll) : 0;
		
		// [ZK] A_Warp in ACS
		case ACSF_Warp:
		{
			int tid_dest = args[0];
			int xofs = args[1];
			int yofs = args[2];
			int zofs = args[3];
			int angle = args[4];
			int flags = args[5];
			const char *statename = argCount > 6 ? FBehavior::StaticLookupString(args[6]) : "";
			bool exact = argCount > 7 ? !!args[7] : false;
			int heightoffset = argCount > 8 ? args[8] : 0;
			int radiusoffset = argCount > 9 ? args[9] : 0;
			int pitch = argCount > 10 ? args[10] : 0;

			FState *state = argCount > 6 ? activator->GetClass()->FindStateByString(statename, exact) : 0;

			AActor *reference;
			if((flags & WARPF_USEPTR) && tid_dest != AAPTR_DEFAULT)
			{
				reference = COPY_AAPTR(activator, tid_dest);
			}
			else
			{
				reference = SingleActorFromTID(tid_dest, activator);
			}

			// If there is no activator or actor to warp to, fail.
			if (activator == NULL || !reference)
				return false;

			if (P_Thing_Warp(activator, reference, ACSToDouble(xofs), ACSToDouble(yofs), ACSToDouble(zofs), ACSToAngle(angle), flags, ACSToDouble(heightoffset), ACSToDouble(radiusoffset), ACSToAngle(pitch)))
			{
				if (state && argCount > 6)
				{
					activator->SetState(state);
				}
				return true;
			}
			return false;
		}
		case ACSF_GetMaxInventory:
			actor = SingleActorFromTID(args[0], activator);
			if (actor != NULL)
			{
				return CheckInventory(actor, FBehavior::StaticLookupString(args[1]), true);
			}
			break;

		case ACSF_SetSectorDamage:
			if (argCount >= 2)
			{
				FSectorTagIterator it(args[0]);
				int s;
				while ((s = it.Next()) >= 0)
				{
					sector_t *sec = &sectors[s];

					sec->damageamount = args[1];
					sec->damagetype = argCount >= 3 ? FName(FBehavior::StaticLookupString(args[2])) : FName(NAME_None);
					sec->damageinterval = argCount >= 4 ? clamp(args[3], 1, INT_MAX) : 32;
					sec->leakydamage = argCount >= 5 ? args[4] : 0;
				}
			}
			break;

		case ACSF_SetSectorTerrain:
			if (argCount >= 3)
			{
				if (args[1] == sector_t::floor || args[1] == sector_t::ceiling)
				{
					int terrain = P_FindTerrain(FBehavior::StaticLookupString(args[2]));
					FSectorTagIterator it(args[0]);
					int s;
					while ((s = it.Next()) >= 0)
					{
						sectors[s].terrainnum[args[1]] = terrain;
					}
				}
			}
			break;
		
		case ACSF_SpawnParticle:
		{
			PalEntry color = args[0];
			bool fullbright = argCount > 1 ? !!args[1] : false;
			int lifetime = argCount > 2 ? args[2] : 35;
			double size = argCount > 3 ? args[3] : 1.;
			int x = argCount > 4 ? args[4] : 0;
			int y = argCount > 5 ? args[5] : 0;
			int z = argCount > 6 ? args[6] : 0;
			int xvel = argCount > 7 ? args[7] : 0;
			int yvel = argCount > 8 ? args[8] : 0;
			int zvel = argCount > 9 ? args[9] : 0;
			int accelx = argCount > 10 ? args[10] : 0;
			int accely = argCount > 11 ? args[11] : 0;
			int accelz = argCount > 12 ? args[12] : 0;
			int startalpha = argCount > 13 ? args[13] : 0xFF; // Byte trans			
			int fadestep = argCount > 14 ? args[14] : -1;
			double endsize = argCount > 15 ? args[15] : -1.;

			startalpha = clamp<int>(startalpha, 0, 255); // Clamp to byte
			lifetime = clamp<int>(lifetime, 0, 255); // Clamp to byte
			fadestep = clamp<int>(fadestep, -1, 255); // Clamp to byte inc. -1 (indicating automatic)
			size = fabs(size);

			if (lifetime != 0)
				P_SpawnParticle(DVector3(ACSToDouble(x), ACSToDouble(y), ACSToDouble(z)), 
								DVector3(ACSToDouble(xvel), ACSToDouble(yvel), ACSToDouble(zvel)),
								DVector3(ACSToDouble(accelx), ACSToDouble(accely), ACSToDouble(accelz)),
								color, startalpha/255., lifetime, size, endsize, fadestep/255., fullbright);
		}
		break;

		case ACSF_SetMusicVolume:
			I_SetMusicVolume(ACSToFloat(args[0]));
			break;

		case ACSF_CheckProximity:
		{
			// [zombie] ACS version of A_CheckProximity
			actor = SingleActorFromTID(args[0], activator);
			PClass *classname = PClass::FindClass(FBehavior::StaticLookupString(args[1]));
			double distance = ACSToDouble(args[2]);
			int count = argCount >= 4 ? args[3] : 1;
			int flags = argCount >= 5 ? args[4] : 0;
			int ptr = argCount >= 6 ? args[5] : AAPTR_DEFAULT;
			return P_Thing_CheckProximity(actor, classname, distance, count, flags, ptr);
		}

		default:
			break;
	}


	return 0;
}

enum
{
	PRINTNAME_LEVELNAME		= -1,
	PRINTNAME_LEVEL			= -2,
	PRINTNAME_SKILL			= -3,
};


#define NEXTWORD	(LittleLong(*pc++))
#define NEXTBYTE	(fmt==ACS_LittleEnhanced?getbyte(pc):NEXTWORD)
#define NEXTSHORT	(fmt==ACS_LittleEnhanced?getshort(pc):NEXTWORD)
#define STACK(a)	(Stack[sp - (a)])
#define PushToStack(a)	(Stack[sp++] = (a))
// Direct instructions that take strings need to have the tag applied.
#define TAGSTR(a)	(a|activeBehavior->GetLibraryID())

inline int getbyte (int *&pc)
{
	int res = *(BYTE *)pc;
	pc = (int *)((BYTE *)pc+1);
	return res;
}

inline int getshort (int *&pc)
{
	int res = LittleShort( *(SWORD *)pc);
	pc = (int *)((BYTE *)pc+2);
	return res;
}

static bool CharArrayParms(int &capacity, int &offset, int &a, int *Stack, int &sp, bool ranged)
{
	if (ranged)
	{
		capacity = STACK(1);
		offset = STACK(2);
		if (capacity < 1 || offset < 0)
		{
			sp -= 4;
			return false;
		}
		sp -= 2;
	}
	else
	{
		capacity = INT_MAX;
		offset = 0;
	}
	a = STACK(1);
	offset += STACK(2);
	sp -= 2;
	return true;
}

int DLevelScript::RunScript ()
{
	DACSThinker *controller = DACSThinker::ActiveThinker;
	SDWORD *locals = localvars;
	ACSLocalArrays noarrays;
	ACSLocalArrays *localarrays = &noarrays;
	ScriptFunction *activeFunction = NULL;
	FRemapTable *translation = 0;
	int resultValue = 1;

	if (InModuleScriptNumber >= 0)
	{
		ScriptPtr *ptr = activeBehavior->GetScriptPtr(InModuleScriptNumber);
		assert(ptr != NULL);
		if (ptr != NULL)
		{
			localarrays = &ptr->LocalArrays;
		}
	}

	// Hexen truncates all special arguments to bytes (only when using an old MAPINFO and old ACS format
	const int specialargmask = ((level.flags2 & LEVEL2_HEXENHACK) && activeBehavior->GetFormat() == ACS_Old) ? 255 : ~0;

	switch (state)
	{
	case SCRIPT_Delayed:
		// Decrement the delay counter and enter state running
		// if it hits 0
		if (--statedata == 0)
			state = SCRIPT_Running;
		break;

	case SCRIPT_TagWait:
		// Wait for tagged sector(s) to go inactive, then enter
		// state running
	{
		int secnum;
		FSectorTagIterator it(statedata);
		while ((secnum = it.Next()) >= 0)
		{
			if (sectors[secnum].floordata || sectors[secnum].ceilingdata)
				return resultValue;
		}

		// If we got here, none of the tagged sectors were busy
		state = SCRIPT_Running;
	}
	break;

	case SCRIPT_PolyWait:
		// Wait for polyobj(s) to stop moving, then enter state running
		if (!PO_Busy (statedata))
		{
			state = SCRIPT_Running;
		}
		break;

	case SCRIPT_ScriptWaitPre:
		// Wait for a script to start running, then enter state scriptwait
		if (controller->RunningScripts.CheckKey(statedata) != NULL)
			state = SCRIPT_ScriptWait;
		break;

	case SCRIPT_ScriptWait:
		// Wait for a script to stop running, then enter state running
		if (controller->RunningScripts.CheckKey(statedata) != NULL)
			return resultValue;

		state = SCRIPT_Running;
		PutFirst ();
		break;

	default:
		break;
	}

	FACSStack stackobj;
	SDWORD *Stack = stackobj.buffer;
	int &sp = stackobj.sp;

	int *pc = this->pc;
	ACSFormat fmt = activeBehavior->GetFormat();
	FBehavior* const savedActiveBehavior = activeBehavior;
	unsigned int runaway = 0;	// used to prevent infinite loops
	int pcd;
	FString work;
	const char *lookup;
	int optstart = -1;
	int temp;

	while (state == SCRIPT_Running)
	{
		if (++runaway > 2000000)
		{
			Printf ("Runaway %s terminated\n", ScriptPresentation(script).GetChars());
			state = SCRIPT_PleaseRemove;
			break;
		}

		if (fmt == ACS_LittleEnhanced)
		{
			pcd = getbyte(pc);
			if (pcd >= 256-16)
			{
				pcd = (256-16) + ((pcd - (256-16)) << 8) + getbyte(pc);
			}
		}
		else
		{
			pcd = NEXTWORD;
		}

		switch (pcd)
		{
		default:
			Printf ("Unknown P-Code %d in %s\n", pcd, ScriptPresentation(script).GetChars());
			activeBehavior = savedActiveBehavior;
			// fall through
		case PCD_TERMINATE:
			DPrintf ("%s finished\n", ScriptPresentation(script).GetChars());
			state = SCRIPT_PleaseRemove;
			break;

		case PCD_NOP:
			break;

		case PCD_SUSPEND:
			state = SCRIPT_Suspended;
			break;

		case PCD_TAGSTRING:
			//Stack[sp-1] |= activeBehavior->GetLibraryID();
			Stack[sp-1] = GlobalACSStrings.AddString(activeBehavior->LookupString(Stack[sp-1]));
			break;

		case PCD_PUSHNUMBER:
			PushToStack (uallong(pc[0]));
			pc++;
			break;

		case PCD_PUSHBYTE:
			PushToStack (*(BYTE *)pc);
			pc = (int *)((BYTE *)pc + 1);
			break;

		case PCD_PUSH2BYTES:
			Stack[sp] = ((BYTE *)pc)[0];
			Stack[sp+1] = ((BYTE *)pc)[1];
			sp += 2;
			pc = (int *)((BYTE *)pc + 2);
			break;

		case PCD_PUSH3BYTES:
			Stack[sp] = ((BYTE *)pc)[0];
			Stack[sp+1] = ((BYTE *)pc)[1];
			Stack[sp+2] = ((BYTE *)pc)[2];
			sp += 3;
			pc = (int *)((BYTE *)pc + 3);
			break;

		case PCD_PUSH4BYTES:
			Stack[sp] = ((BYTE *)pc)[0];
			Stack[sp+1] = ((BYTE *)pc)[1];
			Stack[sp+2] = ((BYTE *)pc)[2];
			Stack[sp+3] = ((BYTE *)pc)[3];
			sp += 4;
			pc = (int *)((BYTE *)pc + 4);
			break;

		case PCD_PUSH5BYTES:
			Stack[sp] = ((BYTE *)pc)[0];
			Stack[sp+1] = ((BYTE *)pc)[1];
			Stack[sp+2] = ((BYTE *)pc)[2];
			Stack[sp+3] = ((BYTE *)pc)[3];
			Stack[sp+4] = ((BYTE *)pc)[4];
			sp += 5;
			pc = (int *)((BYTE *)pc + 5);
			break;

		case PCD_PUSHBYTES:
			temp = *(BYTE *)pc;
			pc = (int *)((BYTE *)pc + temp + 1);
			for (temp = -temp; temp; temp++)
			{
				PushToStack (*((BYTE *)pc + temp));
			}
			break;

		case PCD_DUP:
			Stack[sp] = Stack[sp-1];
			sp++;
			break;

		case PCD_SWAP:
			swapvalues(Stack[sp-2], Stack[sp-1]);
			break;

		case PCD_LSPEC1:
			P_ExecuteSpecial(NEXTBYTE, activationline, activator, backSide,
									STACK(1) & specialargmask, 0, 0, 0, 0);
			sp -= 1;
			break;

		case PCD_LSPEC2:
			P_ExecuteSpecial(NEXTBYTE, activationline, activator, backSide,
									STACK(2) & specialargmask,
									STACK(1) & specialargmask, 0, 0, 0);
			sp -= 2;
			break;

		case PCD_LSPEC3:
			P_ExecuteSpecial(NEXTBYTE, activationline, activator, backSide,
									STACK(3) & specialargmask,
									STACK(2) & specialargmask,
									STACK(1) & specialargmask, 0, 0);
			sp -= 3;
			break;

		case PCD_LSPEC4:
			P_ExecuteSpecial(NEXTBYTE, activationline, activator, backSide,
									STACK(4) & specialargmask,
									STACK(3) & specialargmask,
									STACK(2) & specialargmask,
									STACK(1) & specialargmask, 0);
			sp -= 4;
			break;

		case PCD_LSPEC5:
			P_ExecuteSpecial(NEXTBYTE, activationline, activator, backSide,
									STACK(5) & specialargmask,
									STACK(4) & specialargmask,
									STACK(3) & specialargmask,
									STACK(2) & specialargmask,
									STACK(1) & specialargmask);
			sp -= 5;
			break;

		case PCD_LSPEC5RESULT:
			STACK(5) = P_ExecuteSpecial(NEXTBYTE, activationline, activator, backSide,
									STACK(5) & specialargmask,
									STACK(4) & specialargmask,
									STACK(3) & specialargmask,
									STACK(2) & specialargmask,
									STACK(1) & specialargmask);
			sp -= 4;
			break;

		case PCD_LSPEC5EX:
			P_ExecuteSpecial(NEXTWORD, activationline, activator, backSide,
									STACK(5) & specialargmask,
									STACK(4) & specialargmask,
									STACK(3) & specialargmask,
									STACK(2) & specialargmask,
									STACK(1) & specialargmask);
			sp -= 5;
			break;

		case PCD_LSPEC5EXRESULT:
			STACK(5) = P_ExecuteSpecial(NEXTWORD, activationline, activator, backSide,
									STACK(5) & specialargmask,
									STACK(4) & specialargmask,
									STACK(3) & specialargmask,
									STACK(2) & specialargmask,
									STACK(1) & specialargmask);
			sp -= 4;
			break;

		case PCD_LSPEC1DIRECT:
			temp = NEXTBYTE;
			P_ExecuteSpecial(temp, activationline, activator, backSide,
								uallong(pc[0]) & specialargmask ,0, 0, 0, 0);
			pc += 1;
			break;

		case PCD_LSPEC2DIRECT:
			temp = NEXTBYTE;
			P_ExecuteSpecial(temp, activationline, activator, backSide,
								uallong(pc[0]) & specialargmask,
								uallong(pc[1]) & specialargmask, 0, 0, 0);
			pc += 2;
			break;

		case PCD_LSPEC3DIRECT:
			temp = NEXTBYTE;
			P_ExecuteSpecial(temp, activationline, activator, backSide,
								uallong(pc[0]) & specialargmask,
								uallong(pc[1]) & specialargmask,
								uallong(pc[2]) & specialargmask, 0, 0);
			pc += 3;
			break;

		case PCD_LSPEC4DIRECT:
			temp = NEXTBYTE;
			P_ExecuteSpecial(temp, activationline, activator, backSide,
								uallong(pc[0]) & specialargmask,
								uallong(pc[1]) & specialargmask,
								uallong(pc[2]) & specialargmask,
								uallong(pc[3]) & specialargmask, 0);
			pc += 4;
			break;

		case PCD_LSPEC5DIRECT:
			temp = NEXTBYTE;
			P_ExecuteSpecial(temp, activationline, activator, backSide,
								uallong(pc[0]) & specialargmask,
								uallong(pc[1]) & specialargmask,
								uallong(pc[2]) & specialargmask,
								uallong(pc[3]) & specialargmask,
								uallong(pc[4]) & specialargmask);
			pc += 5;
			break;

		// Parameters for PCD_LSPEC?DIRECTB are by definition bytes so never need and-ing.
		case PCD_LSPEC1DIRECTB:
			P_ExecuteSpecial(((BYTE *)pc)[0], activationline, activator, backSide,
				((BYTE *)pc)[1], 0, 0, 0, 0);
			pc = (int *)((BYTE *)pc + 2);
			break;

		case PCD_LSPEC2DIRECTB:
			P_ExecuteSpecial(((BYTE *)pc)[0], activationline, activator, backSide,
				((BYTE *)pc)[1], ((BYTE *)pc)[2], 0, 0, 0);
			pc = (int *)((BYTE *)pc + 3);
			break;

		case PCD_LSPEC3DIRECTB:
			P_ExecuteSpecial(((BYTE *)pc)[0], activationline, activator, backSide,
				((BYTE *)pc)[1], ((BYTE *)pc)[2], ((BYTE *)pc)[3], 0, 0);
			pc = (int *)((BYTE *)pc + 4);
			break;

		case PCD_LSPEC4DIRECTB:
			P_ExecuteSpecial(((BYTE *)pc)[0], activationline, activator, backSide,
				((BYTE *)pc)[1], ((BYTE *)pc)[2], ((BYTE *)pc)[3],
				((BYTE *)pc)[4], 0);
			pc = (int *)((BYTE *)pc + 5);
			break;

		case PCD_LSPEC5DIRECTB:
			P_ExecuteSpecial(((BYTE *)pc)[0], activationline, activator, backSide,
				((BYTE *)pc)[1], ((BYTE *)pc)[2], ((BYTE *)pc)[3],
				((BYTE *)pc)[4], ((BYTE *)pc)[5]);
			pc = (int *)((BYTE *)pc + 6);
			break;

		case PCD_CALLFUNC:
			{
				int argCount = NEXTBYTE;
				int funcIndex = NEXTSHORT;

				int retval = CallFunction(argCount, funcIndex, &STACK(argCount));
				sp -= argCount-1;
				STACK(1) = retval;
			}
			break;

		case PCD_PUSHFUNCTION:
		{
			int funcnum = NEXTBYTE;
			// Not technically a string, but since we use the same tagging mechanism
			PushToStack(TAGSTR(funcnum));
			break;
		}
		case PCD_CALL:
		case PCD_CALLDISCARD:
		case PCD_CALLSTACK:
			{
				int funcnum;
				int i;
				ScriptFunction *func;
				FBehavior *module;
				SDWORD *mylocals;

				if(pcd == PCD_CALLSTACK)
				{
					funcnum = STACK(1);
					module = FBehavior::StaticGetModule(funcnum>>LIBRARYID_SHIFT);
					--sp;

					funcnum &= 0xFFFF; // Clear out tag
				}
				else
				{
					module = activeBehavior;
					funcnum = NEXTBYTE;
				}
				func = module->GetFunction (funcnum, module);

				if (func == NULL)
				{
					Printf ("Function %d in %s out of range\n", funcnum, ScriptPresentation(script).GetChars());
					state = SCRIPT_PleaseRemove;
					break;
				}
				if (sp + func->LocalCount + 64 > STACK_SIZE)
				{ // 64 is the margin for the function's working space
					Printf ("Out of stack space in %s\n", ScriptPresentation(script).GetChars());
					state = SCRIPT_PleaseRemove;
					break;
				}
				mylocals = locals;
				// The function's first argument is also its first local variable.
				locals = &Stack[sp - func->ArgCount];
				// Make space on the stack for any other variables the function uses.
				for (i = 0; i < func->LocalCount; ++i)
				{
					Stack[sp+i] = 0;
				}
				sp += i;
				::new(&Stack[sp]) CallReturn(activeBehavior->PC2Ofs(pc), activeFunction,
					activeBehavior, mylocals, localarrays, pcd == PCD_CALLDISCARD, runaway);
				sp += (sizeof(CallReturn) + sizeof(int) - 1) / sizeof(int);
				pc = module->Ofs2PC (func->Address);
				localarrays = &func->LocalArrays;
				activeFunction = func;
				activeBehavior = module;
				fmt = module->GetFormat();
			}
			break;

		case PCD_RETURNVOID:
		case PCD_RETURNVAL:
			{
				int value;
				union
				{
					SDWORD *retsp;
					CallReturn *ret;
				};

				if (pcd == PCD_RETURNVAL)
				{
					value = Stack[--sp];
				}
				else
				{
					value = 0;
				}
				sp -= sizeof(CallReturn)/sizeof(int);
				retsp = &Stack[sp];
				activeBehavior->GetFunctionProfileData(activeFunction)->AddRun(runaway - ret->EntryInstrCount);
				sp = int(locals - Stack);
				pc = ret->ReturnModule->Ofs2PC(ret->ReturnAddress);
				activeFunction = ret->ReturnFunction;
				activeBehavior = ret->ReturnModule;
				fmt = activeBehavior->GetFormat();
				locals = ret->ReturnLocals;
				localarrays = ret->ReturnArrays;
				if (!ret->bDiscardResult)
				{
					Stack[sp++] = value;
				}
				ret->~CallReturn();
			}
			break;

		case PCD_ADD:
			STACK(2) = STACK(2) + STACK(1);
			sp--;
			break;

		case PCD_SUBTRACT:
			STACK(2) = STACK(2) - STACK(1);
			sp--;
			break;

		case PCD_MULTIPLY:
			STACK(2) = STACK(2) * STACK(1);
			sp--;
			break;

		case PCD_DIVIDE:
			if (STACK(1) == 0)
			{
				state = SCRIPT_DivideBy0;
			}
			else
			{
				STACK(2) = STACK(2) / STACK(1);
				sp--;
			}
			break;

		case PCD_MODULUS:
			if (STACK(1) == 0)
			{
				state = SCRIPT_ModulusBy0;
			}
			else
			{
				STACK(2) = STACK(2) % STACK(1);
				sp--;
			}
			break;

		case PCD_EQ:
			STACK(2) = (STACK(2) == STACK(1));
			sp--;
			break;

		case PCD_NE:
			STACK(2) = (STACK(2) != STACK(1));
			sp--;
			break;

		case PCD_LT:
			STACK(2) = (STACK(2) < STACK(1));
			sp--;
			break;

		case PCD_GT:
			STACK(2) = (STACK(2) > STACK(1));
			sp--;
			break;

		case PCD_LE:
			STACK(2) = (STACK(2) <= STACK(1));
			sp--;
			break;

		case PCD_GE:
			STACK(2) = (STACK(2) >= STACK(1));
			sp--;
			break;

		case PCD_ASSIGNSCRIPTVAR:
			locals[NEXTBYTE] = STACK(1);
			sp--;
			break;


		case PCD_ASSIGNMAPVAR:
			*(activeBehavior->MapVars[NEXTBYTE]) = STACK(1);
			sp--;
			break;

		case PCD_ASSIGNWORLDVAR:
			ACS_WorldVars[NEXTBYTE] = STACK(1);
			sp--;
			break;

		case PCD_ASSIGNGLOBALVAR:
			ACS_GlobalVars[NEXTBYTE] = STACK(1);
			sp--;
			break;

		case PCD_ASSIGNSCRIPTARRAY:
			localarrays->Set(locals, NEXTBYTE, STACK(2), STACK(1));
			sp -= 2;
			break;

		case PCD_ASSIGNMAPARRAY:
			activeBehavior->SetArrayVal (*(activeBehavior->MapVars[NEXTBYTE]), STACK(2), STACK(1));
			sp -= 2;
			break;

		case PCD_ASSIGNWORLDARRAY:
			ACS_WorldArrays[NEXTBYTE][STACK(2)] = STACK(1);
			sp -= 2;
			break;

		case PCD_ASSIGNGLOBALARRAY:
			ACS_GlobalArrays[NEXTBYTE][STACK(2)] = STACK(1);
			sp -= 2;
			break;

		case PCD_PUSHSCRIPTVAR:
			PushToStack (locals[NEXTBYTE]);
			break;

		case PCD_PUSHMAPVAR:
			PushToStack (*(activeBehavior->MapVars[NEXTBYTE]));
			break;

		case PCD_PUSHWORLDVAR:
			PushToStack (ACS_WorldVars[NEXTBYTE]);
			break;

		case PCD_PUSHGLOBALVAR:
			PushToStack (ACS_GlobalVars[NEXTBYTE]);
			break;

		case PCD_PUSHSCRIPTARRAY:
			STACK(1) = localarrays->Get(locals, NEXTBYTE, STACK(1));
			break;

		case PCD_PUSHMAPARRAY:
			STACK(1) = activeBehavior->GetArrayVal (*(activeBehavior->MapVars[NEXTBYTE]), STACK(1));
			break;

		case PCD_PUSHWORLDARRAY:
			STACK(1) = ACS_WorldArrays[NEXTBYTE][STACK(1)];
			break;

		case PCD_PUSHGLOBALARRAY:
			STACK(1) = ACS_GlobalArrays[NEXTBYTE][STACK(1)];
			break;

		case PCD_ADDSCRIPTVAR:
			locals[NEXTBYTE] += STACK(1);
			sp--;
			break;

		case PCD_ADDMAPVAR:
			*(activeBehavior->MapVars[NEXTBYTE]) += STACK(1);
			sp--;
			break;

		case PCD_ADDWORLDVAR:
			ACS_WorldVars[NEXTBYTE] += STACK(1);
			sp--;
			break;

		case PCD_ADDGLOBALVAR:
			ACS_GlobalVars[NEXTBYTE] += STACK(1);
			sp--;
			break;

		case PCD_ADDSCRIPTARRAY:
			{
				int a = NEXTBYTE, i = STACK(2);
				localarrays->Set(locals, a, i, localarrays->Get(locals, a, i) + STACK(1));
				sp -= 2;
			}
			break;

		case PCD_ADDMAPARRAY:
			{
				int a = *(activeBehavior->MapVars[NEXTBYTE]);
				int i = STACK(2);
				activeBehavior->SetArrayVal (a, i, activeBehavior->GetArrayVal (a, i) + STACK(1));
				sp -= 2;
			}
			break;

		case PCD_ADDWORLDARRAY:
			{
				int a = NEXTBYTE;
				ACS_WorldArrays[a][STACK(2)] += STACK(1);
				sp -= 2;
			}
			break;

		case PCD_ADDGLOBALARRAY:
			{
				int a = NEXTBYTE;
				ACS_GlobalArrays[a][STACK(2)] += STACK(1);
				sp -= 2;
			}
			break;

		case PCD_SUBSCRIPTVAR:
			locals[NEXTBYTE] -= STACK(1);
			sp--;
			break;

		case PCD_SUBMAPVAR:
			*(activeBehavior->MapVars[NEXTBYTE]) -= STACK(1);
			sp--;
			break;

		case PCD_SUBWORLDVAR:
			ACS_WorldVars[NEXTBYTE] -= STACK(1);
			sp--;
			break;

		case PCD_SUBGLOBALVAR:
			ACS_GlobalVars[NEXTBYTE] -= STACK(1);
			sp--;
			break;

		case PCD_SUBSCRIPTARRAY:
			{
				int a = NEXTBYTE, i = STACK(2);
				localarrays->Set(locals, a, i, localarrays->Get(locals, a, i) - STACK(1));
				sp -= 2;
			}
			break;

		case PCD_SUBMAPARRAY:
			{
				int a = *(activeBehavior->MapVars[NEXTBYTE]);
				int i = STACK(2);
				activeBehavior->SetArrayVal (a, i, activeBehavior->GetArrayVal (a, i) - STACK(1));
				sp -= 2;
			}
			break;

		case PCD_SUBWORLDARRAY:
			{
				int a = NEXTBYTE;
				ACS_WorldArrays[a][STACK(2)] -= STACK(1);
				sp -= 2;
			}
			break;

		case PCD_SUBGLOBALARRAY:
			{
				int a = NEXTBYTE;
				ACS_GlobalArrays[a][STACK(2)] -= STACK(1);
				sp -= 2;
			}
			break;

		case PCD_MULSCRIPTVAR:
			locals[NEXTBYTE] *= STACK(1);
			sp--;
			break;

		case PCD_MULMAPVAR:
			*(activeBehavior->MapVars[NEXTBYTE]) *= STACK(1);
			sp--;
			break;

		case PCD_MULWORLDVAR:
			ACS_WorldVars[NEXTBYTE] *= STACK(1);
			sp--;
			break;

		case PCD_MULGLOBALVAR:
			ACS_GlobalVars[NEXTBYTE] *= STACK(1);
			sp--;
			break;

		case PCD_MULSCRIPTARRAY:
			{
				int a = NEXTBYTE, i = STACK(2);
				localarrays->Set(locals, a, i, localarrays->Get(locals, a, i) * STACK(1));
				sp -= 2;
			}
			break;

		case PCD_MULMAPARRAY:
			{
				int a = *(activeBehavior->MapVars[NEXTBYTE]);
				int i = STACK(2);
				activeBehavior->SetArrayVal (a, i, activeBehavior->GetArrayVal (a, i) * STACK(1));
				sp -= 2;
			}
			break;

		case PCD_MULWORLDARRAY:
			{
				int a = NEXTBYTE;
				ACS_WorldArrays[a][STACK(2)] *= STACK(1);
				sp -= 2;
			}
			break;

		case PCD_MULGLOBALARRAY:
			{
				int a = NEXTBYTE;
				ACS_GlobalArrays[a][STACK(2)] *= STACK(1);
				sp -= 2;
			}
			break;

		case PCD_DIVSCRIPTVAR:
			if (STACK(1) == 0)
			{
				state = SCRIPT_DivideBy0;
			}
			else
			{
				locals[NEXTBYTE] /= STACK(1);
				sp--;
			}
			break;

		case PCD_DIVMAPVAR:
			if (STACK(1) == 0)
			{
				state = SCRIPT_DivideBy0;
			}
			else
			{
				*(activeBehavior->MapVars[NEXTBYTE]) /= STACK(1);
				sp--;
			}
			break;

		case PCD_DIVWORLDVAR:
			if (STACK(1) == 0)
			{
				state = SCRIPT_DivideBy0;
			}
			else
			{
				ACS_WorldVars[NEXTBYTE] /= STACK(1);
				sp--;
			}
			break;

		case PCD_DIVGLOBALVAR:
			if (STACK(1) == 0)
			{
				state = SCRIPT_DivideBy0;
			}
			else
			{
				ACS_GlobalVars[NEXTBYTE] /= STACK(1);
				sp--;
			}
			break;

		case PCD_DIVSCRIPTARRAY:
			if (STACK(1) == 0)
			{
				state = SCRIPT_DivideBy0;
			}
			else
			{
				int a = NEXTBYTE, i = STACK(2);
				localarrays->Set(locals, a, i, localarrays->Get(locals, a, i) / STACK(1));
				sp -= 2;
			}
			break;

		case PCD_DIVMAPARRAY:
			if (STACK(1) == 0)
			{
				state = SCRIPT_DivideBy0;
			}
			else
			{
				int a = *(activeBehavior->MapVars[NEXTBYTE]);
				int i = STACK(2);
				activeBehavior->SetArrayVal (a, i, activeBehavior->GetArrayVal (a, i) / STACK(1));
				sp -= 2;
			}
			break;

		case PCD_DIVWORLDARRAY:
			if (STACK(1) == 0)
			{
				state = SCRIPT_DivideBy0;
			}
			else
			{
				int a = NEXTBYTE;
				ACS_WorldArrays[a][STACK(2)] /= STACK(1);
				sp -= 2;
			}
			break;

		case PCD_DIVGLOBALARRAY:
			if (STACK(1) == 0)
			{
				state = SCRIPT_DivideBy0;
			}
			else
			{
				int a = NEXTBYTE;
				ACS_GlobalArrays[a][STACK(2)] /= STACK(1);
				sp -= 2;
			}
			break;

		case PCD_MODSCRIPTVAR:
			if (STACK(1) == 0)
			{
				state = SCRIPT_ModulusBy0;
			}
			else
			{
				locals[NEXTBYTE] %= STACK(1);
				sp--;
			}
			break;

		case PCD_MODMAPVAR:
			if (STACK(1) == 0)
			{
				state = SCRIPT_ModulusBy0;
			}
			else
			{
				*(activeBehavior->MapVars[NEXTBYTE]) %= STACK(1);
				sp--;
			}
			break;

		case PCD_MODWORLDVAR:
			if (STACK(1) == 0)
			{
				state = SCRIPT_ModulusBy0;
			}
			else
			{
				ACS_WorldVars[NEXTBYTE] %= STACK(1);
				sp--;
			}
			break;

		case PCD_MODGLOBALVAR:
			if (STACK(1) == 0)
			{
				state = SCRIPT_ModulusBy0;
			}
			else
			{
				ACS_GlobalVars[NEXTBYTE] %= STACK(1);
				sp--;
			}
			break;

		case PCD_MODSCRIPTARRAY:
			if (STACK(1) == 0)
			{
				state = SCRIPT_ModulusBy0;
			}
			else
			{
				int a = NEXTBYTE, i = STACK(2);
				localarrays->Set(locals, a, i, localarrays->Get(locals, a, i) % STACK(1));
				sp -= 2;
			}
			break;

		case PCD_MODMAPARRAY:
			if (STACK(1) == 0)
			{
				state = SCRIPT_ModulusBy0;
			}
			else
			{
				int a = *(activeBehavior->MapVars[NEXTBYTE]);
				int i = STACK(2);
				activeBehavior->SetArrayVal (a, i, activeBehavior->GetArrayVal (a, i) % STACK(1));
				sp -= 2;
			}
			break;

		case PCD_MODWORLDARRAY:
			if (STACK(1) == 0)
			{
				state = SCRIPT_ModulusBy0;
			}
			else
			{
				int a = NEXTBYTE;
				ACS_WorldArrays[a][STACK(2)] %= STACK(1);
				sp -= 2;
			}
			break;

		case PCD_MODGLOBALARRAY:
			if (STACK(1) == 0)
			{
				state = SCRIPT_ModulusBy0;
			}
			else
			{
				int a = NEXTBYTE;
				ACS_GlobalArrays[a][STACK(2)] %= STACK(1);
				sp -= 2;
			}
			break;

		//[MW] start
		case PCD_ANDSCRIPTVAR:
			locals[NEXTBYTE] &= STACK(1);
			sp--;
			break;

		case PCD_ANDMAPVAR:
			*(activeBehavior->MapVars[NEXTBYTE]) &= STACK(1);
			sp--;
			break;

		case PCD_ANDWORLDVAR:
			ACS_WorldVars[NEXTBYTE] &= STACK(1);
			sp--;
			break;

		case PCD_ANDGLOBALVAR:
			ACS_GlobalVars[NEXTBYTE] &= STACK(1);
			sp--;
			break;

		case PCD_ANDSCRIPTARRAY:
			{
				int a = NEXTBYTE, i = STACK(2);
				localarrays->Set(locals, a, i, localarrays->Get(locals, a, i) & STACK(1));
				sp -= 2;
			}
			break;

		case PCD_ANDMAPARRAY:
			{
				int a = *(activeBehavior->MapVars[NEXTBYTE]);
				int i = STACK(2);
				activeBehavior->SetArrayVal (a, i, activeBehavior->GetArrayVal (a, i) & STACK(1));
				sp -= 2;
			}
			break;

		case PCD_ANDWORLDARRAY:
			{
				int a = NEXTBYTE;
				ACS_WorldArrays[a][STACK(2)] &= STACK(1);
				sp -= 2;
			}
			break;

		case PCD_ANDGLOBALARRAY:
			{
				int a = NEXTBYTE;
				ACS_GlobalArrays[a][STACK(2)] &= STACK(1);
				sp -= 2;
			}
			break;

		case PCD_EORSCRIPTVAR:
			locals[NEXTBYTE] ^= STACK(1);
			sp--;
			break;

		case PCD_EORMAPVAR:
			*(activeBehavior->MapVars[NEXTBYTE]) ^= STACK(1);
			sp--;
			break;

		case PCD_EORWORLDVAR:
			ACS_WorldVars[NEXTBYTE] ^= STACK(1);
			sp--;
			break;

		case PCD_EORGLOBALVAR:
			ACS_GlobalVars[NEXTBYTE] ^= STACK(1);
			sp--;
			break;

		case PCD_EORSCRIPTARRAY:
			{
				int a = NEXTBYTE, i = STACK(2);
				localarrays->Set(locals, a, i, localarrays->Get(locals, a, i) ^ STACK(1));
				sp -= 2;
			}
			break;

		case PCD_EORMAPARRAY:
			{
				int a = *(activeBehavior->MapVars[NEXTBYTE]);
				int i = STACK(2);
				activeBehavior->SetArrayVal (a, i, activeBehavior->GetArrayVal (a, i) ^ STACK(1));
				sp -= 2;
			}
			break;

		case PCD_EORWORLDARRAY:
			{
				int a = NEXTBYTE;
				ACS_WorldArrays[a][STACK(2)] ^= STACK(1);
				sp -= 2;
			}
			break;

		case PCD_EORGLOBALARRAY:
			{
				int a = NEXTBYTE;
				ACS_GlobalArrays[a][STACK(2)] ^= STACK(1);
				sp -= 2;
			}
			break;

		case PCD_ORSCRIPTVAR:
			locals[NEXTBYTE] |= STACK(1);
			sp--;
			break;

		case PCD_ORMAPVAR:
			*(activeBehavior->MapVars[NEXTBYTE]) |= STACK(1);
			sp--;
			break;

		case PCD_ORWORLDVAR:
			ACS_WorldVars[NEXTBYTE] |= STACK(1);
			sp--;
			break;

		case PCD_ORGLOBALVAR:
			ACS_GlobalVars[NEXTBYTE] |= STACK(1);
			sp--;
			break;

		case PCD_ORSCRIPTARRAY:
			{
				int a = NEXTBYTE, i = STACK(2);
				localarrays->Set(locals, a, i, localarrays->Get(locals, a, i) | STACK(1));
				sp -= 2;
			}
			break;

		case PCD_ORMAPARRAY:
			{
				int a = *(activeBehavior->MapVars[NEXTBYTE]);
				int i = STACK(2);
				activeBehavior->SetArrayVal (a, i, activeBehavior->GetArrayVal (a, i) | STACK(1));
				sp -= 2;
			}
			break;

		case PCD_ORWORLDARRAY:
			{
				int a = NEXTBYTE;
				ACS_WorldArrays[a][STACK(2)] |= STACK(1);
				sp -= 2;
			}
			break;

		case PCD_ORGLOBALARRAY:
			{
				int a = NEXTBYTE;
				int i = STACK(2);
				ACS_GlobalArrays[a][STACK(2)] |= STACK(1);
				sp -= 2;
			}
			break;

		case PCD_LSSCRIPTVAR:
			locals[NEXTBYTE] <<= STACK(1);
			sp--;
			break;

		case PCD_LSMAPVAR:
			*(activeBehavior->MapVars[NEXTBYTE]) <<= STACK(1);
			sp--;
			break;

		case PCD_LSWORLDVAR:
			ACS_WorldVars[NEXTBYTE] <<= STACK(1);
			sp--;
			break;

		case PCD_LSGLOBALVAR:
			ACS_GlobalVars[NEXTBYTE] <<= STACK(1);
			sp--;
			break;

		case PCD_LSSCRIPTARRAY:
			{
				int a = NEXTBYTE, i = STACK(2);
				localarrays->Set(locals, a, i, localarrays->Get(locals, a, i) << STACK(1));
				sp -= 2;
			}
			break;

		case PCD_LSMAPARRAY:
			{
				int a = *(activeBehavior->MapVars[NEXTBYTE]);
				int i = STACK(2);
				activeBehavior->SetArrayVal (a, i, activeBehavior->GetArrayVal (a, i) << STACK(1));
				sp -= 2;
			}
			break;

		case PCD_LSWORLDARRAY:
			{
				int a = NEXTBYTE;
				ACS_WorldArrays[a][STACK(2)] <<= STACK(1);
				sp -= 2;
			}
			break;

		case PCD_LSGLOBALARRAY:
			{
				int a = NEXTBYTE;
				ACS_GlobalArrays[a][STACK(2)] <<= STACK(1);
				sp -= 2;
			}
			break;

		case PCD_RSSCRIPTVAR:
			locals[NEXTBYTE] >>= STACK(1);
			sp--;
			break;

		case PCD_RSMAPVAR:
			*(activeBehavior->MapVars[NEXTBYTE]) >>= STACK(1);
			sp--;
			break;

		case PCD_RSWORLDVAR:
			ACS_WorldVars[NEXTBYTE] >>= STACK(1);
			sp--;
			break;

		case PCD_RSGLOBALVAR:
			ACS_GlobalVars[NEXTBYTE] >>= STACK(1);
			sp--;
			break;

		case PCD_RSSCRIPTARRAY:
			{
				int a = NEXTBYTE, i = STACK(2);
				localarrays->Set(locals, a, i, localarrays->Get(locals, a, i) >> STACK(1));
				sp -= 2;
			}
			break;

		case PCD_RSMAPARRAY:
			{
				int a = *(activeBehavior->MapVars[NEXTBYTE]);
				int i = STACK(2);
				activeBehavior->SetArrayVal (a, i, activeBehavior->GetArrayVal (a, i) >> STACK(1));
				sp -= 2;
			}
			break;

		case PCD_RSWORLDARRAY:
			{
				int a = NEXTBYTE;
				ACS_WorldArrays[a][STACK(2)] >>= STACK(1);
				sp -= 2;
			}
			break;

		case PCD_RSGLOBALARRAY:
			{
				int a = NEXTBYTE;
				ACS_GlobalArrays[a][STACK(2)] >>= STACK(1);
				sp -= 2;
			}
			break;
		//[MW] end

		case PCD_INCSCRIPTVAR:
			++locals[NEXTBYTE];
			break;

		case PCD_INCMAPVAR:
			*(activeBehavior->MapVars[NEXTBYTE]) += 1;
			break;

		case PCD_INCWORLDVAR:
			++ACS_WorldVars[NEXTBYTE];
			break;

		case PCD_INCGLOBALVAR:
			++ACS_GlobalVars[NEXTBYTE];
			break;

		case PCD_INCSCRIPTARRAY:
			{
				int a = NEXTBYTE, i = STACK(1);
				localarrays->Set(locals, a, i, localarrays->Get(locals, a, i) + 1);
				sp--;
			}
			break;

		case PCD_INCMAPARRAY:
			{
				int a = *(activeBehavior->MapVars[NEXTBYTE]);
				int i = STACK(1);
				activeBehavior->SetArrayVal (a, i, activeBehavior->GetArrayVal (a, i) + 1);
				sp--;
			}
			break;

		case PCD_INCWORLDARRAY:
			{
				int a = NEXTBYTE;
				ACS_WorldArrays[a][STACK(1)] += 1;
				sp--;
			}
			break;

		case PCD_INCGLOBALARRAY:
			{
				int a = NEXTBYTE;
				ACS_GlobalArrays[a][STACK(1)] += 1;
				sp--;
			}
			break;

		case PCD_DECSCRIPTVAR:
			--locals[NEXTBYTE];
			break;

		case PCD_DECMAPVAR:
			*(activeBehavior->MapVars[NEXTBYTE]) -= 1;
			break;

		case PCD_DECWORLDVAR:
			--ACS_WorldVars[NEXTBYTE];
			break;

		case PCD_DECGLOBALVAR:
			--ACS_GlobalVars[NEXTBYTE];
			break;

		case PCD_DECSCRIPTARRAY:
			{
				int a = NEXTBYTE, i = STACK(1);
				localarrays->Set(locals, a, i, localarrays->Get(locals, a, i) - 1);
				sp--;
			}
			break;

		case PCD_DECMAPARRAY:
			{
				int a = *(activeBehavior->MapVars[NEXTBYTE]);
				int i = STACK(1);
				activeBehavior->SetArrayVal (a, i, activeBehavior->GetArrayVal (a, i) - 1);
				sp--;
			}
			break;

		case PCD_DECWORLDARRAY:
			{
				int a = NEXTBYTE;
				ACS_WorldArrays[a][STACK(1)] -= 1;
				sp--;
			}
			break;

		case PCD_DECGLOBALARRAY:
			{
				int a = NEXTBYTE;
				int i = STACK(1);
				ACS_GlobalArrays[a][STACK(1)] -= 1;
				sp--;
			}
			break;

		case PCD_GOTO:
			pc = activeBehavior->Ofs2PC (LittleLong(*pc));
			break;

		case PCD_GOTOSTACK:
			pc = activeBehavior->Jump2PC (STACK(1));
			sp--;
			break;

		case PCD_IFGOTO:
			if (STACK(1))
				pc = activeBehavior->Ofs2PC (LittleLong(*pc));
			else
				pc++;
			sp--;
			break;

		case PCD_SETRESULTVALUE:
			resultValue = STACK(1);
		case PCD_DROP: //fall through.
			sp--;
			break;

		case PCD_DELAY:
			statedata = STACK(1) + (fmt == ACS_Old && gameinfo.gametype == GAME_Hexen);
			if (statedata > 0)
			{
				state = SCRIPT_Delayed;
			}
			sp--;
			break;

		case PCD_DELAYDIRECT:
			statedata = uallong(pc[0]) + (fmt == ACS_Old && gameinfo.gametype == GAME_Hexen);
			pc++;
			if (statedata > 0)
			{
				state = SCRIPT_Delayed;
			}
			break;

		case PCD_DELAYDIRECTB:
			statedata = *(BYTE *)pc + (fmt == ACS_Old && gameinfo.gametype == GAME_Hexen);
			if (statedata > 0)
			{
				state = SCRIPT_Delayed;
			}
			pc = (int *)((BYTE *)pc + 1);
			break;

		case PCD_RANDOM:
			STACK(2) = Random (STACK(2), STACK(1));
			sp--;
			break;

		case PCD_RANDOMDIRECT:
			PushToStack (Random (uallong(pc[0]), uallong(pc[1])));
			pc += 2;
			break;

		case PCD_RANDOMDIRECTB:
			PushToStack (Random (((BYTE *)pc)[0], ((BYTE *)pc)[1]));
			pc = (int *)((BYTE *)pc + 2);
			break;

		case PCD_THINGCOUNT:
			STACK(2) = ThingCount (STACK(2), -1, STACK(1), -1);
			sp--;
			break;

		case PCD_THINGCOUNTDIRECT:
			PushToStack (ThingCount (uallong(pc[0]), -1, uallong(pc[1]), -1));
			pc += 2;
			break;

		case PCD_THINGCOUNTNAME:
			STACK(2) = ThingCount (-1, STACK(2), STACK(1), -1);
			sp--;
			break;

		case PCD_THINGCOUNTNAMESECTOR:
			STACK(3) = ThingCount (-1, STACK(3), STACK(2), STACK(1));
			sp -= 2;
			break;

		case PCD_THINGCOUNTSECTOR:
			STACK(3) = ThingCount (STACK(3), -1, STACK(2), STACK(1));
			sp -= 2;
			break;

		case PCD_TAGWAIT:
			state = SCRIPT_TagWait;
			statedata = STACK(1);
			sp--;
			break;

		case PCD_TAGWAITDIRECT:
			state = SCRIPT_TagWait;
			statedata = uallong(pc[0]);
			pc++;
			break;

		case PCD_POLYWAIT:
			state = SCRIPT_PolyWait;
			statedata = STACK(1);
			sp--;
			break;

		case PCD_POLYWAITDIRECT:
			state = SCRIPT_PolyWait;
			statedata = uallong(pc[0]);
			pc++;
			break;

		case PCD_CHANGEFLOOR:
			ChangeFlat (STACK(2), STACK(1), 0);
			sp -= 2;
			break;

		case PCD_CHANGEFLOORDIRECT:
			ChangeFlat (uallong(pc[0]), TAGSTR(uallong(pc[1])), 0);
			pc += 2;
			break;

		case PCD_CHANGECEILING:
			ChangeFlat (STACK(2), STACK(1), 1);
			sp -= 2;
			break;

		case PCD_CHANGECEILINGDIRECT:
			ChangeFlat (uallong(pc[0]), TAGSTR(uallong(pc[1])), 1);
			pc += 2;
			break;

		case PCD_RESTART:
			{
				const ScriptPtr *scriptp;

				scriptp = activeBehavior->FindScript (script);
				pc = activeBehavior->GetScriptAddress (scriptp);
			}
			break;

		case PCD_ANDLOGICAL:
			STACK(2) = (STACK(2) && STACK(1));
			sp--;
			break;

		case PCD_ORLOGICAL:
			STACK(2) = (STACK(2) || STACK(1));
			sp--;
			break;

		case PCD_ANDBITWISE:
			STACK(2) = (STACK(2) & STACK(1));
			sp--;
			break;

		case PCD_ORBITWISE:
			STACK(2) = (STACK(2) | STACK(1));
			sp--;
			break;

		case PCD_EORBITWISE:
			STACK(2) = (STACK(2) ^ STACK(1));
			sp--;
			break;

		case PCD_NEGATELOGICAL:
			STACK(1) = !STACK(1);
			break;




		case PCD_NEGATEBINARY:
			STACK(1) = ~STACK(1);
			break;

		case PCD_LSHIFT:
			STACK(2) = (STACK(2) << STACK(1));
			sp--;
			break;

		case PCD_RSHIFT:
			STACK(2) = (STACK(2) >> STACK(1));
			sp--;
			break;

		case PCD_UNARYMINUS:
			STACK(1) = -STACK(1);
			break;

		case PCD_IFNOTGOTO:
			if (!STACK(1))
				pc = activeBehavior->Ofs2PC (LittleLong(*pc));
			else
				pc++;
			sp--;
			break;

		case PCD_LINESIDE:
			PushToStack (backSide);
			break;

		case PCD_SCRIPTWAIT:
			statedata = STACK(1);
			sp--;
scriptwait:
			if (controller->RunningScripts.CheckKey(statedata) != NULL)
				state = SCRIPT_ScriptWait;
			else
				state = SCRIPT_ScriptWaitPre;
			PutLast ();
			break;

		case PCD_SCRIPTWAITDIRECT:
			statedata = uallong(pc[0]);
			pc++;
			goto scriptwait;

		case PCD_SCRIPTWAITNAMED:
			statedata = -FName(FBehavior::StaticLookupString(STACK(1)));
			sp--;
			goto scriptwait;

		case PCD_CLEARLINESPECIAL:
			if (activationline != NULL)
			{
				activationline->special = 0;
				DPrintf("Cleared line special on line %d\n", (int)(activationline - lines));
			}
			break;

		case PCD_CASEGOTO:
			if (STACK(1) == uallong(pc[0]))
			{
				pc = activeBehavior->Ofs2PC (uallong(pc[1]));
				sp--;
			}
			else
			{
				pc += 2;
			}
			break;

		case PCD_CASEGOTOSORTED:
			// The count and jump table are 4-byte aligned
			pc = (int *)(((size_t)pc + 3) & ~3);
			{
				int numcases = uallong(pc[0]); pc++;
				int min = 0, max = numcases-1;
				while (min <= max)
				{
					int mid = (min + max) / 2;
					SDWORD caseval = LittleLong(pc[mid*2]);
					if (caseval == STACK(1))
					{
						pc = activeBehavior->Ofs2PC (LittleLong(pc[mid*2+1]));
						sp--;
						break;
					}
					else if (caseval < STACK(1))
					{
						min = mid + 1;
					}
					else
					{
						max = mid - 1;
					}
				}
				if (min > max)
				{
					// The case was not found, so go to the next instruction.
					pc += numcases * 2;
				}
			}
			break;

		case PCD_BEGINPRINT:
			STRINGBUILDER_START(work);
			break;

		case PCD_PRINTSTRING:
		case PCD_PRINTLOCALIZED:
			lookup = FBehavior::StaticLookupString (STACK(1));
			if (pcd == PCD_PRINTLOCALIZED)
			{
				lookup = GStrings(lookup);
			}
			if (lookup != NULL)
			{
				work += lookup;
			}
			--sp;
			break;

		case PCD_PRINTNUMBER:
			work.AppendFormat ("%d", STACK(1));
			--sp;
			break;

		case PCD_PRINTBINARY:
			IGNORE_FORMAT_PRE
			work.AppendFormat ("%B", STACK(1));
			IGNORE_FORMAT_POST
			--sp;
			break;

		case PCD_PRINTHEX:
			work.AppendFormat ("%X", STACK(1));
			--sp;
			break;

		case PCD_PRINTCHARACTER:
			work += (char)STACK(1);
			--sp;
			break;

		case PCD_PRINTFIXED:
			work.AppendFormat ("%g", ACSToDouble(STACK(1)));
			--sp;
			break;

		// [BC] Print activator's name
		// [RH] Fancied up a bit
		case PCD_PRINTNAME:
			{
				player_t *player = NULL;

				if (STACK(1) < 0)
				{
					switch (STACK(1))
					{
					case PRINTNAME_LEVELNAME:
						work += level.LevelName;
						break;

					case PRINTNAME_LEVEL:
					{
						FString uppername = level.MapName;
						uppername.ToUpper();
						work += uppername;
						break; 
					}

					case PRINTNAME_SKILL:
						work += G_SkillName();
						break;

					default:
						work += ' ';
						break;
					}
					sp--;
					break;

				}
				else if (STACK(1) == 0 || (unsigned)STACK(1) > MAXPLAYERS)
				{
					if (activator)
					{
						player = activator->player;
					}
				}
				else if (playeringame[STACK(1)-1])
				{
					player = &players[STACK(1)-1];
				}
				else
				{
					work.AppendFormat ("Player %d", STACK(1));
					sp--;
					break;
				}
				if (player)
				{
					work += player->userinfo.GetName();
				}
				else if (activator)
				{
					work += activator->GetTag();
				}
				else
				{
					work += ' ';
				}
				sp--;
			}
			break;

		// Print script character array
		case PCD_PRINTSCRIPTCHARARRAY:
		case PCD_PRINTSCRIPTCHRANGE:
			{
				int capacity, offset, a, c;
				if (CharArrayParms(capacity, offset, a, Stack, sp, pcd == PCD_PRINTSCRIPTCHRANGE))
				{
					while (capacity-- && (c = localarrays->Get(locals, a, offset)) != '\0')
					{
						work += (char)c;
						offset++;
					}
				}
			}
			break;

		// [JB] Print map character array
		case PCD_PRINTMAPCHARARRAY:
		case PCD_PRINTMAPCHRANGE:
			{
				int capacity, offset, a, c;
				if (CharArrayParms(capacity, offset, a, Stack, sp, pcd == PCD_PRINTMAPCHRANGE))
				{
					while (capacity-- && (c = activeBehavior->GetArrayVal (a, offset)) != '\0')
					{
						work += (char)c;
						offset++;
					}
				}
			}
			break;

		// [JB] Print world character array
		case PCD_PRINTWORLDCHARARRAY:
		case PCD_PRINTWORLDCHRANGE:
			{
				int capacity, offset, a, c;
				if (CharArrayParms(capacity, offset, a, Stack, sp, pcd == PCD_PRINTWORLDCHRANGE))
				{
					while (capacity-- && (c = ACS_WorldArrays[a][offset]) != '\0')
					{
						work += (char)c;
						offset++;
					}
				}
			}
			break;

		// [JB] Print global character array
		case PCD_PRINTGLOBALCHARARRAY:
		case PCD_PRINTGLOBALCHRANGE:
			{
				int capacity, offset, a, c;
				if (CharArrayParms(capacity, offset, a, Stack, sp, pcd == PCD_PRINTGLOBALCHRANGE))
				{
					while (capacity-- && (c = ACS_GlobalArrays[a][offset]) != '\0')
					{
						work += (char)c;
						offset++;
					}
				}
			}
			break;

		// [GRB] Print key name(s) for a command
		case PCD_PRINTBIND:
			lookup = FBehavior::StaticLookupString (STACK(1));
			if (lookup != NULL)
			{
				int key1 = 0, key2 = 0;

				Bindings.GetKeysForCommand ((char *)lookup, &key1, &key2);

				if (key2)
					work << KeyNames[key1] << " or " << KeyNames[key2];
				else if (key1)
					work << KeyNames[key1];
				else
					work << "??? (" << (char *)lookup << ')';
			}
			--sp;
			break;

		case PCD_ENDPRINT:
		case PCD_ENDPRINTBOLD:
		case PCD_MOREHUDMESSAGE:
		case PCD_ENDLOG:
			if (pcd == PCD_ENDLOG)
			{
				Printf ("%s\n", work.GetChars());
				STRINGBUILDER_FINISH(work);
			}
			else if (pcd != PCD_MOREHUDMESSAGE)
			{
				AActor *screen = activator;
				// If a missile is the activator, make the thing that
				// launched the missile the target of the print command.
				if (screen != NULL &&
					screen->player == NULL &&
					(screen->flags & MF_MISSILE) &&
					screen->target != NULL)
				{
					screen = screen->target;
				}
				if (pcd == PCD_ENDPRINTBOLD || screen == NULL ||
					screen->CheckLocalView (consoleplayer))
				{
					C_MidPrint (activefont, work);
				}
				STRINGBUILDER_FINISH(work);
			}
			else
			{
				optstart = -1;
			}
			break;

		case PCD_OPTHUDMESSAGE:
			optstart = sp;
			break;

		case PCD_ENDHUDMESSAGE:
		case PCD_ENDHUDMESSAGEBOLD:
			if (optstart == -1)
			{
				optstart = sp;
			}
			{
				AActor *screen = activator;
				if (screen != NULL &&
					screen->player == NULL &&
					(screen->flags & MF_MISSILE) &&
					screen->target != NULL)
				{
					screen = screen->target;
				}
				if (pcd == PCD_ENDHUDMESSAGEBOLD || screen == NULL ||
					players[consoleplayer].mo == screen)
				{
					int type = Stack[optstart-6];
					int id = Stack[optstart-5];
					EColorRange color;
					float x = ACSToFloat(Stack[optstart-3]);
					float y = ACSToFloat(Stack[optstart-2]);
					float holdTime = ACSToFloat(Stack[optstart-1]);
					float alpha;
					DHUDMessage *msg;

					if (type & HUDMSG_COLORSTRING)
					{
						color = V_FindFontColor(FBehavior::StaticLookupString(Stack[optstart-4]));
					}
					else
					{
						color = CLAMPCOLOR(Stack[optstart-4]);
					}

					switch (type & 0xFF)
					{
					default:	// normal
						alpha = (optstart < sp) ? ACSToFloat(Stack[optstart]) : 1.f;
						msg = new DHUDMessage (activefont, work, x, y, hudwidth, hudheight, color, holdTime);
						break;
					case 1:		// fade out
						{
							float fadeTime = (optstart < sp) ? ACSToFloat(Stack[optstart]) : 0.5f;
							alpha = (optstart < sp-1) ? ACSToFloat(Stack[optstart+1]) : 1.f;
							msg = new DHUDMessageFadeOut (activefont, work, x, y, hudwidth, hudheight, color, holdTime, fadeTime);
						}
						break;
					case 2:		// type on, then fade out
						{
							float typeTime = (optstart < sp) ? ACSToFloat(Stack[optstart]) : 0.05f;
							float fadeTime = (optstart < sp-1) ? ACSToFloat(Stack[optstart+1]) : 0.5f;
							alpha = (optstart < sp-2) ? ACSToFloat(Stack[optstart+2]) : 1.f;
							msg = new DHUDMessageTypeOnFadeOut (activefont, work, x, y, hudwidth, hudheight, color, typeTime, holdTime, fadeTime);
						}
						break;
					case 3:		// fade in, then fade out
						{
							float inTime = (optstart < sp) ? ACSToFloat(Stack[optstart]) : 0.5f;
							float outTime = (optstart < sp-1) ? ACSToFloat(Stack[optstart+1]) : 0.5f;
							alpha = (optstart < sp-2) ? ACSToFloat(Stack[optstart + 2]) : 1.f;
							msg = new DHUDMessageFadeInOut (activefont, work, x, y, hudwidth, hudheight, color, holdTime, inTime, outTime);
						}
						break;
					}
					msg->SetClipRect(ClipRectLeft, ClipRectTop, ClipRectWidth, ClipRectHeight, HandleAspect);
					if (WrapWidth != 0)
					{
						msg->SetWrapWidth(WrapWidth);
					}
					msg->SetVisibility((type & HUDMSG_VISIBILITY_MASK) >> HUDMSG_VISIBILITY_SHIFT);
					if (type & HUDMSG_NOWRAP)
					{
						msg->SetNoWrap(true);
					}
					if (type & HUDMSG_ALPHA)
					{
						msg->SetAlpha(alpha);
					}
					if (type & HUDMSG_ADDBLEND)
					{
						msg->SetRenderStyle(STYLE_Add);
					}
					StatusBar->AttachMessage (msg, id ? 0xff000000|id : 0,
						(type & HUDMSG_LAYER_MASK) >> HUDMSG_LAYER_SHIFT);
					if (type & HUDMSG_LOG)
					{
						static const char bar[] = TEXTCOLOR_ORANGE "\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36"
					"\36\36\36\36\36\36\36\36\36\36\36\36\37" TEXTCOLOR_NORMAL "\n";
						static const char logbar[] = "\n<------------------------------->\n";
						char consolecolor[3];

						consolecolor[0] = '\x1c';
						consolecolor[1] = color >= CR_BRICK && color <= CR_YELLOW ? color + 'A' : '-';
						consolecolor[2] = '\0';
						AddToConsole (-1, bar);
						AddToConsole (-1, consolecolor);
						AddToConsole (-1, work);
						AddToConsole (-1, bar);
					}
				}
			}
			STRINGBUILDER_FINISH(work);
			sp = optstart-6;
			break;

		case PCD_SETFONT:
			DoSetFont (STACK(1));
			sp--;
			break;

		case PCD_SETFONTDIRECT:
			DoSetFont (TAGSTR(uallong(pc[0])));
			pc++;
			break;

		case PCD_PLAYERCOUNT:
			PushToStack (CountPlayers ());
			break;

		case PCD_GAMETYPE:
			if (gamestate == GS_TITLELEVEL)
				PushToStack (GAME_TITLE_MAP);
			else if (deathmatch)
				PushToStack (GAME_NET_DEATHMATCH);
			else if (multiplayer)
				PushToStack (GAME_NET_COOPERATIVE);
			else
				PushToStack (GAME_SINGLE_PLAYER);
			break;

		case PCD_GAMESKILL:
			PushToStack (G_SkillProperty(SKILLP_ACSReturn));
			break;

// [BC] Start ST PCD's
		case PCD_ISNETWORKGAME:
			PushToStack(netgame);
			break;

		case PCD_PLAYERTEAM:
			if ( activator && activator->player )
				PushToStack( activator->player->userinfo.GetTeam() );
			else
				PushToStack( 0 );
			break;

		case PCD_PLAYERHEALTH:
			if (activator)
				PushToStack (activator->health);
			else
				PushToStack (0);
			break;

		case PCD_PLAYERARMORPOINTS:
			if (activator)
			{
				ABasicArmor *armor = activator->FindInventory<ABasicArmor>();
				PushToStack (armor ? armor->Amount : 0);
			}
			else
			{
				PushToStack (0);
			}
			break;

		case PCD_PLAYERFRAGS:
			if (activator && activator->player)
				PushToStack (activator->player->fragcount);
			else
				PushToStack (0);
			break;

		case PCD_MUSICCHANGE:
			lookup = FBehavior::StaticLookupString (STACK(2));
			if (lookup != NULL)
			{
				S_ChangeMusic (lookup, STACK(1));
			}
			sp -= 2;
			break;

		case PCD_SINGLEPLAYER:
			PushToStack (!multiplayer);
			break;
// [BC] End ST PCD's

		case PCD_TIMER:
			PushToStack (level.time);
			break;

		case PCD_SECTORSOUND:
			lookup = FBehavior::StaticLookupString (STACK(2));
			if (lookup != NULL)
			{
				if (activationline)
				{
					S_Sound (
						activationline->frontsector,
						CHAN_AUTO,	// Not CHAN_AREA, because that'd probably break existing scripts.
						lookup,
						(float)(STACK(1)) / 127.f,
						ATTN_NORM);
				}
				else
				{
					S_Sound (
						CHAN_AUTO,
						lookup,
						(float)(STACK(1)) / 127.f,
						ATTN_NORM);
				}
			}
			sp -= 2;
			break;

		case PCD_AMBIENTSOUND:
			lookup = FBehavior::StaticLookupString (STACK(2));
			if (lookup != NULL)
			{
				S_Sound (CHAN_AUTO,
						 lookup,
						 (float)(STACK(1)) / 127.f, ATTN_NONE);
			}
			sp -= 2;
			break;

		case PCD_LOCALAMBIENTSOUND:
			lookup = FBehavior::StaticLookupString (STACK(2));
			if (lookup != NULL && activator->CheckLocalView (consoleplayer))
			{
				S_Sound (CHAN_AUTO,
						 lookup,
						 (float)(STACK(1)) / 127.f, ATTN_NONE);
			}
			sp -= 2;
			break;

		case PCD_ACTIVATORSOUND:
			lookup = FBehavior::StaticLookupString (STACK(2));
			if (lookup != NULL)
			{
				if (activator != NULL)
				{
					S_Sound (activator, CHAN_AUTO,
							 lookup,
							 (float)(STACK(1)) / 127.f, ATTN_NORM);
				}
				else
				{
					S_Sound (CHAN_AUTO,
							 lookup,
							 (float)(STACK(1)) / 127.f, ATTN_NONE);
				}
			}
			sp -= 2;
			break;

		case PCD_SOUNDSEQUENCE:
			lookup = FBehavior::StaticLookupString (STACK(1));
			if (lookup != NULL)
			{
				if (activationline != NULL)
				{
					SN_StartSequence (activationline->frontsector, CHAN_FULLHEIGHT, lookup, 0);
				}
			}
			sp--;
			break;

		case PCD_SETLINETEXTURE:
			SetLineTexture (STACK(4), STACK(3), STACK(2), STACK(1));
			sp -= 4;
			break;

		case PCD_REPLACETEXTURES:
			ReplaceTextures (STACK(3), STACK(2), STACK(1));
			sp -= 3;
			break;

		case PCD_SETLINEBLOCKING:
			{
				int line;

				FLineIdIterator itr(STACK(2));
				while ((line = itr.Next()) >= 0)
				{
					switch (STACK(1))
					{
					case BLOCK_NOTHING:
						lines[line].flags &= ~(ML_BLOCKING|ML_BLOCKEVERYTHING|ML_RAILING|ML_BLOCK_PLAYERS);
						break;
					case BLOCK_CREATURES:
					default:
						lines[line].flags &= ~(ML_BLOCKEVERYTHING|ML_RAILING|ML_BLOCK_PLAYERS);
						lines[line].flags |= ML_BLOCKING;
						break;
					case BLOCK_EVERYTHING:
						lines[line].flags &= ~(ML_RAILING|ML_BLOCK_PLAYERS);
						lines[line].flags |= ML_BLOCKING|ML_BLOCKEVERYTHING;
						break;
					case BLOCK_RAILING:
						lines[line].flags &= ~(ML_BLOCKEVERYTHING|ML_BLOCK_PLAYERS);
						lines[line].flags |= ML_RAILING|ML_BLOCKING;
						break;
					case BLOCK_PLAYERS:
						lines[line].flags &= ~(ML_BLOCKEVERYTHING|ML_BLOCKING|ML_RAILING);
						lines[line].flags |= ML_BLOCK_PLAYERS;
						break;
					}
				}

				sp -= 2;
			}
			break;

		case PCD_SETLINEMONSTERBLOCKING:
			{
				int line;

				FLineIdIterator itr(STACK(2));
				while ((line = itr.Next()) >= 0)
				{
					if (STACK(1))
						lines[line].flags |= ML_BLOCKMONSTERS;
					else
						lines[line].flags &= ~ML_BLOCKMONSTERS;
				}

				sp -= 2;
			}
			break;

		case PCD_SETLINESPECIAL:
			{
				int linenum = -1;
				int specnum = STACK(6);
				int arg0 = STACK(5);

				// Convert named ACS "specials" into real specials.
				if (specnum >= -ACSF_ACS_NamedExecuteAlways && specnum <= -ACSF_ACS_NamedExecute)
				{
					specnum = NamedACSToNormalACS[-specnum - ACSF_ACS_NamedExecute];
					arg0 = -FName(FBehavior::StaticLookupString(arg0));
				}

				FLineIdIterator itr(STACK(7));
				while ((linenum = itr.Next()) >= 0)
				{
					line_t *line = &lines[linenum];
					line->special = specnum;
					line->args[0] = arg0;
					line->args[1] = STACK(4);
					line->args[2] = STACK(3);
					line->args[3] = STACK(2);
					line->args[4] = STACK(1);
					DPrintf("Set special on line %d (id %d) to %d(%d,%d,%d,%d,%d)\n",
						linenum, STACK(7), specnum, arg0, STACK(4), STACK(3), STACK(2), STACK(1));
				}
				sp -= 7;
			}
			break;

		case PCD_SETTHINGSPECIAL:
			{
				int specnum = STACK(6);
				int arg0 = STACK(5);

				// Convert named ACS "specials" into real specials.
				if (specnum >= -ACSF_ACS_NamedExecuteAlways && specnum <= -ACSF_ACS_NamedExecute)
				{
					specnum = NamedACSToNormalACS[-specnum - ACSF_ACS_NamedExecute];
					arg0 = -FName(FBehavior::StaticLookupString(arg0));
				}

				if (STACK(7) != 0)
				{
					FActorIterator iterator (STACK(7));
					AActor *actor;

					while ( (actor = iterator.Next ()) )
					{
						actor->special = specnum;
						actor->args[0] = arg0;
						actor->args[1] = STACK(4);
						actor->args[2] = STACK(3);
						actor->args[3] = STACK(2);
						actor->args[4] = STACK(1);
					}
				}
				else if (activator != NULL)
				{
					activator->special = specnum;
					activator->args[0] = arg0;
					activator->args[1] = STACK(4);
					activator->args[2] = STACK(3);
					activator->args[3] = STACK(2);
					activator->args[4] = STACK(1);
				}
				sp -= 7;
			}
			break;

		case PCD_THINGSOUND:
			lookup = FBehavior::StaticLookupString (STACK(2));
			if (lookup != NULL)
			{
				FActorIterator iterator (STACK(3));
				AActor *spot;

				while ( (spot = iterator.Next ()) )
				{
					S_Sound (spot, CHAN_AUTO,
							 lookup,
							 (float)(STACK(1))/127.f, ATTN_NORM);
				}
			}
			sp -= 3;
			break;

		case PCD_FIXEDMUL:
			STACK(2) = FixedMul (STACK(2), STACK(1));
			sp--;
			break;

		case PCD_FIXEDDIV:
			STACK(2) = FixedDiv (STACK(2), STACK(1));
			sp--;
			break;

		case PCD_SETGRAVITY:
			level.gravity = ACSToDouble(STACK(1));
			sp--;
			break;

		case PCD_SETGRAVITYDIRECT:
			level.gravity = ACSToDouble(uallong(pc[0]));
			pc++;
			break;

		case PCD_SETAIRCONTROL:
			level.aircontrol = ACSToDouble(STACK(1));
			sp--;
			G_AirControlChanged ();
			break;

		case PCD_SETAIRCONTROLDIRECT:
			level.aircontrol = ACSToDouble(uallong(pc[0]));
			pc++;
			G_AirControlChanged ();
			break;

		case PCD_SPAWN:
			STACK(6) = DoSpawn (STACK(6), STACK(5), STACK(4), STACK(3), STACK(2), STACK(1), false);
			sp -= 5;
			break;

		case PCD_SPAWNDIRECT:
			PushToStack (DoSpawn (TAGSTR(uallong(pc[0])), uallong(pc[1]), uallong(pc[2]), uallong(pc[3]), uallong(pc[4]), uallong(pc[5]), false));
			pc += 6;
			break;

		case PCD_SPAWNSPOT:
			STACK(4) = DoSpawnSpot (STACK(4), STACK(3), STACK(2), STACK(1), false);
			sp -= 3;
			break;

		case PCD_SPAWNSPOTDIRECT:
			PushToStack (DoSpawnSpot (TAGSTR(uallong(pc[0])), uallong(pc[1]), uallong(pc[2]), uallong(pc[3]), false));
			pc += 4;
			break;

		case PCD_SPAWNSPOTFACING:
			STACK(3) = DoSpawnSpotFacing (STACK(3), STACK(2), STACK(1), false);
			sp -= 2;
			break;

		case PCD_CLEARINVENTORY:
			ClearInventory (activator);
			break;

		case PCD_CLEARACTORINVENTORY:
			if (STACK(1) == 0)
			{
				ClearInventory(NULL);
			}
			else
			{
				FActorIterator it(STACK(1));
				AActor *actor;
				for (actor = it.Next(); actor != NULL; actor = it.Next())
				{
					ClearInventory(actor);
				}
			}
			sp--;
			break;

		case PCD_GIVEINVENTORY:
			GiveInventory (activator, FBehavior::StaticLookupString (STACK(2)), STACK(1));
			sp -= 2;
			break;

		case PCD_GIVEACTORINVENTORY:
			{
				const char *type = FBehavior::StaticLookupString(STACK(2));
				if (STACK(3) == 0)
				{
					GiveInventory(NULL, FBehavior::StaticLookupString(STACK(2)), STACK(1));
				}
				else
				{
					FActorIterator it(STACK(3));
					AActor *actor;
					for (actor = it.Next(); actor != NULL; actor = it.Next())
					{
						GiveInventory(actor, type, STACK(1));
					}
				}
				sp -= 3;
			}
			break;

		case PCD_GIVEINVENTORYDIRECT:
			GiveInventory (activator, FBehavior::StaticLookupString (TAGSTR(uallong(pc[0]))), uallong(pc[1]));
			pc += 2;
			break;

		case PCD_TAKEINVENTORY:
			TakeInventory (activator, FBehavior::StaticLookupString (STACK(2)), STACK(1));
			sp -= 2;
			break;

		case PCD_TAKEACTORINVENTORY:
			{
				const char *type = FBehavior::StaticLookupString(STACK(2));
				if (STACK(3) == 0)
				{
					TakeInventory(NULL, type, STACK(1));
				}
				else
				{
					FActorIterator it(STACK(3));
					AActor *actor;
					for (actor = it.Next(); actor != NULL; actor = it.Next())
					{
						TakeInventory(actor, type, STACK(1));
					}
				}
				sp -= 3;
			}
			break;

		case PCD_TAKEINVENTORYDIRECT:
			TakeInventory (activator, FBehavior::StaticLookupString (TAGSTR(uallong(pc[0]))), uallong(pc[1]));
			pc += 2;
			break;

		case PCD_CHECKINVENTORY:
			STACK(1) = CheckInventory (activator, FBehavior::StaticLookupString (STACK(1)), false);
			break;

		case PCD_CHECKACTORINVENTORY:
			STACK(2) = CheckInventory (SingleActorFromTID(STACK(2), NULL),
										FBehavior::StaticLookupString (STACK(1)), false);
			sp--;
			break;

		case PCD_CHECKINVENTORYDIRECT:
			PushToStack (CheckInventory (activator, FBehavior::StaticLookupString (TAGSTR(uallong(pc[0]))), false));
			pc += 1;
			break;

		case PCD_USEINVENTORY:
			STACK(1) = UseInventory (activator, FBehavior::StaticLookupString (STACK(1)));
			break;

		case PCD_USEACTORINVENTORY:
			{
				int ret = 0;
				const char *type = FBehavior::StaticLookupString(STACK(1));
				if (STACK(2) == 0)
				{
					ret = UseInventory(NULL, type);
				}
				else
				{
					FActorIterator it(STACK(2));
					AActor *actor;
					for (actor = it.Next(); actor != NULL; actor = it.Next())
					{
						ret += UseInventory(actor, type);
					}
				}
				STACK(2) = ret;
				sp--;
			}
			break;

		case PCD_GETSIGILPIECES:
			{
				ASigil *sigil;

				if (activator == NULL || (sigil = activator->FindInventory<ASigil>()) == NULL)
				{
					PushToStack (0);
				}
				else
				{
					PushToStack (sigil->NumPieces);
				}
			}
			break;

		case PCD_GETAMMOCAPACITY:
			if (activator != NULL)
			{
				PClass *type = PClass::FindClass (FBehavior::StaticLookupString (STACK(1)));
				AInventory *item;

				if (type != NULL && type->ParentClass == RUNTIME_CLASS(AAmmo))
				{
					item = activator->FindInventory (static_cast<PClassActor *>(type));
					if (item != NULL)
					{
						STACK(1) = item->MaxAmount;
					}
					else
					{
						STACK(1) = ((AInventory *)GetDefaultByType (type))->MaxAmount;
					}
				}
				else
				{
					STACK(1) = 0;
				}
			}
			else
			{
				STACK(1) = 0;
			}
			break;

		case PCD_SETAMMOCAPACITY:
			if (activator != NULL)
			{
				PClass *type = PClass::FindClass (FBehavior::StaticLookupString (STACK(2)));
				AInventory *item;

				if (type != NULL && type->ParentClass == RUNTIME_CLASS(AAmmo))
				{
					item = activator->FindInventory (static_cast<PClassActor *>(type));
					if (item != NULL)
					{
						item->MaxAmount = STACK(1);
					}
					else
					{
						item = activator->GiveInventoryType (static_cast<PClassAmmo *>(type));
						if (item != NULL)
						{
							item->MaxAmount = STACK(1);
							item->Amount = 0;
						}
					}
				}
			}
			sp -= 2;
			break;

		case PCD_SETMUSIC:
			S_ChangeMusic (FBehavior::StaticLookupString (STACK(3)), STACK(2));
			sp -= 3;
			break;

		case PCD_SETMUSICDIRECT:
			S_ChangeMusic (FBehavior::StaticLookupString (TAGSTR(uallong(pc[0]))), uallong(pc[1]));
			pc += 3;
			break;

		case PCD_LOCALSETMUSIC:
			if (activator == players[consoleplayer].mo)
			{
				S_ChangeMusic (FBehavior::StaticLookupString (STACK(3)), STACK(2));
			}
			sp -= 3;
			break;

		case PCD_LOCALSETMUSICDIRECT:
			if (activator == players[consoleplayer].mo)
			{
				S_ChangeMusic (FBehavior::StaticLookupString (TAGSTR(uallong(pc[0]))), uallong(pc[1]));
			}
			pc += 3;
			break;

		case PCD_FADETO:
			DoFadeTo (STACK(5), STACK(4), STACK(3), STACK(2), STACK(1));
			sp -= 5;
			break;

		case PCD_FADERANGE:
			DoFadeRange (STACK(9), STACK(8), STACK(7), STACK(6),
						 STACK(5), STACK(4), STACK(3), STACK(2), STACK(1));
			sp -= 9;
			break;

		case PCD_CANCELFADE:
			{
				TThinkerIterator<DFlashFader> iterator;
				DFlashFader *fader;

				while ( (fader = iterator.Next()) )
				{
					if (activator == NULL || fader->WhoFor() == activator)
					{
						fader->Cancel ();
					}
				}
			}
			break;

		case PCD_PLAYMOVIE:
			STACK(1) = I_PlayMovie (FBehavior::StaticLookupString (STACK(1)));
			break;

		case PCD_SETACTORPOSITION:
			{
				bool result = false;
				AActor *actor = SingleActorFromTID (STACK(5), activator);
				if (actor != NULL)
					result = P_MoveThing(actor, DVector3(ACSToDouble(STACK(4)), ACSToDouble(STACK(3)), ACSToDouble(STACK(2))), !!STACK(1));
				sp -= 4;
				STACK(1) = result;
			}
			break;

		case PCD_GETACTORX:
		case PCD_GETACTORY:
		case PCD_GETACTORZ:
			{
				AActor *actor = SingleActorFromTID(STACK(1), activator);
				if (actor == NULL)
				{
					STACK(1) = 0;
				}
				else if (pcd == PCD_GETACTORZ)
				{
					STACK(1) = DoubleToACS(actor->Z() + actor->GetBobOffset());
				}
				else
				{
					STACK(1) = DoubleToACS(pcd == PCD_GETACTORX ? actor->X() : pcd == PCD_GETACTORY ? actor->Y() : actor->Z());
				}
			}
			break;

		case PCD_GETACTORFLOORZ:
			{
				AActor *actor = SingleActorFromTID(STACK(1), activator);
				STACK(1) = actor == NULL ? 0 : DoubleToACS(actor->floorz);
			}
			break;

		case PCD_GETACTORCEILINGZ:
			{
				AActor *actor = SingleActorFromTID(STACK(1), activator);
				STACK(1) = actor == NULL ? 0 : DoubleToACS(actor->ceilingz);
			}
			break;

		case PCD_GETACTORANGLE:
			{
				AActor *actor = SingleActorFromTID(STACK(1), activator);
				STACK(1) = actor == NULL ? 0 : AngleToACS(actor->Angles.Yaw);
			}
			break;

		case PCD_GETACTORPITCH:
			{
				AActor *actor = SingleActorFromTID(STACK(1), activator);
				STACK(1) = actor == NULL ? 0 : PitchToACS(actor->Angles.Pitch);
			}
			break;

		case PCD_GETLINEROWOFFSET:
			if (activationline != NULL)
			{
				PushToStack (int(activationline->sidedef[0]->GetTextureYOffset(side_t::mid)));
			}
			else
			{
				PushToStack (0);
			}
			break;

		case PCD_GETSECTORFLOORZ:
		case PCD_GETSECTORCEILINGZ:
			// Arguments are (tag, x, y). If you don't use slopes, then (x, y) don't
			// really matter and can be left as (0, 0) if you like.
			// [Dusk] If tag = 0, then this returns the z height at whatever sector
			// is in x, y.
			{
				int tag = STACK(3);
				int secnum;
				double x = double(STACK(2));
				double y = double(STACK(1));
				double z = 0;

				if (tag != 0)
					secnum = P_FindFirstSectorFromTag (tag);
				else
					secnum = int(P_PointInSector (x, y) - sectors);

				if (secnum >= 0)
				{
					if (pcd == PCD_GETSECTORFLOORZ)
					{
						z = sectors[secnum].floorplane.ZatPoint (x, y);
					}
					else
					{
						z = sectors[secnum].ceilingplane.ZatPoint (x, y);
					}
				}
				sp -= 2;
				STACK(1) = DoubleToACS(z);
			}
			break;

		case PCD_GETSECTORLIGHTLEVEL:
			{
				int secnum = P_FindFirstSectorFromTag (STACK(1));
				int z = -1;

				if (secnum >= 0)
				{
					z = sectors[secnum].lightlevel;
				}
				STACK(1) = z;
			}
			break;

		case PCD_SETFLOORTRIGGER:
			new DPlaneWatcher (activator, activationline, backSide, false, STACK(8),
				STACK(7), STACK(6), STACK(5), STACK(4), STACK(3), STACK(2), STACK(1));
			sp -= 8;
			break;

		case PCD_SETCEILINGTRIGGER:
			new DPlaneWatcher (activator, activationline, backSide, true, STACK(8),
				STACK(7), STACK(6), STACK(5), STACK(4), STACK(3), STACK(2), STACK(1));
			sp -= 8;
			break;

		case PCD_STARTTRANSLATION:
			{
				int i = STACK(1);
				sp--;
				if (i >= 1 && i <= MAX_ACS_TRANSLATIONS)
				{
					translation = translationtables[TRANSLATION_LevelScripted].GetVal(i - 1);
					if (translation == NULL)
					{
						translation = new FRemapTable;
						translationtables[TRANSLATION_LevelScripted].SetVal(i - 1, translation);
					}
					translation->MakeIdentity();
				}
			}
			break;

		case PCD_TRANSLATIONRANGE1:
			{ // translation using palette shifting
				int start = STACK(4);
				int end = STACK(3);
				int pal1 = STACK(2);
				int pal2 = STACK(1);
				sp -= 4;

				if (translation != NULL)
					translation->AddIndexRange(start, end, pal1, pal2);
			}
			break;

		case PCD_TRANSLATIONRANGE2:
			{ // translation using RGB values
			  // (would HSV be a good idea too?)
				int start = STACK(8);
				int end = STACK(7);
				int r1 = STACK(6);
				int g1 = STACK(5);
				int b1 = STACK(4);
				int r2 = STACK(3);
				int g2 = STACK(2);
				int b2 = STACK(1);
				sp -= 8;

				if (translation != NULL)
					translation->AddColorRange(start, end, r1, g1, b1, r2, g2, b2);
			}
			break;

		case PCD_TRANSLATIONRANGE3:
			{ // translation using desaturation
				int start = STACK(8);
				int end = STACK(7);
				int r1 = STACK(6);
				int g1 = STACK(5);
				int b1 = STACK(4);
				int r2 = STACK(3);
				int g2 = STACK(2);
				int b2 = STACK(1);
				sp -= 8;

				if (translation != NULL)
					translation->AddDesaturation(start, end,
						ACSToDouble(r1), ACSToDouble(g1), ACSToDouble(b1),
						ACSToDouble(r2), ACSToDouble(g2), ACSToDouble(b2));
			}
			break;

		case PCD_ENDTRANSLATION:
			if (translation != NULL)
			{
				translation->UpdateNative();
				translation = NULL;
			}
			break;

		case PCD_SIN:
			STACK(1) = DoubleToACS(ACSToAngle(STACK(1)).Sin());
			break;

		case PCD_COS:
			STACK(1) = DoubleToACS(ACSToAngle(STACK(1)).Cos());
			break;

		case PCD_VECTORANGLE:
			STACK(2) = AngleToACS(VecToAngle(STACK(2), STACK(1)).Degrees);
			sp--;
			break;

        case PCD_CHECKWEAPON:
            if (activator == NULL || activator->player == NULL || // Non-players do not have weapons
                activator->player->ReadyWeapon == NULL)
            {
                STACK(1) = 0;
            }
            else
            {
				STACK(1) = activator->player->ReadyWeapon->GetClass()->TypeName == FName(FBehavior::StaticLookupString (STACK(1)), true);
            }
            break;

		case PCD_SETWEAPON:
			if (activator == NULL || activator->player == NULL)
			{
				STACK(1) = 0;
			}
			else
			{
				AInventory *item = activator->FindInventory (dyn_cast<PClassActor>(
					PClass::FindClass (FBehavior::StaticLookupString (STACK(1)))));

				if (item == NULL || !item->IsKindOf (RUNTIME_CLASS(AWeapon)))
				{
					STACK(1) = 0;
				}
				else if (activator->player->ReadyWeapon == item)
				{
					// The weapon is already selected, so setweapon succeeds by default,
					// but make sure the player isn't switching away from it.
					activator->player->PendingWeapon = WP_NOCHANGE;
					STACK(1) = 1;
				}
				else
				{
					AWeapon *weap = static_cast<AWeapon *> (item);

					if (weap->CheckAmmo (AWeapon::EitherFire, false))
					{
						// There's enough ammo, so switch to it.
						STACK(1) = 1;
						activator->player->PendingWeapon = weap;
					}
					else
					{
						STACK(1) = 0;
					}
				}
			}
			break;

		case PCD_SETMARINEWEAPON:
			if (STACK(2) != 0)
			{
				AScriptedMarine *marine;
				TActorIterator<AScriptedMarine> iterator (STACK(2));

				while ((marine = iterator.Next()) != NULL)
				{
					marine->SetWeapon ((AScriptedMarine::EMarineWeapon)STACK(1));
				}
			}
			else
			{
				if (activator != NULL && activator->IsKindOf (RUNTIME_CLASS(AScriptedMarine)))
				{
					barrier_cast<AScriptedMarine *>(activator)->SetWeapon (
						(AScriptedMarine::EMarineWeapon)STACK(1));
				}
			}
			sp -= 2;
			break;

		case PCD_SETMARINESPRITE:
			{
				PClassActor *type = PClass::FindActor(FBehavior::StaticLookupString (STACK(1)));

				if (type != NULL)
				{
					if (STACK(2) != 0)
					{
						AScriptedMarine *marine;
						TActorIterator<AScriptedMarine> iterator (STACK(2));

						while ((marine = iterator.Next()) != NULL)
						{
							marine->SetSprite (type);
						}
					}
					else
					{
						if (activator != NULL && activator->IsKindOf (RUNTIME_CLASS(AScriptedMarine)))
						{
							barrier_cast<AScriptedMarine *>(activator)->SetSprite (type);
						}
					}
				}
				else
				{
					Printf ("Unknown actor type: %s\n", FBehavior::StaticLookupString (STACK(1)));
				}
			}
			sp -= 2;
			break;

		case PCD_SETACTORPROPERTY:
			SetActorProperty (STACK(3), STACK(2), STACK(1));
			sp -= 3;
			break;

		case PCD_GETACTORPROPERTY:
			STACK(2) = GetActorProperty (STACK(2), STACK(1));
			sp -= 1;
			break;

		case PCD_GETPLAYERINPUT:
			STACK(2) = GetPlayerInput (STACK(2), STACK(1));
			sp -= 1;
			break;

		case PCD_PLAYERNUMBER:
			if (activator == NULL || activator->player == NULL)
			{
				PushToStack (-1);
			}
			else
			{
				PushToStack (int(activator->player - players));
			}
			break;

		case PCD_PLAYERINGAME:
			if (STACK(1) < 0 || STACK(1) >= MAXPLAYERS)
			{
				STACK(1) = false;
			}
			else
			{
				STACK(1) = playeringame[STACK(1)];
			}
			break;

		case PCD_PLAYERISBOT:
			if (STACK(1) < 0 || STACK(1) >= MAXPLAYERS || !playeringame[STACK(1)])
			{
				STACK(1) = false;
			}
			else
			{
				STACK(1) = (players[STACK(1)].Bot != NULL);
			}
			break;

		case PCD_ACTIVATORTID:
			if (activator == NULL)
			{
				PushToStack (0);
			}
			else
			{
				PushToStack (activator->tid);
			}
			break;

		case PCD_GETSCREENWIDTH:
			PushToStack (SCREENWIDTH);
			break;

		case PCD_GETSCREENHEIGHT:
			PushToStack (SCREENHEIGHT);
			break;

		case PCD_THING_PROJECTILE2:
			// Like Thing_Projectile(Gravity) specials, but you can give the
			// projectile a TID.
			// Thing_Projectile2 (tid, type, angle, speed, vspeed, gravity, newtid);
			P_Thing_Projectile(STACK(7), activator, STACK(6), NULL, STACK(5) * (360. / 256.),
				STACK(4) / 8., STACK(3) / 8., 0, NULL, STACK(2), STACK(1), false);
			sp -= 7;
			break;

		case PCD_SPAWNPROJECTILE:
			// Same, but takes an actor name instead of a spawn ID.
			P_Thing_Projectile(STACK(7), activator, 0, FBehavior::StaticLookupString(STACK(6)), STACK(5) * (360. / 256.),
				STACK(4) / 8., STACK(3) / 8., 0, NULL, STACK(2), STACK(1), false);
			sp -= 7;
			break;

		case PCD_STRLEN:
			{
				const char *str = FBehavior::StaticLookupString(STACK(1));
				if (str != NULL)
				{
					STACK(1) = SDWORD(strlen(str));
					break;
				}

				static bool StrlenInvalidPrintedAlready = false;
				if (!StrlenInvalidPrintedAlready)
				{
					Printf(PRINT_BOLD, "Warning: ACS function strlen called with invalid string argument.\n");
					StrlenInvalidPrintedAlready = true;
				}
				STACK(1) = 0;
			}
			break;

		case PCD_GETCVAR:
			STACK(1) = DoGetCVar(GetCVar(activator, FBehavior::StaticLookupString(STACK(1))), false);
			break;

		case PCD_SETHUDSIZE:
			hudwidth = abs (STACK(3));
			hudheight = abs (STACK(2));
			if (STACK(1) != 0)
			{ // Negative height means to cover the status bar
				hudheight = -hudheight;
			}
			sp -= 3;
			break;

		case PCD_GETLEVELINFO:
			switch (STACK(1))
			{
			case LEVELINFO_PAR_TIME:		STACK(1) = level.partime;			break;
			case LEVELINFO_SUCK_TIME:		STACK(1) = level.sucktime;			break;
			case LEVELINFO_CLUSTERNUM:		STACK(1) = level.cluster;			break;
			case LEVELINFO_LEVELNUM:		STACK(1) = level.levelnum;			break;
			case LEVELINFO_TOTAL_SECRETS:	STACK(1) = level.total_secrets;		break;
			case LEVELINFO_FOUND_SECRETS:	STACK(1) = level.found_secrets;		break;
			case LEVELINFO_TOTAL_ITEMS:		STACK(1) = level.total_items;		break;
			case LEVELINFO_FOUND_ITEMS:		STACK(1) = level.found_items;		break;
			case LEVELINFO_TOTAL_MONSTERS:	STACK(1) = level.total_monsters;	break;
			case LEVELINFO_KILLED_MONSTERS:	STACK(1) = level.killed_monsters;	break;
			default:						STACK(1) = 0;						break;
			}
			break;

		case PCD_CHANGESKY:
			{
				const char *sky1name, *sky2name;

				sky1name = FBehavior::StaticLookupString (STACK(2));
				sky2name = FBehavior::StaticLookupString (STACK(1));
				if (sky1name[0] != 0)
				{
					sky1texture = level.skytexture1 = TexMan.GetTexture (sky1name, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable|FTextureManager::TEXMAN_ReturnFirst);
				}
				if (sky2name[0] != 0)
				{
					sky2texture = level.skytexture2 = TexMan.GetTexture (sky2name, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable|FTextureManager::TEXMAN_ReturnFirst);
				}
				R_InitSkyMap ();
				sp -= 2;
			}
			break;

		case PCD_SETCAMERATOTEXTURE:
			{
				const char *picname = FBehavior::StaticLookupString (STACK(2));
				AActor *camera;

				if (STACK(3) == 0)
				{
					camera = activator;
				}
				else
				{
					FActorIterator it (STACK(3));
					camera = it.Next ();
				}

				if (camera != NULL)
				{
					FTextureID picnum = TexMan.CheckForTexture (picname, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable);
					if (!picnum.Exists())
					{
						Printf ("SetCameraToTexture: %s is not a texture\n", picname);
					}
					else
					{
						FCanvasTextureInfo::Add (camera, picnum, STACK(1));
					}
				}
				sp -= 3;
			}
			break;

		case PCD_SETACTORANGLE:		// [GRB]
			SetActorAngle(activator, STACK(2), STACK(1), false);
			sp -= 2;
			break;

		case PCD_SETACTORPITCH:
			SetActorPitch(activator, STACK(2), STACK(1), false);
			sp -= 2;
			break;

		case PCD_SETACTORSTATE:
			{
				const char *statename = FBehavior::StaticLookupString (STACK(2));
				FState *state;

				if (STACK(3) == 0)
				{
					if (activator != NULL)
					{
						state = activator->GetClass()->FindStateByString (statename, !!STACK(1));
						if (state != NULL)
						{
							activator->SetState (state);
							STACK(3) = 1;
						}
						else
						{
							STACK(3) = 0;
						}
					}
				}
				else
				{
					FActorIterator iterator (STACK(3));
					AActor *actor;
					int count = 0;

					while ( (actor = iterator.Next ()) )
					{
						state = actor->GetClass()->FindStateByString (statename, !!STACK(1));
						if (state != NULL)
						{
							actor->SetState (state);
							count++;
						}
					}
					STACK(3) = count;
				}
				sp -= 2;
			}
			break;

		case PCD_PLAYERCLASS:		// [GRB]
			if (STACK(1) < 0 || STACK(1) >= MAXPLAYERS || !playeringame[STACK(1)])
			{
				STACK(1) = -1;
			}
			else
			{
				STACK(1) = players[STACK(1)].CurrentPlayerClass;
			}
			break;

		case PCD_GETPLAYERINFO:		// [GRB]
			if (STACK(2) < 0 || STACK(2) >= MAXPLAYERS || !playeringame[STACK(2)])
			{
				STACK(2) = -1;
			}
			else
			{
				player_t *pl = &players[STACK(2)];
				userinfo_t *userinfo = &pl->userinfo;
				switch (STACK(1))
				{
				case PLAYERINFO_TEAM:			STACK(2) = userinfo->GetTeam(); break;
				case PLAYERINFO_AIMDIST:		STACK(2) = (SDWORD)(userinfo->GetAimDist() * (0x40000000/90.)); break;	// Yes, this has been returning a BAM since its creation.
				case PLAYERINFO_COLOR:			STACK(2) = userinfo->GetColor(); break;
				case PLAYERINFO_GENDER:			STACK(2) = userinfo->GetGender(); break;
				case PLAYERINFO_NEVERSWITCH:	STACK(2) = userinfo->GetNeverSwitch(); break;
				case PLAYERINFO_MOVEBOB:		STACK(2) = DoubleToACS(userinfo->GetMoveBob()); break;
				case PLAYERINFO_STILLBOB:		STACK(2) = DoubleToACS(userinfo->GetStillBob()); break;
				case PLAYERINFO_PLAYERCLASS:	STACK(2) = userinfo->GetPlayerClassNum(); break;
				case PLAYERINFO_DESIREDFOV:		STACK(2) = (int)pl->DesiredFOV; break;
				case PLAYERINFO_FOV:			STACK(2) = (int)pl->FOV; break;
				default:						STACK(2) = 0; break;
				}
			}
			sp -= 1;
			break;

		case PCD_CHANGELEVEL:
			{
				G_ChangeLevel(FBehavior::StaticLookupString(STACK(4)), STACK(3), STACK(2), STACK(1));
				sp -= 4;
			}
			break;

		case PCD_SECTORDAMAGE:
			{
				int tag = STACK(5);
				int amount = STACK(4);
				FName type = FBehavior::StaticLookupString(STACK(3));
				FName protection = FName (FBehavior::StaticLookupString(STACK(2)), true);
				PClassActor *protectClass = PClass::FindActor (protection);
				int flags = STACK(1);
				sp -= 5;

				P_SectorDamage(tag, amount, type, protectClass, flags);
			}
			break;

		case PCD_THINGDAMAGE2:
			STACK(3) = P_Thing_Damage (STACK(3), activator, STACK(2), FName(FBehavior::StaticLookupString(STACK(1))));
			sp -= 2;
			break;

		case PCD_CHECKACTORCEILINGTEXTURE:
			STACK(2) = DoCheckActorTexture(STACK(2), activator, STACK(1), false);
			sp--;
			break;

		case PCD_CHECKACTORFLOORTEXTURE:
			STACK(2) = DoCheckActorTexture(STACK(2), activator, STACK(1), true);
			sp--;
			break;

		case PCD_GETACTORLIGHTLEVEL:
		{
			AActor *actor = SingleActorFromTID(STACK(1), activator);
			if (actor != NULL)
			{
				sector_t *sector = actor->Sector;
				if (sector->e->XFloor.lightlist.Size())
				{
					unsigned   i;
					TArray<lightlist_t> &lightlist = sector->e->XFloor.lightlist;

					STACK(1) = *lightlist.Last().p_lightlevel;
					for (i = 1; i < lightlist.Size(); i++)
					{
						if (lightlist[i].plane.ZatPoint(actor) <= actor->Z())
						{
							STACK(1) = *lightlist[i - 1].p_lightlevel;
							break;
						}
					}
				}
				else
				{
					STACK(1) = actor->Sector->lightlevel;
				}
			}
			else STACK(1) = 0;
			break;
		}

		case PCD_SETMUGSHOTSTATE:
			StatusBar->SetMugShotState(FBehavior::StaticLookupString(STACK(1)));
			sp--;
			break;

		case PCD_CHECKPLAYERCAMERA:
			{
				int playernum = STACK(1);

				if (playernum < 0 || playernum >= MAXPLAYERS || !playeringame[playernum] || players[playernum].camera == NULL || players[playernum].camera->player != NULL)
				{
					STACK(1) = -1;
				}
				else
				{
					STACK(1) = players[playernum].camera->tid;
				}
			}
			break;

		case PCD_CLASSIFYACTOR:
			STACK(1) = DoClassifyActor(STACK(1));
			break;

		case PCD_MORPHACTOR:
			{
				int tag = STACK(7);
				FName playerclass_name = FBehavior::StaticLookupString(STACK(6));
				PClassPlayerPawn *playerclass = dyn_cast<PClassPlayerPawn>(PClass::FindClass (playerclass_name));
				FName monsterclass_name = FBehavior::StaticLookupString(STACK(5));
				PClassActor *monsterclass = PClass::FindActor(monsterclass_name);
				int duration = STACK(4);
				int style = STACK(3);
				FName morphflash_name = FBehavior::StaticLookupString(STACK(2));
				PClassActor *morphflash = PClass::FindActor(morphflash_name);
				FName unmorphflash_name = FBehavior::StaticLookupString(STACK(1));
				PClassActor *unmorphflash = PClass::FindActor(unmorphflash_name);
				int changes = 0;

				if (tag == 0)
				{
					if (activator != NULL && activator->player)
					{
						changes += P_MorphPlayer(activator->player, activator->player, playerclass, duration, style, morphflash, unmorphflash);
					}
					else
					{
						changes += P_MorphMonster(activator, monsterclass, duration, style, morphflash, unmorphflash);
					}
				}
				else
				{
					FActorIterator iterator (tag);
					AActor *actor;

					while ( (actor = iterator.Next ()) )
					{
						if (actor->player)
						{
							changes += P_MorphPlayer(activator == NULL ? NULL : activator->player,
								actor->player, playerclass, duration, style, morphflash, unmorphflash);
						}
						else
						{
							changes += P_MorphMonster(actor, monsterclass, duration, style, morphflash, unmorphflash);
						}
					}
				}

				STACK(7) = changes;
				sp -= 6;
			}	
			break;

		case PCD_UNMORPHACTOR:
			{
				int tag = STACK(2);
				bool force = !!STACK(1);
				int changes = 0;

				if (tag == 0)
				{
					if (activator->player)
					{
						if (P_UndoPlayerMorph(activator->player, activator->player, 0, force))
						{
							changes++;
						}
					}
					else
					{
						if (activator->GetClass()->IsDescendantOf(RUNTIME_CLASS(AMorphedMonster)))
						{
							AMorphedMonster *morphed_actor = barrier_cast<AMorphedMonster *>(activator);
							if (P_UndoMonsterMorph(morphed_actor, force))
							{
								changes++;
							}
						}
					}
				}
				else
				{
					FActorIterator iterator (tag);
					AActor *actor;

					while ( (actor = iterator.Next ()) )
					{
						if (actor->player)
						{
							if (P_UndoPlayerMorph(activator->player, actor->player, 0, force))
							{
								changes++;
							}
						}
						else
						{
							if (actor->GetClass()->IsDescendantOf(RUNTIME_CLASS(AMorphedMonster)))
							{
								AMorphedMonster *morphed_actor = static_cast<AMorphedMonster *>(actor);
								if (P_UndoMonsterMorph(morphed_actor, force))
								{
									changes++;
								}
							}
						}
					}
				}

				STACK(2) = changes;
				sp -= 1;
			}	
			break;

		case PCD_SAVESTRING:
			// Saves the string
			{
				const int str = GlobalACSStrings.AddString(work);
				PushToStack(str);
				STRINGBUILDER_FINISH(work);
			}		
			break;

		case PCD_STRCPYTOSCRIPTCHRANGE:
		case PCD_STRCPYTOMAPCHRANGE:
		case PCD_STRCPYTOWORLDCHRANGE:
		case PCD_STRCPYTOGLOBALCHRANGE:
			// source: stringid(2); stringoffset(1)
			// destination: capacity (3); stringoffset(4); arrayid (5); offset(6)

			{
				int index = STACK(4);
				int capacity = STACK(3);

				if (index < 0 || STACK(1) < 0)
				{
					// no writable destination, or negative offset to source string
					sp -= 5;
					Stack[sp-1] = 0; // false
					break;
				}

				index += STACK(6);
				
				lookup = FBehavior::StaticLookupString (STACK(2));
				
				if (!lookup) {
					// no data, operation complete
	STRCPYTORANGECOMPLETE:
					sp -= 5;
					Stack[sp-1] = 1; // true
					break;
				}

				for (int i = 0; i < STACK(1); i++)
				{
					if (! (*(lookup++)))
					{
						// no data, operation complete
						goto STRCPYTORANGECOMPLETE;
					}
				}

				switch (pcd)
				{
				case PCD_STRCPYTOSCRIPTCHRANGE:
					{
						int a = STACK(5);

						while (capacity-- > 0)
						{
							localarrays->Set(locals, a, index++, *lookup);
							if (! (*(lookup++))) goto STRCPYTORANGECOMPLETE; // complete with terminating 0
						}
						
						Stack[sp-6] = !(*lookup); // true/success if only terminating 0 was not copied
					}
					break;
				case PCD_STRCPYTOMAPCHRANGE:
					{
						int a = STACK(5);
						if (a < NUM_MAPVARS && a > 0 &&
							activeBehavior->MapVars[a])
						{
							Stack[sp-6] = activeBehavior->CopyStringToArray(*(activeBehavior->MapVars[a]), index, capacity, lookup);
						}
					}
					break;
				case PCD_STRCPYTOWORLDCHRANGE:
					{
						int a = STACK(5);

						while (capacity-- > 0)
						{
							ACS_WorldArrays[a][index++] = *lookup;
							if (! (*(lookup++))) goto STRCPYTORANGECOMPLETE; // complete with terminating 0
						}
						
						Stack[sp-6] = !(*lookup); // true/success if only terminating 0 was not copied
					}
					break;
				case PCD_STRCPYTOGLOBALCHRANGE:
					{
						int a = STACK(5);

						while (capacity-- > 0)
						{
							ACS_GlobalArrays[a][index++] = *lookup;
							if (! (*(lookup++))) goto STRCPYTORANGECOMPLETE; // complete with terminating 0
						}
						
						Stack[sp-6] = !(*lookup); // true/success if only terminating 0 was not copied
					}
					break;
				}
				sp -= 5;
			}
			break;

		case PCD_CONSOLECOMMAND:
		case PCD_CONSOLECOMMANDDIRECT:
			Printf (TEXTCOLOR_RED GAMENAME " doesn't support execution of console commands from scripts\n");
			if (pcd == PCD_CONSOLECOMMAND)
				sp -= 3;
			else
				pc += 3;
			break;
 		}
 	}

	if (runaway != 0 && InModuleScriptNumber >= 0)
	{
		activeBehavior->GetScriptPtr(InModuleScriptNumber)->ProfileData.AddRun(runaway);
	}

	if (state == SCRIPT_DivideBy0)
	{
		Printf ("Divide by zero in %s\n", ScriptPresentation(script).GetChars());
		state = SCRIPT_PleaseRemove;
	}
	else if (state == SCRIPT_ModulusBy0)
	{
		Printf ("Modulus by zero in %s\n", ScriptPresentation(script).GetChars());
		state = SCRIPT_PleaseRemove;
	}
	if (state == SCRIPT_PleaseRemove)
	{
		Unlink ();
		DLevelScript **running;
		if ((running = controller->RunningScripts.CheckKey(script)) != NULL &&
			*running == this)
		{
			controller->RunningScripts.Remove(script);
		}
	}
	else
	{
		this->pc = pc;
		assert (sp == 0);
	}
	return resultValue;
}

#undef PushtoStack

static DLevelScript *P_GetScriptGoing (AActor *who, line_t *where, int num, const ScriptPtr *code, FBehavior *module,
	const int *args, int argcount, int flags)
{
	DACSThinker *controller = DACSThinker::ActiveThinker;
	DLevelScript **running;

	if (controller && !(flags & ACS_ALWAYS) && (running = controller->RunningScripts.CheckKey(num)) != NULL)
	{
		if ((*running)->GetState() == DLevelScript::SCRIPT_Suspended)
		{
			(*running)->SetState(DLevelScript::SCRIPT_Running);
			return *running;
		}
		return NULL;
	}

	return new DLevelScript (who, where, num, code, module, args, argcount, flags);
}

DLevelScript::DLevelScript (AActor *who, line_t *where, int num, const ScriptPtr *code, FBehavior *module,
	const int *args, int argcount, int flags)
	: activeBehavior (module)
{
	if (DACSThinker::ActiveThinker == NULL)
		new DACSThinker;

	script = num;
	assert(code->VarCount >= code->ArgCount);
	numlocalvars = code->VarCount;
	localvars = new SDWORD[code->VarCount];
	memset(localvars, 0, code->VarCount * sizeof(SDWORD));
	for (int i = 0; i < MIN<int>(argcount, code->ArgCount); ++i)
	{
		localvars[i] = args[i];
	}
	pc = module->GetScriptAddress(code);
	InModuleScriptNumber = module->GetScriptIndex(code);
	activator = who;
	activationline = where;
	backSide = flags & ACS_BACKSIDE;
	activefont = SmallFont;
	hudwidth = hudheight = 0;
	ClipRectLeft = ClipRectTop = ClipRectWidth = ClipRectHeight = WrapWidth = 0;
	HandleAspect = true;
	state = SCRIPT_Running;

	// Hexen waited one second before executing any open scripts. I didn't realize
	// this when I wrote my ACS implementation. Now that I know, it's still best to
	// run them right away because there are several map properties that can't be
	// set in an editor. If an open script sets them, it looks dumb if a second
	// goes by while they're in their default state.

	if (!(flags & ACS_ALWAYS))
		DACSThinker::ActiveThinker->RunningScripts[num] = this;

	Link();

	if (level.flags2 & LEVEL2_HEXENHACK)
	{
		PutLast();
	}

	DPrintf("%s started.\n", ScriptPresentation(num).GetChars());
}

static void SetScriptState (int script, DLevelScript::EScriptState state)
{
	DACSThinker *controller = DACSThinker::ActiveThinker;
	DLevelScript **running;

	if (controller != NULL && (running = controller->RunningScripts.CheckKey(script)) != NULL)
	{
		(*running)->SetState (state);
	}
}

void P_DoDeferedScripts ()
{
	acsdefered_t *def;
	const ScriptPtr *scriptdata;
	FBehavior *module;

	// Handle defered scripts in this step, too
	def = level.info->defered;
	while (def)
	{
		acsdefered_t *next = def->next;
		switch (def->type)
		{
		case acsdefered_t::defexecute:
		case acsdefered_t::defexealways:
			scriptdata = FBehavior::StaticFindScript (def->script, module);
			if (scriptdata)
			{
				P_GetScriptGoing ((unsigned)def->playernum < MAXPLAYERS &&
					playeringame[def->playernum] ? players[def->playernum].mo : NULL,
					NULL, def->script,
					scriptdata, module,
					def->args, 3,
					def->type == acsdefered_t::defexealways ? ACS_ALWAYS : 0);
			}
			else
			{
				Printf ("P_DoDeferredScripts: Unknown %s\n", ScriptPresentation(def->script).GetChars());
			}
			break;

		case acsdefered_t::defsuspend:
			SetScriptState (def->script, DLevelScript::SCRIPT_Suspended);
			DPrintf ("Deferred suspend of %s\n", ScriptPresentation(def->script).GetChars());
			break;

		case acsdefered_t::defterminate:
			SetScriptState (def->script, DLevelScript::SCRIPT_PleaseRemove);
			DPrintf ("Deferred terminate of %s\n", ScriptPresentation(def->script).GetChars());
			break;
		}
		delete def;
		def = next;
	}
	level.info->defered = NULL;
}

static void addDefered (level_info_t *i, acsdefered_t::EType type, int script, const int *args, int argcount, AActor *who)
{
	if (i)
	{
		acsdefered_t *def = new acsdefered_t;
		int j;

		def->next = i->defered;
		def->type = type;
		def->script = script;
		for (j = 0; (size_t)j < countof(def->args) && j < argcount; ++j)
		{
			def->args[j] = args[j];
		}
		while ((size_t)j < countof(def->args))
		{
			def->args[j++] = 0;
		}
		if (who != NULL && who->player != NULL)
		{
			def->playernum = int(who->player - players);
		}
		else
		{
			def->playernum = -1;
		}
		i->defered = def;
		DPrintf ("%s on map %s deferred\n", ScriptPresentation(script).GetChars(), i->MapName.GetChars());
	}
}

EXTERN_CVAR (Bool, sv_cheats)

int P_StartScript (AActor *who, line_t *where, int script, const char *map, const int *args, int argcount, int flags)
{
	if (map == NULL || 0 == strnicmp (level.MapName, map, 8))
	{
		FBehavior *module = NULL;
		const ScriptPtr *scriptdata;

		if ((scriptdata = FBehavior::StaticFindScript (script, module)) != NULL)
		{
			if ((flags & ACS_NET) && netgame && !sv_cheats)
			{
				// If playing multiplayer and cheats are disallowed, check to
				// make sure only net scripts are run.
				if (!(scriptdata->Flags & SCRIPTF_Net))
				{
					Printf(PRINT_BOLD, "%s tried to puke %s (\n",
						who->player->userinfo.GetName(), ScriptPresentation(script).GetChars());
					for (int i = 0; i < argcount; ++i)
					{
						Printf(PRINT_BOLD, "%d%s", args[i], i == argcount-1 ? "" : ", ");
					}
					Printf(PRINT_BOLD, ")\n");
					return false;
				}
			}
			DLevelScript *runningScript = P_GetScriptGoing (who, where, script,
				scriptdata, module, args, argcount, flags);
			if (runningScript != NULL)
			{
				if (flags & ACS_WANTRESULT)
				{
					return runningScript->RunScript();
				}
				return true;
			}
			return false;
		}
		else
		{
			if (!(flags & ACS_NET) || (who && who->player == &players[consoleplayer]))
			{
				Printf("P_StartScript: Unknown %s\n", ScriptPresentation(script).GetChars());
			}
		}
	}
	else
	{
		addDefered (FindLevelInfo (map),
					(flags & ACS_ALWAYS) ? acsdefered_t::defexealways : acsdefered_t::defexecute,
					script, args, argcount, who);
		return true;
	}
	return false;
}

void P_SuspendScript (int script, const char *map)
{
	if (strnicmp (level.MapName, map, 8))
		addDefered (FindLevelInfo (map), acsdefered_t::defsuspend, script, NULL, 0, NULL);
	else
		SetScriptState (script, DLevelScript::SCRIPT_Suspended);
}

void P_TerminateScript (int script, const char *map)
{
	if (strnicmp (level.MapName, map, 8))
		addDefered (FindLevelInfo (map), acsdefered_t::defterminate, script, NULL, 0, NULL);
	else
		SetScriptState (script, DLevelScript::SCRIPT_PleaseRemove);
}

FArchive &operator<< (FArchive &arc, acsdefered_t *&defertop)
{
	BYTE more;

	if (arc.IsStoring ())
	{
		acsdefered_t *defer = defertop;
		more = 1;
		while (defer)
		{
			BYTE type;
			arc << more;
			type = (BYTE)defer->type;
			arc << type;
			P_SerializeACSScriptNumber(arc, defer->script, false);
			arc << defer->playernum << defer->args[0] << defer->args[1] << defer->args[2];
			defer = defer->next;
		}
		more = 0;
		arc << more;
	}
	else
	{
		acsdefered_t **defer = &defertop;

		arc << more;
		while (more)
		{
			*defer = new acsdefered_t;
			arc << more;
			(*defer)->type = (acsdefered_t::EType)more;
			P_SerializeACSScriptNumber(arc, (*defer)->script, false);
			arc << (*defer)->playernum << (*defer)->args[0] << (*defer)->args[1] << (*defer)->args[2];
			defer = &((*defer)->next);
			arc << more;
		}
		*defer = NULL;
	}
	return arc;
}

CCMD (scriptstat)
{
	if (DACSThinker::ActiveThinker == NULL)
	{
		Printf ("No scripts are running.\n");
	}
	else
	{
		DACSThinker::ActiveThinker->DumpScriptStatus ();
	}
}

void DACSThinker::DumpScriptStatus ()
{
	static const char *stateNames[] =
	{
		"Running",
		"Suspended",
		"Delayed",
		"TagWait",
		"PolyWait",
		"ScriptWaitPre",
		"ScriptWait",
		"PleaseRemove"
	};
	DLevelScript *script = Scripts;

	while (script != NULL)
	{
		Printf("%s: %s\n", ScriptPresentation(script->script).GetChars(), stateNames[script->state]);
		script = script->next;
	}
}

// Profiling support --------------------------------------------------------

ACSProfileInfo::ACSProfileInfo()
{
	Reset();
}

void ACSProfileInfo::Reset()
{
	TotalInstr = 0;
	NumRuns = 0;
	MinInstrPerRun = UINT_MAX;
	MaxInstrPerRun = 0;
}

void ACSProfileInfo::AddRun(unsigned int num_instr)
{
	TotalInstr += num_instr;
	NumRuns++;
	if (num_instr < MinInstrPerRun)
	{
		MinInstrPerRun = num_instr;
	}
	if (num_instr > MaxInstrPerRun)
	{
		MaxInstrPerRun = num_instr;
	}
}

void ArrangeScriptProfiles(TArray<ProfileCollector> &profiles)
{
	for (unsigned int mod_num = 0; mod_num < FBehavior::StaticModules.Size(); ++mod_num)
	{
		FBehavior *module = FBehavior::StaticModules[mod_num];
		ProfileCollector prof;
		prof.Module = module;
		for (int i = 0; i < module->NumScripts; ++i)
		{
			prof.Index = i;
			prof.ProfileData = &module->Scripts[i].ProfileData;
			profiles.Push(prof);
		}
	}
}

void ArrangeFunctionProfiles(TArray<ProfileCollector> &profiles)
{
	for (unsigned int mod_num = 0; mod_num < FBehavior::StaticModules.Size(); ++mod_num)
	{
		FBehavior *module = FBehavior::StaticModules[mod_num];
		ProfileCollector prof;
		prof.Module = module;
		for (int i = 0; i < module->NumFunctions; ++i)
		{
			ScriptFunction *func = (ScriptFunction *)module->Functions + i;
			if (func->ImportNum == 0)
			{
				prof.Index = i;
				prof.ProfileData = module->FunctionProfileData + i;
				profiles.Push(prof);
			}
		}
	}
}

void ClearProfiles(TArray<ProfileCollector> &profiles)
{
	for (unsigned int i = 0; i < profiles.Size(); ++i)
	{
		profiles[i].ProfileData->Reset();
	}
}

static int sort_by_total_instr(const void *a_, const void *b_)
{
	const ProfileCollector *a = (const ProfileCollector *)a_;
	const ProfileCollector *b = (const ProfileCollector *)b_;

	assert(a != NULL && a->ProfileData != NULL);
	assert(b != NULL && b->ProfileData != NULL);
	return (int)(b->ProfileData->TotalInstr - a->ProfileData->TotalInstr);
}

static int sort_by_min(const void *a_, const void *b_)
{
	const ProfileCollector *a = (const ProfileCollector *)a_;
	const ProfileCollector *b = (const ProfileCollector *)b_;

	return b->ProfileData->MinInstrPerRun - a->ProfileData->MinInstrPerRun;
}

static int sort_by_max(const void *a_, const void *b_)
{
	const ProfileCollector *a = (const ProfileCollector *)a_;
	const ProfileCollector *b = (const ProfileCollector *)b_;

	return b->ProfileData->MaxInstrPerRun - a->ProfileData->MaxInstrPerRun;
}

static int sort_by_avg(const void *a_, const void *b_)
{
	const ProfileCollector *a = (const ProfileCollector *)a_;
	const ProfileCollector *b = (const ProfileCollector *)b_;

	int a_avg = a->ProfileData->NumRuns == 0 ? 0 : int(a->ProfileData->TotalInstr / a->ProfileData->NumRuns);
	int b_avg = b->ProfileData->NumRuns == 0 ? 0 : int(b->ProfileData->TotalInstr / b->ProfileData->NumRuns);
	return b_avg - a_avg;
}

static int sort_by_runs(const void *a_, const void *b_)
{
	const ProfileCollector *a = (const ProfileCollector *)a_;
	const ProfileCollector *b = (const ProfileCollector *)b_;

	return b->ProfileData->NumRuns - a->ProfileData->NumRuns;
}

static void ShowProfileData(TArray<ProfileCollector> &profiles, long ilimit,
	int (*sorter)(const void *, const void *), bool functions)
{
	static const char *const typelabels[2] = { "script", "function" };

	if (profiles.Size() == 0)
	{
		return;
	}

	unsigned int limit;
	char modname[13];
	char scriptname[21];

	qsort(&profiles[0], profiles.Size(), sizeof(ProfileCollector), sorter);

	if (ilimit > 0)
	{
		Printf(TEXTCOLOR_ORANGE "Top %ld %ss:\n", ilimit, typelabels[functions]);
		limit = (unsigned int)ilimit;
	}
	else
	{
		Printf(TEXTCOLOR_ORANGE "All %ss:\n", typelabels[functions]);
		limit = UINT_MAX;
	}

	Printf(TEXTCOLOR_YELLOW "Module       %-20s      Total    Runs     Avg     Min     Max\n", typelabels[functions]);
	Printf(TEXTCOLOR_YELLOW "------------ -------------------- ---------- ------- ------- ------- -------\n");
	for (unsigned int i = 0; i < limit && i < profiles.Size(); ++i)
	{
		ProfileCollector *prof = &profiles[i];
		if (prof->ProfileData->NumRuns == 0)
		{ // Don't list ones that haven't run.
			continue;
		}

		// Module name
		mysnprintf(modname, sizeof(modname), "%s", prof->Module->GetModuleName());

		// Script/function name
		if (functions)
		{
			DWORD *fnames = (DWORD *)prof->Module->FindChunk(MAKE_ID('F','N','A','M'));
			if (prof->Index >= 0 && prof->Index < (int)LittleLong(fnames[2]))
			{
				mysnprintf(scriptname, sizeof(scriptname), "%s",
					(char *)(fnames + 2) + LittleLong(fnames[3+prof->Index]));
			}
			else
			{
				mysnprintf(scriptname, sizeof(scriptname), "Function %d", prof->Index);
			}
		}
		else
		{
			mysnprintf(scriptname, sizeof(scriptname), "%s",
				ScriptPresentation(prof->Module->GetScriptPtr(prof->Index)->Number).GetChars() + 7);
		}
		Printf("%-12s %-20s%11llu%8u%8u%8u%8u\n",
			modname, scriptname,
			prof->ProfileData->TotalInstr,
			prof->ProfileData->NumRuns,
			unsigned(prof->ProfileData->TotalInstr / prof->ProfileData->NumRuns),
			prof->ProfileData->MinInstrPerRun,
			prof->ProfileData->MaxInstrPerRun
			);
	}
}

CCMD(acsprofile)
{
	static int (*sort_funcs[])(const void*, const void *) =
	{
		sort_by_total_instr,
		sort_by_min,
		sort_by_max,
		sort_by_avg,
		sort_by_runs
	};
	static const char *sort_names[] = { "total", "min", "max", "avg", "runs" };
	static const BYTE sort_match_len[] = {   1,     2,     2,     1,      1 };

	TArray<ProfileCollector> ScriptProfiles, FuncProfiles;
	long limit = 10;
	int (*sorter)(const void *, const void *) = sort_by_total_instr;

	assert(countof(sort_names) == countof(sort_match_len));

	ArrangeScriptProfiles(ScriptProfiles);
	ArrangeFunctionProfiles(FuncProfiles);

	if (argv.argc() > 1)
	{
		// `acsprofile clear` will zero all profiling information collected so far.
		if (stricmp(argv[1], "clear") == 0)
		{
			ClearProfiles(ScriptProfiles);
			ClearProfiles(FuncProfiles);
			return;
		}
		for (int i = 1; i < argv.argc(); ++i)
		{
			// If it's a number, set the display limit.
			char *endptr;
			long num = strtol(argv[i], &endptr, 0);
			if (endptr != argv[i])
			{
				limit = num;
				continue;
			}
			// If it's a name, set the sort method. We accept partial matches for
			// options that are shorter than the sort name.
			size_t optlen = strlen(argv[i]);
			unsigned int j;
			for (j = 0; j < countof(sort_names); ++j)
			{
				if (optlen < sort_match_len[j] || optlen > strlen(sort_names[j]))
				{ // Too short or long to match.
					continue;
				}
				if (strnicmp(argv[i], sort_names[j], optlen) == 0)
				{
					sorter = sort_funcs[j];
					break;
				}
			}
			if (j == countof(sort_names))
			{
				Printf("Unknown option '%s'\n", argv[i]);
				Printf("acsprofile clear : Reset profiling information\n");
				Printf("acsprofile [total|min|max|avg|runs] [<limit>]\n");
				return;
			}
		}
	}

	ShowProfileData(ScriptProfiles, limit, sorter, false);
	ShowProfileData(FuncProfiles, limit, sorter, true);
}
