#include "i_interface.h"
#include "st_start.h"
#include "gamestate.h"

static_assert(sizeof(void*) == 8, "32 builds are not supported");

// Some global engine variables taken out of the backend code.
FStartupScreen* StartWindow;
SystemCallbacks sysCallbacks;
FString endoomName;
bool batchrun;
float menuBlurAmount;

bool AppActive = true;
int chatmodeon;
gamestate_t 	gamestate = GS_STARTUP;

