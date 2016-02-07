#ifndef _RE2C_PARSE_INPUT_
#define _RE2C_PARSE_INPUT_

#include <stdio.h>
#include <string>

#include "src/util/forbid_copy.h"

namespace re2c {

struct Input
{
	FILE * file;
	std::string file_name;

	Input (const char * fn);
	~Input ();
	bool open ();

	FORBID_COPY (Input);
};

} // namespace re2c

#endif // _RE2C_PARSE_INPUT_
