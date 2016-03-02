
#ifndef T_FS_H
#define T_FS_H

// global FS interface

struct MapData;
class AActor;

void T_PreprocessScripts();
void T_LoadScripts(MapData * map);
void T_AddSpawnedThing(AActor * );
bool T_RunScript(int snum, AActor * t_trigger);
void FS_Close();

#endif
