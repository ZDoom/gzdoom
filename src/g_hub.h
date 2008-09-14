#ifndef __G_HUB_H
#define __G_HUB_H

#include <stdio.h>

struct PNGHandle;
struct cluster_info_t;
struct wbstartstruct_t;

void G_WriteHubInfo (FILE *file);
void G_ReadHubInfo (PNGHandle *png);
void G_LeavingHub(int mode, cluster_info_t * cluster, struct wbstartstruct_t * wbs);

#endif

