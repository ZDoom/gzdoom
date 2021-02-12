/*
** serializer.cpp
** Savegame wrapper around RapidJSON
**
**---------------------------------------------------------------------------
** Copyright 2016 Christoph Oelckers
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
*/

// The #defines here *MUST* match serializer.cpp, or we will get countless strange errors.
#define RAPIDJSON_48BITPOINTER_OPTIMIZATION 0	// disable this insanity which is bound to make the code break over time.
#define RAPIDJSON_HAS_CXX11_RVALUE_REFS 1
#define RAPIDJSON_HAS_CXX11_RANGE_FOR 1
#define RAPIDJSON_PARSE_DEFAULT_FLAGS kParseFullPrecisionFlag

#include <zlib.h>
#include "rapidjson/rapidjson.h"
#include "rapidjson/writer.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/document.h"
#include "serializer_doom.h"
#include "actor.h"
#include "r_defs.h"
#include "printf.h"
#include "p_lnspec.h"
#include "utf8.h"
#include "g_levellocals.h"
#include "p_conversation.h"
#include "p_terrain.h"

#include "serializer_internal.h"

//==========================================================================
//
// we must explicitly delete all thinkers in the array which did not get linked into the thinker lists.
// Otherwise these objects may survive a level deletion and point to incorrect data.
//
//==========================================================================

void FDoomSerializer::CloseReaderCustom()
{
	for (auto obj : r->mDObjects)
	{
		auto think = dyn_cast<DThinker>(obj);
		if (think != nullptr)
		{
			if (think->NextThinker == nullptr || think->PrevThinker == nullptr)
			{
				think->Destroy();
			}
		}
	}
}


//==========================================================================
//
// Special handler for args (because ACS specials' arg0 needs special treatment.)
//
//==========================================================================

FSerializer &SerializeArgs(FSerializer& arc, const char *key, int *args, int *defargs, int special)
{
	if (arc.isWriting())
	{
		auto &w = arc.w;
		if (w->inObject() && defargs != nullptr && !memcmp(args, defargs, 5 * sizeof(int)))
		{
			return arc;
		}

		arc.WriteKey(key);
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
		auto &r = arc.r;
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
						args[i] = -FName(UnicodeToString(aval.GetString())).GetIndex();
					}
					else
					{
						assert(false && "Integer expected");
						Printf(TEXTCOLOR_RED "Integer expected for '%s[%d]'\n", key, i);
						arc.mErrors++;
					}
				}
			}
			else
			{
				assert(false && "array expected");
				Printf(TEXTCOLOR_RED "array expected for '%s'\n", key);
				arc.mErrors++;
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

FSerializer &SerializeTerrain(FSerializer &arc, const char *key, int &terrain, int *def)
{
	if (arc.isWriting() && def != nullptr && terrain == *def)
	{
		return arc;
	}
	FName terr = P_GetTerrainName(terrain);
	Serialize(arc, key, terr, nullptr);
	if (arc.isReading())
	{
		terrain = P_FindTerrain(terr);
	}
	return arc;
}

//==========================================================================
//
//
//
//==========================================================================

FSerializer &FDoomSerializer::Sprite(const char *key, int32_t &spritenum, int32_t *def)
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
				uint32_t name = *reinterpret_cast<const uint32_t*>(UnicodeToString(val->GetString()));
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

FSerializer& FDoomSerializer::StatePointer(const char* key, void* ptraddr, bool *res)
{
	if (isWriting())
	{
		if (res) *res = true;
		(*this)(key, *(FState**)ptraddr);
	}
	else
	{
		::Serialize(*this, key, *(FState**)ptraddr, nullptr, res);
	}
	return *this;
}



template<> FSerializer &Serialize(FSerializer &ar, const char *key, FPolyObj *&value, FPolyObj **defval)
{
	auto arc = dynamic_cast<FDoomSerializer*>(&ar);
	if (!arc || arc->Level == nullptr) I_Error("Trying to serialize polyobject without a valid level");
	return SerializePointer(*arc, key, value, defval, arc->Level->Polyobjects);
}

template<> FSerializer &Serialize(FSerializer &ar, const char *key, side_t *&value, side_t **defval)
{
	auto arc = dynamic_cast<FDoomSerializer*>(&ar);
	if (!arc || arc->Level == nullptr) I_Error("Trying to serialize SIDEDEF without a valid level");
	return SerializePointer(*arc, key, value, defval, arc->Level->sides);
}

template<> FSerializer &Serialize(FSerializer &ar, const char *key, sector_t *&value, sector_t **defval)
{
	auto arc = dynamic_cast<FDoomSerializer*>(&ar);
	if (!arc || arc->Level == nullptr) I_Error("Trying to serialize sector without a valid level");
	return SerializePointer(*arc, key, value, defval, arc->Level->sectors);
}

template<> FSerializer &Serialize(FSerializer &arc, const char *key, player_t *&value, player_t **defval)
{
	return SerializePointer(arc, key, value, defval, players, MAXPLAYERS);
}

template<> FSerializer &Serialize(FSerializer &ar, const char *key, line_t *&value, line_t **defval)
{
	auto arc = dynamic_cast<FDoomSerializer*>(&ar);
	if (!arc || arc->Level == nullptr) I_Error("Trying to serialize linedef without a valid level");
	return SerializePointer(*arc, key, value, defval, arc->Level->lines);
}

template<> FSerializer &Serialize(FSerializer &ar, const char *key, vertex_t *&value, vertex_t **defval)
{
	auto arc = dynamic_cast<FDoomSerializer*>(&ar);
	if (!arc || arc->Level == nullptr) I_Error("Trying to serialize vertex without a valid level");
	return SerializePointer(*arc, key, value, defval, arc->Level->vertexes);
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
			assert(val->IsString() || val->IsNull());
			if (val->IsString())
			{
				clst = PClass::FindActor(UnicodeToString(val->GetString()));
			}
			else if (val->IsNull())
			{
				clst = nullptr;
			}
			else
			{
				Printf(TEXTCOLOR_RED "string type expected for '%s'\n", key);
				clst = nullptr;
				arc.mErrors++;
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
					arc.w->Uint((uint32_t)(state - info->GetStates()));
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
				assert(cls.IsString() && ndx.IsUint());
				if (cls.IsString() && ndx.IsUint())
				{
					PClassActor *clas = PClass::FindActor(UnicodeToString(cls.GetString()));
					if (clas && ndx.GetUint() < (unsigned)clas->GetStateCount())
					{
						state = clas->GetStates() + ndx.GetUint();
					}
					else
					{
						// this can actually happen by changing the DECORATE so treat it as a warning, not an error.
						state = nullptr;
						Printf(TEXTCOLOR_ORANGE "Invalid state '%s+%d' for '%s'\n", cls.GetString(), ndx.GetInt(), key);
					}
				}
				else
				{
					assert(false && "not a state");
					Printf(TEXTCOLOR_RED "data does not represent a state for '%s'\n", key);
					arc.mErrors++;
				}
			}
			else if (!retcode)
			{
				assert(false && "not an array");
				Printf(TEXTCOLOR_RED "array type expected for '%s'\n", key);
				arc.mErrors++;
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
	auto doomarc = static_cast<FDoomSerializer*>(&arc);
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
			assert(val->IsUint() || val->IsNull());
			if (val->IsNull())
			{
				node = nullptr;
			}
			else if (val->IsUint())
			{
				if (val->GetUint() >= doomarc->Level->StrifeDialogues.Size())
				{
					node = nullptr;
				}
				else
				{
					node = doomarc->Level->StrifeDialogues[val->GetUint()];
				}
			}
			else
			{
				Printf(TEXTCOLOR_RED "integer expected for '%s'\n", key);
				arc.mErrors++;
				node = nullptr;
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
			assert(val->IsNull() || val->IsString());
			if (val->IsNull())
			{
				pstr = nullptr;
			}
			else if (val->IsString())
			{
				pstr = AActor::mStringPropertyData.Alloc(UnicodeToString(val->GetString()));
			}
			else
			{
				Printf(TEXTCOLOR_RED "string expected for '%s'\n", key);
				pstr = nullptr;
				arc.mErrors++;
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
			assert(val->IsNull() || val->IsString());
			if (val->IsNull())
			{
				pstr = nullptr;
			}
			else if (val->IsString())
			{
				pstr = copystring(UnicodeToString(val->GetString()));
			}
			else
			{
				Printf(TEXTCOLOR_RED "string expected for '%s'\n", key);
				pstr = nullptr;
				arc.mErrors++;
			}
		}
	}
	return arc;
}

//==========================================================================
//
// This is a bit of a cheat because it never actually writes out the pointer.
// The rules for levels are that they must be self-contained.
// No level and no object that is part of a level may reference a different one.
//
// When writing, this merely checks if the rules are obeyed and if not errors out.
// When reading, it assumes that the object was properly written and restores
// the reference from the owning level
//
// The only exception are null pointers which are allowed
//
//==========================================================================

template<> FSerializer &Serialize(FSerializer &arc, const char *key, FLevelLocals *&lev, FLevelLocals **def)
{
	auto doomarc = static_cast<FDoomSerializer*>(&arc);
	if (arc.isWriting())
	{
		if (!arc.w->inObject() || lev == nullptr)
		{
			arc.WriteKey(key);
			if (lev == nullptr)
			{
				arc.w->Null();
			}
			else
			{
				// This MUST be the currently serialized level, anything else will error out here as a sanity check.
				if (doomarc->Level == nullptr || lev != doomarc->Level)
				{
					I_Error("Attempt to serialize invalid level reference");
				}
				if (!arc.w->inObject())
				{
					arc.w->Bool(true);	// In the unlikely case this is used in an array, write a filler.
				}
			}
		}
	}
	else
	{
		auto val = arc.r->FindKey(key);
		if (val != nullptr)
		{
			assert(val->IsNull() || val->IsBool());
			if (val->IsNull())
			{
				lev = nullptr;
			}
			else 
			{
				lev = doomarc->Level;
			}
		}
		else lev = doomarc->Level;
	}
	return arc;
}

