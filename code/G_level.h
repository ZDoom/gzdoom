#ifndef __G_LEVEL_H__
#define __G_LEVEL_H__

#include "doomtype.h"
#include "doomdef.h"
#include "m_fixed.h"

#define LEVEL_NOINTERMISSION	0x00000001
#define	LEVEL_DOUBLESKY			0x00000004
#define LEVEL_NOSOUNDCLIPPING	0x00000008

#define LEVEL_MAP07SPECIAL		0x00000010
#define LEVEL_BRUISERSPECIAL	0x00000020
#define LEVEL_CYBORGSPECIAL		0x00000040
#define LEVEL_SPIDERSPECIAL		0x00000080

#define LEVEL_SPECLOWERFLOOR	0x00000100
#define LEVEL_SPECOPENDOOR		0x00000200
#define LEVEL_SPECACTIONSMASK	0x00000300

#define LEVEL_MONSTERSTELEFRAG	0x00000400

#define LEVEL_CHANGEMAPCHEAT	0x40000000		// Don't display cluster messages
#define LEVEL_VISITED			0x80000000		// Used for intermission map


struct level_info_s {
	char		mapname[8];
	int			levelnum;
	char		*level_name;
	char		pname[8];
	char		nextmap[8];
	char		secretmap[8];
	int			partime;
	char		skypic1[8];
	char		music[8];
	unsigned	flags;
	int			cluster;
};
typedef struct level_info_s level_info_t;

struct level_pwad_info_s {
	char		mapname[8];
	int			levelnum;
	char		*level_name;
	char		pname[8];
	char		nextmap[8];
	char		secretmap[8];
	int			partime;
	char		skypic1[8];
	char		music[8];
	unsigned	flags;
	int			cluster;

	char		skypic2[8];
	fixed_t		skyspeed1;
	fixed_t		skyspeed2;
	unsigned	fadeto;
};
typedef struct level_pwad_info_s level_pwad_info_t;


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

	unsigned	fadeto;					// The color the palette fades to (usually black)

	char		music[8];
	char		skypic1[8];
	char		skypic2[8];

	fixed_t		skyspeed1;				// Scrolling speed of first sky texture
	fixed_t		skyspeed2;

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

void G_InitNew (char *mapname);

// Can be called by the startup code or M_Responder.
// A normal game starts at map 1,
// but a warp test can start elsewhere
void G_DeferedInitNew (char *mapname);

void G_ExitLevel (void);
void G_SecretExitLevel (void);

void G_DoLoadLevel (void);

void G_InitLevelLocals (void);

void G_SetLevelStrings (void);

cluster_info_t *FindClusterInfo (int cluster);
level_info_t *FindLevelInfo (char *mapname);

char *CalcMapName (int episode, int level);

void G_ParseMapInfo (void);

#endif //__G_LEVEL_H__