#include <string.h>
#include "name.h"
#include "c_dispatch.h"

// Define the predefined names.
static const char *PredefinedNames[] =
{
#define xx(n) #n,
#include "namedef.h"
#undef xx
};

int FName::Buckets[FName::HASH_SIZE];
TArray<FName::MainName> FName::NameArray;
bool FName::Inited;

FName::MainName::MainName (int next)
: NextHash(next)
{
}

int FName::FindName (const char *text, bool noCreate)
{
	if (!Inited) InitBuckets ();

	if (text == NULL)
	{
		return 0;
	}

	int hash = MakeKey (text) % HASH_SIZE;
	int scanner = Buckets[hash];

	// See if the name already exists.
	while (scanner >= 0)
	{
		if (stricmp (NameArray[scanner].Text.GetChars(), text) == 0)
		{
			return scanner;
		}
		scanner = NameArray[scanner].NextHash;
	}

	// If we get here, then the name does not exist.
	if (noCreate)
	{
		return 0;
	}

	// Assigning the string to the name after pushing it avoids needless
	// manipulation of the string pool.
	MainName entry (Buckets[hash]);
	unsigned int index = NameArray.Push (entry);
	NameArray[index].Text = text;
	Buckets[hash] = index;
	return index;
}

void FName::InitBuckets ()
{
	size_t i;

	Inited = true;
	for (i = 0; i < HASH_SIZE; ++i)
	{
		Buckets[i] = -1;
	}

	// Register built-in names. 'None' must be name 0.
	for (i = 0; i < countof(PredefinedNames); ++i)
	{
		FindName (PredefinedNames[i], false);
	}
}
