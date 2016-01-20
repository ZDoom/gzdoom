#ifndef __PATHEXPANDER_H
#define __PATHEXPANDER_H

#include "tarray.h"
#include "zstring.h"
#include "files.h"

class PathExpander
{
	TArray<FString> PathList;
	
public:
	int openmode;
	
	enum
	{
		OM_FILEORLUMP = 0,
		OM_LUMP,
		OM_FILE
	};
	
	PathExpander(int om = OM_FILEORLUMP)
	{
		openmode = om;
	}
	void addToPathlist(const char *s);
	void clearPathlist();
	FileReader *openFileReader(const char *name, int *plumpnum);
};

#endif
