#ifndef __GAMECONFIGFILE_H__
#define __GAMECONFIGFILE_H__

#include "configfile.h"

class FGameConfigFile : public FConfigFile
{
public:
	FGameConfigFile ();
	~FGameConfigFile ();

	void DoGlobalSetup ();
	void DoGameSetup (const char *gamename);
	void ArchiveGlobalData ();
	void ArchiveGameData (const char *gamename);

protected:
	void WriteCommentHeader (FILE *file) const;

private:
	static void MigrateStub (const char *pathname, FConfigFile *config, void *userdata);

	char *GetConfigPath ();
	void RunAutoexec (const char *game);
	void DoRunAutoexec (const char *file);
	void MigrateOldConfig ();
	void SetRavenDefaults (bool isHexen);
	void ReadCVars (DWORD flags);

	bool bMigrating;
};

#endif //__GAMECONFIGFILE_H__