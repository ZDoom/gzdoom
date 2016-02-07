#include "src/parse/input.h"

namespace re2c {

Input::Input (const char * fn)
	: file (NULL)
	, file_name (fn)
{}

bool Input::open ()
{
	if (file_name == "<stdin>")
	{
		file = stdin;
	}
	else
	{
		file = fopen (file_name.c_str (), "rb");
	}
	return file != NULL;
}

Input::~Input ()
{
	if (file != NULL && file != stdin)
	{
		fclose (file);
	}
}

} // namespace re2c
