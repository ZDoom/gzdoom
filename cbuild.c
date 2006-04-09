#define foobarbaz /*
# A shell script within a source file. Ugly, but it works...

TRGT=${0/.c/.exe}
for i in $@ ; do
   if [ "$i" = "--make-compiled" ] ; then
      echo "Compiling '$TRGT', please wait..."
      gcc -W -Wall -O2 -o $TRGT $0
      exit $?
   fi
done

gcc -W -Wall -Werror -o /tmp/$TRGT "$0" && { "/tmp/$TRGT" $@ ; RET=$? ; rm -f "/tmp/$TRGT" ; exit $RET ; }
exit $?
*/

/* CBuild written by Chris Robinson, copyright 2005-2006
 *
 * Notice of Terms of Use
 *
 * CBuild is provided as-is for personal use. You may use and redistribute it
 * in any manner, with the following terms and conditions:
 *
 * * You MAY NOT sell it, or otherwise redistribute it for profit, unless it is
 *   bundled with a seperate commercial product and is used to build said
 *   product.
 * * You MAY modify it without recompense or notification to the author, as
 *   long as it is clearly marked as a derivitive work.
 * * You MAY NOT claim it as your own sole work.
 * * You MAY NOT remove or modify this notice.
 */

/* Despite what some people may think, no, the name CBuild did not come from my
 * first name. I got the name from the fact that it uses pure C, and only C. ;)
 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <setjmp.h>

#ifdef __unix__
#include <unistd.h>
#endif
#if defined(__unix__) || defined(__GNUC__)
#include <sys/param.h>
#include <dirent.h>
#endif

#ifdef _WIN32

#include <io.h>

#ifdef _MSC_VER
// MSVC Sucks
#define strcasecmp stricmp
#define strncasecmp strnicmp
#define snprintf _snprintf

#define PATH_MAX _MAX_PATH
#define lstat slat 
#define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR) 
#define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG) 
#define S_IRUSR _S_IREAD 
#define S_IWUSR _S_IWRITE 
#define S_IXUSR _S_IEXEC 
#define S_IRWXU (_S_IREAD | _S_IWRITE | _S_IEXEC)

struct dirent {
	char *d_name;
};

typedef struct {
	long                handle; /* -1 for failed rewind */
	struct _finddata_t  info;
	struct dirent       result; /* d_name null iff first time */
	char                *name;  /* null-terminated char string */
} DIR;

DIR *opendir(const char *name)
{
	DIR *dir = 0;

	if(name && name[0])
	{
		/* search pattern must end with suitable wildcard */
		size_t base_length = strlen(name);
		const char *all = strchr("/\\", name[base_length - 1]) ? "*" : "/*";

		if((dir=(DIR*)malloc(sizeof(*dir))) != 0 &&
		   (dir->name=(char*)malloc(base_length+strlen(all)+1)) != 0)
		{
			strcat(strcpy(dir->name, name), all);

			if((dir->handle = (long) _findfirst(dir->name, &dir->info)) != -1)
			{
				dir->result.d_name = 0;
			}
			else /* rollback */
			{
				free(dir->name);
				free(dir);
				dir = NULL;
			}
		}
		else /* rollback */
		{
			free(dir);
			dir   = NULL;
			errno = ENOMEM;
		}
	}
	else
		errno = EINVAL;

	return dir;
}

int closedir(DIR *dir)
{
	int result = -1;

	if(dir)
	{
		if(dir->handle != -1)
			result = _findclose(dir->handle);

		free(dir->name);
		free(dir);
	}

	if(result == -1) /* map all errors to EBADF */
		errno = EBADF;

	return result;
}

struct dirent *readdir(DIR *dir)
{
	struct dirent *result = 0;

	if(dir && dir->handle != -1)
	{
		if(!dir->result.d_name || _findnext(dir->handle, &dir->info) != -1)
		{
			result         = &dir->result;
			result->d_name = dir->info.name;
		}
	}
	else
		errno = EBADF;

	return result;
}

void rewinddir(DIR *dir)
{
	if(dir && dir->handle != -1)
	{
		_findclose(dir->handle);
		dir->handle = (long)_findfirst(dir->name, &dir->info);
		dir->result.d_name = 0;
	}
	else
		errno = EBADF;
}

#endif /* _MSC_VER */


static int setenv(const char *env, const char *val, int overwrite)
{
	static char buf[64*1024];
	if(!overwrite && getenv(env))
		return 0;

	if(val && *val)
		snprintf(buf, sizeof(buf), "%s=%s", env, val);
	else
		snprintf(buf, sizeof(buf), "%s", env);
	return _putenv(buf);
}

static int unsetenv(const char *env)
{
	return setenv(env, "", 1);
}

#endif /* Win32 */

#define INVOKE_BKP_SIZE 16
struct {
	FILE *f;
	char *fname;
	char *bkp_lbuf;
	char *bkp_nextline;
	int bkp_line;
	int bkp_did_else;
	int bkp_did_cmds;
	int bkp_do_level;
} invoke_backup[INVOKE_BKP_SIZE];

static char **argv;
static int argc;
static FILE *infile;

static FILE *f;
static char *fname;
static struct stat statbuf;
static char linebuf[64*1024];
static char buffer[64*1024];
static char *loaded_files;
static char *sources;
static char obj[PATH_MAX];
#define SRC_PATH_SIZE 32
static char *src_paths[SRC_PATH_SIZE];

static int curr_line = 0;
static char *nextline;

static jmp_buf jmpbuf;


static int libify_name(char *buf, size_t buflen, char *name);


/* getvar: Safely gets an environment variable, returning an empty string
 * instead of NULL
 */
static const char *getvar(const char *env)
{
	const char *var = getenv(env);
	return (var?var:"");
}

/* find_src: Attempts to find the named sourcefile by searching the paths
 * listed in src_paths. It returns the passed string if the file exists as-is,
 * or if it couldn't be found.
 */
static char *find_src(char *src)
{
	static char buf[PATH_MAX];
	struct stat statbuf;
	int i;

	if(stat(src, &statbuf) == 0 || !src_paths[0])
		return src;

	for(i = 0;src_paths[i] && i < SRC_PATH_SIZE;++i)
	{
		snprintf(buf, sizeof(buf), "%s/%s", src_paths[i], src);
		if(stat(buf, &statbuf) == 0)
			return buf;
	}
	return src;
}


/* grab_word: Gets a word starting at the string pointed to by 'str'. If
 * the word begins with a ' or " character, everything until that same
 * character will be considered part of the word. Otherwise, the word ends at
 * the first encountered whitespace. Returns the beginning of the next word,
 * or the end of the string.
 */
static char *grab_word(char **str, char *term)
{
	char *end;
	char c;

	c = **str;
	if(!c)
	{
		if(term)
			*term = 0;
		return *str;
	}

	if(c == '\'' || c == '"')
	{
		++(*str);
		end = *str;
		while(*end && *end != c)
			++end;
	}
	else
	{
		end = *str;
		while(!isspace(*end) && *end)
			++end;
	}

	if(term)
		*term = *end;

	if(*end) *(end++) = 0;
	while(isspace(*end) && *end)
		++end;

	return end;
}

#define BUF_SIZE (64*1024)
/*
 */
static char *expand_string(char *str, const char *stp, size_t len,
                           int fillmore)
{
	char *buf, *ptr, *last_pos;
	int in_quotes = 0;
	int use_hard_quotes = 0;
	char *end;
	int i;

	if(!(*str) && !fillmore)
		return str;

	buf = malloc(BUF_SIZE);
	if(!buf)
	{
		strcpy(linebuf, "exit -1\n");
		longjmp(jmpbuf, 1);
	}

	last_pos = str;
	do {
		ptr = last_pos;
		while(1)
		{
			if(!(*ptr))
			{
				if(fillmore)
				{
					if(fgets(ptr, len+str-ptr, f) == NULL)
					{
						free(buf);
						fprintf(stderr, "\n\n!!! Premature EOF !!!\n%s\n",
						        (in_quotes?"!!! Unterminated string !!!\n":
						                   ""));
						strcpy(linebuf, "exit -1\n");
						longjmp(jmpbuf, 1);
					}
					++curr_line;
				}
			}

			if(!in_quotes)
			{
				i = 0;
				do {
					if((stp[i] == ' ' &&  isspace(*ptr)) ||
					   (stp[i] == '^' && !isalpha(*ptr)) ||
					   *ptr == stp[i])
					{
						free(buf);
						return ptr;
					}
				} while(stp[i++]);
			}


			if(*ptr == '$')
			{
				if(in_quotes != '\'')
					break;
			}
			else if(*ptr == '#')
			{
				if(!in_quotes)
				{
					char *next = strchr(ptr, '\n');
					if(!next) next = "";
					memmove(ptr, next, strlen(next)+1);
					continue;
				}
			}
			else if(*ptr == '&')
			{
				if(in_quotes != '\'')
				{
					unsigned long val = '?';

					end = NULL;

					if(ptr[1] != '#')
					{
						static struct {
							char *name;
							unsigned long val;
						} val_tab[] = {
							{ "Aacute", 0x00C1 }, { "aacute", 0x00E1 },
							{ "Acirc", 0x00C2 }, { "acirc", 0x00E2 },
							{ "acute", 0x00B4 }, { "AElig", 0x00C6 },
							{ "aelig", 0x00E6 }, { "Agrave", 0x00C0 },
							{ "agrave", 0x00E0 }, { "alefsym", 0x2135 },
							{ "Alpha", 0x0391 }, { "alpha", 0x03B1 },
							{ "amp", 0x0026 }, { "and", 0x2227 },
							{ "ang", 0x2220 }, { "Aring", 0x00C5 },
							{ "aring", 0x00E5 }, { "asymp", 0x2248 },
							{ "Atilde", 0x00C3 }, { "atilde", 0x00E3 },
							{ "Auml", 0x00C4 }, { "auml", 0x00E4 },
							{ "bdquo", 0x201E }, { "Beta", 0x0392 },
							{ "beta", 0x03B2 }, { "brvbar", 0x00A6 },
							{ "bull", 0x2022 }, { "cap", 0x2229 },
							{ "Ccedil", 0x00C7 }, { "ccedil", 0x00E7 },
							{ "cedil", 0x00B8 }, { "cent", 0x00A2 },
							{ "Chi", 0x03A7 }, { "chi", 0x03C7 },
							{ "circ", 0x02C6 }, { "clubs", 0x2663 },
							{ "cong", 0x2245 }, { "copy", 0x00A9 },
							{ "crarr", 0x21B5 }, { "cup", 0x222A },
							{ "curren", 0x00A4 }, { "Dagger", 0x2021 },
							{ "dagger", 0x2020 }, { "dArr", 0x21D3 },
							{ "darr", 0x2193 }, { "deg", 0x00B0 },
							{ "Delta", 0x0394 }, { "delta", 0x03B4 },
							{ "diams", 0x2666 }, { "divide", 0x00F7 },
							{ "Eacute", 0x00C9 }, { "eacute", 0x00E9 },
							{ "Ecirc", 0x00CA }, { "ecirc", 0x00EA },
							{ "Egrave", 0x00C8 }, { "egrave", 0x00E8 },
							{ "empty", 0x2205 }, { "emsp", 0x2003 },
							{ "ensp", 0x2002 }, { "Epsilon", 0x0395 },
							{ "epsilon", 0x03B5 }, { "equiv", 0x2261 },
							{ "Eta", 0x0397 }, { "eta", 0x03B7 },
							{ "ETH", 0x00D0 }, { "eth", 0x00F0 },
							{ "Euml", 0x00CB }, { "euml", 0x00EB },
							{ "euro", 0x20AC }, { "exist", 0x2203 },
							{ "fnof", 0x0192 }, { "forall", 0x2200 },
							{ "frac12", 0x00BD }, { "frac14", 0x00BC },
							{ "frac34", 0x00BE }, { "frasl", 0x2044 },
							{ "Gamma", 0x0393 }, { "gamma", 0x03B3 },
							{ "ge", 0x2265 }, { "gt", 0x003E },
							{ "hArr", 0x21D4 }, { "harr", 0x2194 },
							{ "hearts", 0x2665 }, { "hellip", 0x2026 },
							{ "Iacute", 0x00CD }, { "iacute", 0x00ED },
							{ "Icirc", 0x00CE }, { "icirc", 0x00EE },
							{ "iexcl", 0x00A1 }, { "Igrave", 0x00CC },
							{ "igrave", 0x00EC }, { "image", 0x2111 },
							{ "infin", 0x221E }, { "int", 0x222B },
							{ "Iota", 0x0399 }, { "iota", 0x03B9 },
							{ "iquest", 0x00BF }, { "isin", 0x2208 },
							{ "Iuml", 0x00CF }, { "iuml", 0x00EF },
							{ "Kappa", 0x039A }, { "kappa", 0x03BA },
							{ "Lambda", 0x039B }, { "lambda", 0x03BB },
							{ "lang", 0x2329 }, { "laquo", 0x00AB },
							{ "lArr", 0x21D0 }, { "larr", 0x2190 },
							{ "lceil", 0x2308 }, { "ldquo", 0x201C },
							{ "le", 0x2264 }, { "lfloor", 0x230A },
							{ "lowast", 0x2217 }, { "loz", 0x25CA },
							{ "lrm", 0x200E }, { "lsaquo", 0x2039 },
							{ "lsquo", 0x2018 }, { "lt", 0x003C },
							{ "macr", 0x00AF }, { "mdash", 0x2014 },
							{ "micro", 0x00B5 }, { "middot", 0x00B7 },
							{ "minus", 0x2212 }, { "Mu", 0x039C },
							{ "mu", 0x03BC }, { "nabla", 0x2207 },
							{ "nbsp", 0x00A0 }, { "ndash", 0x2013 },
							{ "ne", 0x2260 }, { "ni", 0x220B },
							{ "not", 0x00AC }, { "notin", 0x2209 },
							{ "nsub", 0x2284 }, { "Ntilde", 0x00D1 },
							{ "ntilde", 0x00F1 }, { "Nu", 0x039D },
							{ "nu", 0x03BD }, { "Oacute", 0x00D3 },
							{ "oacute", 0x00F3 }, { "Ocirc", 0x00D4 },
							{ "ocirc", 0x00F4 }, { "OElig", 0x0152 },
							{ "oelig", 0x0153 }, { "Ograve", 0x00D2 },
							{ "ograve", 0x00F2 }, { "oline", 0x203E },
							{ "Omega", 0x03A9 }, { "omega", 0x03C9 },
							{ "Omicron", 0x039F }, { "omicron", 0x03BF },
							{ "oplus", 0x2295 }, { "or", 0x2228 },
							{ "ordf", 0x00AA }, { "ordm", 0x00BA },
							{ "Oslash", 0x00D8 }, { "oslash", 0x00F8 },
							{ "Otilde", 0x00D5 }, { "otilde", 0x00F5 },
							{ "otimes", 0x2297 }, { "Ouml", 0x00D6 },
							{ "ouml", 0x00F6 }, { "para", 0x00B6 },
							{ "part", 0x2202 }, { "permil", 0x2030 },
							{ "perp", 0x22A5 }, { "Phi", 0x03A6 },
							{ "phi", 0x03C6 }, { "Pi", 0x03A0 },
							{ "pi", 0x03C0 }, { "piv", 0x03D6 },
							{ "plusmn", 0x00B1 }, { "pound", 0x00A3 },
							{ "Prime", 0x2033 }, { "prime", 0x2032 },
							{ "prod", 0x220F }, { "prop", 0x221D },
							{ "Psi", 0x03A8 }, { "psi", 0x03C8 },
							{ "quot", 0x0022 }, { "radic", 0x221A },
							{ "rang", 0x232A }, { "raquo", 0x00BB },
							{ "rArr", 0x21D2 }, { "rarr", 0x2192 },
							{ "rceil", 0x2309 }, { "rdquo", 0x201D },
							{ "real", 0x211C }, { "reg", 0x00AE },
							{ "rfloor", 0x230B }, { "Rho", 0x03A1 },
							{ "rho", 0x03C1 }, { "rlm", 0x200F },
							{ "rsaquo", 0x203A }, { "rsquo", 0x2019 },
							{ "sbquo", 0x201A }, { "Scaron", 0x0160 },
							{ "scaron", 0x0161 }, { "sdot", 0x22C5 },
							{ "sect", 0x00A7 }, { "shy", 0x00AD },
							{ "Sigma", 0x03A3 }, { "sigma", 0x03C3 },
							{ "sigmaf", 0x03C2 }, { "sim", 0x223C },
							{ "spades", 0x2660 }, { "sub", 0x2282 },
							{ "sube", 0x2286 }, { "sum", 0x2211 },
							{ "sup", 0x2283 }, { "sup1", 0x00B9 },
							{ "sup2", 0x00B2 }, { "sup3", 0x00B3 },
							{ "supe", 0x2287 }, { "szlig", 0x00DF },
							{ "Tau", 0x03A4 }, { "tau", 0x03C4 },
							{ "there4", 0x2234 }, { "Theta", 0x0398 },
							{ "theta", 0x03B8 }, { "thetasym", 0x03D1 },
							{ "thinsp", 0x2009 }, { "THORN", 0x00DE },
							{ "thorn", 0x00FE }, { "tilde", 0x02DC },
							{ "times", 0x00D7 }, { "trade", 0x2122 },
							{ "Uacute", 0x00DA }, { "uacute", 0x00FA },
							{ "uArr", 0x21D1 }, { "uarr", 0x2191 },
							{ "Ucirc", 0x00DB }, { "ucirc", 0x00FB },
							{ "Ugrave", 0x00D9 }, { "ugrave", 0x00F9 },
							{ "uml", 0x00A8 }, { "upsih", 0x03D2 },
							{ "Upsilon", 0x03A5 }, { "upsilon", 0x03C5 },
							{ "Uuml", 0x00DC }, { "uuml", 0x00FC },
							{ "weierp", 0x2118 }, { "Xi", 0x039E },
							{ "xi", 0x03BE }, { "Yacute", 0x00DD },
							{ "yacute", 0x00FD }, { "yen", 0x00A5 },
							{ "Yuml", 0x0178 }, { "yuml", 0x00FF },
							{ "Zeta", 0x0396 }, { "zeta", 0x03B6 },
							{ "zwj", 0x200D }, { "zwnj", 0x200C },
							{ NULL, '?' }
						};
						int i;

						end = expand_string(ptr+1, " ;", len+str-ptr-1,
						                    fillmore);
						if(*end == ';')
							*(end++) = 0;

						for(i = 0;val_tab[i].name;++i)
						{
							if(strncmp(val_tab[i].name, ptr+1, end-ptr-1) == 0)
								break;
						}
						val = val_tab[i].val;
					}
					else
					{
						val = strtoul(ptr+2, &end, 0);
						if(*end == ';')
							++end;
						if(end == ptr+2)
							val = '?';
					}

					if(val <= 0x7F)
					{
						if(val > 0)
							*(ptr++) = val;
					}
					else if(val <= 0x7FF)
					{
						*(ptr++) = 0xC0 | (val>>6);
						*(ptr++) = 0x80 | (val&0x3F);
					}
					else if(val <= 0xFFFF)
					{
						*(ptr++) = 0xE0 | (val>>12);
						*(ptr++) = 0x80 | ((val>>6)&0x3F);
						*(ptr++) = 0x80 | (val&0x3F);
					}
					else if(val <= 0x10FFFF)
					{
						*(ptr++) = 0xF0 | (val>>18);
						*(ptr++) = 0x80 | ((val>>12)&0x3F);
						*(ptr++) = 0x80 | ((val>>6)&0x3F);
						*(ptr++) = 0x80 | (val&0x3F);
					}

					if(end != ptr)
						memmove(ptr, end, strlen(end)+1);
					continue;
				}
			}
			else if(*ptr == '"' || *ptr == '\'')
			{
				if(!in_quotes || in_quotes == *ptr)
				{
					in_quotes = *ptr ^ in_quotes;
					memmove(ptr, ptr+1, strlen(ptr));
					continue;
				}
			}
			else if(*ptr == '\\')
				memmove(ptr, ptr+1, strlen(ptr));

			if(*ptr) ++ptr;
		}

		last_pos = ptr;
		*(ptr++) = 0;

		use_hard_quotes = 0;

		/* Run a special command, replacing the section */
		if(*ptr == '(')
		{
			char *next = NULL;
			char *opt;

			++ptr;

			if(*ptr == '*')
			{
				use_hard_quotes = 1;
				++ptr;
			}

			opt = expand_string(ptr, " )", len+str-ptr, fillmore);
			if(!(*opt) || !isspace(*opt))
			{
				*opt = 0;
				printf("\n\n!!! %s error, line %d !!!\n"
				       "Malformed '%s' sub-command!\n\n", fname,
				       curr_line, ptr);
				free(buf);
				strcpy(linebuf, "exit -1\n");
				longjmp(jmpbuf, 1);
			}
			*(opt++) = 0;
			while(isspace(*opt) && *opt)
				++opt;

			if(use_hard_quotes)
				i = snprintf(buf, BUF_SIZE, "%s'", str);
			else
				i = snprintf(buf, BUF_SIZE, "%s", str);

			/* Replaces the section with the specified command line
			 * option's value (in the format 'option=value') */
			if(strcasecmp(ptr, "getoptval") == 0)
			{
				const char *val = "";
				int optlen, idx;

				next = expand_string(opt, ")", len+str-opt, fillmore);
				if(*next) *(next++) = 0;

				optlen = strlen(opt);
				for(idx = 1;idx < argc;++idx)
				{
					if(strncasecmp(opt, argv[idx], optlen) == 0)
					{
						if(argv[idx][optlen] == '=')
							val = argv[idx]+optlen+1;
						else if(argv[idx][optlen] == 0)
							val = argv[idx+1];
						else
							continue;
						break;
					}
				}
				snprintf(buf+i, BUF_SIZE-i, "%s", val);
			}

			else if(strcasecmp(ptr, "word") == 0)
			{
				unsigned long val;
				char *sep = expand_string(opt, ",)", len+str-opt, fillmore);
				if(*sep != ',')
				{
					printf("\n\n!!! %s error, line %d !!!\n"
					       "Malformed '%s' sub-command!\n\n", fname,
					       curr_line, ptr);
					free(buf);
					strcpy(linebuf, "exit -1\n");
					longjmp(jmpbuf, 1);
				}
				*(sep++) = 0;
				val = strtoul(opt, NULL, 0);

				opt = sep;
				while(isspace(*opt))
					++opt;

				while(1)
				{
					char *next_word = expand_string(opt, " )", len+str-opt,
					                                fillmore);
					--val;
					if(isspace(*next_word) && *next_word)
					{
						*(next_word++) = 0;
						while(isspace(*next_word) && *next_word)
							++next_word;
					}
					else
					{
						next = next_word;
						if(*next) *(next++) = 0;
						if(val == 0)
							snprintf(buf+i, BUF_SIZE-i, "%s", opt);
						break;
					}

					if(val == 0)
					{
						snprintf(buf+i, BUF_SIZE-i, "%s", opt);
						opt = next_word;
						next = expand_string(opt, ")", len+str-opt, fillmore);
						if(*next) *(next++) = 0;
						break;
					}
					opt = next_word;
				}
			}

/*			else if(strcasecmp(ptr, "popfront") == 0)
			{
				char *front_word;
				char *varstr;

				next = expand_string(opt, ")", len+str-opt, fillmore);
				if(*next) *(next++) = 0;

				varstr = malloc(65536);
				if(!varstr)
				{
					free(buf);
					exit(-1);
				}
				strncpy(varstr, getvar(opt), 65536);
				front_word = varstr;
				while(*varstr && isspace(*varstr))
					++varstr;
				if(front_word != varstr)
					memmove(front_word, varstr, strlen(varstr)+1);

				varstr = expand_string(front_word, " ", 65536, 0);
				if(*varstr) *(varstr++) = 0;
				while(*varstr && isspace(*varstr))
					++varstr;

				snprintf(buf+i, BUF_SIZE-i, "%s", front_word);
				setenv(opt, varstr, 1);

				free(front_word);
			}*/

			else if(strcasecmp(ptr, "add")==0 || strcasecmp(ptr, "sub") == 0 ||
			        strcasecmp(ptr, "mult")==0 || strcasecmp(ptr, "div") == 0)
			{
				long val1, val2;
				char *sep = expand_string(opt, ",)", len+str-opt, fillmore);
				if(*sep != ',')
				{
					printf("\n\n!!! %s error, line %d !!!\n"
					       "Malformed '%s' sub-command!\n\n", fname,
					       curr_line, ptr);
					free(buf);
					strcpy(linebuf, "exit -1\n");
					longjmp(jmpbuf, 1);
				}
				*(sep++) = 0;

				next = expand_string(sep, ")", len+str-sep, fillmore);
				if(*next) *(next++) = 0;

				val1 = atoi(opt);
				val2 = atoi(sep);

				if(strcasecmp(ptr, "add") == 0)
					val1 += val2;
				else if(strcasecmp(ptr, "sub") == 0)
					val1 -= val2;
				else if(strcasecmp(ptr, "mult") == 0)
					val1 *= val2;
				else if(strcasecmp(ptr, "div") == 0)
				{
					if(!val2)
					{
						printf("\n\n!!! %s error, line %d !!!\n"
						       "Divide-by-0 attempted!\n\n", fname,
						       curr_line);
						free(buf);
						strcpy(linebuf, "exit -1\n");
						longjmp(jmpbuf, 1);
					}
					val1 /= val2;
				}

				snprintf(buf+i, BUF_SIZE-i, "%ld", val1);
			}

			/* Returns a library-style name from the specified
			 * filename */
			else if(strcasecmp(ptr, "libname") == 0)
			{
				next = expand_string(opt, ")", len+str-opt, fillmore);
				if(*next) *(next++) = 0;

				libify_name(obj, sizeof(obj), opt);
				snprintf(buf+i, BUF_SIZE-i, "%s", obj);
			}

			/* Returns the full filename for the specified source file by
			 * searching src_path */
			else if(strcasecmp(ptr, "findsrc") == 0)
			{
				int inc = i;
				int loop = 1;

				while(loop)
				{
					char *next_word = expand_string(opt, " )", len+str-opt,
					                                fillmore);
					if(isspace(*next_word) && *next_word)
					{
						*(next_word++) = 0;
						while(isspace(*next_word) && *next_word)
							++next_word;
					}
					else
					{
						if(*next_word) *(next_word++) = 0;
						loop = 0;
					}

					inc += snprintf(buf+inc, BUF_SIZE-inc, "%s ",
					                find_src(opt));

					opt = next_word;
				}
				if(inc > i) buf[inc-1] = 0;
				next = opt;
			}

			/* Returns the string, lower-cased */
			else if(strcasecmp(ptr, "tolower") == 0)
			{
				int inc = i;

				next = expand_string(opt, ")", len+str-opt, fillmore);
				if(*next) *(next++) = 0;

				snprintf(buf+i, BUF_SIZE-i, "%s", opt);
				while(buf[inc])
				{
					buf[inc] = tolower(buf[inc]);
					++inc;
				}
			}

			/* Returns the string, upper-cased */
			else if(strcasecmp(ptr, "toupper") == 0)
			{
				int inc = i;

				next = expand_string(opt, ")", len+str-opt, fillmore);
				if(*next) *(next++) = 0;

				snprintf(buf+i, BUF_SIZE-i, "%s", opt);
				while(buf[inc])
				{
					buf[inc] = toupper(buf[inc]);
					++inc;
				}
			}


			else if(strcasecmp("ifeq", ptr) == 0)
			{
				char *var2;
				char *val;

				var2 = expand_string(opt, ",)", len+str-opt, fillmore);
				if(*var2 != ',')
				{
					printf("\n\n!!! %s error, line %d !!!\n"
					       "Malformed 'ifeq' sub-command!\n\n",
					       fname, curr_line);
					free(buf);
					strcpy(linebuf, "exit -1\n");
					longjmp(jmpbuf, 1);
				}
				*(var2++) = 0;
				val = expand_string(var2, ",)", len+str-var2, fillmore);
				if(*val != ',')
				{
					printf("\n\n!!! %s error, line %d !!!\n"
					       "Malformed 'ifeq' sub-command!\n\n",
					       fname, curr_line);
					free(buf);
					strcpy(linebuf, "exit -1\n");
					longjmp(jmpbuf, 1);
				}
				*(val++) = 0;
				if(strcmp(opt, var2) == 0)
				{
					char *sep = expand_string(val, ",)", len+str-val,
					                          fillmore);
					if(*sep) *(sep++) = 0;
					next = expand_string(sep, ")", len+str-sep, fillmore);
					if(*next) *(next++) = 0;
				}
				else
				{
					val = expand_string(val, ",)", len+str-val, fillmore);
					if(*val == ',') ++val;
					next = expand_string(val, ")", len+str-val, fillmore);
					if(*next) *(next++) = 0;
				}

				if(val)
					snprintf(buf+i, BUF_SIZE-i, "%s", val);
			}

			else if(strcasecmp("suffix", ptr) == 0)
			{
				char *val;
				int inc = i;
				int loop = 1;

				while(loop)
				{
					char *next_word = expand_string(opt, " )", len+str-opt,
					                                fillmore);
					if(isspace(*next_word) && *next_word)
					{
						*(next_word++) = 0;
						while(isspace(*next_word) && *next_word)
							++next_word;
					}
					else
					{
						if(*next_word) *(next_word++) = 0;
						loop = 0;
					}

					val = strrchr(opt, '/');
					if(!val)
						val = opt;
					val = strrchr(val, '.');
					if(val)
						inc += snprintf(buf+inc, BUF_SIZE-inc, "%s ", val);
					opt = next_word;
				}
				if(inc > i) buf[inc-1] = 0;
				next = opt;
			}

			else if(strcasecmp("basename", ptr) == 0)
			{
				char *val;
				int inc = i;
				int loop = 1;

				while(loop)
				{
					char *next_word = expand_string(opt, " )", len+str-opt,
					                                fillmore);
					if(isspace(*next_word) && *next_word)
					{
						*(next_word++) = 0;
						while(isspace(*next_word) && *next_word)
							++next_word;
					}
					else
					{
						if(*next_word) *(next_word++) = 0;
						loop = 0;
					}

					val = strrchr(opt, '/');
					if(!val)
						val = opt;
					val = strrchr(val, '.');
					if(val) *val = 0;
					inc += snprintf(buf+inc, BUF_SIZE-inc, "%s ", opt);

					opt = next_word;
				}
				if(inc > i) buf[inc-1] = 0;
				next = opt;
			}


			else if(strcasecmp("addprefix", ptr) == 0)
			{
				char *val;
				int inc = i;
				int loop = 1;

				val = opt;
				opt = expand_string(opt, ",)", len+str-opt, fillmore);
				if(*opt != ',')
				{
					printf("\n\n!!! %s error, line %d !!!\n"
					       "Malformed 'addprefix' sub-command!\n\n",
					       fname, curr_line);
					free(buf);
					strcpy(linebuf, "exit -1\n");
					longjmp(jmpbuf, 1);
				}
				*(opt++) = 0;
				while(isspace(*opt) && *opt)
					++opt;

				while(loop)
				{
					char *next_word = expand_string(opt, " )", len+str-opt,
					                                fillmore);
					if(isspace(*next_word) && *next_word)
					{
						*(next_word++) = 0;
						while(isspace(*next_word) && *next_word)
							++next_word;
					}
					else
					{
						if(*next_word) *(next_word++) = 0;
						loop = 0;
					}

					inc += snprintf(buf+inc, BUF_SIZE-inc, "%s%s ", val,
					                opt);

					opt = next_word;
				}
				if(inc > i) buf[inc-1] = 0;
				next = opt;
			}

			else if(strcasecmp("addsuffix", ptr) == 0)
			{
				char *val;
				int inc = i;
				int loop = 1;

				val = opt;
				opt = expand_string(opt, ",)", len+str-opt, fillmore);
				if(*opt != ',')
				{
					printf("\n\n!!! %s error, line %d !!!\n"
					       "Malformed 'addprefix' sub-command!\n\n",
					       fname, curr_line);
					free(buf);
					strcpy(linebuf, "exit -1\n");
					longjmp(jmpbuf, 1);
				}
				*(opt++) = 0;
				while(isspace(*opt) && *opt)
					++opt;

				while(loop)
				{
					char *next_word = expand_string(opt, " )", len+str-opt,
					                                fillmore);
					if(isspace(*next_word) && *next_word)
					{
						*(next_word++) = 0;
						while(isspace(*next_word) && *next_word)
							++next_word;
					}
					else
					{
						if(*next_word) *(next_word++) = 0;
						loop = 0;
					}

					inc += snprintf(buf+inc, BUF_SIZE-inc, "%s%s ", opt,
					                val);

					opt = next_word;
				}
				if(inc > i) buf[inc-1] = 0;
				next = opt;
			}

/*			else if(strcasecmp("join", ptr) == 0)
			{
				char *val;
				int inc = i;

				val = opt;
				opt = strchr(val, ',');
				if(!opt)
				{
					printf("\n\n!!! %s error, line %d !!!\n"
					       "Malformed 'addprefix' sub-command!\n\n",
					       fname, curr_line);
					free(buf);
					strcpy(linebuf, "exit -1\n");
					longjmp(jmpbuf, 1);
				}
				*(opt++) = 0;
				while(isspace(*opt) && *opt)
					++opt;

				while(*opt || *val)
				{
					char *next_word = grab_word(&opt, NULL);
					char *next_word2 = grab_word(&val, NULL);

					inc += snprintf(buf+inc, BUF_SIZE-inc, "%s%s ", val,
					                opt);

					opt = next_word;
					val = next_word2;
				}
				if(inc > i) buf[inc-1] = 0;

				next = expand_string(opt, ")", len+str-opt, fillmore);
				if(*next) *(next++) = 0;
			}*/

			else if(strcasecmp("dir", ptr) == 0)
			{
				char *val;
				int inc = i;
				int loop = 1;

				while(loop)
				{
					char *next_word = expand_string(opt, " )", len+str-opt,
					                                fillmore);
					if(isspace(*next_word) && *next_word)
					{
						*(next_word++) = 0;
						while(isspace(*next_word) && *next_word)
							++next_word;
					}
					else
					{
						if(*next_word) *(next_word++) = 0;
						loop = 0;
					}

					val = strrchr(opt, '/');
					if(val)
					{
						val[1] = 0;
						inc += snprintf(buf+inc, BUF_SIZE-inc, "%s ", opt);
					}
					else
						inc += snprintf(buf+inc, BUF_SIZE-inc, "./ ");

					opt = next_word;
				}
				if(inc > i) buf[inc-1] = 0;
				next = opt;
			}

			else if(strcasecmp("notdir", ptr) == 0)
			{
				char *val;
				int inc = i;
				int loop = 1;

				while(loop)
				{
					char *next_word = expand_string(opt, " )", len+str-opt,
					                                fillmore);
					if(isspace(*next_word) && *next_word)
					{
						*(next_word++) = 0;
						while(isspace(*next_word) && *next_word)
							++next_word;
					}
					else
					{
						if(*next_word) *(next_word++) = 0;
						loop = 0;
					}

					val = strrchr(opt, '/');
					if(!val)
						val = opt;
					else
						++val;
					opt = next_word;
					if(val[0])
						inc += snprintf(buf+inc, BUF_SIZE-inc, "%s ", val);
				}
				if(inc > i) buf[inc-1] = 0;
				next = opt;
			}

			/* Finds an executable command by searching the PATH and
			   returns the full absolute path plus the filename, or an
			   empty string if it can't be found */
			else if(strcasecmp(ptr, "which") == 0)
			{
				char *path = strdup(getvar("PATH"));
				char *direc;
				char sep = ':';

				next = expand_string(opt, ")", len+str-opt, fillmore);
				if(*next) *(next++) = 0;

				if(strchr(path, ';') != NULL || (tolower(path[0]) >= 'a' &&
				                                 tolower(path[0]) <= 'z' &&
				                                 path[1] == ':'))
					sep = ';';


				if(*opt)
				{
					direc = path;
					while(direc && *direc)
					{
						struct stat statbuf;
						char *slash;
						char *next = strchr(direc, sep);
						if(next) *(next++) = 0;

						snprintf(buf+i, BUF_SIZE-i, "%s/%s", direc, opt);
						while((slash=strchr(buf+i, '\\')) != NULL)
							*slash = '/';
						if(stat(buf+i, &statbuf) == 0)
							break;
						buf[i] = 0;

						direc = next;
					}
				}

				free(path);
			}

			else if(strcasecmp("ls", ptr) == 0 || strcasecmp("lsa", ptr) == 0)
			{
				struct stat st;
				int inc = i;
				int show_hidden = 0;
				int loop = 1;

				if(strcasecmp("lsa", ptr) == 0)
					show_hidden = 1;

				while(loop)
				{
					DIR *dp;
					char *next_word = expand_string(opt, " )", len+str-opt,
					                                fillmore);
					if(isspace(*next_word) && *next_word)
					{
						*(next_word++) = 0;
						while(isspace(*next_word) && *next_word)
							++next_word;
					}
					else
					{
						if(*next_word) *(next_word++) = 0;
						loop = 0;
					}

					dp = opendir(opt);
					if(dp)
					{
						struct dirent *ent;
						char *slash = strrchr(opt, '/');
						if(slash && !slash[1]) *slash = 0;

						while((ent=readdir(dp)) != NULL)
						{
							int i;

							if(!show_hidden && ent->d_name[0] == '.')
								continue;

							i = snprintf(buf+inc, BUF_SIZE-inc, "%s/%s",
							             opt, ent->d_name);

							if(stat(buf+inc, &st) == 0 &&
							   S_ISDIR(st.st_mode))
								i += snprintf(buf+inc+i, BUF_SIZE-inc-i,
								              "/ ");
							else
								i += snprintf(buf+inc+i, BUF_SIZE-inc-i,
								              " ");
							inc += i;
						}
						closedir(dp);
					}
					else
					{
						if(show_hidden || opt[0] != '.')
						{
							if(stat(opt, &st) == 0)
								inc += snprintf(buf+inc, BUF_SIZE-inc, "%s ",
								                opt);
						}
					}
					opt = next_word;
				}
				if(inc > i) buf[inc-1] = 0;
				next = opt;
			}

			else
			{
				printf("\n\n!!! %s error, line %d !!!\n"
				       "Unknown sub-command '%s'\n\n", fname, curr_line, ptr);
				free(buf);
				strcpy(linebuf, "exit -1\n");
				longjmp(jmpbuf, 1);
			}

			end = next;
		}
		else
		{
			/* Insert the named environment var (in the form $FOO or ${FOO}) */
			if(*ptr == '{')
			{
				++ptr;

				if(*ptr == '\'')
				{
					use_hard_quotes = 1;
					++ptr;
					end = "'}";
				}
				else
					end = "}";

				end = expand_string(ptr, end, len+str-ptr, fillmore);
				if(*end == '\'')
				{
					*(end++) = 0;
					end = expand_string(end, "}", len+str-ptr, fillmore);
				}
				if(*end)
					*(end++) = 0;
			}
			else
				end = expand_string(ptr, "^", len+str-ptr, fillmore);

			if(use_hard_quotes)
				i = snprintf(buf, BUF_SIZE, "%s'", str);
			else
				i = snprintf(buf, BUF_SIZE, "%s", str);

			if(*ptr >= '0' && *ptr <= '9')
			{
				const char *val = "";
				int idx = atoi(ptr);
				if(idx < argc && idx >= 0)
					val = argv[idx];
				snprintf(buf+i, BUF_SIZE-i, "%s", val);
			}
			else if(strncmp("*", ptr, end-ptr) == 0 ||
			        (use_hard_quotes && strncmp("@", ptr, end-ptr) == 0))
			{
				int idx = 1;
				int inc = i;
				while(idx < argc)
				{
					inc += snprintf(buf+inc, BUF_SIZE-inc, "%s ", argv[idx]);
					++idx;
				}
				if(inc > i) buf[inc-1] = 0;
			}
			else if(strncmp("@", ptr, end-ptr) == 0)
			{
				int idx = 1;
				while(idx < argc)
				{
					i += snprintf(buf+i, BUF_SIZE-i, "${'%d'}", idx);

					++idx;
					if(idx < argc)
						i += snprintf(buf+i, BUF_SIZE-i, " ");
				}
				snprintf(buf+i, BUF_SIZE-i, "%s", end);
				strcpy(str, buf);
				continue;
			}
			else
			{
				char c = *end;
				*end = 0;
				snprintf(buf+i, BUF_SIZE-i, "%s", getvar(ptr));
				*end = c;
			}
		}

		while(buf[i])
		{
			if(buf[i] == '&' || buf[i] == '\'' || buf[i] == '"' ||
			   buf[i] == '\\')
			{
				memmove(buf+i+1, buf+i, BUF_SIZE-i-1);
				buf[i] = '\\';
				++i;
			}
			++i;
		}

		if(use_hard_quotes)
			snprintf(buf+i, BUF_SIZE-i, "'%s", end);
		else
			snprintf(buf+i, BUF_SIZE-i, "%s", end);
		strcpy(str, buf);
	} while(1);
}


/* extract_word: Extract a word starting at the string pointed to by 'str'. If
 * the word begins with a ' or " character, everything until that same
 * character will be considered part of the word. Otherwise, the word ends at
 * the first encountered whitespace. Returns the beginning of the next word,
 * or the end of the string.
 */
static char *extract_word(char *str, size_t len)
{
	char *end = str;

	if(!(*end))
		return end;

	end = expand_string(end, " \n", len+str-end, 1);

	if(*end && *end != '\n')
		*(end++) = 0;

	while(*end && isspace(*end))
	{
		if(*end == '\n')
		{
			if(end[1] != 0)
				nextline = strdup(end+1);
			*end = 0;
			break;
		}
		++end;
	}

	return end;
}


static void extract_line(char *str, size_t len)
{
	char *end = str;

	if(!(*end))
		return;

	end = expand_string(end, "\n", len+str-end, 1);

	if(end[0] == '\n' && end[1] != 0)
		nextline = strdup(end+1);
	*end = 0;
}


/* check_obj_deps: Checks a file's dependancy list. The dependancy file is a
 * file expected to be in dep_dir and with the same name, but with a different
 * extension. The format of the file is simply: 'file.o: dependancy list...'. A
 * '\' at the end of the line can be used as a next-line continuation. If the
 * dependancy file exists, none of the dependancies are missing, and none have
 * a modification time after 'obj_time', the function will return 0. Otherwise
 * 1 is returned denoting a rebuild may be required.
 */
static int check_obj_deps(char *base, char *src, time_t obj_time)
{
	static char dep[PATH_MAX];
	char *buf;
	int bufsize;

	struct stat statbuf;
	char *ptr = obj;
	FILE *df;

	ptr = strrchr(base, '/');
	if(!ptr) ptr = base;
	ptr = strrchr(ptr, '.');
	if(ptr) *ptr = 0;
	snprintf(dep, sizeof(dep), "${DEP_DIR}/'%s'${DEP_EXT}", base);
	expand_string(dep, "", sizeof(dep), 0);
	if(ptr) *ptr = '.';

	df = fopen(dep, "r");
	if(!df)
	{
		if(stat(src, &statbuf) != 0 || statbuf.st_mtime-obj_time > 0)
			return 1;
		return 0;
	}

	fseek(df, 0, SEEK_END);
	bufsize = ftell(df)+1;
	buf = malloc(bufsize);
	if(!buf)
	{
		fclose(df);
		return 1;
	}

	fseek(df, 0, SEEK_SET);
	fread(buf, 1, bufsize, df);
	buf[bufsize-1] = 0;

	fclose(df);

	ptr = strchr(buf, ':');
	if(!ptr)
	{
		free(buf);
		return 1;
	}

	++ptr;
	while(*ptr && *ptr != '\n' && isspace(*ptr))
		++ptr;

	while(*ptr)
	{
		char *stp = ptr;

		while(*stp && !isspace(*stp))
		{
			if(*stp == '\\')
				memmove(stp, stp+1, strlen(stp));
			if(*stp) ++stp;
		}
		if(*stp) *(stp++) = 0;
		while(*stp && isspace(*stp))
			++stp;

		if(strcmp(ptr, "\n") != 0 && (stat(ptr, &statbuf) != 0 ||
		                              statbuf.st_mtime > obj_time))
		{
			free(buf);
			return 1;
		}

		ptr = stp;
	}
	free(buf);

	buf = (char*)getvar("EXTRA_SRC_DEPS");
	if(*buf)
	{
		char *ptr = malloc(BUF_SIZE);
		strncpy(ptr, buf, BUF_SIZE);
		buf = ptr;

		while(*ptr)
		{
			char *next = expand_string(ptr, " ", BUF_SIZE+ptr-buf, 0);
			if(*next) *(next++) = 0;
			while(*next && isspace(*next))
				++next;

			if(strcmp(ptr, "\n") != 0 && (stat(ptr, &statbuf) != 0 ||
			                              statbuf.st_mtime > obj_time))
			{
				free(buf);
				return 1;
			}

			ptr = next;
		}

		free(buf);
	}

	return 0;
}

/* copy_file: Copies the source file 'sf' to 'df', preserving the source's
 * file mode and permissions, if possible.
 */
static int copy_file(const char *sf, const char *df)
{
	struct stat statbuf;
	FILE *src, *dst;
	int ret, i;
	int fd;

	if(stat(sf, &statbuf) != 0)
		return 1;

#ifdef O_BINARY
	fd = open(df, O_WRONLY|O_BINARY|O_TRUNC|O_CREAT, statbuf.st_mode);
#else
	fd = open(df, O_WRONLY|O_TRUNC|O_CREAT, statbuf.st_mode);
#endif
	if(fd < 0)
		return 1;
	dst = fdopen(fd, "wb");
	if(!dst)
	{
		close(fd);
		return 1;
	}

	src = fopen(sf, "rb");
	if(!src)
	{
		fclose(dst);
		return 1;
	}

	ret = 0;
	do {
		i = fread(buffer, 1, sizeof(buffer), src);
		if(i > 0)
			i = fwrite(buffer, 1, i, dst);
		if(i < 0)
			ret = 1;
	} while(i > 0);

	fclose(src);
	fclose(dst);
	return ret;
}


static int libify_name(char *buf, size_t buflen, char *name)
{
	int i;
	char *curr = strrchr(name, '/');
	if(curr)
	{
		*curr = 0;
		i = snprintf(buf, buflen, "'%s'/${LIB_PRE}'%s'${LIB_EXT}", name,
		             curr+1);
		*curr = '/';
	}
	else
		i = snprintf(buf, buflen, "${LIB_PRE}'%s'${LIB_EXT}", name);
	expand_string(buf, "", buflen, 0);
	return i;
}


static struct {
	char *ext;
	char *cmd;
} *associations;
static size_t num_assocs;

static void add_association(const char *ext, const char *cmd)
{
	size_t i;
	for(i = 0;i < num_assocs;++i)
	{
		if(associations[i].cmd[0] == 0 ||
		   strcasecmp(ext, associations[i].ext) == 0)
			break;
	}
	if(i == num_assocs)
	{
		void *tmp = realloc(associations, (i+1)*sizeof(*associations));
		if(!tmp)
		{
			strcpy(linebuf, "exit -1\n");
			longjmp(jmpbuf, 1);
		}

		associations = tmp;
		associations[i].ext = NULL;
		associations[i].cmd = NULL;

		++num_assocs;
	}
	free(associations[i].ext);
	free(associations[i].cmd);
	associations[i].ext = strdup(ext);
	associations[i].cmd = strdup(cmd);
	if(!associations[i].ext || !associations[i].cmd)
	{
		strcpy(linebuf, "exit -1\n");
		longjmp(jmpbuf, 1);
	}
}

static int build_command(char *buffer, size_t bufsize, char *barename,
                         const char *srcname, const char *objname)
{
	char dummy[2] = ".";
	const char *ptr;
	char *ext;
	size_t i;

	ext = strrchr(barename, '.');
	if(strchr(ext, '/') || !ext)
		ext = dummy;

	for(i = 0;i < num_assocs;++i)
	{
		if(strcasecmp(ext+1, associations[i].ext) == 0)
			break;
	}
	if(i == num_assocs)
		return 1;

	*ext = 0;
	ptr = associations[i].cmd;
	while(*ptr && i+1 < bufsize)
	{
		if(strncasecmp(ptr, "<*>", 3) == 0)
		{
			i += snprintf(buffer+i, bufsize-i, "'%s'", barename);
			ptr += 3;
		}
		else if(strncasecmp(ptr, "<!>", 3) == 0)
		{
			i += snprintf(buffer+i, bufsize-i, "\\\"'%s'\\\"", srcname);
			ptr += 3;
		}
		else if(strncasecmp(ptr, "<@>", 3) == 0)
		{
			i += snprintf(buffer+i, bufsize-i, "\\\"'%s'\\\"", objname);
			ptr += 3;
		}
		else
		{
			if(ptr[0] == '\\' && ptr[1] != 0 && i+2 < bufsize)
				buffer[i++] = *(ptr++);
			buffer[i++] = *(ptr++);
			buffer[i] = 0;
		}
	}
	*ext = '.';
	return 0;
}


/* build_obj_list: Builds a list of object files from the list of sources. If
 * any of the objects don't exist or have a modification time later than
 * 'base_time', the variable pointed to by 'do_link' will be set non-zero.
 */
static int build_obj_list(char *buffer, size_t bufsize, time_t base_time,
                          int *do_link)
{
	static char buf[PATH_MAX];

	struct stat statbuf;
	char *next = sources;
	char *ptr;
	int i = 0;

	while(*(ptr=next))
	{
		char *ext;
		char c;

		next = grab_word(&ptr, &c);

		ext = strrchr(ptr, '/');
		if(!ext) ext = ptr;
		ext = strrchr(ext, '.');

		if(ext) *ext = 0;
		snprintf(buf, sizeof(buf), "${OBJ_DIR}/'%s'${OBJ_EXT}", ptr);
		expand_string(buf, "", sizeof(buf), 0);
		if(ext) *ext = '.';

		if(!(*do_link) && (stat(buf, &statbuf) != 0 ||
		                   base_time < statbuf.st_mtime))
			*do_link = 1;

		if(ext) *ext = 0;
		i += snprintf(buffer+i, bufsize-i,
		              " \\\"${OBJ_DIR}/'%s'${OBJ_EXT}\\\"", ptr);
		if(ext) *ext = '.';

		ptr += strlen(ptr);
		if(next > ptr)
			*ptr = c;
	}
	return i;
}


static struct {
	char *name;
	char *val;
} *defines;
static size_t num_defines;

#define MAX_CMD_ARGC 32
static char  *cmd_argv[MAX_CMD_ARGC+1];
static size_t cmd_argc;


void cleanup(void)
{
	size_t i;

	fflush(stdout);

	for(i = 0;i < cmd_argc;++i)
	{
		free(cmd_argv[i]);
		cmd_argv[i] = NULL;
	}
	cmd_argc = 0;

	for(i = 0;i < num_defines;++i)
	{
		free(defines[i].name);
		free(defines[i].val);
	}
	free(defines);
	defines = NULL;

	i = 0;
	while(i < SRC_PATH_SIZE && src_paths[i])
		free(src_paths[i++]);
	i = 0;
	while(i < INVOKE_BKP_SIZE && invoke_backup[i].f)
	{
		fclose(invoke_backup[i].f);
		free(invoke_backup[i].fname);
		free(invoke_backup[i].bkp_lbuf);
		free(invoke_backup[i].bkp_nextline);
		++i;
	}
	for(i = 0;i < num_assocs;++i)
	{
		free(associations[i].ext);
		free(associations[i].cmd);
	}
	free(associations);
	free(loaded_files);
	free(nextline);
	free(sources);
	free(fname);
	if(f)
		fclose(f);
	if(infile && infile != stdin)
		fclose(infile);
}


unsigned int do_level = 0;
unsigned int did_cmds = 0, did_else = 0;
unsigned int wait_for_done = 0;
int ret = 0, tmp = 0;
int ignored_errors = 0;
int verbose = 0;
int shh = 0;
int ignore_err = 0;
int has_do = 0;

int main(int _argc, char **_argv)
{
	char *ptr;
	int i;

	setenv("CC", "gcc", 0);
	setenv("CXX", "g++", 0);
	setenv("LD", "gcc", 0);
	setenv("OUT_OPT", "-o ", 0);
	setenv("SRC_OPT", "-c ", 0);
	setenv("DEP_OPT", "-MMD -MF ", 0);
	setenv("OBJ_EXT", ".o", 0);
	setenv("OBJ_DIR", ".", 0);
	setenv("DEP_EXT", ".d", 0);
	setenv("DEP_DIR", ".", 0);
#if (defined __WIN32__ || defined __DOS__)
	setenv("EXE_EXT", ".exe", 0);
#else
	setenv("EXE_EXT", "", 0);
#endif
	setenv("AR", "ar", 0);
	setenv("AR_OPT", "", 0);
	setenv("LIB_PRE", "lib", 0);
	setenv("LIB_EXT", ".a", 0);
	setenv("CPPFLAGS", "", 0);
	setenv("CFLAGS", "", 0);
	setenv("CXXFLAGS", "", 0);
	setenv("LDFLAGS", "", 0);

	/* Open the default file */
	fname = strdup("default.cbd");
	f = fopen(fname, "r");
	if(!f)
	{
		fprintf(stderr, "\n\n\n*** Critical Error ***\n"
		                "Could not open default.cbd!\n\n");
		exit(1);
	}

	argc = _argc;
	argv = _argv;
	infile = stdin;

	atexit(cleanup);

	if(setjmp(jmpbuf))
	{
		free(nextline);
		nextline = NULL;
		goto reparse;
	}

main_loop_start:
	while(1)
	{
		int reverse;
		ignore_err = 0;
		has_do = 0;

		if(nextline)
		{
			/* If we already have the next line set, go do it */
			strncpy(linebuf, nextline, strlen(nextline)+1);
			free(nextline);
			nextline = NULL;
			goto reparse;
		}

		/* Grab the next line and increment the line count */
		if(fgets(linebuf, sizeof(linebuf), f) == NULL)
		{
			/* If end of file, check if we should uninvoke and continue */
			snprintf(linebuf, sizeof(linebuf), "uninvoke 0\n");
			goto reparse;
		}
		++curr_line;

reparse:
		reverse = 0;
		shh = 0;

		/* Chew up leading whitespace */
		for(i = 0;linebuf[i];++i)
		{
			if(!isspace(linebuf[i]))
			{
				if(i > 0)
				{
					memmove(linebuf, linebuf+i, strlen(linebuf)+1-i);
					i = 0;
				}
				break;
			}
		}
		if(!linebuf[i])
			continue;

		ptr = expand_string(linebuf, " =\n", sizeof(linebuf), 1);
		if(isspace(linebuf[0]))
			goto reparse;

		if(!linebuf[0] || ptr == linebuf || linebuf[0] == ':')
		{
			extract_line(linebuf, sizeof(linebuf));
			continue;
		}

		while(*ptr && isspace(*ptr))
		{
			if(*ptr == '\n')
			{
				*ptr = 0;
				if(ptr[1] != 0)
					nextline = strdup(ptr+1);
				break;
			}
			*(ptr++) = 0;
		}

		/* Check for special 'leveling' commands */

		/* The 'do' command pushes us up a level, and checks for the next
		 * if-type command, which itself will let us know if we should start
		 * ignoring commands */
		if(strcasecmp("do", linebuf) == 0)
		{
			++do_level;
			if(do_level >= sizeof(int)*8)
			{
				printf("\n\n!!! %s error, line %d !!!\n"
				       "Too many 'do' commands enountered (max: %ud)!\n\n",
				       fname, curr_line, sizeof(int)*8 - 1);
				snprintf(linebuf, sizeof(linebuf), "exit -1\n");
				goto reparse;
			}
			has_do = 1;
			memmove(linebuf, ptr, strlen(ptr)+1);
			goto reparse;
		}
		/* 'else' toggles ignoring commands at the current level */
		if(strcasecmp("else", linebuf) == 0)
		{
			if(do_level <= 0)
				do_level = 1;

			/* allow another if-type command to follow an else on the same
			 * level */
			if(!(wait_for_done & (1<<(do_level-1))))
			{
				wait_for_done |= 1<<(do_level-1);
				did_cmds |= 1<<(do_level-1);
				has_do = 0;
			}

			if(!(did_cmds & (1<<(do_level-1))))
			{
				wait_for_done &= ~(1<<(do_level-1));
				has_do = 1;
			}

			memmove(linebuf, ptr, strlen(ptr)+1);
			goto reparse;
		}
		/* 'done' takes us down a level */
		if(strcasecmp("done", linebuf) == 0)
		{
			if(do_level > 0)
			{
				wait_for_done &= ~(1<<(--do_level));
				did_else &= ~(1<<do_level);
				did_cmds &= ~(1<<do_level);
			}
			continue;
		}

		if((wait_for_done & ((1<<do_level)-1)))
		{
			extract_line(ptr, sizeof(linebuf)+linebuf-ptr);
			continue;
		}


		/* The if command checks if 'val1 = val2' is true and processes the
		 * rest of the line if so. */
		if(strcasecmp("if", linebuf) == 0)
value_check:
		{
			char *next, *var2;

			var2 = extract_word(ptr, sizeof(linebuf)+linebuf-ptr);
			if(*var2 != '=')
			{
				next = var2;
				var2 = strchr(ptr, '=');
				if(!var2)
				{
					printf("\n\n!!! %s error, line %d !!!\n"
					       "Malformed 'if' command!\n\n", fname, curr_line);
					snprintf(linebuf, sizeof(linebuf), "exit 1\n");
					goto reparse;
				}
				*(var2++) = 0;
				while(*var2 && isspace(*var2))
					++var2;
			}
			else
				next = extract_word(var2, sizeof(linebuf)+linebuf-var2);

			if((strcmp(ptr, var2) == 0) != !!reverse)
			{
				memmove(linebuf, next, strlen(next)+1);
				goto reparse;
			}

			if(has_do)
				wait_for_done |= 1 << (do_level-1);
			extract_line(next, sizeof(linebuf)+linebuf-next);
			continue;
		}
		/* Same as above, except if the comparison is false or it equates 0 */
		if(strcasecmp("ifnot", linebuf) == 0)
		{
			reverse = 1;
			goto value_check;
		}

		/* Checks the last command's return value against the supplied value,
		 * and runs the rest of the line if they're equal */
		if(strcasecmp("ifret", linebuf) == 0)
retval_check:
		{
			char *next;
			next = extract_word(ptr, sizeof(linebuf)+linebuf-ptr);

			if((atoi(ptr) == ret) != !!reverse)
			{
				memmove(linebuf, next, strlen(next)+1);
				goto reparse;
			}

			if(has_do)
				wait_for_done |= 1 << (do_level-1);
			extract_line(next, sizeof(linebuf)+linebuf-next);
			continue;
		}
		if(strcasecmp("ifnret", linebuf) == 0 )
		{
			reverse = 1;
			goto retval_check;
		}

		/* Checks the platform we're running on against the user supplied
		 * value */
		if(strcasecmp("ifplat", linebuf) == 0)
platform_check:
		{
			char *next = extract_word(ptr, sizeof(linebuf)+linebuf-ptr);
			int pass = 0;

			/* Throw in a dummy if() so the following ifs can use else if and
			 * avoid multiple checks for platforms where multiple targets apply
			 */
			if(0 == 1)
				pass = 0;

#if defined(_AIX) || defined(__TOS_AIX__)
			else if(strcasecmp("aix", ptr) == 0)
				pass = 1;
#endif
#if defined(AMIGA) || defined(__amigaos__)
			else if(strcasecmp("amigaos", ptr) == 0)
				pass = 1;
#endif
#if defined(__BEOS__)
			else if(strcasecmp("beos", ptr) == 0)
				pass = 1;
#endif
#if defined(_UNICOS)
			else if(strcasecmp("unicos", ptr) == 0)
				pass = 1;
#endif
#if defined(DGUX) || defined(__DGUX__) || defined(__dgux__)
			else if(strcasecmp("dgux", ptr) == 0)
				pass = 1;
#endif
#if defined(_SEQUENT_) || defined(sequent)
			else if(strcasecmp("dynix", ptr) == 0)
				pass = 1;
#endif
#ifdef __FreeBSD__
			else if(strcasecmp("freebsd", ptr) == 0)
				pass = 1;
#endif
#if defined(__GNU__)
			else if(strcasecmp("gnuhurd", ptr) == 0)
				pass = 1;
#endif
#if defined(hpux) || defined (_hpux) || defined(_HPUX_SOURCE)
			else if(strcasecmp("hpux", ptr) == 0)
				pass = 1;
#endif
#if defined(sgi) || defined(__sgi) || defined(mips) || defined(_SGI_SOURCE)
			else if(strcasecmp("irix", ptr) == 0)
				pass = 1;
#endif
#if defined(linux) || defined(__linux)
			else if(strcasecmp("linux", ptr) == 0)
				pass = 1;
#endif
#if defined(__MACOSX__) || defined(__APPLE__)
			else if(strcasecmp("macosx", ptr) == 0)
				pass = 1;
#endif
#if defined(mpeix) || defined(__mpexl)
			else if(strcasecmp("mpeix", ptr) == 0)
				pass = 1;
#endif
#if defined(MSDOS) || defined(__MSDOS__) || defined(_MSDOS) || defined(__DOS__)
			else if(strcasecmp("msdos", ptr) == 0)
				pass = 1;
#endif
#if defined(__NetBSD__)
			else if(strcasecmp("netbsd", ptr) == 0)
				pass = 1;
#endif
#if defined(__OpenBSD__)
			else if(strcasecmp("openbsd", ptr) == 0)
				pass = 1;
#endif
#if defined(OS2) || defined(_OS2) || defined(__OS2__) || defined(__TOS_OS2__)
			else if(strcasecmp("os2", ptr) == 0)
				pass = 1;
#endif
#if defined(pyr)
			else if(strcasecmp("pyramiddc", ptr) == 0)
				pass = 1;
#endif
#if defined(__QNX__)
			else if(strcasecmp("qnx4", ptr) == 0)
				pass = 1;
#endif
#if defined(__QNXNTO__)
			else if(strcasecmp("qnx6", ptr) == 0)
				pass = 1;
#endif
#if defined(sinix)
			else if(strcasecmp("reliantunix", ptr) == 0)
				pass = 1;
#endif
#if defined(M_I386) || defined(M_XENIX) || defined(_SCO_DS) || defined(_SCO_C_DIALECT)
			else if(strcasecmp("scoopenserver", ptr) == 0)
				pass = 1;
#endif
#if defined(sun) || defined(__sun)
# if defined(__SVR4) || defined(__svr4__)
			else if(strcasecmp("solaris", ptr) == 0)
				pass = 1;
# else
			else if(strcasecmp("sunos", ptr) == 0)
				pass = 1;
# endif
#elif defined(__SUNPRO_C)
			else if(strcasecmp("solaris", ptr) == 0)
				pass = 1;
#endif
#if defined(__SYMBIAN32__)
			else if(strcasecmp("symbian", ptr) == 0)
				pass = 1;
#endif
#if defined(__osf__) || defined(__osf)
			else if(strcasecmp("osf1", ptr) == 0 ||
			        strcasecmp("tru64", ptr) == 0)
				pass = 1;
#endif
#if defined(ultrix) || defined(__ultrix) || defined(__ultrix__) || (defined(unix) && defined(vax))
			else if(strcasecmp("ultrix", ptr) == 0)
				pass = 1;
#endif
#if defined(sco) || defined(_UNIXWARE7)
			else if(strcasecmp("unixware", ptr) == 0)
				pass = 1;
#endif
#if defined(VMS) || defined(__VMS)
			else if(strcasecmp("vms", ptr) == 0)
				pass = 1;
#endif
#if defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN32__)
			else if(strcasecmp("win32", ptr) == 0)
				pass = 1;
#endif

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__bsdi__) || defined(_SYSTYPE_BSD)
			else if(strcasecmp("bsd", ptr) == 0)
				pass = 1;
#endif
#if defined(__sysv__) || defined(__SVR4) || defined(__svr4__) || defined(_SYSTYPE_SVR4)
			else if(strcasecmp("svr4", ptr) == 0)
				pass = 1;
#endif
#if defined(unix) || defined(__unix) || defined(__unix__)
			else if(strcasecmp("unix", ptr) == 0)
				pass = 1;
#endif
#if defined(_UWIN)
			else if(strcasecmp("uwin", ptr) == 0)
				pass = 1;
#endif
#if defined(_WINDU_SOURCE)
			else if(strcasecmp("windu", ptr) == 0)
				pass = 1;
#endif

#if defined(__i386__) || defined(__i386) || (_M_IX86 > 300)
			else if(strcasecmp("i386", ptr) == 0)
				pass = 1;
#endif
#if defined(__ia64__) || defined(_M_IA64)
			else if(strcasecmp("ia64", ptr) == 0)
				pass = 1;
#endif
#if defined(__powerpc__) || defined(_M_PPC)
			else if(strcasecmp("powerpc", ptr) == 0)
				pass = 1;
#endif
#if defined(__sparc__) || defined(__sparc)
			else if(strcasecmp("sparc", ptr) == 0)
				pass = 1;
#endif

			if((pass != 0) != !!reverse)
			{
				memmove(linebuf, next, strlen(next)+1);
				goto reparse;
			}

			if(has_do)
				wait_for_done |= 1 << (do_level-1);
			extract_line(next, sizeof(linebuf)+linebuf-next);
			continue;
		}
		if(strcasecmp("ifnplat", linebuf) == 0)
		{
			reverse = 1;
			goto platform_check;
		}

		/* Checks if the supplied option name was specified on the command
		 * line */
		if(strcasecmp("ifopt", linebuf) == 0)
option_check:
		{
			char *next = extract_word(ptr, sizeof(linebuf)+linebuf-ptr);
			int pass = 0;

			for(i = 1;i < argc;++i)
			{
				size_t len = strlen(ptr);
				char *eq = strrchr(argv[i], '=');

				if(eq && (size_t)(eq-argv[i]) != len)
					continue;

				if(strncasecmp(ptr, argv[i], len) == 0)
				{
					pass = 1;
					break;
				}
			}

			if((pass != 0) != !!reverse)
			{
				memmove(linebuf, next, strlen(next)+1);
				goto reparse;
			}

			if(has_do)
				wait_for_done |= 1 << (do_level-1);
			extract_line(next, sizeof(linebuf)+linebuf-next);
			continue;
		}
		if(strcasecmp("ifnopt", linebuf) == 0)
		{
			reverse = 1;
			goto option_check;
		}

		/* Checks if a file or directory exists */
		if(strcasecmp("ifexist", linebuf) == 0)
file_dir_check:
		{
			struct stat statbuf;
			char *next;

			next = extract_word(ptr, sizeof(linebuf)+linebuf-ptr);
			if((stat(ptr, &statbuf) == 0) != !!reverse)
			{
				memmove(linebuf, next, strlen(next)+1);
				goto reparse;
			}

			if(has_do)
				wait_for_done |= 1 << (do_level-1);
			extract_line(next, sizeof(linebuf)+linebuf-next);
			continue;
		}
		if(strcasecmp("ifnexist", linebuf) == 0)
		{
			reverse = 1;
			goto file_dir_check;
		}

		/* Checks if we have write permissions to the specified file or
		 * directory */
		if(strcasecmp("ifwrite", linebuf) == 0)
dir_write_check:
		{
			char *next = extract_word(ptr, sizeof(linebuf)+linebuf-ptr);
			int val = 1;

#ifdef __unix__
			{
				struct stat stbuf;
				val = 0;
				if(stat(ptr, &stbuf) == 0)
				{
					if(stbuf.st_uid == getuid())
					{
						if((stbuf.st_mode&S_IWUSR))
							val = 1;
					}
					else if(stbuf.st_gid == getgid())
					{
						if((stbuf.st_mode&S_IWGRP))
							val = 1;
					}
					else if((stbuf.st_mode&S_IWOTH))
						val = 1;
				}
			}
#endif

			if((val != 0) != !!reverse)
			{
				memmove(linebuf, next, strlen(next)+1);
				goto reparse;
			}

			if(has_do)
				wait_for_done |= 1 << (do_level-1);
			extract_line(next, sizeof(linebuf)+linebuf-next);
			continue;
		}
		if(strcasecmp("ifnwrite", linebuf) == 0)
		{
			reverse = 1;
			goto dir_write_check;
		}



		/* Set an environment variable. The value is space sensitive, so if you
		 * wish to use spaces, encapsulate the value in ''s. Using '?=' instead
		 * of '=' will only set the variable if it isn't already set. */
		if(strncmp(ptr, "+=", 2) == 0 || strncmp(ptr, "-=", 2) == 0 ||
		   strncmp(ptr, "?=", 2) == 0 || ptr[0] == '=')
		{
			char *val = ptr;
			int ovr = 1;

			if(val[0] != '=')
				++val;
			ptr = linebuf;

			if(*(val-1) == '+')
			{
				*(val-1) = 0;
				++val;
				while(*val && isspace(*val))
					++val;
				extract_line(val, sizeof(linebuf)+linebuf-val);
				if(*val)
				{
					snprintf(buffer, sizeof(buffer), "%s%s", getvar(ptr), val);
					setenv(ptr, buffer, 1);
				}
				continue;
			}

			if(*(val-1) == '-')
			{
				char *pos;
				int len;

				*(val-1) = 0;
				++val;
				while(*val && isspace(*val))
					++val;

				extract_line(val, sizeof(linebuf)+val-linebuf);
				len = strlen(val);

				if(len <= 0)
					continue;

				snprintf(buffer, sizeof(buffer), "%s", getvar(ptr));
				while((pos=strstr(buffer, val)) != NULL)
				{
					int len = strlen(val);
					memmove(pos, pos+len, strlen(pos+len)+1);
				}
				setenv(ptr, buffer, 1);
				continue;
			}

			if(*(val-1) == '?')
			{
				*(val-1) = 0;
				if(*getvar(ptr))
					ovr = 0;
			}
			else
				*val = 0;

			++val;
			while(*val && isspace(*val))
				++val;
			extract_line(val, sizeof(linebuf)+linebuf-val);
			if(*val)
				setenv(ptr, val, ovr);
			else if(ovr)
				unsetenv(ptr);
			continue;
		}


		/* Reset the return value and 'do' status */
		has_do = 0;
//		ret = 0;

		/* Don't display normal output */
		if(linebuf[0] == '@')
		{
			shh = 1;
			memmove(linebuf, linebuf+1, strlen(linebuf));
		}

		/* Set error suppression level */
		if(linebuf[0] == '!')
		{
			ignore_err = 2;
			memmove(linebuf, linebuf+1, strlen(linebuf));
		}
		else if(linebuf[0] == '-')
		{
			ignore_err = 1;
			memmove(linebuf, linebuf+1, strlen(linebuf));
		}


		/* Call an external program via the system() function. Parameters are
		 * passed along as-is. */
		if(strcasecmp("call", linebuf) == 0)
		{
			extract_line(ptr, sizeof(linebuf)+linebuf-ptr);
			if(!shh)
			{
				printf("%s\n", ptr);
				fflush(stdout);
			}
			if((ret=system(ptr)) != 0)
			{
				if(!ignore_err)
				{
					snprintf(linebuf, sizeof(linebuf), "exit %d\n", ret);
					goto reparse;
				}
				if(ignore_err < 2)
				{
					printf("--- Error %d ignored. ---\n\n", ++ignored_errors);
					fflush(stdout);
				}
			}

			continue;
		}

		/* Re-invokes the current script, possibly passing a different set of
		 * command line options, before resuming. Pass '.' to indicate no
		 * arguments */
		if(strcasecmp("reinvoke", linebuf) == 0)
		{
			extract_line(ptr, sizeof(linebuf)+linebuf-ptr);
			snprintf(buffer, sizeof(buffer), "%s %s", argv[0],
			         ((strcmp(ptr, ".")==0)?"":ptr));
			if((ret=system(buffer)) != 0)
			{
				if(ignore_err < 2)
					printf("!!! %s error, line %d !!!\n"
					       "'reinvoke' returned with exitcode %d!\n",
					       fname, ++ignored_errors, ret);
				if(!ignore_err)
				{
					snprintf(linebuf, sizeof(linebuf), "exit %d\n", ret);
					goto reparse;
				}
				if(ignore_err < 2)
					printf("--- Error %d ignored. ---\n\n", ++ignored_errors);
				fflush(stdout);
			}

			continue;
		}

		/* Invokes an alternate script within the same context, and returns
		 * control when it exits. All variables and command-line options will
		 * persists into and out of invoked scripts. Errors and exit calls will
		 * cause all scripts to abort. To safely exit from an invoked script
		 * before the end of the file, and continue the previous, use
		 * 'uninvoke' */
		if(strcasecmp("invoke", linebuf) == 0)
		{
			int i;
			FILE *f2;
			char *fname2;

			extract_line(ptr, sizeof(linebuf)+linebuf-ptr);
			f2 = f;
			fname2 = fname;
			f = fopen(ptr, "r");
			if(!f)
			{
				f = f2;
				ret = 1;
				if(!ignore_err)
				{
					printf("Could not open script '%s'!\n", ptr);
					snprintf(linebuf, sizeof(linebuf), "exit %d\n", ret);
					goto reparse;
				}
				if(ignore_err < 2)
				{
					printf("Could not open script '%s'!\n"
					       "--- Error %d ignored. ---\n\n", ptr,
					       ++ignored_errors);
					fflush(stdout);
				}
				continue;
			}
			ret = 0;
			fname = strdup(ptr);

			for(i = 0;i < INVOKE_BKP_SIZE;++i)
			{
				if(!invoke_backup[i].f)
				{
					invoke_backup[i].f = f2;
					invoke_backup[i].fname = fname2;

					invoke_backup[i].bkp_lbuf = strdup(linebuf);
					invoke_backup[i].bkp_nextline = nextline;

					invoke_backup[i].bkp_line = curr_line;
					invoke_backup[i].bkp_did_else = did_else;
					invoke_backup[i].bkp_did_cmds = did_cmds;
					invoke_backup[i].bkp_do_level = do_level;

					break;
				}
			}

			nextline = NULL;

			curr_line = 0;
			did_else = 0;
			did_cmds = 0;
			do_level = 0;

			continue;
		}

		if(strcasecmp(linebuf, "popfront") == 0)
		{
			char *front_word;
			char *varstr;
			char *next;

			next = extract_word(ptr, sizeof(linebuf)+linebuf-ptr);
			extract_word(next, sizeof(linebuf)+linebuf-next);

			varstr = malloc(65536);
			if(!varstr)
			{
				snprintf(linebuf, sizeof(linebuf), "exit -1\n");
				goto reparse;
			}
			strncpy(varstr, getvar(ptr), 65536);
			front_word = varstr;
			while(*varstr && isspace(*varstr))
				++varstr;
			if(front_word != varstr)
				memmove(front_word, varstr, strlen(varstr)+1);

			varstr = expand_string(front_word, " ", 65536, 0);
			while(isspace(*varstr))
				++varstr;

			setenv(ptr, varstr, 1);

			free(front_word);
			continue;
		}


		if(strcasecmp("define", linebuf) == 0)
		{
			char *val = extract_word(ptr, sizeof(linebuf)+linebuf-ptr);
			extract_line(val, sizeof(linebuf)+linebuf-val);

			if(*val == 0)
			{
				for(i = 0;(size_t)i < num_defines;++i)
				{
					if(strcasecmp(ptr, defines[i].name) != 0)
						continue;
					free(defines[i].name);
					free(defines[i].val);
					--num_defines;
					if((size_t)i < num_defines)
						memmove(&defines[i], &defines[i+1], sizeof(defines[0])*
						        (num_defines-i-1));
					defines = realloc(defines, sizeof(defines[0])*num_defines);
					break;
				}
			}
			else
			{
				for(i = 0;(size_t)i < num_defines;++i)
				{
					if(strcasecmp(ptr, defines[i].name) == 0)
						break;
				}
				if((size_t)i == num_defines)
				{
					void *tmp = realloc(defines, sizeof(defines[0])*
					                             (num_defines+1));
					if(!tmp)
					{
						fprintf(stderr, "\n\n\n*** Critical Error ***\n"
						                "Out of memory allocating %ud defines!\n\n",
						                num_defines+1);
						snprintf(linebuf, sizeof(linebuf), "exit -1\n");
						goto reparse;
					}
					++num_defines;
					defines = tmp;
					defines[i].val = NULL;
					defines[i].name = strdup(ptr);
					if(!defines[i].name)
					{
						fprintf(stderr, "\n\n\n*** Critical Error ***\n"
						                "Out of memory duplicating string '%s'!!\n\n",
						                val);
						snprintf(linebuf, sizeof(linebuf), "exit -1\n");
						goto reparse;
					}
				}

				free(defines[i].val);
				defines[i].val = strdup(val);
				if(!defines[i].val)
				{
					fprintf(stderr, "\n\n\n*** Critical Error ***\n"
					                "Out of memory duplicating string '%s'!!\n\n",
					                val);
					snprintf(linebuf, sizeof(linebuf), "exit -1\n");
					goto reparse;
				}
			}

			continue;
		}

		/* Compiles a list of source files, and stores them in a list to be
		 * linked later. Compiled files are placed in the 'OBJ_DIR' with the
		 * extension changed to 'OBJ_EXT'. C and C++ files are compiled by the
		 * programs stored in 'CC' and 'CXX' respectively, using the flags
		 * stored in 'CPPFLAGS', 'CFLAGS', and 'CXXFLAGS' as expected. Unknown
		 * file types are silently ignored. */
		if(strcasecmp("compile", linebuf) == 0)
		{
			extract_line(ptr, sizeof(linebuf)+linebuf-ptr);

			free(sources);
			sources = strdup(ptr);
			if(!sources)
			{
				fprintf(stderr, "\n\n\n** Critical Error **\n"
				                "Out of memory duplicating string '%s'\n\n",
				                ptr);
				snprintf(linebuf, sizeof(linebuf), "exit -1\n");
				goto reparse;
			}
compile_more_sources:
			tmp = 0;
			while(ptr && *ptr)
			{
				char *src, *ext, *next, c;
				struct stat statbuf;

				next = grab_word(&ptr, &c);

				ext = strrchr(ptr, '/');
				if(!ext) ext = ptr;
				ext = strrchr(ext, '.');
				if(!ext)
					goto next_src_file;

				*ext = 0;
				snprintf(obj, sizeof(obj), "${OBJ_DIR}/'%s'${OBJ_EXT}", ptr);
				expand_string(obj, "", sizeof(obj), 0);
				*ext = '.';

				src = find_src(ptr);

				if(stat(obj, &statbuf) == 0)
				{
					if(!check_obj_deps(ptr, src, statbuf.st_mtime))
						goto next_src_file;
				}

				i = 0;
				if(build_command(buffer, sizeof(buffer), ptr, src, obj) == 0)
					goto compile_it;

				if(strcasecmp(ext+1, "c") == 0)
					i += snprintf(buffer+i, sizeof(buffer)-i,
					              "${CC} ${CPPFLAGS} ${CFLAGS}");
				else if(strcasecmp(ext+1, "cc") == 0 ||
				        strcasecmp(ext+1, "cpp") == 0 ||
				        strcasecmp(ext+1, "cxx") == 0)
					i += snprintf(buffer+i, sizeof(buffer)-i,
					              "${CXX} ${CPPFLAGS} ${CXXFLAGS}");
				else
					goto next_src_file;

				*ext = 0;
				if(*getvar("DEP_OPT"))
					i += snprintf(buffer+i, sizeof(buffer)-i,
					            " ${DEP_OPT}\\\"${DEP_DIR}/'%s'${DEP_EXT}\\\"",
					              ptr);
				i += snprintf(buffer+i, sizeof(buffer)-i,
				              " ${OUT_OPT}\\\"${OBJ_DIR}/'%s'${OBJ_EXT}\\\"",
				              ptr);
				*ext = '.';

				i += snprintf(buffer+i, sizeof(buffer)-i,
				              " ${SRC_OPT}\\\"'%s'\\\"", src);

compile_it:
				expand_string(buffer, "", sizeof(buffer), 0);
				if(!shh)
				{
					if(verbose)
						printf("%s\n", buffer);
					else
						printf("Compiling %s...\n", src);
					fflush(stdout);
				}

				if((ret=system(buffer)) != 0)
				{
					tmp = ret;
					if(!ignore_err)
					{
						snprintf(linebuf, sizeof(linebuf), "exit %d\n", ret);
						goto reparse;
					}
					if(ignore_err < 2)
					{
						printf("--- Error %d ignored. ---\n\n",
						       ++ignored_errors);
						fflush(stdout);
					}
				}

next_src_file:
				ptr += strlen(ptr);
				if(*next)
					*(ptr++) = c;
			}
			ret = tmp;
			continue;
		}

		/* Adds more source files to the list, and compiles them as above */
		if(strcasecmp("compileadd", linebuf) == 0)
		{
			int pos = 0;
			char *t;

			extract_line(ptr, sizeof(linebuf)+linebuf-ptr);

			if(sources)
				pos = strlen(sources);
			t = realloc(sources, pos + strlen(ptr) + 2);
			if(!t)
			{
				fprintf(stderr, "\n\n\n** Critical Error **\n"
				                "Out of memory appending string '%s'\n\n",
				                ptr);
				snprintf(linebuf, sizeof(linebuf), "exit -1\n");
				goto reparse;
			}
			sources = t;
			sources[pos] = ' ';
			strcpy(sources+pos+1, ptr);
			ptr = sources+pos+1;

			for(i = 0;isspace(sources[i]);++i)
				;
			if(i > 0)
			{
				memmove(sources, sources+i, strlen(sources+i)+1);
				ptr -= i;
			}

			goto compile_more_sources;
		}

		/* Links an executable file, using the previously-compiled source
		 * files. The executable will have 'EXE_EXT' appended. */
		if(strcasecmp("linkexec", linebuf) == 0)
		{
			struct stat statbuf;
			time_t exe_time = 0;
			int do_link = 1;

			if(!sources)
			{
				ret = 1;
				if(ignore_err < 2)
					fprintf(stderr, "\n\n!!! %s error, line %d !!!\n"
					                "Trying to build %s with undefined "
					                "sources!\n\n", fname, curr_line, ptr);
				if(!ignore_err)
				{
					snprintf(linebuf, sizeof(linebuf), "exit %d\n", ret);
					goto reparse;
				}
				if(ignore_err < 2)
					printf("--- Error %d ignored. ---\n\n", ++ignored_errors);
				fflush(stdout);
				continue;
			}
			ret = 0;

			extract_line(ptr, sizeof(linebuf)+linebuf-ptr);

			snprintf(obj, sizeof(obj), "'%s'${EXE_EXT}", ptr);
			expand_string(obj, "", sizeof(obj), 0);
			if(stat(obj, &statbuf) == 0)
			{
				exe_time = statbuf.st_mtime;
				do_link = 0;
			}

			i = 0;
			i += snprintf(buffer+i, sizeof(buffer)-i, "${LD}");
			i += snprintf(buffer+i, sizeof(buffer)-i, " ${OUT_OPT}'\"%s\"'",
			              obj);

			i += build_obj_list(buffer+i, sizeof(buffer)-i, exe_time,
			                    &do_link);

			if(!do_link)
			{
				char *buf = (char*)getvar("EXTRA_LD_DEPS");
				if(*buf)
				{
					char *ptr = malloc(BUF_SIZE);
					strncpy(ptr, buf, BUF_SIZE);
					buf = ptr;

					while(*ptr)
					{
						char *next = expand_string(ptr, " ", BUF_SIZE+ptr-buf, 0);
						if(*next) *(next++) = 0;
						while(*next && isspace(*next))
							++next;

						if(strcmp(ptr, "\n") != 0 && (stat(ptr, &statbuf) != 0 ||
						                              statbuf.st_mtime > exe_time))
						{
							free(buf);
							buf = NULL;

							do_link = 1;
							break;
						}

						ptr = next;
					}

					free(buf);
				}

				if(!do_link)
					continue;
			}

			i += snprintf(buffer+i, sizeof(buffer)-i, " ${LDFLAGS}");
			expand_string(buffer, "", sizeof(buffer), 0);

			if(!shh)
			{
				if(verbose)
					printf("%s\n", buffer);
				else
					printf("Linking %s...\n", obj);
				fflush(stdout);
			}
			if((ret=system(buffer)) != 0)
			{
				if(!ignore_err)
				{
					snprintf(linebuf, sizeof(linebuf), "exit %d\n", ret);
					goto reparse;
				}
				if(ignore_err < 2)
				{
					printf("--- Error %d ignored. ---\n\n", ++ignored_errors);
					fflush(stdout);
				}
			}

			continue;
		}

		/* Links a standard archive library, using the previously-compiled
		 * source files. The file will have 'LIB_PRE' prepended, and is
		 * sub-directory-aware, as well as have 'LIB_EXT' appended. */
		if(strcasecmp("linklib", linebuf) == 0)
		{
			struct stat statbuf;
			time_t lib_time = 0;
			int do_link = 1;

			if(!sources)
			{
				ret = 1;
				if(ignore_err < 2)
					fprintf(stderr, "\n\n!!! %s error, line %d !!!\n"
					                "Trying to build %s with undefined "
					                "sources!\n\n", fname, curr_line, ptr);
				if(!ignore_err)
				{
					snprintf(linebuf, sizeof(linebuf), "exit %d\n", ret);
					goto reparse;
				}
				if(ignore_err < 2)
					printf("--- Error %d ignored. ---\n\n", ++ignored_errors);
				fflush(stdout);
				continue;
			}
			ret = 0;

			extract_line(ptr, sizeof(linebuf)+linebuf-ptr);

			libify_name(obj, sizeof(obj), ptr);
			if(stat(obj, &statbuf) == 0)
			{
				lib_time = statbuf.st_mtime;
				do_link = 0;
			}

			i = 0;
			i += snprintf(buffer+i, sizeof(buffer)-i, "${AR} ${AR_OPT}");
			i += snprintf(buffer+i, sizeof(buffer)-i, " '\"%s\"'", obj);

			i += build_obj_list(buffer+i, sizeof(buffer)-i, lib_time,
			                    &do_link);

			if(!do_link)
				continue;

			remove(obj);
			expand_string(buffer, "", sizeof(buffer), 0);

			if(!shh)
			{
				if(verbose)
					printf("%s\n", buffer);
				else
					printf("Linking %s...\n", obj);
				fflush(stdout);
			}
			if((ret=system(buffer)) != 0)
			{
				if(!ignore_err)
				{
					snprintf(linebuf, sizeof(linebuf), "exit %d\n", ret);
					goto reparse;
				}
				if(ignore_err < 2)
				{
					printf("--- Error %d ignored. ---\n\n", ++ignored_errors);
					fflush(stdout);
				}
			}

			continue;
		}

		/* Loads a list of words into a buffer, to later execute an action on
		 * them */
		if(strcasecmp("loadlist", linebuf) == 0)
		{
			extract_line(ptr, sizeof(linebuf)+linebuf-ptr);
			free(loaded_files);
			loaded_files = strdup(ptr);
			if(!loaded_files)
			{
				fprintf(stderr, "\n\n\n** Critical Error **\n"
				                "Out of memory duplicating string '%s'\n\n",
				                ptr);
				snprintf(linebuf, sizeof(linebuf), "exit -1\n");
				goto reparse;
			}
			continue;
		}

		/* Executes a command on the loaded file list */
		if(strcasecmp("execlist", linebuf) == 0)
		{
			char *curr, *next;

			if(!loaded_files)
			{
				ret = 1;
				if(ignore_err < 2)
					printf("\n\n!!! %s error, line %d !!!\n"
					       "'loadexec' called with no list!\n\n", fname,
					       curr_line);
				if(!ignore_err)
				{
					snprintf(linebuf, sizeof(linebuf), "exit %d\n", ret);
					goto reparse;
				}
				if(ignore_err < 2)
					printf("--- Error %d ignored. ---\n\n", ++ignored_errors);
				fflush(stdout);
				continue;
			}
			ret = 0;

			next = loaded_files;
			tmp = 0;
			extract_line(ptr, sizeof(linebuf)+linebuf-ptr);

			while(*(curr=next))
			{
				char *cmd_ptr = ptr;
				char c;
				int i;

				next = grab_word(&curr, &c);

				i = 0;
				while(*cmd_ptr && i+1 < (signed)sizeof(buffer))
				{
					if(strncasecmp(cmd_ptr, "<@>", 3) == 0)
					{
						i += snprintf(buffer+i, sizeof(buffer)-i, "%s",
						              curr);
						cmd_ptr += 3;
					}
					else
					{
						if(cmd_ptr[0] == '\\' && cmd_ptr[1] != 0 &&
						   i+2 < (signed)sizeof(buffer))
							buffer[i++] = *(cmd_ptr++);
						buffer[i++] = *(cmd_ptr++);
						buffer[i] = 0;
					}
				}

				if(!shh)
				{
					printf("%s\n", buffer);
					fflush(stdout);
				}
				if((ret=system(buffer)) != 0)
				{
					tmp = ret;
					if(!ignore_err)
					{
						snprintf(linebuf, sizeof(linebuf), "exit %d\n", ret);
						goto reparse;
					}
					if(ignore_err < 2)
						printf("--- Error %d ignored. ---\n\n",
						       ++ignored_errors);
					fflush(stdout);
				}


				curr += strlen(curr);
				if(next > curr)
					*curr = c;
			}

			ret = tmp;
			continue;
		}

		/* Prints a string to the console. If a '.' char is used at the
		 * beginning of string, it will be skipped */
		if(strcasecmp("echo", linebuf) == 0)
		{
			extract_line(ptr, sizeof(linebuf)+linebuf-ptr);
			printf("%s\n", ptr);
			fflush(stdout);
			continue;
		}

		/* Prints a string to the console without a newline. If a '.' char is
		 * used at the beginning of string, it will be skipped */
		if(strcasecmp("put", linebuf) == 0)
		{
			extract_line(ptr, sizeof(linebuf)+linebuf-ptr);
			printf("%s", ptr);
			fflush(stdout);
			continue;
		}

		/* Creates/truncates a file and writes a string to it */
		if(strcasecmp("writefile", linebuf) == 0)
		{
			char *str;
			FILE *o;

			str = extract_word(ptr, sizeof(linebuf)+linebuf-ptr);
			o = fopen(ptr, "w");
			if(!o)
			{
				ret = 1;
				if(!ignore_err)
				{
					printf("Could not create file '%s'!\n", ptr);
					snprintf(linebuf, sizeof(linebuf), "exit %d\n", ret);
					goto reparse;
				}
				if(ignore_err < 2)
				{
					printf("Could not create file '%s'!\n"
					       "--- Error %d ignored. ---\n\n", ptr,
					       ++ignored_errors);
					fflush(stdout);
				}
				continue;
			}
			ret = 0;

			extract_line(str, sizeof(linebuf)+linebuf-str);
			fprintf(o, "%s\n", str);
			fclose(o);
			continue;
		}

		/* Appends a string to a file */
		if(strcasecmp("appendfile", linebuf) == 0)
		{
			char *str;
			FILE *o;

			str = extract_word(ptr, sizeof(linebuf)+linebuf-ptr);
			o = fopen(ptr, "a");
			if(!o)
			{
				ret = 1;
				if(!ignore_err)
				{
					printf("Could not create file '%s'!\n", ptr);
					snprintf(linebuf, sizeof(linebuf), "exit %d\n", ret);
					goto reparse;
				}
				if(ignore_err < 2)
				{
					printf("Could not create file '%s'!\n"
					       "--- Error %d ignored. ---\n\n", ptr,
					       ++ignored_errors);
					fflush(stdout);
				}
				continue;
			}
			ret = 0;

			extract_line(str, sizeof(linebuf)+linebuf-str);
			fprintf(o, "%s\n", str);
			fclose(o);
			continue;
		}

		/* Jumps to the specified label. A label is denoted by a '#:' combo
		 * prepended to it at the beginning of a line */
		if(strcasecmp("goto", linebuf) == 0)
		{
			int src_line;
			char *lbl;

			lbl = extract_word(ptr, sizeof(linebuf)+linebuf-ptr);
			extract_line(lbl, sizeof(linebuf)+linebuf-ptr);
			lbl = strdup(ptr);
			if(!lbl)
			{
				fprintf(stderr, "\n\n\n** Critical Error **\n"
				                "Out of memory duplicating string '%s'\n\n",
				                lbl);
				snprintf(linebuf, sizeof(linebuf), "exit -1\n");
				goto reparse;
			}

			rewind(f);
			src_line = curr_line;
			curr_line = 0;
			wait_for_done = 0;
			did_else = 0;
			did_cmds = 0;
			do_level = 0;
			while(fgets(linebuf, sizeof(linebuf), f) != NULL)
			{
				++curr_line;
				while(linebuf[0])
				{
					int i;
					extract_line(linebuf, sizeof(linebuf));

					for(i = 0;isspace(linebuf[i]);++i)
						;
					memmove(linebuf, &linebuf[i], strlen(&linebuf[i])+1);
					if(linebuf[0] == ':' && strcasecmp(linebuf+1, lbl) == 0)
					{
						free(lbl);
						goto main_loop_start;
					}
					if(!nextline)
						break;
					strcpy(linebuf, nextline);
					free(nextline);
					nextline = NULL;
				}
			}
			fprintf(stderr, "\n\n!!! %s error, line %d !!!\n"
			                "Label target '%s' not found!\n\n", fname,
			                src_line, lbl);
			free(lbl);
			snprintf(linebuf, sizeof(linebuf), "exit 1\n");
			goto reparse;
		}


		/* Stores a list of paths to look for source files in. Passing only
		 * '.' clears the list. */
		if(strcasecmp("src_paths", linebuf) == 0)
		{
			unsigned int count = 0;
			char *next;

			while(count < SRC_PATH_SIZE && src_paths[count])
			{
				free(src_paths[count]);
				src_paths[count] = NULL;
				++count;
			}

			count = 0;
			while(*ptr)
			{
				if(count >= SRC_PATH_SIZE)
				{
					printf("\n\n!!! %s error, line %d !!!\n"
					       "Too many source paths specified!\n\n", fname,
					       curr_line);
					snprintf(linebuf, sizeof(linebuf), "exit 1\n");
					goto reparse;
				}

				next = extract_word(ptr, sizeof(linebuf)+linebuf-ptr);
				if(count == 0 && (strcmp(ptr, ".") == 0 || strlen(ptr) == 0))
					break;

				src_paths[count] = strdup(ptr);
				if(!src_paths[count])
				{
					fprintf(stderr, "\n\n\n** Critical Error **\n"
					                "Out of memory duplicating string "
					                "'%s'\n\n", ptr);
					snprintf(linebuf, sizeof(linebuf), "exit -1\n");
					goto reparse;
				}
				++count;
				ptr = next;
			}
			continue;
		}

		/* Deletes the specified executables, appending 'EXE_EXT' to the names
		 * as needed */
		if(strcasecmp("rmexec", linebuf) == 0)
		{
			char *next = ptr;
			while(*(ptr=next))
			{
				next = extract_word(ptr, sizeof(linebuf)+linebuf-ptr);
				snprintf(buffer, sizeof(buffer), "'%s'${EXE_EXT}", ptr);
				expand_string(buffer, "", sizeof(buffer), 0);

				if(stat(buffer, &statbuf) == -1 && errno == ENOENT)
					continue;

				if(!shh)
				{
					if(verbose) printf("remove(\"%s\");\n", buffer);
					else        printf("Deleting %s...\n", buffer);
					fflush(stdout);
				}
				if((ret=remove(buffer)) != 0)
				{
					if(ignore_err < 2)
						printf("!!! Could not delete file !!!\n");
					if(!ignore_err)
					{
						snprintf(linebuf, sizeof(linebuf), "exit %d\n", ret);
						goto reparse;
					}
					if(ignore_err < 2)
						printf("--- Error %d ignored. ---\n\n",
						       ++ignored_errors);
					fflush(stdout);
				}
			}

			continue;
		}

		/* Deletes the specified libraries, prepending 'LIB_PRE' to the
		 * filename portions and appending with 'LIB_EXT' */
		if(strcasecmp("rmlib", linebuf) == 0)
		{
			char *next = ptr;
			while(*(ptr=next))
			{
				next = extract_word(ptr, sizeof(linebuf)+linebuf-ptr);
				libify_name(buffer, sizeof(buffer), ptr);

				if(stat(buffer, &statbuf) == -1 && errno == ENOENT)
					continue;

				if(!shh)
				{
					if(verbose) printf("remove(\"%s\");\n", buffer);
					else        printf("Deleting %s...\n", buffer);
					fflush(stdout);
				}
				if((ret=remove(buffer)) != 0)
				{
					if(ignore_err < 2)
						printf("!!! Could not delete file !!!\n");
					if(!ignore_err)
					{
						snprintf(linebuf, sizeof(linebuf), "exit %d\n", ret);
						goto reparse;
					}
					if(ignore_err < 2)
						printf("--- Error %d ignored. ---\n\n",
						       ++ignored_errors);
					fflush(stdout);
				}
			} while(*(ptr=next));
			continue;
		}

		/* Removes a list of object files and their associated dependancy
		 * files, replacing the extension of the named file as necesarry */
		if(strcasecmp("rmobj", linebuf) == 0)
		{
			char *ext;
			char *next = ptr;
			while(*(ptr=next))
			{
				next = extract_word(ptr, sizeof(linebuf)+linebuf-ptr);

				ext = strrchr(ptr, '/');
				if(!ext) ext = ptr;
				ext = strrchr(ext, '.');

				if(ext) *ext = 0;
				snprintf(buffer, sizeof(buffer), "${OBJ_DIR}/'%s'${OBJ_EXT}",
				         ptr);
				expand_string(buffer, "", sizeof(buffer), 0);
				if(ext) *ext = '.';

				if(stat(buffer, &statbuf) == 0 || errno != ENOENT)
				{
					if(!shh)
					{
						if(verbose)	printf("remove(\"%s\");\n", buffer);
						else		printf("Deleting %s...\n", buffer);
						fflush(stdout);
					}
					if((ret=remove(buffer)) != 0)
					{
						if(ignore_err < 2)
							printf("!!! Could not delete file !!!\n");
						if(!ignore_err)
						{
							snprintf(linebuf, sizeof(linebuf), "exit %d\n", ret);
							goto reparse;
						}
						if(ignore_err < 2)
							printf("--- Error %d ignored. ---\n\n",
							       ++ignored_errors);
					}
				}

				if(ext) *ext = 0;
				snprintf(buffer, sizeof(buffer), "${DEP_DIR}/'%s'${DEP_EXT}",
				         ptr);
				expand_string(buffer, "", sizeof(buffer), 0);
				if(ext) *ext = '.';

				if(stat(buffer, &statbuf) == -1 && errno == ENOENT)
					continue;

				if(!shh)
				{
					if(verbose)	printf("remove(\"%s\");\n", buffer);
					else		printf("Deleting %s...\n", buffer);
					fflush(stdout);
				}
				if((ret=remove(buffer)) != 0)
				{
					if(ignore_err < 2)
						printf("!!! Could not delete file !!!\n");
					if(!ignore_err)
					{
						snprintf(linebuf, sizeof(linebuf), "exit %d\n", ret);
						goto reparse;
					}
					if(ignore_err < 2)
						printf("--- Error %d ignored. ---\n\n",
						       ++ignored_errors);
					fflush(stdout);
				}
			}
			continue;
		}

		/* Removes a list of files or empty directories */
		if(strcasecmp("rm", linebuf) == 0)
		{
			char *next = ptr;
			while(*(ptr=next))
			{
				next = extract_word(ptr, sizeof(linebuf)+linebuf-ptr);

				if(stat(ptr, &statbuf) == -1 && errno == ENOENT)
					continue;

				if(!shh)
				{
					if(verbose) printf("remove(\"%s\");\n", ptr);
					else        printf("Deleting %s...\n", ptr);
					fflush(stdout);
				}
				if((ret=remove(ptr)) != 0)
				{
					if(ignore_err < 2)
						printf("!!! Could not delete !!!\n");
					if(!ignore_err)
					{
						snprintf(linebuf, sizeof(linebuf), "exit %d\n", ret);
						goto reparse;
					}
					if(ignore_err < 2)
						printf("--- Error %d ignored. ---\n\n",
						       ++ignored_errors);
					fflush(stdout);
				}
			}
			continue;
		}

		/* Creates a directory (with mode 700 in Unix) */
		if(strcasecmp("mkdir", linebuf) == 0)
		{
			extract_line(ptr, sizeof(linebuf)+linebuf-ptr);

			if(!shh)
			{
				if(!verbose) printf("Creating directory %s/...\n", ptr);
#if defined(_WIN32)
				else         printf("mkdir(\"%s\");\n", ptr);
				fflush(stdout);
			}
			if((ret=mkdir(ptr)) != 0)
#else
				else         printf("mkdir(\"%s\", S_IRWXU);\n", ptr);
				fflush(stdout);
			}
			if((ret=mkdir(ptr, S_IRWXU)) != 0)
#endif
			{
				if(ignore_err < 2)
					printf("!!! Could not create directory !!!\n");
				if(!ignore_err)
				{
					snprintf(linebuf, sizeof(linebuf), "exit %d\n", ret);
					goto reparse;
				}
				if(ignore_err < 2)
					printf("--- Error %d ignored. ---\n\n", ++ignored_errors);
				fflush(stdout);
			}
			continue;
		}

		/* Enables/disables command verboseness */
		if(strcasecmp("verbose", linebuf) == 0)
		{
			extract_line(ptr, sizeof(linebuf)+linebuf-ptr);
			if(*ptr)
				verbose = atoi(ptr);
			continue;
		}

		/* Leaves the current script, falling back to the previous if the
		 * current was started with the invoke command. Otherwise, it behaves
		 * like exit */
		if(strcasecmp("uninvoke", linebuf) == 0)
		{
			extract_line(ptr, sizeof(linebuf)+linebuf-ptr);
			ret = atoi(ptr);
			if(!invoke_backup[0].f)
			{
				snprintf(linebuf, sizeof(linebuf), "exit %d\n", ret);
				goto reparse;
			}

			for(i = 1;i <= INVOKE_BKP_SIZE;++i)
			{
				if(i == INVOKE_BKP_SIZE || !invoke_backup[i].f)
				{
					--i;
					fclose(f);
					f = invoke_backup[i].f;
					invoke_backup[i].f = NULL;

					free(fname);
					fname = invoke_backup[i].fname;

					strcpy(linebuf, invoke_backup[i].bkp_lbuf);
					free(invoke_backup[i].bkp_lbuf);

					free(nextline);
					nextline = invoke_backup[i].bkp_nextline;

					curr_line = invoke_backup[i].bkp_line;
					did_else = invoke_backup[i].bkp_did_else;
					did_cmds = invoke_backup[i].bkp_did_cmds;
					do_level = invoke_backup[i].bkp_do_level;

					break;
				}
			}

			if(ret != 0)
			{
				if(!ignore_err)
				{
					snprintf(linebuf, sizeof(linebuf), "exit %d\n", ret);
					goto reparse;
				}
				if(ignore_err < 2)
					printf("--- Error %d ignored. ---\n\n", ++ignored_errors);
				fflush(stdout);
			}

			continue;
		}

		/* Copies a file */
		if(strcasecmp("copy", linebuf) == 0)
		{
			char *dfile, *end;
			struct stat st;

			dfile = extract_word(ptr, sizeof(linebuf)+linebuf-ptr);
			if(!(*dfile))
			{
				ret = 1;
				if(ignore_err < 2)
					printf("\n\n!!! %s error, line %d !!! \n"
					       "Improper arguments to 'copy'!\n", fname,
					        curr_line);
				if(!ignore_err)
				{
					snprintf(linebuf, sizeof(linebuf), "exit %d\n", ret);
					goto reparse;
				}
				if(ignore_err < 2)
					printf("--- Error %d ignored. ---\n", ++ignored_errors);
				fflush(stdout);
				continue;
			}
			ret = 0;
			end = extract_word(dfile, sizeof(linebuf)+linebuf-dfile);
			if(dfile[strlen(dfile)-1] == '/' || (stat(dfile, &st) == 0 &&
			                                     S_ISDIR(st.st_mode)))
			{
				char *fn = strrchr(ptr, '/');
				snprintf(obj, sizeof(obj), "%s%s%s", dfile,
				         ((dfile[strlen(dfile)-1]=='/')?"":"/"),
				         (fn?(fn+1):ptr));
				dfile = obj;
			}
			if(!shh)
			{
				if(verbose) printf("copy_file(\"%s\", \"%s\");\n", ptr, dfile);
				else        printf("Copying '%s' to '%s'...\n", ptr, dfile);
				fflush(stdout);
			}
			if((ret=copy_file(ptr, dfile)) != 0)
			{
				if(ignore_err < 2)
					printf("!!! Could not copy !!!\n");
				if(!ignore_err)
				{
					snprintf(linebuf, sizeof(linebuf), "exit %d\n", ret);
					goto reparse;
				}
				if(ignore_err < 2)
					printf("--- Error %d ignored. ---\n\n", ++ignored_errors);
				fflush(stdout);
			}
			extract_line(end, sizeof(linebuf)+linebuf-end);
			continue;
		}

		/* Copies a library file, prepending and appending the names as
		 * necesarry */
		if(strcasecmp("copylib", linebuf) == 0)
		{
			char *dfile, *end;

			dfile = extract_word(ptr, sizeof(linebuf)+linebuf-ptr);
			if(!(*dfile))
			{
				ret = 1;
				if(ignore_err < 2)
					printf("\n\n!!! %s error, line %d !!! \n"
					       "Improper arguments to 'copylib'!\n", fname,
					       curr_line);
				if(!ignore_err)
				{
					snprintf(linebuf, sizeof(linebuf), "exit %d\n", ret);
					goto reparse;
				}
				if(ignore_err < 2)
					printf("--- Error %d ignored. ---\n", ++ignored_errors);
				fflush(stdout);
				continue;
			}
			ret = 0;
			end = extract_word(dfile, sizeof(linebuf)+linebuf-dfile);
			if(dfile[strlen(dfile)-1] == '/')
			{
				char *fn = strrchr(ptr, '/');
				snprintf(obj, sizeof(obj), "%s%s", dfile, (fn?(fn+1):ptr));
				libify_name(buffer, sizeof(buffer), obj);
			}
			else
				libify_name(buffer, sizeof(buffer), dfile);

			libify_name(obj, sizeof(obj), ptr);

			if(!shh)
			{
				if(verbose) printf("copy_file(\"%s\", \"%s\");\n", obj, buffer);
				else        printf("Copying '%s' to '%s'...\n", obj, buffer);
				fflush(stdout);
			}
			if((ret=copy_file(obj, buffer)) != 0)
			{
				if(ignore_err < 2)
					printf("!!! Could not copy !!!\n");
				if(!ignore_err)
				{
					snprintf(linebuf, sizeof(linebuf), "exit %d\n", ret);
					goto reparse;
				}
				if(ignore_err < 2)
					printf("--- Error %d ignored. ---\n\n", ++ignored_errors);
				fflush(stdout);
			}
			extract_line(end, sizeof(linebuf)+linebuf-end);
			continue;
		}

		if(strcasecmp("chdir", linebuf) == 0)
		{
			extract_line(ptr, sizeof(linebuf)+linebuf-ptr);

			if(!shh)
			{
				if(verbose) printf("chdir(\"%s\");\n", ptr);
				else        printf("Moving to directory '%s'\n", ptr);
				fflush(stdout);
			}
			if((ret=chdir(ptr)) != 0)
			{
				if(ignore_err < 2)
					printf("!!! Could not change directory !!!\n");
				if(!ignore_err)
				{
					snprintf(linebuf, sizeof(linebuf), "exit %d\n", ret);
					goto reparse;
				}
				if(ignore_err < 2)
					printf("--- Error %d ignored. ---\n\n", ++ignored_errors);
				fflush(stdout);
			}
			continue;
		}

		/* Creates an association between a file extension and a command to
		 * compile files with that association via the compile command */
		if(strcasecmp("associate", linebuf) == 0)
		{
			char *cmd = extract_word(ptr, sizeof(linebuf)+linebuf-ptr);
			extract_line(cmd, sizeof(linebuf)+linebuf-cmd);

			add_association(ptr, cmd);
			continue;
		}

		/* Yay for DOS/Windows allowing \ as a directory seperator. Modify the
		 * named environment variable to replace \ with / */
		if(strcasecmp("fixpath", linebuf) == 0)
		{
			char *str, *val;
			char *end = extract_word(ptr, sizeof(linebuf)+linebuf-ptr);
			extract_line(end, sizeof(linebuf)+linebuf-ptr);

			val = strdup(getvar(ptr));
			str = val;

			while((str=strchr(str, '\\')) != NULL)
				*(str++) = '/';

			setenv(ptr, val, 1);
			free(val);

			continue;
		}

		/* Our "special" rem command. Using this, you can make a cbuild script
		 * double as a DOS/Windows .bat file. */
		if(strcasecmp("rem", linebuf) == 0)
		{
			memmove(linebuf, ptr, strlen(ptr)+1);
			goto reparse;
		}

		/* Changes the file to read input from. Pass nothing to switch to
		 * stdin. */
		if(strcasecmp("setinput", linebuf) == 0)
		{
			extract_line(ptr, sizeof(linebuf)+linebuf-ptr);
			
			if(infile != stdin)
				fclose(infile);

			ret = 0;
			if(*ptr)
			{
				infile = fopen(ptr, "r");
				if(!infile)
				{
					ret = 1;
					if(ignore_err < 2)
						printf("!!! Could not open file '%s' to read !!!\n",
						       ptr);
					if(!ignore_err)
					{
						snprintf(linebuf, sizeof(linebuf), "exit %d\n", ret);
						goto reparse;
					}
					if(ignore_err < 2)
						printf("--- Error %d ignored. ---\n\n",
						       ++ignored_errors);
					fflush(stdout);
				}
			}
			else
				infile = stdin;

			continue;
		}

		/* Reads keyboard input and stores the string in the named var. The
		 * trailing newline is stripped. */
		if(strcasecmp("read", linebuf) == 0)
		{
			extract_line(ptr, sizeof(linebuf)+linebuf-ptr);
			if(!(*ptr))
			{
				ret = 1;
				if(ignore_err < 2)
					printf("!!! No storage specified for read (line %d) !!!\n",
					       curr_line);
				if(!ignore_err)
				{
					snprintf(linebuf, sizeof(linebuf), "exit %d\n", ret);
					goto reparse;
				}
				if(ignore_err < 2)
					printf("--- Error %d ignored. ---\n\n", ++ignored_errors);
				fflush(stdout);
			}
			ret = 0;

			buffer[0] = 0;
			if(!infile || fgets(buffer, sizeof(buffer), infile) == NULL)
				ret = 1;

			while(strlen(buffer) > 0 && (buffer[strlen(buffer)-1] == '\n' ||
			                             buffer[strlen(buffer)-1] == '\r'))
				buffer[strlen(buffer)-1] = 0;

			if(!strlen(buffer))
				ret |= unsetenv(ptr);
			else
				ret |= setenv(ptr, buffer, 1);

			continue;
		}


		/* Exits the script with the specified exitcode */
		if(strcasecmp("exit", linebuf) == 0)
		{
			static int already_exited = 0;
			int inc = 0;
			int retval;

			if(already_exited)
			{
				printf("\n\n*** Critical error ***\n"
				       "Recursive exit call, aborting now!\n\n");
				exit(-1);
			}
			already_exited = 1;

			extract_line(ptr, sizeof(linebuf)+linebuf-ptr);
			retval = atoi(ptr);

			for(i = num_defines-1;(size_t)i < num_defines;--i)
			{
				if(strncasecmp(defines[i].name, "atexit_", 7) == 0)
					inc += snprintf(linebuf+inc, sizeof(linebuf)-inc, "%s\n",
					                defines[i].val);
			}
			snprintf(linebuf+inc, sizeof(linebuf)-inc, "__really_exit %d\n",
			         retval);

			free(nextline);
			nextline = NULL;
			goto reparse;
		}


		/* Does nothing. Ignores the line */
		if(strcasecmp("noop", linebuf) == 0)
		{
			extract_line(ptr, sizeof(linebuf)+linebuf-ptr);
			continue;
		}


		if(strcasecmp("__reset_cmd_args__", linebuf) == 0)
		{
			extract_line(ptr, sizeof(linebuf)+linebuf-ptr);

			for(i = 0;(size_t)i < cmd_argc;++i)
			{
				free(cmd_argv[i]);
				cmd_argv[i] = NULL;
			}
			cmd_argc = 0;
			argv = _argv;
			argc = _argc;
			continue;
		}

		/* Exits the script with the specified exitcode */
		if(strcasecmp("__really_exit", linebuf) == 0)
		{
			extract_line(ptr, sizeof(linebuf)+linebuf-ptr);
			exit(atoi(ptr));
		}


		for(i = 0;(size_t)i < num_defines;++i)
		{
			char *next;

			if(strcasecmp(defines[i].name, linebuf) != 0)
				continue;

			cmd_argv[cmd_argc++] = strdup(linebuf);
			while(*ptr != 0)
			{
				next = extract_word(ptr, sizeof(linebuf)+linebuf-ptr);
				cmd_argv[cmd_argc++] = strdup(ptr);
				ptr = next;
			}
			cmd_argv[cmd_argc] = NULL;

			argv = cmd_argv;
			argc = cmd_argc;

			snprintf(linebuf, sizeof(linebuf), "%s\n__reset_cmd_args__\n%s\n",
			         defines[i].val, (nextline?nextline:""));
			free(nextline);
			nextline = NULL;
			goto reparse;
		}


		printf("\n\n!!! %s error, line %d !!!\n"
		       "Unknown command '%s'\n\n", fname, curr_line, linebuf);
		break;
	}

	return ret;
}
