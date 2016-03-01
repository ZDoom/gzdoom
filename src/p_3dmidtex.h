
#ifndef __3DMIDTEX_H
#define __3DMIDTEX_H

#include "doomtype.h"

class DInterpolation;
struct sector_t;
struct line_t;
class AActor;

bool P_Scroll3dMidtex(sector_t *sector, int crush, fixed_t move, bool ceiling);
void P_Start3dMidtexInterpolations(TArray<DInterpolation *> &list, sector_t *sec, bool ceiling);
void P_Attach3dMidtexLinesToSector(sector_t *dest, int lineid, int tag, bool ceiling);
bool P_GetMidTexturePosition(const line_t *line, int sideno, fixed_t *ptextop, fixed_t *ptexbot);
bool P_Check3dMidSwitch(AActor *actor, line_t *line, int side);
bool P_LineOpening_3dMidtex(AActor *thing, const line_t *linedef, struct FLineOpening &open, bool restrict=false);

bool P_MoveLinkedSectors(sector_t *sector, int crush, fixed_t move, bool ceiling);
void P_StartLinkedSectorInterpolations(TArray<DInterpolation *> &list, sector_t *sector, bool ceiling);
bool P_AddSectorLinks(sector_t *control, int tag, INTBOOL ceiling, int movetype);
void P_AddSectorLinksByID(sector_t *control, int id, INTBOOL ceiling);


#endif
