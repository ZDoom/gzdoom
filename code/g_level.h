#ifndef __G_LEVEL_H__
#define __G_LEVEL_H__

#include "doomtype.h"
#include "doomdef.h"

#define LEVEL_NOINTERMISSION	1
#define LEVEL_SECRET			2
struct level_info_s {
	char		mapname[8];
	int			levelnum;
	char		*level_name;
	char		pname[8];
	char		nextmap[8];
	char		secretmap[8];
	int			partime;
	char		skypic1[8];
	char		skypic2[8];
	char		music[8];
	unsigned	flags;
	int			cluster;
};
typedef struct level_info_s level_info_t;

struct level_locals_s {
	int			time;
	int			starttime;
	int			partime;

	level_info_t *info;
	int			cluster;
	int			levelnum;
	char		level_name[64];			// the descriptive name (Outer Base, etc)
	char		mapname[8];				// the server name (base1, etc)
	char		nextmap[8];				// go here when fraglimit is hit
	char		secretmap[8];			// map to go to when used secret exit

	unsigned	flags;

	char		music[8];
	char		skypic1[8];
	char		skypic2[8];

	int			total_secrets;
	int			found_secrets;

	int			total_items;
	int			found_items;

	int			total_monsters;
	int			killed_monsters;
};
typedef struct level_locals_s level_locals_t;

struct cluster_info_s {
	int			cluster;
	char		messagemusic[9];
	char		finaleflat[8];
	char		*exittext;
	char		*entertext;
};
typedef struct cluster_info_s cluster_info_t;

extern level_locals_t level;
extern level_info_t LevelInfos[];
extern cluster_info_t ClusterInfos[];

void G_InitNew (skill_t skill, char *mapname);

// Can be called by the startup code or M_Responder.
// A normal game starts at map 1,
// but a warp test can start elsewhere
void G_DeferedInitNew (skill_t skill, char *mapname);

void G_ExitLevel (void);
void G_SecretExitLevel (void);

void G_DoLoadLevel (void);

void G_InitLevelLocals (void);

cluster_info_t *FindClusterInfo (int cluster);
level_info_t *FindLevelInfo (char *mapname);

char *CalcMapName (int episode, int level);

#endif //__G_LEVEL_H__