#ifndef __RAVENSHARED_H__
#define __RAVENSHARED_H__

class AActor;
class player_s;

bool P_MorphPlayer (player_s *player);
bool P_UndoPlayerMorph (player_s *player, bool force);

bool P_MorphMonster (AActor *actor);
bool P_UpdateMorphedMonster (AActor *actor, int tics);

#endif //__RAVENSHARED_H__
