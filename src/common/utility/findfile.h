#pragma once
// Directory searching routines

#include <stdint.h>
#include <string>
#include <vector>

class FConfigFile;

bool D_AddFile(std::vector<std::string>& wadfiles, const char* file, bool check, int position, FConfigFile* config);
void D_AddWildFile(std::vector<std::string>& wadfiles, const char* value, const char *extension, FConfigFile* config);
void D_AddConfigFiles(std::vector<std::string>& wadfiles, const char* section, const char* extension, FConfigFile* config);
void D_AddDirectory(std::vector<std::string>& wadfiles, const char* dir, const char *filespec, FConfigFile* config);
const char* BaseFileSearch(const char* file, const char* ext, bool lookfirstinprogdir, FConfigFile* config);
