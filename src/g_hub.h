#ifndef __G_HUB_H
#define __G_HUB_H

#include <stdio.h>
//#include <m_png.h>
#include "g_level.h"
#include "wi_stuff.h"

void G_WriteHubInfo (FILE *file);
void G_ReadHubInfo (PNGHandle *png);
void G_LeavingHub(int mode, cluster_info_t * cluster, struct wbstartstruct_s * wbs);

#endif

