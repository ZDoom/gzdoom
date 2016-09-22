#define RAPIDJSON_48BITPOINTER_OPTIMIZATION 0	// disable this insanity which is bound to make the code break over time.
#define RAPIDJSON_HAS_CXX11_RVALUE_REFS 1
#define RAPIDJSON_HAS_CXX11_RANGE_FOR 1
#define RAPIDJSON_PARSE_DEFAULT_FLAGS kParseFullPrecisionFlag

#include "rapidjson/rapidjson.h"
#include "rapidjson/writer.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/document.h"
#include "serializer.h"
#include "r_data/colormaps.h"
#include "r_data/r_interpolate.h"
#include "r_defs.h"
#include "r_state.h"
#include "p_lnspec.h"
#include "i_system.h"
#include "w_wad.h"
#include "p_terrain.h"
#include "c_dispatch.h"
#include "p_setup.h"
#include "p_conversation.h"
#include "dsectoreffect.h"
#include "d_player.h"
#include "r_data/r_interpolate.h"
#include "g_shared/a_sharedglobal.h"
#include "po_man.h"
#include "v_font.h"
#include "w_zip.h"

char nulspace[1024 * 1024 * 4];

//==========================================================================
//
//
//
//==========================================================================

struct FJSONObject
{
	rapidjson::Value *mObject;
	rapidjson::Value::MemberIterator mIterator;
	int mIndex;
	bool mRandomAccess;

	FJSONObject(rapidjson::Value *v, bool randomaccess = false)
	{
		mObject = v;
		mRandomAccess = randomaccess;
		if (v->IsObject()) mIterator = v->MemberBegin();
		else if (v->IsArray())
		{
			mIndex = 0;
		}
	}
};

//==========================================================================
//
// some wrapper stuff to keep the RapidJSON dependencies out of the global headers.
// FSerializer should not expose any of this.
//
//==========================================================================

struct FWriter
{
	typedef rapidjson::Writer<rapidjson::StringBuffer, rapidjson::ASCII<> > Writer;
	typedef rapidjson::PrettyWriter<rapidjson::StringBuffer, rapidjson::ASCII<> > PrettyWriter;

	Writer *mWriter1;
	PrettyWriter *mWriter2;
	TArray<bool> mInObject;
	rapidjson::StringBuffer mOutString;
	TArray<DObject *> mDObjects;
	TMap<DObject *, int> mObjectMap;
	
	FWriter(bool pretty)
	{
		if (!pretty)
		{
			mWriter1 = new Writer(mOutString);
			mWriter2 = nullptr;
		}
		else
		{
			mWriter1 = nullptr;
			mWriter2 = new PrettyWriter(mOutString);
		}
	}

	~FWriter()
	{
		if (mWriter1) delete mWriter1;
		if (mWriter2) delete mWriter2;
	}


	bool inObject() const
	{
		return mInObject.Size() > 0 && mInObject.Last();
	}

	void StartObject()
	{
		if (mWriter1) mWriter1->StartObject();
		else if (mWriter2) mWriter2->StartObject();
	}

	void EndObject()
	{
		if (mWriter1) mWriter1->EndObject();
		else if (mWriter2) mWriter2->EndObject();
	}

	void StartArray()
	{
		if (mWriter1) mWriter1->StartArray();
		else if (mWriter2) mWriter2->StartArray();
	}

	void EndArray()
	{
		if (mWriter1) mWriter1->EndArray();
		else if (mWriter2) mWriter2->EndArray();
	}

	void Key(const char *k)
	{
		if (mWriter1) mWriter1->Key(k);
		else if (mWriter2) mWriter2->Key(k);
	}

	void Null()
	{
		if (mWriter1) mWriter1->Null();
		else if (mWriter2) mWriter2->Null();
	}

	void String(const char *k)
	{
		if (mWriter1) mWriter1->String(k);
		else if (mWriter2) mWriter2->String(k);
	}

	void String(const char *k, int size)
	{
		if (mWriter1) mWriter1->String(k, size);
		else if (mWriter2) mWriter2->String(k, size);
	}

	void Bool(bool k)
	{
		if (mWriter1) mWriter1->Bool(k);
		else if (mWriter2) mWriter2->Bool(k);
	}

	void Int(int32_t k)
	{
		if (mWriter1) mWriter1->Int(k);
		else if (mWriter2) mWriter2->Int(k);
	}

	void Int64(int64_t k)
	{
		if (mWriter1) mWriter1->Int64(k);
		else if (mWriter2) mWriter2->Int64(k);
	}

	void Uint(uint32_t k)
	{
		if (mWriter1) mWriter1->Uint(k);
		else if (mWriter2) mWriter2->Uint(k);
	}

	void Uint64(int64_t k)
	{
		if (mWriter1) mWriter1->Uint64(k);
		else if (mWriter2) mWriter2->Uint64(k);
	}

	void Double(double k)
	{
		if (mWriter1) mWriter1->Double(k);
		else if (mWriter2) mWriter2->Double(k);
	}

};

//==========================================================================
//
//
//
//==========================================================================

struct FReader
{
	TArray<FJSONObject> mObjects;
	rapidjson::Document mDoc;

	FReader(const char *buffer, size_t length, bool randomaccess)
	{
		rapidjson::Document doc;
		mDoc.Parse(buffer, length);
		mObjects.Push(FJSONObject(&mDoc, randomaccess));
	}
	
	rapidjson::Value *FindKey(const char *key)
	{
		FJSONObject &obj = mObjects.Last();
		
		if (obj.mObject->IsObject())
		{
			if (!obj.mRandomAccess)
			{
				if (obj.mIterator != obj.mObject->MemberEnd())
				{
					if (!strcmp(key, obj.mIterator->name.GetString()))
					{
						return &(obj.mIterator++)->value;
					}
				}
			}
			else
			{
				// for unordered searches. This is slower but will not rely on sequential order of items.
				auto it = obj.mObject->FindMember(key);
				if (it == obj.mObject->MemberEnd()) return nullptr;
				return &it->value;
			}
		}
		else if (obj.mObject->IsArray() && (unsigned)obj.mIndex < obj.mObject->Size())
		{
			return &(*obj.mObject)[obj.mIndex++];
		}
		return nullptr;
	}
};


//==========================================================================
//
//
//
//==========================================================================

bool FSerializer::OpenWriter(bool pretty)
{
	if (w != nullptr || r != nullptr) return false;
	w = new FWriter(pretty);

	BeginObject(nullptr);
	return true;
}

//==========================================================================
//
//
//
//==========================================================================

bool FSerializer::OpenReader(const char *buffer, size_t length, bool randomaccess)
{
	if (w != nullptr || r != nullptr) return false;
	r = new FReader(buffer, length, randomaccess);
	return true;
}

//==========================================================================
//
//
//
//==========================================================================

bool FSerializer::OpenReader(FCompressedBuffer *input, bool randomaccess)
{
	if (input->mSize <= 0 || input->mBuffer == nullptr) return false;
	if (w != nullptr || r != nullptr) return false;

	if (input->mMethod == METHOD_STORED)
	{
		r = new FReader((char*)input->mBuffer, input->mSize, randomaccess);
		return true;
	}
	else
	{
		char *unpacked = new char[input->mSize];
		input->Decompress(unpacked);
		r = new FReader(unpacked, input->mSize, randomaccess);
		return true;
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FSerializer::Close()
{	
	if (w != nullptr)
	{
		delete w;
		w = nullptr;
	}
	if (r != nullptr)
	{
		delete r;
		r = nullptr;
	}
}

//==========================================================================
//
//
//
//==========================================================================

int FSerializer::ArraySize()
{
	if (r != nullptr && r->mObjects.Last().mObject->IsArray())
	{
		return r->mObjects.Last().mObject->Size();
	}
	else
	{
		return 0;
	}
}

//==========================================================================
//
//
//
//==========================================================================

bool FSerializer::canSkip() const
{
	return isWriting() && w->inObject();
}

//==========================================================================
//
//
//
//==========================================================================

void FSerializer::WriteKey(const char *key)
{
	if (isWriting() && w->inObject())
	{
		assert(key != nullptr);
		if (key == nullptr)
		{
			I_Error("missing element name");
		}
		w->Key(key);
	}
}

//==========================================================================
//
//
//
//==========================================================================

bool FSerializer::BeginObject(const char *name, bool randomaccess)
{
	if (isWriting())
	{
		WriteKey(name);
		w->StartObject();
		w->mInObject.Push(true);
	}
	else
	{
		auto val = r->FindKey(name);
		if (val != nullptr)
		{
			if (val->IsObject())
			{
				r->mObjects.Push(FJSONObject(val, randomaccess));
			}
			else
			{
				I_Error("Object expected for '%s'", name);
			}
		}
		else
		{
			return false;
		}
	}
	return true;
}

//==========================================================================
//
//
//
//==========================================================================

void FSerializer::EndObject()
{
	if (isWriting())
	{
		if (w->inObject())
		{
			w->EndObject();
			w->mInObject.Pop();
		}
		else
		{
			I_Error("EndObject call not inside an object");
		}
	}
	else
	{
		r->mObjects.Pop();
	}
}

//==========================================================================
//
//
//
//==========================================================================

bool FSerializer::BeginArray(const char *name)
{
	if (isWriting())
	{
		WriteKey(name);
		w->StartArray();
		w->mInObject.Push(false);
	}
	else
	{
		auto val = r->FindKey(name);
		if (val != nullptr)
		{
			if (val->IsArray())
			{
				r->mObjects.Push(FJSONObject(val));
			}
			else
			{
				I_Error("Array expected for '%s'", name);
			}
		}
		else
		{
			return false;
		}
	}
	return true;
}

//==========================================================================
//
//
//
//==========================================================================

void FSerializer::EndArray()
{
	if (isWriting())
	{
		if (!w->inObject())
		{
			w->EndArray();
			w->mInObject.Pop();
		}
		else
		{
			I_Error("EndArray call not inside an array");
		}
	}
	else
	{
		r->mObjects.Pop();
	}
}

//==========================================================================
//
// Special handler for args (because ACS specials' arg0 needs special treatment.)
//
//==========================================================================

FSerializer &FSerializer::Args(const char *key, int *args, int *defargs, int special)
{
	if (isWriting())
	{
		if (w->inObject() && defargs != nullptr && !memcmp(args, defargs, 5 * sizeof(int)))
		{
			return *this;
		}

		WriteKey(key);
		w->StartArray();
		for (int i = 0; i < 5; i++)
		{
			if (i == 0 && args[i] < 0 && P_IsACSSpecial(special))
			{
				w->String(FName(ENamedName(-args[i])).GetChars());
			}
			else
			{
				w->Int(args[i]);
			}
		}
		w->EndArray();
	}
	else
	{
		auto val = r->FindKey(key);
		if (val != nullptr)
		{
			if (val->IsArray())
			{
				unsigned int cnt = MIN<unsigned>(val->Size(), 5);
				for (unsigned int i = 0; i < cnt; i++)
				{
					const rapidjson::Value &aval = (*val)[i];
					if (aval.IsInt())
					{
						args[i] = aval.GetInt();
					}
					else if (i == 0 && aval.IsString())
					{
						args[i] = -FName(aval.GetString());
					}
					else
					{
						I_Error("Integer expected for '%s[%d]'", key, i);
					}
				}
			}
			else
			{
				I_Error("array expected for '%s'", key);
			}
		}
	}
	return *this;
}

//==========================================================================
//
// Special handler for script numbers
//
//==========================================================================

FSerializer &FSerializer::ScriptNum(const char *key, int &num)
{
	if (isWriting())
	{
		WriteKey(key);
		if (num < 0)
		{
			w->String(FName(ENamedName(-num)).GetChars());
		}
		else
		{
			w->Int(num);
		}
	}
	else
	{
		auto val = r->FindKey(key);
		if (val != nullptr)
		{
			if (val->IsInt())
			{
				num = val->GetInt();
			}
			else if (val->IsString())
			{
				num = -FName(val->GetString());
			}
			else
			{
				I_Error("Integer expected for '%s'", key);
			}
		}
	}
	return *this;
}

//==========================================================================
//
//
//
//==========================================================================

FSerializer &FSerializer::Terrain(const char *key, int &terrain, int *def)
{
	if (isWriting() && def != nullptr && terrain == *def)
	{
		return *this;
	}
	FName terr = P_GetTerrainName(terrain);
	Serialize(*this, key, terr, nullptr);
	if (isReading())
	{
		terrain = P_FindTerrain(terr);
	}
	return *this;
}

//==========================================================================
//
//
//
//==========================================================================

FSerializer &FSerializer::Sprite(const char *key, int32_t &spritenum, int32_t *def)
{
	if (isWriting())
	{
		if (w->inObject() && def != nullptr && *def == spritenum) return *this;
		WriteKey(key);
		w->String(sprites[spritenum].name, 4);
	}
	else
	{
		auto val = r->FindKey(key);
		if (val != nullptr)
		{
			if (val->IsString())
			{
				int name = *reinterpret_cast<const int*>(val->GetString());
				for (auto hint = NumStdSprites; hint-- != 0; )
				{
					if (sprites[hint].dwName == name)
					{
						spritenum = hint;
						break;
					}
				}
			}
		}
	}
	return *this;
}

//==========================================================================
//
//
//
//==========================================================================

FSerializer &FSerializer::StringPtr(const char *key, const char *&charptr)
{
	if (isWriting())
	{
		WriteKey(key);
		w->String(charptr);
	}
	else
	{
		auto val = r->FindKey(key);
		if (val != nullptr)
		{
			if (val->IsString())
			{
				charptr = val->GetString();
			}
			else
			{
				charptr = nullptr;
			}
		}
	}
	return *this;
}

//==========================================================================
//
//
//
//==========================================================================

FSerializer &FSerializer::AddString(const char *key, const char *charptr)
{
	if (isWriting())
	{
		WriteKey(key);
		w->String(charptr);
	}
	return *this;
}

//==========================================================================
//
//
//
//==========================================================================

unsigned FSerializer::GetSize(const char *group)
{
	if (isWriting()) return -1;	// we do not know this when writing.

	const rapidjson::Value &val = r->mDoc[group];
	if (!val.IsArray()) return -1;
	return val.Size();
}

//==========================================================================
//
//
//
//==========================================================================

const char *FSerializer::GetKey()
{
	if (isWriting()) return nullptr;	// we do not know this when writing.
	if (!r->mObjects.Last().mObject->IsObject()) return nullptr;	// non-objects do not have keys.
	auto &it = r->mObjects.Last().mIterator;
	if (it == r->mObjects.Last().mObject->MemberEnd()) return nullptr;
	return it->name.GetString();
}

//==========================================================================
//
// Writes out all collected objects
//
//==========================================================================

void FSerializer::WriteObjects()
{
	if (isWriting() && w->mDObjects.Size())
	{
		BeginArray("objects");
		// we cannot use the C++11 shorthand syntax here because the array may grow while being processed.
		for (unsigned i = 0; i < w->mDObjects.Size(); i++)
		{
			auto obj = w->mDObjects[i];
			player_t *player;

			BeginObject(nullptr);
			w->Key("classtype");
			w->String(obj->GetClass()->TypeName.GetChars());

			if (obj->IsKindOf(RUNTIME_CLASS(AActor)) &&
				(player = static_cast<AActor *>(obj)->player) &&
				player->mo == obj)
			{
				w->Key("playerindex");
				w->Int(int(player - players));
			}

			obj->SerializeUserVars(*this);
			obj->Serialize(*this);
			obj->CheckIfSerialized();
			EndObject();
		}
		EndArray();
	}
}

//==========================================================================
//
//
//
//==========================================================================

const char *FSerializer::GetOutput(unsigned *len)
{
	if (isReading()) return nullptr;
	WriteObjects();
	EndObject();
	if (len != nullptr)
	{
		*len = (unsigned)w->mOutString.GetSize();
	}
	return w->mOutString.GetString();
}

//==========================================================================
//
//
//
//==========================================================================

FCompressedBuffer FSerializer::GetCompressedOutput()
{
	if (isReading()) return{ 0,0,0,0,0,nullptr };
	FCompressedBuffer buff;
	WriteObjects();
	EndObject();
	buff.mSize = (unsigned)w->mOutString.GetSize();
	buff.mZipFlags = 0;
	buff.mCRC32 = crc32(0, (const Bytef*)w->mOutString.GetString(), buff.mSize);

	uint8_t *compressbuf = new uint8_t[buff.mSize+1];

	z_stream stream;
	int err;

	stream.next_in = (Bytef *)w->mOutString.GetString();
	stream.avail_in = buff.mSize;
	stream.next_out = (Bytef*)compressbuf;
	stream.avail_out = buff.mSize;
	stream.zalloc = (alloc_func)0;
	stream.zfree = (free_func)0;
	stream.opaque = (voidpf)0;

	// create output in zip-compatible form as required by FCompressedBuffer
	err = deflateInit2(&stream, 8, Z_DEFLATED, -15, 9, Z_DEFAULT_STRATEGY);
	if (err != Z_OK)
	{
		goto error;
	}

	err = deflate(&stream, Z_FINISH);
	if (err != Z_STREAM_END) 
	{
		deflateEnd(&stream);
		goto error;
	}
	buff.mCompressedSize = stream.total_out;

	err = deflateEnd(&stream);
	if (err == Z_OK)
	{
		buff.mBuffer = new char[buff.mCompressedSize];
		buff.mMethod = METHOD_DEFLATE;
		memcpy(buff.mBuffer, compressbuf, buff.mCompressedSize);
		delete[] compressbuf;
		return buff;
	}

error:
	memcpy(compressbuf, w->mOutString.GetString(), buff.mSize + 1);
	buff.mCompressedSize = buff.mSize;
	buff.mMethod = METHOD_STORED;
	return buff;
}

//==========================================================================
//
//
//
//==========================================================================

FSerializer &Serialize(FSerializer &arc, const char *key, bool &value, bool *defval)
{
	if (arc.isWriting())
	{
		if (!arc.w->inObject() || defval == nullptr || value != *defval)
		{
			arc.WriteKey(key);
			arc.w->Bool(value);
		}
	}
	else
	{
		auto val = arc.r->FindKey(key);
		if (val != nullptr)
		{
			if (val->IsBool())
			{
				value = val->GetBool();
			}
			else
			{
				I_Error("boolean type expected for '%s'", key);
			}
		}
	}
	return arc;
}

//==========================================================================
//
//
//
//==========================================================================

FSerializer &Serialize(FSerializer &arc, const char *key, int64_t &value, int64_t *defval)
{
	if (arc.isWriting())
	{
		if (!arc.w->inObject() || defval == nullptr || value != *defval)
		{
			arc.WriteKey(key);
			arc.w->Int64(value);
		}
	}
	else
	{
		auto val = arc.r->FindKey(key);
		if (val != nullptr)
		{
			if (val->IsInt64())
			{
				value = val->GetInt64();
			}
			else
			{
				I_Error("integer type expected for '%s'", key);
			}
		}
	}
	return arc;
}

//==========================================================================
//
//
//
//==========================================================================

FSerializer &Serialize(FSerializer &arc, const char *key, uint64_t &value, uint64_t *defval)
{
	if (arc.isWriting())
	{
		if (!arc.w->inObject() || defval == nullptr || value != *defval)
		{
			arc.WriteKey(key);
			arc.w->Uint64(value);
		}
	}
	else
	{
		auto val = arc.r->FindKey(key);
		if (val != nullptr)
		{
			if (val->IsUint64())
			{
				value = val->GetUint64();
			}
			else
			{
				I_Error("integer type expected for '%s'", key);
			}
		}
	}
	return arc;
}


//==========================================================================
//
//
//
//==========================================================================

FSerializer &Serialize(FSerializer &arc, const char *key, int32_t &value, int32_t *defval)
{
	if (arc.isWriting())
	{
		if (!arc.w->inObject() || defval == nullptr || value != *defval)
		{
			arc.WriteKey(key);
			arc.w->Int(value);
		}
	}
	else
	{
		auto val = arc.r->FindKey(key);
		if (val != nullptr)
		{
			if (val->IsInt())
			{
				value = val->GetInt();
			}
			else
			{
				I_Error("integer type expected for '%s'", key);
			}
		}
	}
	return arc;
}

//==========================================================================
//
//
//
//==========================================================================

FSerializer &Serialize(FSerializer &arc, const char *key, uint32_t &value, uint32_t *defval)
{
	if (arc.isWriting())
	{
		if (!arc.w->inObject() || defval == nullptr || value != *defval)
		{
			arc.WriteKey(key);
			arc.w->Uint(value);
		}
	}
	else
	{
		auto val = arc.r->FindKey(key);
		if (val != nullptr)
		{
			if (val->IsUint())
			{
				value = val->GetUint();
			}
			else
			{
				I_Error("integer type expected for '%s'", key);
			}
		}
	}
	return arc;
}

//==========================================================================
//
//
//
//==========================================================================

FSerializer &Serialize(FSerializer &arc, const char *key, int8_t &value, int8_t *defval)
{
	int32_t vv = value;
	int32_t vvd = defval? *defval : value-1;
	Serialize(arc, key, vv, &vvd);
	value = (int8_t)vv;
	return arc;
}

FSerializer &Serialize(FSerializer &arc, const char *key, uint8_t &value, uint8_t *defval)
{
	uint32_t vv = value;
	uint32_t vvd = defval ? *defval : value - 1;
	Serialize(arc, key, vv, &vvd);
	value = (uint8_t)vv;
	return arc;
}

FSerializer &Serialize(FSerializer &arc, const char *key, int16_t &value, int16_t *defval)
{
	int32_t vv = value;
	int32_t vvd = defval ? *defval : value - 1;
	Serialize(arc, key, vv, &vvd);
	value = (int16_t)vv;
	return arc;
}

FSerializer &Serialize(FSerializer &arc, const char *key, uint16_t &value, uint16_t *defval)
{
	uint32_t vv = value;
	uint32_t vvd = defval ? *defval : value - 1;
	Serialize(arc, key, vv, &vvd);
	value = (uint16_t)vv;
	return arc;
}

//==========================================================================
//
//
//
//==========================================================================

FSerializer &Serialize(FSerializer &arc, const char *key, double &value, double *defval)
{
	if (arc.isWriting())
	{
		if (!arc.w->inObject() || defval == nullptr || value != *defval)
		{
			arc.WriteKey(key);
			arc.w->Double(value);
		}
	}
	else
	{
		auto val = arc.r->FindKey(key);
		if (val != nullptr)
		{
			if (val->IsDouble())
			{
				value = val->GetDouble();
			}
			else
			{
				I_Error("float type expected for '%s'", key);
			}
		}
	}
	return arc;
}

//==========================================================================
//
//
//
//==========================================================================

FSerializer &Serialize(FSerializer &arc, const char *key, float &value, float *defval)
{
	double vv = value;
	double vvd = defval ? *defval : value - 1;
	Serialize(arc, key, vv, &vvd);
	value = (float)vv;
	return arc;
}

//==========================================================================
//
//
//
//==========================================================================

template<class T>
FSerializer &SerializePointer(FSerializer &arc, const char *key, T *&value, T **defval, T *base)
{
	if (arc.isReading() || !arc.w->inObject() || defval == nullptr || value != *defval)
	{
		ptrdiff_t vv = value == nullptr ? -1 : value - base;
		Serialize(arc, key, vv, nullptr);
		value = vv < 0 ? nullptr : base + vv;
	}
	return arc;
}

template<> FSerializer &Serialize(FSerializer &arc, const char *key, FPolyObj *&value, FPolyObj **defval)
{
	return SerializePointer(arc, key, value, defval, polyobjs);
}

template<> FSerializer &Serialize(FSerializer &arc, const char *key, const FPolyObj *&value, const FPolyObj **defval)
{
	return SerializePointer<const FPolyObj>(arc, key, value, defval, polyobjs);
}

template<> FSerializer &Serialize(FSerializer &arc, const char *key, side_t *&value, side_t **defval)
{
	return SerializePointer(arc, key, value, defval, sides);
}

template<> FSerializer &Serialize(FSerializer &arc, const char *key, sector_t *&value, sector_t **defval)
{
	return SerializePointer(arc, key, value, defval, sectors);
}

template<> FSerializer &Serialize(FSerializer &arc, const char *key, const sector_t *&value, const sector_t **defval)
{
	return SerializePointer<const sector_t>(arc, key, value, defval, sectors);
}

template<> FSerializer &Serialize(FSerializer &arc, const char *key, player_t *&value, player_t **defval)
{
	return SerializePointer(arc, key, value, defval, players);
}

template<> FSerializer &Serialize(FSerializer &arc, const char *key, line_t *&value, line_t **defval)
{
	return SerializePointer(arc, key, value, defval, lines);
}

template<> FSerializer &Serialize(FSerializer &arc, const char *key, vertex_t *&value, vertex_t **defval)
{
	return SerializePointer(arc, key, value, defval, vertexes);
}

//==========================================================================
//
//
//
//==========================================================================

FSerializer &Serialize(FSerializer &arc, const char *key, FTextureID &value, FTextureID *defval)
{
	if (arc.isWriting())
	{
		if (!arc.w->inObject() || defval == nullptr || value != *defval)
		{
			if (!value.Exists())
			{
				arc.WriteKey(key);
				arc.w->Null();
				return arc;
			}
			if (value.isNull())
			{
				// save 'no texture' in a more space saving way
				arc.WriteKey(key);
				arc.w->Int(0);
				return arc;
			}
			FTextureID chk = value;
			if (chk.GetIndex() >= TexMan.NumTextures()) chk.SetNull();
			FTexture *pic = TexMan[chk];
			const char *name;

			if (Wads.GetLinkedTexture(pic->SourceLump) == pic)
			{
				name = Wads.GetLumpFullName(pic->SourceLump);
			}
			else
			{
				name = pic->Name;
			}
			arc.WriteKey(key);
			arc.w->StartArray();
			arc.w->String(name);
			arc.w->Int(pic->UseType);
			arc.w->EndArray();
		}
	}
	else
	{
		auto val = arc.r->FindKey(key);
		if (val != nullptr)
		{
			if (val->IsObject())
			{
				const rapidjson::Value &nameval = (*val)[0];
				const rapidjson::Value &typeval = (*val)[1];
				if (nameval.IsString() && typeval.IsInt())
				{
					value = TexMan.GetTexture(nameval.GetString(), typeval.GetInt());
				}
				else
				{
					I_Error("object does not represent a texture for '%s'", key);
				}
			}
			else if (val->IsNull())
			{
				value.SetInvalid();
			}
			else if (val->IsInt() && val->GetInt() == 0)
			{
				value.SetNull();
			}
			else
			{
				I_Error("object does not represent a texture for '%s'", key);
			}
		}
	}
	return arc;
}

//==========================================================================
//
// This never uses defval and instead uses 'null' as default
// because object pointers cannot be safely defaulted to anything else.
//
//==========================================================================

FSerializer &Serialize(FSerializer &arc, const char *key, DObject *&value, DObject ** /*defval*/, bool *retcode)
{
	if (retcode) *retcode = true;
	if (arc.isWriting())
	{
		if (value != nullptr)
		{
			int ndx;
			if (value == WP_NOCHANGE)
			{
				ndx = -1;
			}
			else
			{
				int *pndx = arc.w->mObjectMap.CheckKey(value);
				if (pndx != nullptr)
				{
					ndx = *pndx;
				}
				else
				{
					ndx = arc.w->mDObjects.Push(value);
					arc.w->mObjectMap[value] = ndx;
				}
			}
			Serialize(arc, key, ndx, nullptr);
		}
	}
	else
	{
		auto val = arc.r->FindKey(key);
		if (val != nullptr && val->IsInt())
		{
			if (val->GetInt() == -1)
			{
				value = WP_NOCHANGE;
			}
			else
			{
			}
		}
		else if (!retcode)
		{
			value = nullptr;
		}
		else
		{
			*retcode = false;
		}
	}
	return arc;
}

//==========================================================================
//
//
//
//==========================================================================

FSerializer &Serialize(FSerializer &arc, const char *key, FName &value, FName *defval)
{
	if (arc.isWriting())
	{
		if (!arc.w->inObject() || defval == nullptr || value != *defval)
		{
			arc.WriteKey(key);
			arc.w->String(value.GetChars());
		}
	}
	else
	{
		auto val = arc.r->FindKey(key);
		if (val != nullptr)
		{
			if (val->IsString())
			{
				value = val->GetString();
			}
			else
			{
				I_Error("String expected for '%s'", key);
			}
		}
	}
	return arc;
}

//==========================================================================
//
//
//
//==========================================================================

template<> FSerializer &Serialize(FSerializer &arc, const char *key, FDynamicColormap *&cm, FDynamicColormap **def)
{
	if (arc.isWriting())
	{
		if (arc.w->inObject() && def != nullptr && cm->Color == (*def)->Color && cm->Fade == (*def)->Fade && cm->Desaturate == (*def)->Desaturate)
		{
			return arc;
		}

		arc.WriteKey(key);
		arc.w->StartArray();
		arc.w->Uint(cm->Color);
		arc.w->Uint(cm->Fade);
		arc.w->Uint(cm->Desaturate);
		arc.w->EndArray();
	}
	else
	{
		auto val = arc.r->FindKey(key);
		if (val != nullptr)
		{
			if (val->IsObject())
			{
				const rapidjson::Value &colorval = (*val)[0];
				const rapidjson::Value &fadeval = (*val)[1];
				const rapidjson::Value &desatval = (*val)[2];
				if (colorval.IsUint() && fadeval.IsUint() && desatval.IsUint())
				{
					cm = GetSpecialLights(colorval.GetUint(), fadeval.GetUint(), desatval.GetUint());
				}
				else
				{
					I_Error("object does not represent a colormap for '%s'", key);
				}
			}
			else
			{
				I_Error("object does not represent a colormap for '%s'", key);
			}
		}
	}
	return arc;
}

//==========================================================================
//
//
//
//==========================================================================

FSerializer &Serialize(FSerializer &arc, const char *key, FSoundID &sid, FSoundID *def)
{
	if (arc.isWriting())
	{
		if (!arc.w->inObject() || def == nullptr || sid != *def)
		{
			arc.WriteKey(key);
			const char *sn = (const char*)sid;
			if (sn != nullptr) arc.w->String(sn);
			else arc.w->Null();
		}
	}
	else
	{
		auto val = arc.r->FindKey(key);
		if (val != nullptr)
		{
			if (val->IsString())
			{
				sid = val->GetString();
			}
			else if (val->IsNull())
			{
				sid = 0;
			}
			else
			{
				I_Error("string type expected for '%s'", key);
			}
		}
	}
	return arc;

}

//==========================================================================
//
//
//
//==========================================================================

template<> FSerializer &Serialize(FSerializer &arc, const char *key, PClassActor *&clst, PClassActor **def)
{
	if (arc.isWriting())
	{
		if (!arc.w->inObject() || def == nullptr || clst != *def)
		{
			arc.WriteKey(key);
			if (clst == nullptr)
			{
				arc.w->Null();
			}
			else
			{
				arc.w->String(clst->TypeName.GetChars());
			}
		}
	}
	else
	{
		auto val = arc.r->FindKey(key);
		if (val != nullptr)
		{
			if (val->IsString())
			{
				clst = PClass::FindActor(val->GetString());
			}
			else if (val->IsNull())
			{
				clst = nullptr;
			}
			else
			{
				I_Error("string type expected for '%s'", key);
			}
		}
	}
	return arc;

}

//==========================================================================
//
// almost, but not quite the same as the above.
//
//==========================================================================

template<> FSerializer &Serialize(FSerializer &arc, const char *key, PClass *&clst, PClass **def)
{
	if (arc.isWriting())
	{
		if (!arc.w->inObject() || def == nullptr || clst != *def)
		{
			arc.WriteKey(key);
			if (clst == nullptr)
			{
				arc.w->Null();
			}
			else
			{
				arc.w->String(clst->TypeName.GetChars());
			}
		}
	}
	else
	{
		auto val = arc.r->FindKey(key);
		if (val != nullptr)
		{
			if (val->IsString())
			{
				clst = PClass::FindClass(val->GetString());
			}
			else if (val->IsNull())
			{
				clst = nullptr;
			}
			else
			{
				I_Error("string type expected for '%s'", key);
			}
		}
	}
	return arc;

}

//==========================================================================
//
//
//
//==========================================================================

FSerializer &Serialize(FSerializer &arc, const char *key, FState *&state, FState **def, bool *retcode)
{
	if (retcode) *retcode = false;
	if (arc.isWriting())
	{
		if (!arc.w->inObject() || def == nullptr || state != *def)
		{
			if (retcode) *retcode = true;
			arc.WriteKey(key);
			if (state == nullptr)
			{
				arc.w->Null();
			}
			else
			{
				PClassActor *info = FState::StaticFindStateOwner(state);

				if (info != NULL)
				{
					arc.w->StartArray();
					arc.w->String(info->TypeName.GetChars());
					arc.w->Uint((uint32_t)(state - info->OwnedStates));
					arc.w->EndArray();
				}
				else
				{
					arc.w->Null();
				}
			}
		}
	}
	else
	{
		auto val = arc.r->FindKey(key);
		if (val != nullptr)
		{
			if (val->IsNull())
			{
				if (retcode) *retcode = true;
				state = nullptr;
			}
			else if (val->IsArray())
			{
				if (retcode) *retcode = true;
				const rapidjson::Value &cls = (*val)[0];
				const rapidjson::Value &ndx = (*val)[1];

				state = nullptr;
				if (cls.IsString() && ndx.IsInt())
				{
					PClassActor *clas = PClass::FindActor(cls.GetString());
					if (clas)
					{
						state = clas->OwnedStates + ndx.GetInt();
					}
				}
			}
			else if (!retcode)
			{
				I_Error("array type expected for '%s'", key);
			}
		}
	}
	return arc;

}

//==========================================================================
//
//
//
//==========================================================================

template<> FSerializer &Serialize(FSerializer &arc, const char *key, FStrifeDialogueNode *&node, FStrifeDialogueNode **def)
{
	if (arc.isWriting())
	{
		if (!arc.w->inObject() || def == nullptr || node != *def)
		{
			arc.WriteKey(key);
			if (node == nullptr)
			{
				arc.w->Null();
			}
			else
			{
				arc.w->Uint(node->ThisNodeNum);
			}
		}
	}
	else
	{
		auto val = arc.r->FindKey(key);
		if (val != nullptr)
		{
			if (val->IsNull())
			{
				node = nullptr;
			}
			else if (val->IsUint())
			{
				if (val->GetUint() >= StrifeDialogues.Size())
				{
					node = NULL;
				}
				else
				{
					node = StrifeDialogues[val->GetUint()];
				}
			}
			else
			{
				I_Error("integer expected for '%s'", key);
			}
		}
	}
	return arc;

}

//==========================================================================
//
//
//
//==========================================================================

template<> FSerializer &Serialize(FSerializer &arc, const char *key, FString *&pstr, FString **def)
{
	if (arc.isWriting())
	{
		if (!arc.w->inObject() || def == nullptr || pstr != *def)
		{
			arc.WriteKey(key);
			if (pstr == nullptr)
			{
				arc.w->Null();
			}
			else
			{
				arc.w->String(pstr->GetChars());
			}
		}
	}
	else
	{
		auto val = arc.r->FindKey(key);
		if (val != nullptr)
		{
			if (val->IsNull())
			{
				pstr = nullptr;
			}
			else if (val->IsString())
			{
				pstr = AActor::mStringPropertyData.Alloc(val->GetString());
			}
			else
			{
				I_Error("string expected for '%s'", key);
			}
		}
	}
	return arc;

}

//==========================================================================
//
//
//
//==========================================================================

FSerializer &Serialize(FSerializer &arc, const char *key, FString &pstr, FString *def)
{
	if (arc.isWriting())
	{
		if (!arc.w->inObject() || def == nullptr || pstr.Compare(*def) != 0)
		{
			arc.WriteKey(key);
			arc.w->String(pstr.GetChars());
		}
	}
	else
	{
		auto val = arc.r->FindKey(key);
		if (val != nullptr)
		{
			if (val->IsNull())
			{
				pstr = "";
			}
			else if (val->IsString())
			{
				pstr = val->GetString();
			}
			else
			{
				I_Error("string expected for '%s'", key);
			}
		}
	}
	return arc;

}

//==========================================================================
//
//
//
//==========================================================================

template<> FSerializer &Serialize(FSerializer &arc, const char *key, char *&pstr, char **def)
{
	if (arc.isWriting())
	{
		if (!arc.w->inObject() || def == nullptr || strcmp(pstr, *def))
		{
			arc.WriteKey(key);
			if (pstr == nullptr)
			{
				arc.w->Null();
			}
			else
			{
				arc.w->String(pstr);
			}
		}
	}
	else
	{
		auto val = arc.r->FindKey(key);
		if (val != nullptr)
		{
			if (val->IsNull())
			{
				pstr = nullptr;
			}
			else if (val->IsString())
			{
				pstr = copystring(val->GetString());
			}
			else
			{
				I_Error("string expected for '%s'", key);
			}
		}
	}
	return arc;
}

//==========================================================================
//
//
//
//==========================================================================

template<> FSerializer &Serialize(FSerializer &arc, const char *key, FFont *&font, FFont **def)
{
	if (arc.isWriting())
	{
		const char *n = font->GetName();
		return arc.StringPtr(key, n);
	}
	else
	{
		const char *n;
		arc.StringPtr(key, n);
		font = V_GetFont(n);
		if (font == nullptr)
		{
			Printf("Could not load font %s\n", n);
			font = SmallFont;
		}
		return arc;
	}

}

//==========================================================================
//
// Handler to retrieve a numeric value of any kind.
//
//==========================================================================

FSerializer &Serialize(FSerializer &arc, const char *key, NumericValue &value, NumericValue *defval)
{
	if (arc.isWriting())
	{
		if (!arc.w->inObject() || defval == nullptr || value != *defval)
		{
			arc.WriteKey(key);
			switch (value.type)
			{
			case NumericValue::NM_signed:
				arc.w->Int64(value.signedval);
				break;
			case NumericValue::NM_unsigned:
				arc.w->Uint64(value.unsignedval);
				break;
			case NumericValue::NM_float:
				arc.w->Double(value.floatval);
				break;
			default:
				arc.w->Null();
				break;
			}
		}
	}
	else
	{
		auto val = arc.r->FindKey(key);
		value.signedval = 0;
		value.type = NumericValue::NM_invalid;
		if (val != nullptr)
		{
			if (val->IsUint64())
			{
				value.unsignedval = val->GetUint64();
				value.type = NumericValue::NM_unsigned;
			}
			else if (val->IsInt64())
			{
				value.signedval = val->GetInt64();
				value.type = NumericValue::NM_signed;
			}
			else if (val->IsDouble())
			{
				value.floatval = val->GetDouble();
				value.type = NumericValue::NM_float;
			}
		}
	}
	return arc;
}
