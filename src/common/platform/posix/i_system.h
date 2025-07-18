#ifndef __I_SYSTEM__
#define __I_SYSTEM__

#include <dirent.h>
#include <ctype.h>

#if defined(__sun) || defined(__sun__) || defined(__SRV4) || defined(__srv4__)
#define __solaris__ 1
#endif

#include <thread>
#include <algorithm>
#include "tarray.h"
#include "zstring.h"

struct WadStuff;
struct FStartupSelectionInfo;

#ifndef SHARE_DIR
#define SHARE_DIR "/usr/local/share/"
#endif

void CalculateCPUSpeed(void);

// Return a seed value for the RNG.
unsigned int I_MakeRNGSeed();



void I_StartFrame (void);

void I_StartTic (void);

// Print a console string
void I_PrintStr (const char *str);

// Set the title string of the startup window
void I_SetIWADInfo ();

// Pick from multiple IWADs to use
bool I_PickIWad (bool showwin, FStartupSelectionInfo& info);

// [RH] Checks the registry for Steam's install path, so we can scan its
// directories for IWADs if the user purchased any through Steam.
TArray<FString> I_GetSteamPath();

TArray<FString> I_GetGogPaths();

TArray<FString> I_GetBethesdaPath();

// The ini could not be saved at exit
bool I_WriteIniFailed (const char* filename);

class FGameTexture;
bool I_SetCursor(FGameTexture *);

inline int I_GetNumaNodeCount() { return 1; }
inline int I_GetNumaNodeThreadCount(int numaNode) { return std::max<int>(std::thread::hardware_concurrency(), 1); }
inline void I_SetThreadNumaNode(std::thread &thread, int numaNode) { }

FString I_GetCWD();
bool I_ChDir(const char* path);
void I_OpenShellFolder(const char*);

#endif
