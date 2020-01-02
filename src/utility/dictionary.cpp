#include "utility/dictionary.h"

#include "scripting/vm/vm.h"
#include "serializer.h"

//=====================================================================================
//
// Dictionary exports
//
//=====================================================================================

DEFINE_ACTION_FUNCTION(_Dictionary, Create)
{
	ACTION_RETURN_POINTER(new Dictionary);
}

static void DictInsert(Dictionary *dict, const FString &key, const FString &value)
{
	dict->Insert(key, value);
}

DEFINE_ACTION_FUNCTION_NATIVE(_Dictionary, Insert, DictInsert)
{
	PARAM_SELF_STRUCT_PROLOGUE(Dictionary);
	PARAM_STRING(key);
	PARAM_STRING(value);
	DictInsert(self, key, value);
	return 0;
}

static void DictAt(const Dictionary *dict, const FString &key, FString *result)
{
	const FString *value = dict->CheckKey(key);
	*result = value ? *value : "";
}

DEFINE_ACTION_FUNCTION_NATIVE(_Dictionary, At, DictAt)
{
	PARAM_SELF_STRUCT_PROLOGUE(Dictionary);
	PARAM_STRING(key);
	FString result;
	DictAt(self, key, &result);
	ACTION_RETURN_STRING(result);
}

static void DictToString(const Dictionary *dict, FString *result)
{
	*result = DictionaryToString(*dict);
}

DEFINE_ACTION_FUNCTION_NATIVE(_Dictionary, ToString, DictToString)
{
	PARAM_SELF_STRUCT_PROLOGUE(Dictionary);
	FString result;
	DictToString(self, &result);
	ACTION_RETURN_STRING(result);
}

static Dictionary *DictFromString(const FString& string)
{
	return DictionaryFromString(string);
}

DEFINE_ACTION_FUNCTION_NATIVE(_Dictionary, FromString, DictFromString)
{
	PARAM_PROLOGUE;
	PARAM_STRING(string);
	ACTION_RETURN_POINTER(DictFromString(string));
}

static void DictRemove(Dictionary *dict, const FString &key)
{
	dict->Remove(key);
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
// DictionaryIterator exports
//
//=====================================================================================

DictionaryIterator::DictionaryIterator(const Dictionary &dict)
	: Iterator(dict)
	, Pair(nullptr)
{
}

static DictionaryIterator *DictIteratorCreate(const Dictionary *dict)
{
	return new DictionaryIterator(*dict);
}

DEFINE_ACTION_FUNCTION_NATIVE(_DictionaryIterator, Create, DictIteratorCreate)
{
	PARAM_PROLOGUE;
	PARAM_POINTER(dict, Dictionary);
	ACTION_RETURN_POINTER(DictIteratorCreate(dict));
}

static int DictIteratorNext(DictionaryIterator *self)
{
	return self->Iterator.NextPair(self->Pair);
}

DEFINE_ACTION_FUNCTION_NATIVE(_DictionaryIterator, Next, DictIteratorNext)
{
	PARAM_SELF_STRUCT_PROLOGUE(DictionaryIterator);
	ACTION_RETURN_BOOL(DictIteratorNext(self));
}

static void DictIteratorKey(const DictionaryIterator *self, FString *result)
{
	*result = self->Pair ? self->Pair->Key : FString {};
}

DEFINE_ACTION_FUNCTION_NATIVE(_DictionaryIterator, Key, DictIteratorKey)
{
	PARAM_SELF_STRUCT_PROLOGUE(DictionaryIterator);
	FString result;
	DictIteratorKey(self, &result);
	ACTION_RETURN_STRING(result);
}

static void DictIteratorValue(const DictionaryIterator *self, FString *result)
{
	*result = self->Pair ? self->Pair->Value : FString {};
}

DEFINE_ACTION_FUNCTION_NATIVE(_DictionaryIterator, Value, DictIteratorValue)
{
	PARAM_SELF_STRUCT_PROLOGUE(DictionaryIterator);
	FString result;
	DictIteratorValue(self, &result);
	ACTION_RETURN_STRING(result);
}
