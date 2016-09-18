#define RAPIDJSON_48BITPOINTER_OPTIMIZATION 0	// disable this insanity which is bound to make the code break over time.
#define RAPIDJSON_HAS_CXX11_RVALUE_REFS 1
#define RAPIDJSON_HAS_CXX11_RANGE_FOR 1

//#define PRETTY

#include "rapidjson/rapidjson.h"
#include "rapidjson/writer.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/document.h"
#include "r_defs.h"
#include "r_local.h"
#include "p_lnspec.h"
#include "i_system.h"
#include "w_wad.h"
#include "p_terrain.h"
#include "c_dispatch.h"
#include "p_setup.h"
#include "p_conversation.h"
#include "dsectoreffect.h"
#include "actor.h"
#include "r_data/r_interpolate.h"
#include "g_shared/a_sharedglobal.h"

TArray<sector_t>	loadsectors;
TArray<line_t>	loadlines;
TArray<side_t>	loadsides;

//==========================================================================
//
//
//
//==========================================================================
FSerializer &Serialize(FSerializer &arc, const char *key, FName &value);


//==========================================================================
//
//
//
//==========================================================================
typedef rapidjson::Value FJSONValue;

struct FJSONObject
{
	rapidjson::Value *mObject;
	rapidjson::Value::MemberIterator mIterator;

	FJSONObject(rapidjson::Value *v)
	{
		mObject = v;
		mIterator = v->MemberEnd();
	}
};



class FSerializer
{
#ifndef PRETTY
	typedef rapidjson::Writer<rapidjson::StringBuffer, rapidjson::ASCII<> > Writer;
#else
	typedef rapidjson::PrettyWriter<rapidjson::StringBuffer, rapidjson::ASCII<> > Writer;
#endif

public:
	Writer *mWriter = nullptr;
	TArray<bool> mInObject;
	rapidjson::StringBuffer mOutString;
	TArray<DObject *> mDObjects;
	TMap<DObject *, int> mObjectMap;
	TArray<FJSONObject> mObjects;
	rapidjson::Value mDocObj;	// just because RapidJSON is stupid and does not allow direct access to what's in the document.

	int ArraySize()
	{
		if (mObjects.Last().mObject->IsArray())
		{
			return mObjects.Last().mObject->Size();
		}
		else
		{
			return 0;
		}
	}

	rapidjson::Value *FindKey(const char *key)
	{
		FJSONObject &obj = mObjects.Last();
		if (obj.mObject->IsObject())
		{
			// This will continue the search from the last found key, assuming
			// that key are being read in the same order in which they were written.
			// As long as this is the case this will reduce time here significantly.
			// Do at most as many iterations as the object has members. Otherwise the key cannot be present.
			for (rapidjson::SizeType i = 0; i < obj.mObject->MemberCount(); i++)
			{
				if (obj.mIterator == obj.mObject->MemberEnd()) obj.mIterator = obj.mObject->MemberBegin();
				else obj.mIterator++;
				if (!strcmp(key, obj.mIterator->name.GetString()))
				{
					return &obj.mIterator->value;
				}
			}
		}
		else if (obj.mObject->IsArray())
		{
			// todo: Get element at current index and increment.
		}
		return nullptr;
	}

public:

	bool OpenWriter(bool pretty)
	{
		if (mWriter != nullptr || mObjects.Size() > 0)
		{
			return false;
		}
		mWriter = new Writer(mOutString);
		return true;
	}

	bool OpenReader(rapidjson::Document *doc)
	{
		if (mWriter != nullptr || mObjects.Size() > 0 || !doc->IsObject())
		{
			return false;
		}
		mDocObj = doc->GetObject();
		mObjects.Push(FJSONObject(&mDocObj));
		return true;
	}

	bool isReading() const
	{
		return mWriter == nullptr;
	}

	bool isWriting() const
	{
		return mWriter != nullptr;
	}

	void WriteKey(const char *key)
	{
		if (mInObject.Size() > 0 && mInObject.Last())
		{
			mWriter->Key(key);
		}
	}

	FSerializer &BeginObject(const char *name)
	{
		if (isWriting())
		{
			WriteKey(name);
			mWriter->StartObject();
			mInObject.Push(true);
		}
		else
		{
			auto val = FindKey(name);
			if (val != nullptr)
			{
				if (val->IsObject())
				{
					mObjects.Push(FJSONObject(val));
				}
				else
				{
					I_Error("Object expected for '%s'", name);
				}
			}
			else
			{
				I_Error("'%s' not found", name);
			}
		}
		return *this;
	}

	FSerializer &EndObject()
	{
		if (isWriting())
		{
			mWriter->EndObject();
			mInObject.Pop();
		}
		else
		{
			mObjects.Pop();
		}
		return *this;
	}

	bool BeginArray(const char *name)
	{
		if (isWriting())
		{
			WriteKey(name);
			mWriter->StartArray();
			mInObject.Push(false);
		}
		else
		{
			auto val = FindKey(name);
			if (val != nullptr)
			{
				if (val->IsArray())
				{
					mObjects.Push(FJSONObject(val));
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

	FSerializer &EndArray()
	{
		if (isWriting())
		{
			mWriter->EndArray();
			mInObject.Pop();
		}
		else
		{
			mObjects.Pop();
		}
		return *this;
	}


	//==========================================================================
	//
	// Special handler for args (because ACS specials' arg0 needs special treatment.)
	//
	//==========================================================================

	FSerializer &Args(const char *key, int *args, int special)
	{
		if (isWriting())
		{
			WriteKey(key);
			mWriter->StartArray();
			for (int i = 0; i < 5; i++)
			{
				if (i == 0 && args[i] < 0 && P_IsACSSpecial(special))
				{
					mWriter->String(FName(ENamedName(-args[i])).GetChars());
				}
				else
				{
					mWriter->Int(args[i]);
				}
			}
			mWriter->EndArray();
		}
		else
		{
			auto val = FindKey(key);
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
			}
			else
			{
				I_Error("array expected for '%s'", key);
			}
		}
		return *this;
	}

	template<class T>
	FSerializer &operator()(const char *key, T &obj)
	{
		return Serialize(*this, key, obj);
	}

	template<class T>
	FSerializer &operator()(const char *key, T &obj, T & def)
	{
		if (isWriting() && !memcmp(&obj, &def, sizeof(T))) return *this;
		return Serialize(*this, key, obj);
	}

	template<class T>
	FSerializer &Array(const char *key, T *obj, int count)
	{
		if (BeginArray(key))
		{
			for (int i = 0; i < count; i++)
			{
				Serialize(*this, nullptr, obj[i]);
			}
			EndArray();
		}
		return *this;
	}

	FSerializer &Terrain(const char *key, int &terrain)
	{
		FName terr = P_GetTerrainName(terrain);
		Serialize(*this, key, terr);
		if (isReading())
		{
			terrain = P_FindTerrain(terr);
		}
		return *this;
	}

	FSerializer &Sprite(const char *key, uint16_t &spritenum)
	{
		if (isWriting())
		{
			WriteKey(key);
			mWriter->String(sprites[spritenum].name, 4);
		}
		return *this;
	}
};

//==========================================================================
//
//
//
//==========================================================================

FSerializer &Serialize(FSerializer &arc, const char *key, bool &value)
{
	if (arc.isWriting())
	{
		arc.WriteKey(key);
		arc.mWriter->Bool(value);
	}
	else
	{
		auto val = arc.FindKey(key);
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

FSerializer &Serialize(FSerializer &arc, const char *key, int64_t &value)
{
	if (arc.isWriting())
	{
		arc.WriteKey(key);
		arc.mWriter->Int64(value);
	}
	else
	{
		auto val = arc.FindKey(key);
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

FSerializer &Serialize(FSerializer &arc, const char *key, uint64_t &value)
{
	if (arc.isWriting())
	{
		arc.WriteKey(key);
		arc.mWriter->Uint64(value);
	}
	else
	{
		auto val = arc.FindKey(key);
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

FSerializer &Serialize(FSerializer &arc, const char *key, int32_t &value)
{
	if (arc.isWriting())
	{
		arc.WriteKey(key);
		arc.mWriter->Int(value);
	}
	else
	{
		auto val = arc.FindKey(key);
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

FSerializer &Serialize(FSerializer &arc, const char *key, uint32_t &value)
{
	if (arc.isWriting())
	{
		arc.WriteKey(key);
		arc.mWriter->Uint(value);
	}
	else
	{
		auto val = arc.FindKey(key);
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

FSerializer &Serialize(FSerializer &arc, const char *key, int8_t &value)
{
	int32_t vv = value;
	Serialize(arc, key, vv);
	value = (int8_t)vv;
	return arc;
}

FSerializer &Serialize(FSerializer &arc, const char *key, uint8_t &value)
{
	uint32_t vv = value;
	Serialize(arc, key, vv);
	value = (uint8_t)vv;
	return arc;
}

FSerializer &Serialize(FSerializer &arc, const char *key, int16_t &value)
{
	int32_t vv = value;
	Serialize(arc, key, vv);
	value = (int16_t)vv;
	return arc;
}

FSerializer &Serialize(FSerializer &arc, const char *key, uint16_t &value)
{
	uint32_t vv = value;
	Serialize(arc, key, vv);
	value = (uint16_t)vv;
	return arc;
}

//==========================================================================
//
//
//
//==========================================================================

FSerializer &Serialize(FSerializer &arc, const char *key, double &value)
{
	if (arc.isWriting())
	{
		arc.WriteKey(key);
		arc.mWriter->Double(value);
	}
	else
	{
		auto val = arc.FindKey(key);
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

FSerializer &Serialize(FSerializer &arc, const char *key, float &value)
{
	double vv = value;
	Serialize(arc, key, vv);
	value = (float)vv;
	return arc;
}

//==========================================================================
//
//
//
//==========================================================================

FSerializer &Serialize(FSerializer &arc, const char *key, side_t *&value)
{
	ptrdiff_t vv = value == nullptr? -1 : value - sides;
	Serialize(arc, key, vv);
	value = vv < 0 ? nullptr : sides + vv;
	return arc;
}

//==========================================================================
//
//
//
//==========================================================================

FSerializer &Serialize(FSerializer &arc, const char *key, sector_t *&value)
{
	ptrdiff_t vv = value == nullptr ? -1 : value - sectors;
	Serialize(arc, key, vv);
	value = vv < 0 ? nullptr : sectors + vv;
	return arc;
}

//==========================================================================
//
//
//
//==========================================================================

FSerializer &Serialize(FSerializer &arc, const char *key, player_t *&value)
{
	ptrdiff_t vv = value == nullptr ? -1 : value - players;
	Serialize(arc, key, vv);
	value = vv < 0 ? nullptr : players + vv;
	return arc;
}

//==========================================================================
//
//
//
//==========================================================================

FSerializer &Serialize(FSerializer &arc, const char *key, line_t *&value)
{
	ptrdiff_t vv = value == nullptr ? -1 : value - lines;
	Serialize(arc, key, vv);
	value = vv < 0 ? nullptr : lines + vv;
	return arc;
}

//==========================================================================
//
//
//
//==========================================================================

FSerializer &Serialize(FSerializer &arc, const char *key, FTextureID &value)
{
	if (arc.isWriting())
	{
		if (!value.Exists())
		{
			arc.WriteKey(key);
			arc.mWriter->Null();
			return arc;
		}
		if (value.isNull())
		{
			// save 'no texture' in a more space saving way
			arc.WriteKey(key);
			arc.mWriter->Int(0);
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
		arc.mWriter->StartObject();
		arc.mWriter->Key("name");
		arc.mWriter->String(name);
		arc.mWriter->Key("usetype");
		arc.mWriter->Int(pic->UseType);
		arc.mWriter->EndObject();
	}
	else
	{
		auto val = arc.FindKey(key);
		if (val != nullptr)
		{
			if (val->IsObject())
			{
				const rapidjson::Value &nameval = (*val)["name"];
				const rapidjson::Value &typeval = (*val)["type"];
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
//
//
//==========================================================================

FSerializer &Serialize(FSerializer &arc, const char *key, DObject *&value)
{
	if (arc.isWriting())
	{
		if (value != nullptr)
		{
			int ndx;
			int *pndx = arc.mObjectMap.CheckKey(value);
			if (pndx != nullptr) ndx = *pndx;
			else
			{
				ndx = arc.mDObjects.Push(value);
				arc.mObjectMap[value] = ndx;
			}
			Serialize(arc, key, ndx);
		}
	}
	else
	{
		auto val = arc.FindKey(key);
		if (val != nullptr)
		{
		}
		else
		{
			value = nullptr;
		}
	}
	return arc;
}

template<class T>
FSerializer &Serialize(FSerializer &arc, const char *key, T *&value)
{
	DObject *v = static_cast<DObject*>(value);
	Serialize(arc, key, v);
	value = static_cast<T*>(v);
	return arc;
}

template<class T>
FSerializer &Serialize(FSerializer &arc, const char *key, TObjPtr<T> &value)
{
	DObject *v = static_cast<DObject*>(value);
	Serialize(arc, key, v);
	value = static_cast<T*>(v);
	return arc; 
}

template<class T, class TT>
FSerializer &Serialize(FSerializer &arc, const char *key, TArray<T, TT> &value)
{
	if (arc.isWriting())
	{
		if (value.Size() == 0) return arc;	// do not save empty arrays
	}
	bool res = arc.BeginArray(key);
	if (arc.isReading())
	{
		if (!res)
		{
			value.Clear();
			return arc;
		}
		value.Resize(arc.ArraySize());
	}
	for (unsigned i = 0; i < value.Size(); i++)
	{
		Serialize(arc, nullptr, value[i]);
	}
	return arc.EndArray();
}

FSerializer &Serialize(FSerializer &arc, const char *key, DVector3 &p)
{
	return arc.Array(key, &p[0], 3);
}

FSerializer &Serialize(FSerializer &arc, const char *key, DRotator &p)
{
	return arc.Array(key, &p[0], 3);
}

FSerializer &Serialize(FSerializer &arc, const char *key, DVector2 &p)
{
	return arc.Array(key, &p[0], 2);
}

FSerializer &Serialize(FSerializer &arc, const char *key, DAngle &p)
{
	return Serialize(arc, key, p.Degrees);
}

FSerializer &Serialize(FSerializer &arc, const char *key, FName &value)
{
	if (arc.isWriting())
	{
		arc.WriteKey(key);
		arc.mWriter->String(value.GetChars());
	}
	else
	{
		auto val = arc.FindKey(key);
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

FSerializer &Serialize(FSerializer &arc, const char *key, line_t &line)
{
	return arc.BeginObject(key)
		("flags", line.flags)
		("activation", line.activation)
		("special", line.special)
		("alpha", line.alpha)
		.Args("args", line.args, line.special)
		("portalindex", line.portalindex)
		//.Array("sides", line.sidedef, 2)
		.EndObject();

	// no need to store the sidedef references. Unless the map loader is changed they will not change between map loads.
}

//==========================================================================
//
//
//
//==========================================================================

FSerializer &Serialize(FSerializer &arc, const char *key, side_t::part &part)
{
	return arc.BeginObject(key)
		("xoffset", part.xOffset)
		("yoffset", part.yOffset)
		("xscale", part.xScale)
		("yscale", part.yScale)
		("texture", part.texture)
		("interpolation", part.interpolation)
		.EndObject();
}

//==========================================================================
//
//
//
//==========================================================================

FSerializer &Serialize(FSerializer &arc, const char *key, side_t &side)
{
	return arc.BeginObject(key)
		.Array("textures", side.textures, 3)
		("light", side.Light)
		("flags", side.Flags)
		//("leftside", side.LeftSide)
		//("rightside", side.RightSide)
		//("index", side.Index)
		("attacheddecals", side.AttachedDecals)
		.EndObject();
}

FSerializer &Serialize(FSerializer &arc, const char *key, FLinkedSector &ls)
{
	return  arc.BeginObject(key)
		("sector", ls.Sector)
		("type", ls.Type)
		.EndObject();
}

FSerializer &Serialize(FSerializer &arc, const char *key, sector_t::splane &p)
{
	return  arc.BeginObject(key)
		("xoffs", p.xform.xOffs)
		("yoffs", p.xform.yOffs)
		("xscale", p.xform.xScale)
		("yscale", p.xform.yScale)
		("angle", p.xform.Angle)
		("baseyoffs", p.xform.baseyOffs)
		("baseangle", p.xform.baseAngle)
		("flags", p.Flags)
		("light", p.Light)
		("texture", p.Texture)
		("texz", p.TexZ)
		("alpha", p.alpha)
		.EndObject();
}

FSerializer &Serialize(FSerializer &arc, const char *key, secplane_t &p)
{
	arc.BeginObject(key)
		("normal", p.normal)
		("d", p.D)
		.EndObject();

	if (arc.isReading() && p.normal.Z != 0)
	{
		p.negiC = 1 / p.normal.Z;
	}
	return arc;
}

FSerializer &Serialize(FSerializer &arc, const char *key, PalEntry &pe)
{
	return Serialize(arc, key, pe.d);
}

FSerializer &Serialize(FSerializer &arc, const char *key, FDynamicColormap *&cm)
{
	if (arc.isWriting())
	{
		arc.WriteKey(key);
		arc.mWriter->StartArray();
		arc.mWriter->Uint(cm->Color);
		arc.mWriter->Uint(cm->Fade);
		arc.mWriter->Uint(cm->Desaturate);
		arc.mWriter->EndArray();
	}
	else
	{
		auto val = arc.FindKey(key);
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


FSerializer &Serialize(FSerializer &arc, const char *key, sector_t &p)
{
	return arc.BeginObject(key)
		("floorplane", p.floorplane)
		("ceilingplane", p.ceilingplane)
		("lightlevel", p.lightlevel)
		("special", p.special)
		("soundtraversed", p.soundtraversed)
		("seqtype", p.seqType)
		("seqname", p.SeqName)
		("friction", p.friction)
		("movefactor", p.movefactor)
		("stairlock", p.stairlock)
		("prevsec", p.prevsec)
		("nextsec", p.nextsec)
		.Array("planes", p.planes, 2)
		//("heightsec", p.heightsec)
		//("bottommap", p.bottommap)
		//("midmap", p.midmap)
		//("topmap", p.topmap)
		("damageamount", p.damageamount)
		("damageinterval", p.damageinterval)
		("leakydamage", p.leakydamage)
		("damagetype", p.damagetype)
		("sky", p.sky)
		("moreflags", p.MoreFlags)
		("flags", p.Flags)
		.Array("portals", p.Portals, 2)
		("zonenumber", p.ZoneNumber)
		.Array("interpolations", p.interpolations, 4)
		("soundtarget", p.SoundTarget)
		("secacttarget", p.SecActTarget)
		("floordata", p.floordata)
		("ceilingdata", p.ceilingdata)
		("lightingdata", p.lightingdata)
		("fakefloor_sectors", p.e->FakeFloor.Sectors)
		("midtexf_lines", p.e->Midtex.Floor.AttachedLines)
		("midtexf_sectors", p.e->Midtex.Floor.AttachedSectors)
		("midtexc_lines", p.e->Midtex.Ceiling.AttachedLines)
		("midtexc_sectors", p.e->Midtex.Ceiling.AttachedSectors)
		("linked_floor", p.e->Linked.Floor.Sectors)
		("linked_ceiling", p.e->Linked.Ceiling.Sectors)
		("colormap", p.ColorMap)
		.Terrain("floorterrain", p.terrainnum[0])
		.Terrain("ceilingterrain", p.terrainnum[1])
		.EndObject();
}


FSerializer &Serialize(FSerializer &arc, const char *key, subsector_t *&ss)
{
	BYTE by;

	if (arc.isWriting())
	{
		if (hasglnodes)
		{
			TArray<char> encoded((numsubsectors + 5) / 6);
			int p = 0;
			for (int i = 0; i < numsubsectors; i += 6)
			{
				by = 0;
				for (int j = 0; j < 6; j++)
				{
					if (i + j < numsubsectors && (subsectors[i + j].flags & SSECF_DRAWN))
					{
						by |= (1 << j);
					}
				}
				if (by < 10) by += '0';
				else if (by < 36) by += 'A' - 10;
				else if (by < 62) by += 'a' - 36;
				else if (by == 62) by = '-';
				else if (by == 63) by = '+';
				encoded[p++] = by;
			}
			arc.mWriter->Key(key);
			arc.mWriter->StartArray();
			arc.mWriter->Int(numvertexes);
			arc.mWriter->Int(numsubsectors);
			arc.mWriter->Int(numnodes);
			arc.mWriter->String(&encoded[0], (numsubsectors + 5) / 6);
			arc.mWriter->EndArray();
		}
	}
	else
	{
		int num_verts, num_subs, num_nodes;
	}
	return arc;
}

FSerializer &Serialize(FSerializer &arc, const char *key, ReverbContainer *&c)
{
	int id = (arc.isReading() || c == nullptr) ? 0 : c->ID;
	Serialize(arc, key, id);
	if (arc.isReading())
	{
		c = S_FindEnvironment(id);
	}
	return arc;
}

FSerializer &Serialize(FSerializer &arc, const char *key, zone_t &z)
{
	return Serialize(arc, key, z.Environment);
}

//============================================================================
//
// Save a line portal for savegames.
//
//============================================================================

FSerializer &Serialize(FSerializer &arc, const char *key, FLinePortal &port)
{
	return arc.BeginObject(key)
		("origin", port.mOrigin)
		("destination", port.mDestination)
		("displacement", port.mDisplacement)
		("type", port.mType)
		("flags", port.mFlags)
		("defflags", port.mDefFlags)
		("align", port.mAlign)
		.EndObject();
}

//============================================================================
//
// Save a sector portal for savegames.
//
//============================================================================

FSerializer &Serialize(FSerializer &arc, const char *key, FSectorPortal &port)
{
	return arc.BeginObject(key)
		("type", port.mType)
		("flags", port.mFlags)
		("partner", port.mPartner)
		("plane", port.mPlane)
		("origin", port.mOrigin)
		("destination", port.mDestination)
		("displacement", port.mDisplacement)
		("planez", port.mPlaneZ)
		("skybox", port.mSkybox)
		.EndObject();
}


void DThinker::SaveList(FSerializer &arc, DThinker *node)
{
	if (node != NULL)
	{
		while (!(node->ObjectFlags & OF_Sentinel))
		{
			assert(node->NextThinker != NULL && !(node->NextThinker->ObjectFlags & OF_EuthanizeMe));
			::Serialize(arc, nullptr, node);
			node = node->NextThinker;
		}
	}
}

void DThinker::SerializeThinkers(FSerializer &arc, bool hubLoad)
{
	DThinker *thinker;
	BYTE stat;
	int statcount;
	int i;

	if (arc.isWriting())
	{
		arc.BeginArray("thinkers");
		for (i = 0; i <= MAX_STATNUM; i++)
		{
			arc.BeginArray(nullptr);
			SaveList(arc, Thinkers[i].GetHead());
			SaveList(arc, FreshThinkers[i].GetHead());
			arc.EndArray();
		}
		arc.EndArray();
	}
	else
	{
	}
}


FSerializer &Serialize(FSerializer &arc, const char *key, FRenderStyle &style)
{
	return arc.BeginObject(key)
		("blendop", style.BlendOp)
		("srcalpha", style.SrcAlpha)
		("dstalpha", style.DestAlpha)
		("flags", style.Flags)
		.EndObject();
}

template<class T, class TT>
FSerializer &Serialize(FSerializer &arc, const char *key, TFlags<T, TT> &flags)
{
	return Serialize(arc, key, flags.Value);
}

FSerializer &Serialize(FSerializer &arc, const char *key, FSoundID &sid)
{
	if (arc.isWriting())
	{
		arc.WriteKey(key);
		const char *sn = (const char*)sid;
		if (sn != nullptr) arc.mWriter->String(sn);
		else arc.mWriter->Null();
	}
	else
	{
		auto val = arc.FindKey(key);
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

FSerializer &Serialize(FSerializer &arc, const char *key, FState *&state)
{
	if (arc.isWriting())
	{
		arc.WriteKey(key);
		if (state == nullptr)
		{
			arc.mWriter->Null();
		}
		else
		{
			PClassActor *info = FState::StaticFindStateOwner(state);

			if (info != NULL)
			{
				arc.mWriter->StartArray();
				arc.mWriter->String(info->TypeName.GetChars());
				arc.mWriter->Uint((uint32_t)(state - info->OwnedStates));
				arc.mWriter->EndArray();
			}
			else
			{
				arc.mWriter->Null();
			}
		}
	}
	else
	{
		auto val = arc.FindKey(key);
		if (val != nullptr)
		{
			if (val->IsNull())
			{
				state = nullptr;
			}
			else if (val->IsArray())
			{
				//rapidjson::Value cls = (*val)[0];
				//rapidjson::Value ndx = (*val)[1];
			}
			else
			{
				I_Error("array type expected for '%s'", key);
			}
		}
	}
	return arc;

}

FSerializer &Serialize(FSerializer &arc, const char *key, FStrifeDialogueNode *&node)
{
	uint32_t convnum;
	if (arc.isWriting())
	{
		arc.WriteKey(key);
		if (node == nullptr)
		{
			arc.mWriter->Null();
		}
		else
		{
			arc.mWriter->Uint(node->ThisNodeNum);
		}
	}
	else
	{
		auto val = arc.FindKey(key);
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

FSerializer &Serialize(FSerializer &arc, const char *key, FString *&pstr)
{
	uint32_t convnum;
	if (arc.isWriting())
	{
		arc.WriteKey(key);
		if (pstr == nullptr)
		{
			arc.mWriter->Null();
		}
		else
		{
			arc.mWriter->String(pstr->GetChars());
		}
	}
	else
	{
		auto val = arc.FindKey(key);
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


/*
{
FString tagstr;
if (arc.IsStoring() && Tag != NULL && Tag->Len() > 0) tagstr = *Tag;
arc << tagstr;
if (arc.IsLoading())
{
if (tagstr.Len() == 0) Tag = NULL;
else Tag = mStringPropertyData.Alloc(tagstr);
}
}
*/


void SerializeWorld(FSerializer &arc)
{
	arc.Array("linedefs", lines, numlines)
		.Array("sidedefs", sides, numsides)
		.Array("sectors", sectors, numsectors)
		("subsectors", subsectors)
		("zones", Zones)
		("lineportals", linePortals)
		("sectorportals", sectorPortals);
}

void DObject::SerializeUserVars(FSerializer &arc)
{
	PSymbolTable *symt;
	FName varname;
	DWORD count, j;
	int *varloc = NULL;

	symt = &GetClass()->Symbols;

	if (arc.isWriting())
	{
		// Write all fields that aren't serialized by native code.
		//GetClass()->WriteValue(arc, this);
	}
	else 
	{
		//GetClass()->ReadValue(arc, this);
	}
}

void DObject::Serialize(FSerializer &arc)
{
	ObjectFlags |= OF_SerialSuccess;
}

void SerializeObjects(FSerializer &arc)
{
	if (arc.isWriting())
	{
		arc.BeginArray("objects");
		for (unsigned i = 0; i < arc.mDObjects.Size(); i++)
		{
			arc.BeginObject(nullptr);
			arc.WriteKey("classtype");
			arc.mWriter->String(arc.mDObjects[i]->GetClass()->TypeName.GetChars());
			arc.mDObjects[i]->Serialize(arc);
			arc.EndObject();
		}
		arc.EndArray();
	}
}

//==========================================================================
//
// AActor :: Serialize
//
//==========================================================================

#define A(a,b) ((a), (b), def->b)

void AActor::Serialize(FSerializer &arc)
{
	int damage = 0;	// just a placeholder until the insanity surrounding the damage property can be fixed
	AActor *def = GetDefault();

	Super::Serialize(arc);

	arc
		.Sprite("sprite", sprite)
		A("pos", __Pos)
		A("angles", Angles)
		A("frame", frame)
		A("scale", Scale)
		A("renderstyle", RenderStyle)
		A("renderflags", renderflags)
		A("picnum", picnum)
		A("floorpic", floorpic)
		A("ceilingpic", ceilingpic)
		A("tidtohate", TIDtoHate)
		A("lastlookpn", LastLookPlayerNumber)
		("lastlookactor", LastLookActor)
		A("effects", effects)
		A("alpha", Alpha)
		A("fillcolor", fillcolor)
		A("sector", Sector)
		A("floorz", floorz)
		A("ceilingz", ceilingz)
		A("dropoffz", dropoffz)
		A("floorsector", floorsector)
		A("ceilingsector", ceilingsector)
		A("radius", radius)
		A("height", Height)
		A("ppassheight", projectilepassheight)
		A("vel", Vel)
		A("tics", tics)
		("state", state)
		("damage", damage)
		.Terrain("floorterrain", floorterrain)
		A("projectilekickback", projectileKickback)
		A("flags", flags)
		A("flags2", flags2)
		A("flags3", flags3)
		A("flags4", flags4)
		A("flags5", flags5)
		A("flags6", flags6)
		A("flags7", flags7)
		A("weaponspecial", weaponspecial)
		A("special1", special1)
		A("special2", special2)
		A("specialf1", specialf1)
		A("specialf2", specialf2)
		A("health", health)
		A("movedir", movedir)
		A("visdir", visdir)
		A("movecount", movecount)
		A("strafecount", strafecount)
		("target", target)
		("lastenemy", lastenemy)
		("lastheard", LastHeard)
		A("reactiontime", reactiontime)
		A("threshold", threshold)
		A("player", player)
		A("spawnpoint", SpawnPoint)
		A("spawnangle", SpawnAngle)
		A("starthealth", StartHealth)
		A("skillrespawncount", skillrespawncount)
		("tracer", tracer)
		A("floorclip", Floorclip)
		A("tid", tid)
		A("special", special)
		.Args("args", args, special)
		A("accuracy", accuracy)
		A("stamina", stamina)
		("goal", goal)
		A("waterlevel", waterlevel)
		A("minmissilechance", MinMissileChance)
		A("spawnflags", SpawnFlags)
		("inventory", Inventory)
		A("inventoryid", InventoryID)
		A("floatbobphase", FloatBobPhase)
		A("translation", Translation)
		A("seesound", SeeSound)
		A("attacksound", AttackSound)
		A("paimsound", PainSound)
		A("deathsound", DeathSound)
		A("activesound", ActiveSound)
		A("usesound", UseSound)
		A("bouncesound", BounceSound)
		A("wallbouncesound", WallBounceSound)
		A("crushpainsound", CrushPainSound)
		A("speed", Speed)
		A("floatspeed", FloatSpeed)
		A("mass", Mass)
		A("painchance", PainChance)
		A("spawnstate", SpawnState)
		A("seestate", SeeState)
		A("meleestate", MeleeState)
		A("missilestate", MissileState)
		A("maxdropoffheight", MaxDropOffHeight)
		A("maxstepheight", MaxStepHeight)
		A("bounceflags", BounceFlags)
		A("bouncefactor", bouncefactor)
		A("wallbouncefactor", wallbouncefactor)
		A("bouncecount", bouncecount)
		A("maxtargetrange", maxtargetrange)
		A("meleethreshold", meleethreshold)
		A("meleerange", meleerange)
		A("damagetype", DamageType)
		A("damagetypereceived", DamageTypeReceived)
		A("paintype", PainType)
		A("deathtype", DeathType)
		A("gravity", Gravity)
		A("fastchasestrafecount", FastChaseStrafeCount)
		("master", master)
		A("smokecounter", smokecounter)
		("blockingmobj", BlockingMobj)
		A("blockingline", BlockingLine)
		A("visibletoteam", VisibleToTeam)
		A("pushfactor", pushfactor)
		A("species", Species)
		A("score", Score)
		A("designatedteam", DesignatedTeam)
		A("lastpush", lastpush)
		A("lastbump", lastbump)
		A("painthreshold", PainThreshold)
		A("damagefactor", DamageFactor)
		A("damagemultiply", DamageMultiply)
		A("waveindexxy", WeaveIndexXY)
		A("weaveindexz", WeaveIndexZ)
		A("pdmgreceived", PoisonDamageReceived)
		A("pdurreceived", PoisonDurationReceived)
		A("ppreceived", PoisonPeriodReceived)
		A("poisoner", Poisoner)
		A("posiondamage", PoisonDamage)
		A("poisonduration", PoisonDuration)
		A("poisonperiod", PoisonPeriod)
		A("poisondamagetype", PoisonDamageType)
		A("poisondmgtypereceived", PoisonDamageTypeReceived)
		A("conversationroot", ConversationRoot)
		A("conversation", Conversation)
		A("friendplayer", FriendPlayer)
		A("telefogsourcetype", TeleFogSourceType)
		A("telefogdesttype", TeleFogDestType)
		A("ripperlevel", RipperLevel)
		A("riplevelmin", RipLevelMin)
		A("riplevelmax", RipLevelMax)
		A("devthreshold", DefThreshold)
		A("spriteangle", SpriteAngle)
		A("spriterotation", SpriteRotation)
		("alternative", alternative)
		("tag", Tag);


}


CCMD(writejson)
{
	DWORD t = I_MSTime();
	FSerializer arc;
	arc.OpenWriter(true);
	arc.BeginObject(nullptr);
	DThinker::SerializeThinkers(arc, false);
	SerializeWorld(arc);
	SerializeObjects(arc);
	arc.EndObject();
	DWORD tt = I_MSTime();
	Printf("JSON generation took %d ms\n", tt - t);
	FILE *f = fopen("out.json", "wb");
	fwrite(arc.mOutString.GetString(), 1, arc.mOutString.GetSize(), f);
	fclose(f);
	DWORD ttt = I_MSTime();
	Printf("JSON save took %d ms\n", ttt - tt);
	rapidjson::Document doc;
	doc.Parse(arc.mOutString.GetString());
	DWORD tttt = I_MSTime();
	Printf("JSON parse took %d ms\n", tttt - ttt);
	doc.ParseInsitu((char*)arc.mOutString.GetString());
	DWORD ttttt = I_MSTime();
	Printf("JSON parse insitu took %d ms\n", ttttt - tttt);
}
