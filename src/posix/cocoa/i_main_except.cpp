// Workaround for GCC Objective-C++ with C++ exceptions bug.

#include "doomerrors.h"
#include <stdlib.h>

// Import some functions from i_main.mm
void call_terms();
void Mac_I_FatalError(const char* const message);
void OriginalMainTry(int argc, char** argv);

void OriginalMainExcept(int argc, char** argv)
{
	try
	{
		OriginalMainTry(argc, argv);
	}
	catch(const CDoomError& error)
	{
		const char* const message = error.GetMessage();

		if (NULL != message)
		{
			fprintf(stderr, "%s\n", message);
			Mac_I_FatalError(message);
		}

		exit(-1);
	}
	catch(...)
	{
		call_terms();
		throw;
	}
}
