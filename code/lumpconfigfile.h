#ifndef __LUMPCONFIGFILE_H__
#define __LUMPCONFIGFILE_H__

#include "configfile.h"

class FLumpConfigFile : public FConfigFile
{
public:
	FLumpConfigFile (int lump);
	~FLumpConfigFile ();

	void LoadConfigFile ();

protected:
	virtual char *ReadLine (char *string, int n, void *file) const;

private:
	struct LumpState
	{
		int lump;
		char *lumpdata;
		char *end;
		char *pos;
	};

	LumpState State;
};

#endif //__LUMPCONFIGFILE_H__