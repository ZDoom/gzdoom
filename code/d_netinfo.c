#include <math.h>

#include "doomtype.h"
#include "doomdef.h"
#include "doomstat.h"
#include "d_netinfo.h"

cvar_t *autoaim;

void D_SendUserInfoChange (userinfo_t info, char *change)
{
	// Quick hack until I get different network packets implemented
	// You'd better make sure every client is the same where it counts
	int i;

	for (i = 0; i < MAXPLAYERS; i++) {
		switch (info) { 
			case ui_autoaim:
				players[i].aimdist = (int)(atof (change) * 16384.0);
				break;
		}
	}
}

void D_UserInfoChanged (cvar_t *cvar)
{
	userinfo_t info = -1;

	if (cvar == autoaim)
		info = ui_autoaim;

	if (info > -1)
		D_SendUserInfoChange (info, cvar->string);
}