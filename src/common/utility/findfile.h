#pragma once
// Directory searching routines

#include <stdint.h>
#include <string>
#include <vector>

class FConfigFile;

enum EFileRequirements
{
    REQUIRE_NONE    = 0,
    REQUIRE_IWAD    = 1,
    REQUIRE_FILE    = 2,
    REQUIRE_OPTFILE = 4,

    REQUIRE_DEFAULT = REQUIRE_IWAD|REQUIRE_FILE,
    REQUIRE_ALL = REQUIRE_IWAD|REQUIRE_FILE|REQUIRE_OPTFILE
};

bool D_AddFile(std::vector<std::string>& wadfiles, const char* file, bool check, int position, FConfigFile* config, bool optional = false);
void D_AddWildFile(std::vector<std::string>& wadfiles, const char* value, const char *extension, FConfigFile* config, bool optional = false);
void D_AddConfigFiles(std::vector<std::string>& wadfiles, const char* section, const char* extension, FConfigFile* config, bool optional = false);
void D_AddDirectory(std::vector<std::string>& wadfiles, const char* dir, const char *filespec, FConfigFile* config, bool optional = false);
const char* BaseFileSearch(const char* file, const char* ext, bool lookfirstinprogdir, FConfigFile* config);
void D_FileNotFound(EFileRequirements test, const char* type, const char* file);
