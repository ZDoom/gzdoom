#ifndef __D_NETINFO_H__
#define __D_NETINFO_H__

#include "c_cvars.h"

extern cvar_t *autoaim;

#define MAXPLAYERNAME	15

#define GENDER_MALE		0
#define GENDER_FEMALE	1
#define GENDER_NEUTER	2

struct userinfo_s {
	char		netname[MAXPLAYERNAME+1];
	char		team[MAXPLAYERNAME+1];
	fixed_t		aimdist;
	int			color;
	int			skin;
	int			gender;
	BOOL		neverswitch;
};
typedef struct userinfo_s userinfo_t;

void D_SetupUserInfo (void);

void D_UserInfoChanged (cvar_t *info);

void D_SendServerInfoChange (const cvar_t *cvar, const char *value);
void D_DoServerInfoChange (byte **stream);

void D_WriteUserInfoStrings (int player, byte **stream);
void D_ReadUserInfoStrings (int player, byte **stream, BOOL update);

#endif //__D_CLIENTINFO_H__