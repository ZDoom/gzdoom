#ifndef __D_NETINFO_H__
#define __D_NETINFO_H__

#include "c_cvars.h"

typedef enum {
	ui_autoaim
} userinfo_t;

void D_SendUserInfoChange (userinfo_t info, char *change);
void D_UserInfoChanged (cvar_t *info);

#endif //__D_CLIENTINFO_H__