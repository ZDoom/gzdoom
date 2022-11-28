#ifndef EDATA_H
#define EDATA_H

struct FMapThing;
struct line_t;

void ProcessEDMapthing(FMapThing *mt, int recordnum);
void ProcessEDLinedef(line_t *line, int recordnum);
void ProcessEDSectors();
void LoadMapinfoACSLump();

#endif