#ifndef __D_NETINFO_H__
#define __D_NETINFO_H__

#include "c_cvars.h"

EXTERN_CVAR (Float, autoaim)

#define MAXPLAYERNAME	15

#define GENDER_MALE		0
#define GENDER_FEMALE	1
#define GENDER_NEUTER	2

struct userinfo_s
{
	char		netname[MAXPLAYERNAME+1];
	int			team;
	int			aimdist;
	int			color;
	int			skin;
	int			gender;
	bool		neverswitch;
};
typedef struct userinfo_s userinfo_t;

enum ETeams
{
	TEAM_Red,
	TEAM_Blue,
	TEAM_Green,
	TEAM_Gold,

	NUM_TEAMS,
	TEAM_None		= 255
};

extern const char *TeamNames[NUM_TEAMS];

FArchive &operator<< (FArchive &arc, userinfo_t &info);

void D_SetupUserInfo (void);

void D_UserInfoChanged (FBaseCVar *info);

void D_SendServerInfoChange (const FBaseCVar *cvar, UCVarValue value, ECVarType type);
void D_DoServerInfoChange (byte **stream);

void D_WriteUserInfoStrings (int player, byte **stream, bool compact=false);
void D_ReadUserInfoStrings (int player, byte **stream, bool update);

#endif //__D_CLIENTINFO_H__