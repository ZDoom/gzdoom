#pragma once

#include "zstring.h"

#if defined(__unix__) || defined(__HAIKU__)
FString GetUserFile (const char *path);
#endif
FString M_GetAppDataPath(bool create);
FString M_GetCachePath(bool create);
FString M_GetAutoexecPath();
FString M_GetConfigPath(bool for_reading);
FString M_GetScreenshotsPath();
FString M_GetSavegamesPath();
FString M_GetDocumentsPath();
FString M_GetDemoPath();

FString M_GetNormalizedPath(const char* path);


#ifdef __APPLE__
FString M_GetMacAppSupportPath(const bool create = true);
void M_GetMacSearchDirectories(FString& user_docs, FString& user_app_support, FString& local_app_support);
#endif // __APPLE__
