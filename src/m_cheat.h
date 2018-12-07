#ifndef __M_CHEAT_H__
#define __M_CHEAT_H__

//
// CHEAT SEQUENCE PACKAGE
//

// [RH] Functions that actually perform the cheating
class FString;
class player_t;
class PClassActor;

void cht_DoMDK(player_t *player, const char *mod);
void cht_DoCheat (player_t *player, int cheat);
void cht_Give (player_t *player, const char *item, int amount=1);
void cht_Take (player_t *player, const char *item, int amount=1);
void cht_SetInv(player_t *player, const char *item, int amount = 1, bool beyondMax = false);
void cht_Suicide (player_t *player);
FString cht_Morph (player_t *player, PClassActor *morphclass, bool quickundo);
void cht_Takeweaps(player_t *player);

#endif
