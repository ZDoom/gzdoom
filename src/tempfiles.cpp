#include <stdio.h>
#include <malloc.h>
#include "tempfiles.h"

FTempFileName::FTempFileName (const char *prefix)
{
// Under Linux, ld will complain that tempnam is dangerous, and
// mkstemp should be used instead. However, there is no mkstemp
// under VC++, and even if there was, I still need to know the
// file name so that it can be used as input to Timidity.

	Name = tempnam (NULL, prefix);
}

FTempFileName::~FTempFileName ()
{
	if (Name != NULL)
	{
		remove (Name);
		free (Name);
		Name = NULL;
	}
}
