#pragma once

#include "tarray.h"
#include "zstring.h"

using Dictionary = TMap<FString, FString>;

struct DictionaryIterator
{
	explicit DictionaryIterator(const Dictionary &dict);

	Dictionary::ConstIterator Iterator;
	Dictionary::ConstPair *Pair;
};
