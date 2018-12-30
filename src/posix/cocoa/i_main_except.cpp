/*
 ** i_main_except.cpp
 **
 **---------------------------------------------------------------------------
 ** Copyright 2012-2015 Alexey Lysiuk
 ** All rights reserved.
 **
 ** Redistribution and use in source and binary forms, with or without
 ** modification, are permitted provided that the following conditions
 ** are met:
 **
 ** 1. Redistributions of source code must retain the above copyright
 **    notice, this list of conditions and the following disclaimer.
 ** 2. Redistributions in binary form must reproduce the above copyright
 **    notice, this list of conditions and the following disclaimer in the
 **    documentation and/or other materials provided with the distribution.
 ** 3. The name of the author may not be used to endorse or promote products
 **    derived from this software without specific prior written permission.
 **
 ** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 ** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 ** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 ** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 ** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 ** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 ** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 ** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 ** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 ** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **---------------------------------------------------------------------------
 **
 */
// Workaround for GCC Objective-C++ with C++ exceptions bug.

#include <stdlib.h>

#include "doomerrors.h"
#include "vm.h"

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
	catch(const std::exception& error)
	{
		const char* const message = error.what();

		if (strcmp(message, "NoRunExit"))
		{
			if (CVMAbortException::stacktrace.IsNotEmpty())
			{
				Printf("%s", CVMAbortException::stacktrace.GetChars());
			}

			if (batchrun)
			{
				Printf("%s\n", message);
			}
			else
			{
				Mac_I_FatalError(message);
			}
		}

		exit(-1);
	}
	catch(...)
	{
		call_terms();
		throw;
	}
}
