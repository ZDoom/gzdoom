/* $Id: main.cc,v 1.10 2004/05/13 03:47:52 nuffer Exp $ */
#ifdef HAVE_CONFIG_H
#include "config.h"
#elif _WIN32
#include "configwin.h"
#endif

#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <string.h>

#include "globals.h"
#include "parser.h"
#include "dfa.h"
#include "mbo_getopt.h"

char *fileName = 0;
char *outputFileName = 0;
bool sFlag = false;
bool bFlag = false;
unsigned int oline = 1;

using namespace std;

static char *opt_arg = NULL;
static int opt_ind = 1;

static const mbo_opt_struct OPTIONS[] = {
	{'?', 0, "help"},
	{'b', 0, "bit-vectors"},
	{'e', 0, "ecb"},
	{'h', 0, "help"},
	{'s', 0, "nested-ifs"},
	{'o', 1, "output"},
	{'v', 0, "version"}
};

static void usage()
{
	cerr << "usage: re2c [-esbvh] file\n"
		"\n"
		"-? -h   --help          Display this info.\n"
		"\n"
		"-b      --bit-vectors   Implies -s. Use bit vectors as well in the attempt to\n"
		"                        coax better code out of the compiler. Most useful for\n"
		"                        specifications with more than a few keywords (e.g. for\n"
		"                        most programming languages).\n"
		"\n"
		"-e      --ecb           Cross-compile from an ASCII platform to\n"
		"                        an EBCDIC one.\n"
		"\n"
		"-s      --nested-ifs    Generate nested ifs for some switches. Many compilers\n"
		"                        need this assist to generate better code.\n"
		"\n"
		"-o      --output=output Specify the output file instead of stdout\n"
		"\n"
		"-v      --version       Show version information.\n";
}

int main(int argc, char *argv[])
{
	int c;
	fileName = NULL;

	if (argc == 1) {
		usage();
		return 2;
	}

	while ((c = mbo_getopt(argc, argv, OPTIONS, &opt_arg, &opt_ind, 0))!=-1) {
		switch (c) {
			case 'b':
				sFlag = true;
				bFlag = true;
				break;
			case 'e':
				xlat = asc2ebc;
				talx = ebc2asc;
				break;
			case 's':
				sFlag = true;
				break;
			case 'o':
				outputFileName = opt_arg;
				break;
			case 'v':
				cerr << "re2c " << PACKAGE_VERSION << "\n";
				return 2;
			case 'h':
			case '?':
			default:
				usage();
				return 2;
		}
	}

	if (argc == opt_ind + 1)
	{
		fileName = argv[opt_ind];
	}
	else
	{
		usage();
		return 2;
	}

	// set up the input stream
	istream* input = 0;
	ifstream inputFile;
	if (fileName[0] == '-' && fileName[1] == '\0')
	{
		fileName = "<stdin>";
		input = &cin;
	}
	else
	{
		inputFile.open(fileName);
		if (!inputFile)
		{
			cerr << "can't open " << fileName << "\n";
			return 1;
		}
		input = &inputFile;
	}

	// set up the output stream
	ostream* output = 0;
	ofstream outputFile;
	if (outputFileName == 0 || (fileName[0] == '-' && fileName[1] == '\0'))
	{
		outputFileName = "<stdout>";
		output = &cout;
	}
	else
	{
		outputFile.open(outputFileName);
		if (!outputFile)
		{
			cerr << "can't open " << outputFileName << "\n";
			return 1;
		}
		output = &outputFile;
	}

	parse(*input, *output);
	return 0;

}
