
#ifndef T_FS_H
#define T_FS_H

// global FS interface

struct MapData;
class AActor;

void T_PreprocessScripts();
void T_LoadScripts(MapData * map);
void T_AddSpawnedThing(AActor * );

#endif
