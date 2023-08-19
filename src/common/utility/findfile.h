#pragma once
// Directory searching routines

#include <stdint.h>
#include "zstring.h"


class FConfigFile;

bool D_AddFile(TArray<FString>& wadfiles, const char* file, bool check, int position, FConfigFile* config);
void D_AddWildFile(TArray<FString>& wadfiles, const char* value, const char *extension, FConfigFile* config);
void D_AddConfigFiles(TArray<FString>& wadfiles, const char* section, const char* extension, FConfigFile* config);
void D_AddDirectory(TArray<FString>& wadfiles, const char* dir, const char *filespec, FConfigFile* config);
const char* BaseFileSearch(const char* file, const char* ext, bool lookfirstinprogdir, FConfigFile* config);
