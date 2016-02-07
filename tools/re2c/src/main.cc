#include "src/util/c99_stdint.h"
#include <string>

#include "src/codegen/output.h"
#include "src/conf/msg.h"
#include "src/conf/opt.h"
#include "src/conf/warn.h"
#include "src/globals.h"
#include "src/parse/input.h"
#include "src/parse/parser.h"
#include "src/parse/scanner.h"

namespace re2c
{

bool bUsedYYBitmap  = false;
bool bWroteGetState = false;
bool bWroteCondCheck = false;
uint32_t last_fill_index = 0;
std::string yySetupRule = "";

} // end namespace re2c

using namespace re2c;

int main(int, char *argv[])
{
	switch (parse_opts (argv, opts))
	{
		case OK:        break;
		case EXIT_OK:   return 0;
		case EXIT_FAIL: return 1;
	}

	// set up the source stream
	re2c::Input input (opts.source_file);
	if (!input.open ())
	{
		error ("cannot open source file: %s", opts.source_file);
		return 1;
	}

	// set up the output streams
	re2c::Output output (opts.output_file, opts->header_file);
	if (!output.source.open ())
	{
		error ("cannot open output file: %s", opts.output_file);
		return 1;
	}
	if (opts->tFlag && !output.header.open ())
	{
		error ("cannot open header file: %s", opts->header_file);
		return 1;
	}

	Scanner scanner (input, output.source);
	parse (scanner, output);

	return warn.error () ? 1 : 0;
}
