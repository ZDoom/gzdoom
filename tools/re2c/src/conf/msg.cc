#include <stdarg.h>
#include <stdio.h>
#include <string>

#if defined(_MSC_VER) && _MSC_VER < 1500
#include "config.msc.h"
#else
#include "config.h"
#endif
#include "src/conf/msg.h"

namespace re2c {

void error (const char * fmt, ...)
{
	fprintf (stderr, "re2c: error: ");

	va_list args;
	va_start (args, fmt);
	vfprintf (stderr, fmt, args);
	va_end (args);

	fprintf (stderr, "\n");
}

void error_encoding ()
{
	error ("only one of switches -e, -w, -x, -u and -8 must be set");
}

void error_arg (const char * option)
{
	error ("expected argument to option %s", option);
}

void warning_start (uint32_t line, bool error)
{
	static const char * msg = error ? "error" : "warning";
	fprintf (stderr, "re2c: %s: line %u: ", msg, line);
}

void warning_end (const char * type, bool error)
{
	if (type != NULL)
	{
		const char * prefix = error ? "error-" : "";
		fprintf (stderr, " [-W%s%s]", prefix, type);
	}
	fprintf (stderr, "\n");
}

void warning (const char * type, uint32_t line, bool error, const char * fmt, ...)
{
	warning_start (line, error);

	va_list args;
	va_start (args, fmt);
	vfprintf (stderr, fmt, args);
	va_end (args);

	warning_end (type, error);
}

void usage ()
{
	fprintf (stderr,
	"usage: re2c [-bcdDefFghirsuvVwx18] [-o of] [-t th] file\n"
	"\n"
	"-? -h  --help           Display this info.\n"
	"\n"
	"-b     --bit-vectors    Implies -s. Use bit vectors as well in the attempt to\n"
	"                        coax better code out of the compiler. Most useful for\n"
	"                        specifications with more than a few keywords (e.g. for\n"
	"                        most programming languages).\n"
	"\n"
	"-c     --conditions     Require start conditions.\n"
	"\n"
	"-d     --debug-output   Creates a parser that dumps information during\n"
	"                        about the current position and in which state the\n"
	"                        parser is.\n"
	"\n"
	"-D     --emit-dot       Emit a Graphviz dot view of the DFA graph\n"
	"\n"
	"-e     --ecb            Generate a parser that supports EBCDIC. The generated code\n"
	"                        can deal with any character up to 0xFF. In this mode re2c\n"
	"                        assumes that input character size is 1 byte. This switch is\n"
	"                        incompatible with -w, -u, -x and -8\n"
	"\n"
	"-f     --storable-state Generate a scanner that supports storable states.\n"
	"\n"
	"-F     --flex-syntax    Partial support for flex syntax.\n"
	"\n"
	"-g     --computed-gotos Implies -b. Generate computed goto code (only useable\n"
	"                        with gcc).\n"
	"\n"
	"-i     --no-debug-info  Do not generate '#line' info (useful for versioning).\n"
	"\n"
	"-o of  --output=of      Specify the output file (of) instead of stdout\n"
	"\n"
	"-r     --reusable       Allow reuse of scanner definitions.\n"
	"\n"
	"-s     --nested-ifs     Generate nested ifs for some switches. Many compilers\n"
	"                        need this assist to generate better code.\n"
	"\n"
	"-t th  --type-header=th Generate a type header file (th) with type definitions.\n"
	"\n"
	"-u     --unicode        Generate a parser that supports UTF-32. The generated code\n"
	"                        can deal with any valid Unicode character up to 0x10FFFF.\n"
	"                        In this mode re2c assumes that input character size is 4 bytes.\n"
	"                        This switch is incompatible with -e, -w, -x and -8. It implies -s.\n"
	"\n"
	"-v     --version        Show version information.\n"
	"\n"
	"-V     --vernum         Show version as one number.\n"
	"\n"
	"-w     --wide-chars     Generate a parser that supports UCS-2. The generated code can\n"
	"                        deal with any valid Unicode character up to 0xFFFF. In this mode\n"
	"                        re2c assumes that input character size is 2 bytes. This switch is\n"
	"                        incompatible with -e, -x, -u and -8. It implies -s."
	"\n"
	"-x     --utf-16         Generate a parser that supports UTF-16. The generated code can\n"
	"                        deal with any valid Unicode character up to 0x10FFFF. In this mode\n"
	"                        re2c assumes that input character size is 2 bytes. This switch is\n"
	"                        incompatible with -e, -w, -u and -8. It implies -s."
	"\n"
	"-8     --utf-8          Generate a parser that supports UTF-8. The generated code can\n"
	"                        deal with any valid Unicode character up to 0x10FFFF. In this mode\n"
	"                        re2c assumes that input character size is 1 byte. This switch is\n"
	"                        incompatible with -e, -w, -x and -u."
	"\n"
	"--no-generation-date    Suppress date output in the generated file.\n"
	"\n"
	"--no-version            Suppress version output in the generated file.\n"
	"\n"
	"--case-insensitive      All strings are case insensitive, so all \"-expressions\n"
	"                        are treated in the same way '-expressions are.\n"
	"\n"
	"--case-inverted         Invert the meaning of single and double quoted strings.\n"
	"                        With this switch single quotes are case sensitive and\n"
	"                        double quotes are case insensitive.\n"
	"\n"
	"--encoding-policy ep    Specify what re2c should do when given bad code unit.\n"
	"                        ep can be one of the following: fail, substitute, ignore.\n"
	"\n"
	"--input i               Specify re2c input API.\n"
	"                        i can be one of the following: default, custom.\n"
	"\n"
	"--skeleton              Instead of embedding re2c-generated code into C/C++ source,\n"
	"                        generate a self-contained program for the same DFA.\n"
	"                        Most useful for correctness and performance testing.\n"
	"\n"
	"--empty-class policy    What to do if user inputs empty character class. policy can be\n"
	"                        one of the following: 'match-empty' (match empty input, default),\n"
	"                        'match-none' (fail to match on any input), 'error' (compilation\n"
	"                        error). Note that there are various ways to construct empty class,\n"
	"                        e.g: [], [^\\x00-\\xFF], [\\x00-\\xFF]\\[\\x00-\\xFF].\n"
	"\n"
	"--dfa-minimization <table | moore>\n"
	"                        Internal algorithm used by re2c to minimize DFA (defaults to\n"
	"                        'moore'). Both table filling and Moore's algorithms should\n"
	"                        produce identical DFA (up to states relabelling). Table filling\n"
	"                        algorithm is much simpler and slower; it serves as a reference\n"
	"                        implementation.\n"
	"\n"
	"-1     --single-pass    Deprecated and does nothing (single pass is by default now).\n"
	"\n"
	"-W                      Turn on all warnings.\n"
	"\n"
	"-Werror                 Turn warnings into errors. Note that this option along doesn't\n"
	"                        turn on any warnings, it only affects those warnings that have\n"
	"                        been turned on so far or will be turned on later.\n"
	"\n"
	"-W<warning>             Turn on individual warning.\n"
	"\n"
	"-Wno-<warning>          Turn off individual warning.\n"
	"\n"
	"-Werror-<warning>       Turn on individual warning and treat it as error (this implies\n"
	"                        '-W<warning>').\n"
	"\n"
	"-Wno-error-<warning>    Don't treat this particular warning as error. This doesn't turn\n"
	"                        off the warning itself.\n"
	"\n"
	"Warnings:\n"
	"\n"
	"-Wcondition-order       Warn if the generated program makes implicit assumptions about\n"
	"                        condition numbering. One should use either '-t, --type-header'\n"
	"                        option or '/*!types:re2c*/' directive to generate mapping of\n"
	"                        condition names to numbers and use autogenerated condition names.\n"
	"\n"
	"-Wempty-character-class Warn if regular expression contains empty character class. From\n"
	"                        the rational point of view trying to match empty character class\n"
	"                        makes no sense: it should always fail. However, for backwards\n"
	"                        compatibility reasons re2c allows empty character class and treats\n"
	"                        it as empty string. Use '--empty-class' option to change default\n"
	"                        behaviour.\n"
	"\n"
	"-Wmatch-empty-string    Warn if regular expression in a rule is nullable (matches empty\n"
	"                        string). If DFA runs in a loop and empty match is unintentional\n"
	"                        (input position in not advanced manually), lexer may get stuck\n"
	"                        in eternal loop.\n"
	"\n"
	"-Wswapped-range         Warn if range lower bound is greater that upper bound. Default\n"
	"                        re2c behaviour is to silently swap range bounds.\n"
	"\n"
	"-Wundefined-control-flow\n"
	"                        Warn if some input strings cause undefined control flow in lexer\n"
	"                        (the faulty patterns are reported). This is the most dangerous\n"
	"                        and common mistake. It can be easily fixed by adding default rule\n"
	"                        '*' (this rule has the lowest priority, matches any code unit\n"
	"                        and consumes exactly one code unit).\n"
	"\n"
	"-Wuseless-escape        Warn if a symbol is escaped when it shouldn't be. By default re2c\n"
	"                        silently ignores escape, but this may as well indicate a typo\n"
	"                        or an error in escape sequence.\n"
	"\n"
	);
}

void vernum ()
{
	std::string vernum (PACKAGE_VERSION);
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
	vernum.resize(6, '0');

	printf ("%s\n", vernum.c_str ());
}

void version ()
{
	printf ("re2c %s\n", PACKAGE_VERSION);
}

std::string incond (const std::string & cond)
{
	std::string s;
	if (!cond.empty ())
	{
		s += "in condition '";
		s += cond;
		s += "' ";
	}
	return s;
}

} // namespace re2c
