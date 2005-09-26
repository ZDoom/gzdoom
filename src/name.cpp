#include <string.h>
#include "name.h"
#include "c_dispatch.h"

int name::Buckets[name::HASH_SIZE];
TArray<name::MainName> name::NameArray;
bool name::Inited;

name::MainName::MainName (int next)
: NextHash(next)
{
}
int name::FindName (const char *text, bool noCreate)
{
	if (!Inited) InitBuckets ();

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

void name::InitBuckets ()
{
	Inited = true;
	for (int i = 0; i < HASH_SIZE; ++i)
	{
		Buckets[i] = -1;
	}
	name::FindName ("None", false);	// 'None' is always name 0.
}
