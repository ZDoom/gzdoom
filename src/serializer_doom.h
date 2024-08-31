#pragma once

#include "serializer.h"

class player_t;
struct sector_t;
struct line_t;
struct side_t;
struct vertex_t;
struct ticcmd_t;
struct usercmd_t;
class PClassActor;
struct FStrifeDialogueNode;
struct FState;
struct FDoorAnimation;
struct FPolyObj;
struct FInterpolator;
struct FLevelLocals;

class FDoomSerializer : public FSerializer
{
	
	void CloseReaderCustom() override;
	
public:
	FLevelLocals* Level;

	FDoomSerializer(FLevelLocals *l) : Level(l)
	{
		SetUniqueSoundNames();
	}

	FSerializer &Sprite(const char *key, int32_t &spritenum, int32_t *def) override;
	FSerializer& StatePointer(const char* key, void* ptraddr, bool *res) override;

};

FSerializer &SerializeArgs(FSerializer &arc, const char *key, int *args, int *defargs, int special);
FSerializer &SerializeTerrain(FSerializer &arc, const char *key, int &terrain, int *def = nullptr);

FSerializer& Serialize(FSerializer& arc, const char* key, char& value, char* defval);
FSerializer &Serialize(FSerializer &arc, const char *key, ticcmd_t &sid, ticcmd_t *def);
FSerializer &Serialize(FSerializer &arc, const char *key, usercmd_t &cmd, usercmd_t *def);
FSerializer &Serialize(FSerializer &arc, const char *key, FInterpolator &rs, FInterpolator *def);

template<> FSerializer &Serialize(FSerializer &arc, const char *key, FPolyObj *&value, FPolyObj **defval);
template<> FSerializer &Serialize(FSerializer &arc, const char *key, sector_t *&value, sector_t **defval);
template<> FSerializer &Serialize(FSerializer &arc, const char *key, const FPolyObj *&value, const FPolyObj **defval);
template<> FSerializer &Serialize(FSerializer &arc, const char *key, const sector_t *&value, const sector_t **defval);
template<> FSerializer &Serialize(FSerializer &arc, const char *key, player_t *&value, player_t **defval);
template<> FSerializer &Serialize(FSerializer &arc, const char *key, line_t *&value, line_t **defval);
template<> FSerializer &Serialize(FSerializer &arc, const char *key, side_t *&value, side_t **defval);
template<> FSerializer &Serialize(FSerializer &arc, const char *key, vertex_t *&value, vertex_t **defval);
template<> FSerializer &Serialize(FSerializer &arc, const char *key, PClassActor *&clst, PClassActor **def);
template<> FSerializer &Serialize(FSerializer &arc, const char *key, FStrifeDialogueNode *&node, FStrifeDialogueNode **def);
template<> FSerializer &Serialize(FSerializer &arc, const char *key, FString *&pstr, FString **def);
template<> FSerializer &Serialize(FSerializer &arc, const char *key, FDoorAnimation *&pstr, FDoorAnimation **def);
template<> FSerializer &Serialize(FSerializer &arc, const char *key, char *&pstr, char **def);
template<> FSerializer &Serialize(FSerializer &arc, const char *key, FLevelLocals *&font, FLevelLocals **def);
FSerializer &Serialize(FSerializer &arc, const char *key, FState *&state, FState **def, bool *retcode);
template<> inline FSerializer &Serialize(FSerializer &arc, const char *key, FState *&state, FState **def)
{
	return Serialize(arc, key, state, def, nullptr);
}
