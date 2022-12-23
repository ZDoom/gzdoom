#ifndef A_KEYS_H
#define A_KEYS_H

class AActor;
class PClassActor;

int P_CheckKeys (AActor *owner, int keynum, bool remote, bool quiet = false);
int P_ValidLock (int lock);
void P_InitKeyMessages ();
int P_GetMapColorForLock (int lock);
int P_GetMapColorForKey (AActor *key);
int P_GetKeyTypeCount();
PClassActor *P_GetKeyType(int num);

#endif
