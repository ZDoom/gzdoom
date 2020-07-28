#include "dictionary.h"

#include "vm.h"
#include "serializer.h"

#include <cassert>

//=====================================================================================
//
// DObject implementations for Dictionary and DictionaryIterator
//
//=====================================================================================

IMPLEMENT_CLASS(Dictionary, false, false);

IMPLEMENT_CLASS(DictionaryIterator, false, true);

IMPLEMENT_POINTERS_START(DictionaryIterator)
IMPLEMENT_POINTER(Dict)
IMPLEMENT_POINTERS_END

//=====================================================================================
//
// Dictionary functions
//
//=====================================================================================

void Dictionary::Serialize(FSerializer &arc)
{
	Super::Serialize(arc);

	static const char key[] = "dictionary";

	if (arc.isWriting())
	{
		// Pass this instance to serializer.
		Dictionary *pointerToThis = this;
		arc(key, pointerToThis);
	}
	else
	{
		// Receive new Dictionary, copy contents, clean up.
		Dictionary *pointerToDeserializedDictionary;
		arc(key, pointerToDeserializedDictionary);
		Map.TransferFrom(pointerToDeserializedDictionary->Map);
		pointerToDeserializedDictionary->Destroy();
	}
}

static Dictionary *DictCreate()
{
	Dictionary *dict { Create<Dictionary>() };

	return dict;
}

static void DictInsert(Dictionary *dict, const FString &key, const FString &value)
{
	dict->Map.Insert(key, value);
}

static void DictAt(const Dictionary *dict, const FString &key, FString *result)
{
	const FString *value = dict->Map.CheckKey(key);
	*result = value ? *value : FString();
}

static void DictToString(const Dictionary *dict, FString *result)
{
	*result = DictionaryToString(*dict);
}

static void DictRemove(Dictionary *dict, const FString &key)
{
	dict->Map.Remove(key);
}

//=====================================================================================
//
// Dictionary exports
//
//=====================================================================================

DEFINE_ACTION_FUNCTION_NATIVE(_Dictionary, Create, DictCreate)
{
	ACTION_RETURN_POINTER(DictCreate());
}

DEFINE_ACTION_FUNCTION_NATIVE(_Dictionary, Insert, DictInsert)
{
	PARAM_SELF_STRUCT_PROLOGUE(Dictionary);
	PARAM_STRING(key);
	PARAM_STRING(value);
	DictInsert(self, key, value);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(_Dictionary, At, DictAt)
{
	PARAM_SELF_STRUCT_PROLOGUE(Dictionary);
	PARAM_STRING(key);
	FString result;
	DictAt(self, key, &result);
	ACTION_RETURN_STRING(result);
}

DEFINE_ACTION_FUNCTION_NATIVE(_Dictionary, ToString, DictToString)
{
	PARAM_SELF_STRUCT_PROLOGUE(Dictionary);
	FString result;
	DictToString(self, &result);
	ACTION_RETURN_STRING(result);
}

DEFINE_ACTION_FUNCTION_NATIVE(_Dictionary, FromString, DictionaryFromString)
{
	PARAM_PROLOGUE;
	PARAM_STRING(string);
	ACTION_RETURN_POINTER(DictionaryFromString(string));
}

DEFINE_ACTION_FUNCTION_NATIVE(_Dictionary, Remove, DictRemove)
{
	PARAM_SELF_STRUCT_PROLOGUE(Dictionary);
	PARAM_STRING(key);
	DictRemove(self, key);
	return 0;
}

//=====================================================================================
//
// DictionaryIterator functions
//
//=====================================================================================

DictionaryIterator::DictionaryIterator()
	: Iterator(nullptr)
	, Pair(nullptr)
	, Dict(nullptr)
{
}

void DictionaryIterator::Serialize(FSerializer &arc)
{
	if (arc.isWriting())
	{
		I_Error("Attempt to save pointer to unhandled type DictionaryIterator");
	}
}

void DictionaryIterator::init(Dictionary *dict)
{
	Iterator = std::make_unique<Dictionary::ConstIterator>(dict->Map);
	Dict = dict;

	GC::WriteBarrier(this, Dict);
}

static DictionaryIterator *DictIteratorCreate(Dictionary *dict)
{
	DictionaryIterator *iterator = Create<DictionaryIterator>();
	iterator->init(dict);

	return iterator;
}

static int DictIteratorNext(DictionaryIterator *self)
{
	assert(self->Iterator != nullptr);

	const bool hasNext { self->Iterator->NextPair(self->Pair) };
	return hasNext;
}

static void DictIteratorKey(const DictionaryIterator *self, FString *result)
{
	*result = self->Pair ? self->Pair->Key : FString {};
}

static void DictIteratorValue(const DictionaryIterator *self, FString *result)
{
	*result = self->Pair ? self->Pair->Value : FString {};
}

//=====================================================================================
//
// DictionaryIterator exports
//
//=====================================================================================

DEFINE_ACTION_FUNCTION_NATIVE(_DictionaryIterator, Create, DictIteratorCreate)
{
	PARAM_PROLOGUE;
	PARAM_POINTER(dict, Dictionary);
	ACTION_RETURN_POINTER(DictIteratorCreate(dict));
}

DEFINE_ACTION_FUNCTION_NATIVE(_DictionaryIterator, Next, DictIteratorNext)
{
	PARAM_SELF_STRUCT_PROLOGUE(DictionaryIterator);
	ACTION_RETURN_BOOL(DictIteratorNext(self));
}

DEFINE_ACTION_FUNCTION_NATIVE(_DictionaryIterator, Key, DictIteratorKey)
{
	PARAM_SELF_STRUCT_PROLOGUE(DictionaryIterator);
	FString result;
	DictIteratorKey(self, &result);
	ACTION_RETURN_STRING(result);
}

DEFINE_ACTION_FUNCTION_NATIVE(_DictionaryIterator, Value, DictIteratorValue)
{
	PARAM_SELF_STRUCT_PROLOGUE(DictionaryIterator);
	FString result;
	DictIteratorValue(self, &result);
	ACTION_RETURN_STRING(result);
}
