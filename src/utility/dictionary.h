#pragma once

#include "tarray.h"
#include "zstring.h"
#include "dobject.h"

#include <memory>

/**
 * @brief The Dictionary class exists to be exported to ZScript.
 *
 * It is a string-to-string map.
 *
 * It is derived from DObject to be a part of normal GC process.
 */
class Dictionary final : public DObject
{
	DECLARE_CLASS(Dictionary, DObject)

public:

	using StringMap = TMap<FString, FString>;
	using ConstIterator = StringMap::ConstIterator;
	using ConstPair = StringMap::ConstPair;

	void Serialize(FSerializer &arc) override;

	StringMap Map;
};

/**
 * @brief The DictionaryIterator class exists to be exported to ZScript.
 *
 * It provides iterating over a Dictionary. The order is not specified.
 *
 * It is derived from DObject to be a part of normal GC process.
 */
class DictionaryIterator final : public DObject
{
	DECLARE_CLASS(DictionaryIterator, DObject)
	HAS_OBJECT_POINTERS

public:

	~DictionaryIterator() override = default;

	/**
	 * IMPLEMENT_CLASS macro needs a constructor without parameters.
	 *
	 * @see init().
	 */
	DictionaryIterator();

	void Serialize(FSerializer &arc) override;

	/**
	 * @brief init function complements constructor.
	 * @attention always call init after constructing DictionaryIterator.
	 */
	void init(Dictionary *dict);

	std::unique_ptr<Dictionary::ConstIterator> Iterator;
	Dictionary::ConstPair *Pair;

	/**
	 * @brief Dictionary attribute exists for holding a pointer to iterated
	 * dictionary, so it is known by GC.
	 */
	Dictionary *Dict;
};
