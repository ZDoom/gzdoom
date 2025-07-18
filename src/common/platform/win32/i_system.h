
#ifndef __I_SYSTEM__
#define __I_SYSTEM__

#include "basics.h"
#include <thread>
#include "tarray.h"
#include "zstring.h"
#include "utf8.h"

struct WadStuff;
struct FStartupSelectionInfo;

// [RH] Detects the OS the game is running under.
void I_DetectOS (void);

// Called by DoomMain.
void CalculateCPUSpeed (void);

// Return a seed value for the RNG.
unsigned int I_MakeRNGSeed();


void I_StartFrame (void);
void I_StartTic (void);

// Set the mouse cursor. The texture must be 32x32.
class FGameTexture;
bool I_SetCursor(FGameTexture *cursor);

// Repaint the pre-game console
void I_PaintConsole (void);

// Print a console string
void I_PrintStr (const char *cp);

// Set the title string of the startup window
void I_SetIWADInfo ();

// Pick from multiple IWADs to use
bool I_PickIWad(bool showwin, FStartupSelectionInfo& info);

// The ini could not be saved at exit
bool I_WriteIniFailed (const char* filename);

// [RH] Checks the registry for Steam's install path, so we can scan its
// directories for IWADs if the user purchased any through Steam.
TArray<FString> I_GetSteamPath();

// [GZ] Same deal for GOG paths
TArray<FString> I_GetGogPaths();

// Again for the Bethesda.net Launcher path
TArray<FString> I_GetBethesdaPath();

// Damn Microsoft for doing Get/SetWindowLongPtr half-assed. Instead of
// giving them proper prototypes under Win32, they are just macros for
// Get/SetWindowLong, meaning they take LONGs and not LONG_PTRs.
#ifdef _WIN64
typedef long long WLONG_PTR;
#elif _MSC_VER
typedef _W64 long WLONG_PTR;
#else
typedef long WLONG_PTR;
#endif

// Wrapper for GetLongPathName
FString I_GetLongPathName(const FString &shortpath);

// Mirror WIN32_FIND_DATAA in <winbase.h>
#ifndef MAX_PATH
#define MAX_PATH 260
#endif
#ifndef PATH_MAX
#define PATH_MAX 260
#endif

int I_GetNumaNodeCount();
int I_GetNumaNodeThreadCount(int numaNode);
void I_SetThreadNumaNode(std::thread &thread, int numaNode);

void I_OpenShellFolder(const char*);
FString I_GetCWD();
bool I_ChDir(const char* path);

#endif
