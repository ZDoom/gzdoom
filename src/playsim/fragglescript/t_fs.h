
#ifndef T_FS_H
#define T_FS_H

// global FS interface

struct MapData;
class AActor;

void T_PreprocessScripts(FLevelLocals *Level);
void T_LoadScripts(FLevelLocals *Level, MapData * map);
void T_AddSpawnedThing(FLevelLocals *Level, AActor * );
bool T_RunScript(FLevelLocals *l, int snum, AActor * t_trigger);

#endif
