/* $Id: main.cc 691 2007-04-22 15:07:39Z helly $ */
#ifdef HAVE_CONFIG_H
#include "config.h"
#elif defined(_WIN32)
#include "config_w32.h"
#endif

#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <string.h>

#include "globals.h"
#include "parser.h"
#include "dfa.h"
#include "mbo_getopt.h"

namespace re2c
{

file_info sourceFileInfo;
file_info outputFileInfo;

bool bFlag = false;
bool dFlag = false;
bool eFlag = false;
bool fFlag = false;
bool gFlag = false;
bool iFlag = false;
bool sFlag = false;
bool uFlag = false;
bool wFlag = false;

bool bNoGenerationDate = false;

bool bSinglePass = false;
bool bFirstPass  = true;
bool bLastPass   = false;

bool bUsedYYAccept  = false;
bool bUsedYYMaxFill = false;
bool bUsedYYMarker  = true;

bool bUseStartLabel  = false;
bool bUseStateNext   = false;
bool bUseYYFill      = true;
bool bUseYYFillParam = true;

std::string startLabelName;
std::string labelPrefix("yy");
std::string yychConversion("");
uint maxFill = 1;
uint next_label = 0;
uint cGotoThreshold = 9;

uint topIndent = 0;
std::string indString("\t");
bool yybmHexTable = false;
bool bUseStateAbort = false;
bool bWroteGetState = false;

uint nRealChars = 256;

uint next_fill_index = 0;
uint last_fill_index = 0;
std::set<uint> vUsedLabels;
re2c::CodeNames mapCodeName;

free_list<RegExp*> RegExp::vFreeList;
free_list<Range*>  Range::vFreeList;

using namespace std;

static char *opt_arg = NULL;
static int opt_ind = 1;

static const mbo_opt_struct OPTIONS[] =
{
	mbo_opt_struct('?', 0, "help"),
	mbo_opt_struct('b', 0, "bit-vectors"),
	mbo_opt_struct('d', 0, "debug-output"),
	mbo_opt_struct('e', 0, "ecb"),
	mbo_opt_struct('f', 0, "storable-state"),
	mbo_opt_struct('g', 0, "computed-gotos"),
	mbo_opt_struct('h', 0, "help"),
	mbo_opt_struct('i', 0, "no-debug-info"),
	mbo_opt_struct('o', 1, "output"),
	mbo_opt_struct('s', 0, "nested-ifs"),
	mbo_opt_struct('u', 0, "unicode"),
	mbo_opt_struct('v', 0, "version"),
	mbo_opt_struct('V', 0, "vernum"),
	mbo_opt_struct('w', 0, "wide-chars"),      
	mbo_opt_struct('1', 0, "single-pass"),
	mbo_opt_struct(10,  0, "no-generation-date"),
	mbo_opt_struct('-', 0, NULL) /* end of args */
};

static void usage()
{
	cerr << "usage: re2c [-bdefghisvVw1] [-o file] file\n"
	"\n"
	"-? -h  --help           Display this info.\n"
	"\n"
	"-b     --bit-vectors    Implies -s. Use bit vectors as well in the attempt to\n"
	"                        coax better code out of the compiler. Most useful for\n"
	"                        specifications with more than a few keywords (e.g. for\n"
	"                        most programming languages).\n"
	"\n"
	"-d     --debug-output   Creates a parser that dumps information during\n"
	"                        about the current position and in which state the\n"
	"                        parser is.\n"
	"\n"
	"-e     --ecb            Cross-compile from an ASCII platform to\n"
	"                        an EBCDIC one.\n"
	"\n"
	"-f     --storable-state Generate a scanner that supports storable states.\n"
	"\n"
	"-g     --computed-gotos Implies -b. Generate computed goto code (only useable\n"
	"                        with gcc).\n"
	"\n"
	"-i     --no-debug-info  Do not generate '#line' info (usefull for versioning).\n"
	"\n"
	"-o     --output=output  Specify the output file instead of stdout\n"
	"                        This cannot be used together with -e switch.\n"
	"\n"
	"-s     --nested-ifs     Generate nested ifs for some switches. Many compilers\n"
	"                        need this assist to generate better code.\n"
	"\n"
	"-u     --unicode        Implies -w but supports the full Unicode character set.\n"
	"\n"
	"-v     --version        Show version information.\n"
	"\n"
	"-V     --vernum         Show version as one number.\n"
	"\n"
	"-w     --wide-chars     Create a parser that supports wide chars (UCS-2). This\n"
	"                        implies -s and cannot be used together with -e switch.\n"
	"\n"
	"-1     --single-pass    Force single pass generation, this cannot be combined\n"
	"                        with -f and disables YYMAXFILL generation prior to last\n"
	"                        re2c block.\n"
	"\n"
	"--no-generation-date    Suppress date output in the generated output so that it\n"
	"                        only shows the re2c version.\n"
	;
}

} // end namespace re2c

using namespace re2c;

int main(int argc, char *argv[])
{
	int c;
	const char *sourceFileName = 0;
	const char *outputFileName = 0;

	if (argc == 1)
	{
		usage();
		return 2;
	}

	while ((c = mbo_getopt(argc, argv, OPTIONS, &opt_arg, &opt_ind, 0)) != -1)
	{
		switch (c)
		{

			case 'b':
			bFlag = true;
			sFlag = true;
			break;

			case 'e':
			xlat = asc2ebc;
			talx = ebc2asc;
			eFlag = true;
			break;

			case 'd':
			dFlag = true;
			break;

			case 'f':
			fFlag = true;
			break;

			case 'g':
			gFlag = true;
			bFlag = true;
			sFlag = true;
			break;

			case 'i':
			iFlag = true;
			break;

			case 'o':
			outputFileName = opt_arg;
			break;

			case 's':
			sFlag = true;
			break;
			
			case '1':
			bSinglePass = true;
			break;

			case 'v':
			cout << "re2c " << PACKAGE_VERSION << "\n";
			return 2;

			case 'V': {
				string vernum(PACKAGE_VERSION);

				if (vernum[1] == '.')
				{
					vernum.insert(0, "0");
				}
				vernum.erase(2, 1);
				if (vernum[3] == '.')
				{
					vernum.insert(2, "0");
				}
				vernum.erase(4, 1);
				if (vernum.length() < 6 || vernum[5] < '0' || vernum[5] > '9')
				{
					vernum.insert(4, "0");
				}
				vernum.resize(6);
				cout << vernum << endl;
				return 2;
			}
			
			case 'w':
			nRealChars = (1<<16); /* 0x10000 */
			sFlag = true;
			wFlag = true;
			break;

			case 'u':
			nRealChars = 0x110000; /* 17 times w-Flag */
			sFlag = true;
			uFlag = true;
			break;

			case 'h':
			case '?':
			default:
			usage();
			return 2;

			case 10:
			bNoGenerationDate = true;
			break;
		}
	}

	if ((bFlag || fFlag) && bSinglePass) {
		std::cerr << "re2c: error: Cannot combine -1 and -b or -f switch\n";
		return 1;
	}

	if (wFlag && eFlag)
	{
		std::cerr << "re2c: error: Cannot combine -e with -w or -u switch\n";
		return 2;
	}
	if (wFlag && uFlag)
	{
		std::cerr << "re2c: error: Cannot combine -u with -w switch\n";
		return 2;
	}

	if (uFlag)
	{
		wFlag = true;
	}
	
	if (argc == opt_ind + 1)
	{
		sourceFileName = argv[opt_ind];
	}
	else
	{
		usage();
		return 2;
	}

	// set up the source stream
	re2c::ifstream_lc source;

	if (sourceFileName[0] == '-' && sourceFileName[1] == '\0')
	{
		if (fFlag)
		{
			std::cerr << "re2c: error: multiple /*!re2c stdin is not acceptable when -f is specified\n";
			return 1;
		}
		sourceFileName = "<stdin>";
		source.open(stdin);
	}
	else if (!source.open(sourceFileName).is_open())
	{
		cerr << "re2c: error: cannot open " << sourceFileName << "\n";
		return 1;
	}

	// set up the output stream
	re2c::ofstream_lc output;

	if (outputFileName == 0 || (sourceFileName[0] == '-' && sourceFileName[1] == '\0'))
	{
		outputFileName = "<stdout>";
		output.open(stdout);
	}
	else if (!output.open(outputFileName).is_open())
	{
		cerr << "re2c: error: cannot open " << outputFileName << "\n";
		return 1;
	}
	Scanner scanner(sourceFileName, source, output);
	sourceFileInfo = file_info(sourceFileName, &scanner);
	outputFileInfo = file_info(outputFileName, &output);

	if (!bSinglePass)
	{
		bUsedYYMarker = false;

		re2c::ifstream_lc null_source;
		
		if (!null_source.open(sourceFileName).is_open())
		{
			cerr << "re2c: error: cannot re-open " << sourceFileName << "\n";
			return 1;
		}

		null_stream  null_dev;
		Scanner null_scanner(sourceFileName, null_source, null_dev);
		parse(null_scanner, null_dev);
		next_label = 0;
		next_fill_index = 0;
		bWroteGetState = false;
		bUsedYYMaxFill = false;
		bFirstPass = false;
	}

	bLastPass = true;
	parse(scanner, output);
	return 0;
}
