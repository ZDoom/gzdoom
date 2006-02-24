#define foobarbaz /*
# A shell script within a source file. Ugly, but it works...

TRGT='cbuild'
for i in $@ ; do
   if [ "$i" = "--make-compiled" ] ; then
      echo "Compiling '$TRGT', please wait..."
      gcc -W -Wall -O2 -o $TRGT $0
      exit $?
   fi
done

gcc -W -Wall -Werror -o /tmp/$TRGT "$0" && { "/tmp/$TRGT" `for i in $@ ; do echo "$i" ; done` ; RET=$? ; rm -f "/tmp/$TRGT" ; exit $RET ; }
exit $?

*/


#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#ifdef _WIN32

static void setenv(const char *env, const char *val, int overwrite)
{
	static char buf[128];
	if(!overwrite && getenv(env))
		return;

	snprintf(buf, sizeof(buf), "%s=%s", env, val);
	_putenv(buf);
}

static void unsetenv(const char *env)
{
	setenv(env, "", 1);
}

#ifdef _MSC_VER
#define strcasecmp stricmp
#define strncasecmp strnicmp
#define snprintf _snprintf
#endif

#endif /* Win32 */

#define INVOKE_BKP_SIZE 16
struct {
	FILE *f;
	char *bkp_lbuf;
	char *bkp_nextline;
	int bkp_line;
	int bkp_did_else;
	int bkp_did_cmds;
	int bkp_do_level;
} invoke_backup[INVOKE_BKP_SIZE];

static struct stat statbuf;
static char linebuf[64*1024];
static char buffer[64*1024];
static char *loaded_files;
static char *sources;
static char obj[64];
#define SRC_PATH_SIZE 32
static char *src_paths[SRC_PATH_SIZE];

/* getvar: Safely gets an environment variable, returning an empty string
 * instead of NULL
 */
static const char *getvar(const char *env)
{
	const char *var;
	var = getenv(env);
	return (var?var:"");
}

/* find_src: Attempts to find the named sourcefile by searching the paths
 * listed in src_paths. It returns the passed string if the file exists as-is,
 * or if it couldn't be found.
 */
static char *find_src(char *src)
{
	static char buf[64];
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

/* check_obj_deps: Checks a file's dependancy list. The dependancy file is a
 * file expected to be in dep_dir and with the same name, but with a different
 * extension. The format of the file is simply: 'file.o: dependancies...' where
 * a '\' at the end of the line can be used as a next-line continuation. If the
 * dependancy file exists, none of the dependancies are missing, and none have
 * a modification time after 'obj_time', the function will return 0. Otherwise
 * 1 is returned denoting a rebuild may be required.
 */
static int check_obj_deps(char *src, time_t obj_time)
{
	static char dep[64];
	char *buf;
	int bufsize;

	struct stat statbuf;
	char *ptr = obj;
	FILE *df;
	size_t i;

	ptr = strrchr(src, '/');
	if(!ptr) ptr = src;
	ptr = strrchr(ptr, '.');
	if(ptr) *ptr = 0;
	snprintf(dep, sizeof(dep), "%s/%s%s", getvar("DEP_DIR"), src,
	         getvar("DEP_EXT"));
	if(ptr) *ptr = '.';

	df = fopen(dep, "r");
	if(!df)
	{
		if(stat(src, &statbuf) != 0 || statbuf.st_mtime > obj_time)
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

	i = 0;
	while(fgets(buf+i, bufsize-i, df) != NULL)
	{
		i = strlen(buf);
		if(buf[i-1] != '\\')
			break;
	}

	fclose(df);

	ptr = strchr(buf, ':');
	if(!ptr)
	{
		free(buf);
		return 1;
	}
	++ptr;
	while(1)
	{
		char *stp;
		while(*ptr && isspace(*ptr))
		{
			if(*ptr == '\n')
			{
				free(buf);
				return 0;
			}
			++ptr;
		}
		if(!(*ptr))
		{
			free(buf);
			return 0;
		}

		stp = ptr;
		while(*stp && !isspace(*stp))
		{
			if(*stp == '\\')
				memmove(stp, stp+1, strlen(stp));
			++stp;
		}
		*(stp++) = 0;

		if(strcmp(ptr, "\n") != 0 && (stat(ptr, &statbuf) != 0 ||
		                              statbuf.st_mtime > obj_time))
		{
			free(buf);
			return 1;
		}
		ptr = stp;
	}
}

/* copy_file: Copies the source file  'sf' to 'df', preserving the source's
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

	fd = open(df, O_WRONLY|O_TRUNC|O_CREAT, statbuf.st_mode);
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

/* extract_word: Extract a word starting at the string pointed to by 'str'. If
 * the word begins with a ' or " character, everything until that same
 * character will be considered part of the word. Otherwise, the word ends at
 * the first encountered whitespace. Returns the beginning of the next word,
 * or the end of the string.
 */
static char *extract_word(char **str)
{
	char *end;
	char c;

	c = **str;
	if(c == '"' || c == '\'')
	{
		++(*str);
		end = *str;
		while(*end)
		{
			if(*end == c)
				break;
			++end;
		}
	}
	else
	{
		end = *str;
		while(!isspace(*end) && *end)
			++end;
	}
	if(*end) *(end++) = 0;
	while(isspace(*end) && *end)
		++end;

	return end;
}


/* build_obj_list: Builds a list of object files from the list of sources. If
 * any of the objects don't exist or have a modification time later than
 * 'base_time', the variable pointed to by 'do_link' will be set non-zero.
 */
static int build_obj_list(char *buffer, size_t bufsize, time_t base_time,
                          int *do_link)
{
	static char buf[64];

	struct stat statbuf;
	char *next = sources;
	char *ptr;
	int i = 0;

	while(*(ptr=next))
	{
		char *ext;
		char c = ' ';

		next = extract_word(&ptr);
		if(ptr > sources)
		{
			c = *(ptr-1);
			if(c != '"' && c != '\'')
				c = ' ';
		}

		ext = strrchr(ptr, '/');
		if(!ext) ext = ptr;
		ext = strrchr(ext, '.');

		if(ext) *ext = 0;
		snprintf(buf, sizeof(buf), "%s/%s%s", getvar("OBJ_DIR"), ptr,
		         getvar("OBJ_EXT"));
		if(ext) *ext = '.';

		if(!(*do_link) && (stat(buf, &statbuf) != 0 ||
		                   base_time < statbuf.st_mtime))
			*do_link = 1;

		i += snprintf(buffer+i, bufsize-i, " \"%s\"", buf);

		ptr += strlen(ptr);
		if(next > ptr)
			*ptr = c;
	}
	return i;
}

static int libify_name(char *buf, size_t buflen, char *name)
{
	int i;
	char *curr = strrchr(name, '/');
	if(curr)
	{
		*curr = 0;
		i = snprintf(buf, buflen, "%s/%s%s%s", name, getvar("LIB_PRE"),
		             curr+1, getvar("LIB_EXT"));
		*curr = '/';
	}
	else
		i = snprintf(buf, buflen, "%s%s%s", getvar("LIB_PRE"), name,
		             getvar("LIB_EXT"));
	return i;
}


static char *strrpbrk(char *str, char *brk, int len)
{
	char *ptr = str+len;

	while((--ptr) >= str)
	{
		char *c = brk;
		while(*c)
		{
			if(*(c++) == *ptr)
				return ptr;
		}
	}

	return NULL;
}


int main(int argc, char **argv)
{
	FILE *f;
	int do_level = 0, wait_for_done = 0;
	int did_cmds = 0, did_else = 0;
	int curr_line = 0;
	char *ptr, *nextline = NULL, *nextcmd = NULL;
	int ret = 0, tmp = 0, i;
	int ignored_errors = 0;
	int verbose = 0;
	int shh;

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
#ifdef _WIN32
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
	f = fopen("default.cbd", "r");
	if(!f)
	{
		fprintf(stderr, "\n\n\n*** Critical Error ***\n"
		                "Could not open 'default.cbd'!\n\n");
		exit(1);
	}

main_loop_start:
	while(1)
	{
		int line_count, len;
		int ignore_err = 0;
		int in_quote = 0;
		int has_do = 0;
		int reverse;

		/* If we already have the next line set, go do it */
		if(nextcmd)
		{
			memmove(linebuf, nextcmd, strlen(nextcmd)+1);
			nextcmd = NULL;
			goto reparse;
		}

		/* If we already have the next line set, go do it */
		if(nextline)
		{
			memmove(linebuf, nextline, strlen(nextline)+1);
			nextline = NULL;
			goto reparse;
		}

		/* Grab the next line and increment the line count */
		if(fgets(linebuf, sizeof(linebuf), f) == NULL)
		{
			if(!invoke_backup[0].f)
				break;

			/* If end of file, check if we should uninvoke and continue */
			for(i = 1;i <= INVOKE_BKP_SIZE;++i)
			{
				if(i == INVOKE_BKP_SIZE || !invoke_backup[i].f)
				{
					--i;
					fclose(f);
					f = invoke_backup[i].f;
					invoke_backup[i].f = NULL;

					strcpy(linebuf, invoke_backup[i].bkp_lbuf);
					free(invoke_backup[i].bkp_lbuf);

					if(invoke_backup[i].bkp_nextline)
					{
						nextline = linebuf+strlen(linebuf)+1;
						strcpy(nextline, invoke_backup[i].bkp_nextline);
						free(invoke_backup[i].bkp_nextline);
					}

					curr_line = invoke_backup[i].bkp_line;
					did_else = invoke_backup[i].bkp_did_else;
					did_cmds = invoke_backup[i].bkp_did_cmds;
					do_level = invoke_backup[i].bkp_do_level;

					goto main_loop_start;
				}
			}
			break;
		}
		++curr_line;

		line_count = 0;
		ptr = linebuf;
		do {
			while((ptr=strpbrk(ptr, "$'\"#")) != NULL)
			{
				/* Enter in-quote mode (disables ''-quoting and #-comments) */
				if(*ptr == '\"')
				{
					in_quote ^= 1;
					++ptr;
					continue;
				}

				/* A '#' kills the rest of the line */
				if(*ptr == '#' && !in_quote)
				{
					char *next = strchr(ptr, '\n');
					if(next)
						memmove(ptr, next, strlen(next)+1);
					else
						*ptr = 0;
					continue;
				}

				/* Jump past a hard quote (don't expand anything) */
				if(*ptr == '\'' && !in_quote)
				{
					int add_count = 0;
					char *next = strchr(ptr+1, '\'');
					/* Find the closing '. If it's not found, assume it's a
					 * multi-line quote */
					while(!next)
					{
						next = ptr+strlen(ptr);
						if(sizeof(linebuf) == next-linebuf)
						{
							printf("!!! Error, line %d !!!\n"
							       "Unterminated hard quote!", curr_line);
							ret = -1;
							goto end;
						}

						if(fgets(next, sizeof(linebuf)-(next-linebuf), f) ==
						   NULL)
							break;
						next = strchr(ptr+1, '\'');
						++add_count;
					}
					ptr = next;
					curr_line += add_count;
					++ptr;
					continue;
				}

				/* Apend the next line to this one */
				if(strcmp(ptr, "$\n") == 0)
				{
					ptr += 2;
					if(fgets(ptr, sizeof(linebuf)-(ptr-linebuf), f) == NULL)
						break;
					++curr_line;
					continue;
				}

				++ptr;
			}
			if(!in_quote)
				break;
			ptr = linebuf+strlen(linebuf);
			if(fgets(ptr, sizeof(linebuf)-(ptr-linebuf), f) == NULL)
			{
				printf("!!! Error, line %d !!!\n"
				       "Unterminated quote!", curr_line);
				ret = -1;
				goto end;
			}
			++line_count;
		} while(1);
		curr_line += line_count;

		/* Check for the special '$' char, expanding variables as needed */
		do {
			len = strlen(linebuf);
			in_quote = 0;
			tmp = 0;
			ptr = linebuf;

			while((ptr=strrpbrk(linebuf, "$'\"", len)) != NULL)
			{
				len = ptr-linebuf;

				/* Enter in-quote mode (disables ''-quoting and #-comments) */
				if(*ptr == '\"')
				{
					in_quote ^= 1;
					continue;
				}

				/* A '#' kills the rest of the line */
				if(*ptr == '#' && !in_quote)
				{
					char *next = strchr(ptr, '\n');
					if(next)
						memmove(ptr, next, strlen(next)+1);
					else
						*ptr = 0;
					continue;
				}

				/* Jump past a hard quote (don't expand anything) */
				if(*ptr == '\'' && !in_quote)
				{
					char *next;

					*ptr = 0;
					next = strrchr(linebuf, '\'');
					if(!next)
					{
						printf("!!! Error, line %d !!!\n"
						       "Unterminated hard quote!", curr_line);
						ret = -1;
						goto end;
					}
					*ptr = '\'';

					len = next-linebuf;
					continue;
				}

				++ptr;

				/* Insert the environment var named between the {} */
				if(*ptr == '{')
				{
					char *end = strchr(ptr, '}');
					if(!end)
						end = ptr+strlen(ptr);
					else
						*(end++) = 0;
					*(--ptr) = 0;
					snprintf(buffer, sizeof(buffer), "%s%s%s", linebuf,
					         getvar(ptr+2), end);
					strcpy(linebuf, buffer);
					tmp = 1;
					continue;
				}

				/* Run a special command, replacing the section */
				if(*ptr == '(')
				{
					char *next = strchr(ptr, ')'), *opt;
					if(!next)
						continue;

					tmp = 1;
					*(ptr-1) = 0;
					*(next++) = 0;

					++ptr;
					opt = extract_word(&ptr);

					/* Replaces the section with the specified command line
					 * option's value (in the format 'option=value') */
					if(strcasecmp(ptr, "getoptval") == 0)
					{
						const char *val = "";
						int len;
						len = snprintf(buffer, sizeof(buffer), "%s=", opt);
						for(i = 1;i < argc;++i)
						{
							if(strncasecmp(buffer, argv[i], len) == 0)
							{
								val = argv[i]+len;
								break;
							}
						}
						snprintf(buffer, sizeof(buffer), "%s%s%s", linebuf,
						         val, next);
						strcpy(linebuf, buffer);
						continue;
					}

					/* Returns a library-style name from the specified
					 * filename */
					if(strcasecmp(ptr, "libname") == 0)
					{
						libify_name(obj, sizeof(obj), opt);
						snprintf(buffer, sizeof(buffer), "%s%s%s", linebuf,
						         obj, next);
						strcpy(linebuf, buffer);
						continue;
					}

					if(strcasecmp("ifeq", ptr) == 0)
					{
						char *var2;
						char *val;

						var2 = strchr(opt, ',');
						if(!var2)
						{
							printf("\n\n!!! Error, line %d !!!\n"
							       "Malformed 'ifeq' sub-command!\n\n",
							       curr_line);
							ret = 1;
							goto end;
						}
						*(var2++) = 0;
						val = strchr(var2, ',');
						if(!val)
						{
							printf("\n\n!!! Error, line %d !!!\n"
							       "Malformed 'ifeq' sub-command!\n\n",
							       curr_line);
							ret = 1;
							goto end;
						}
						*(val++) = 0;
						if(strcmp(opt, var2) == 0)
						{
							char *sep = strchr(val, ',');
							if(sep) *sep = 0;
						}
						else
						{
							val = strchr(val, ',');
							if(val) ++val;
						}

						snprintf(buffer, sizeof(buffer), "%s%s%s", linebuf,
						         (val?val:""), next);
						strcpy(linebuf, buffer);
						continue;
					}

					printf("\n\n!!! Error, line %d !!!\n"
					       "Unknown sub-command '%s'\n\n", curr_line, ptr);
					goto end;
				}
			}
		} while(tmp);

		/* Now check for '$'-escapes */
		ptr = linebuf;
		while((ptr=strpbrk(ptr, "$'\"")) != NULL)
		{
			if(*ptr == '"')
			{
				ptr = strchr(ptr+1, '"');
				if(!ptr)
					break;
				continue;
			}

			if(*ptr == '\'' && !in_quote)
			{
				ptr = strchr(ptr+1, '\'');
				if(!ptr)
					break;
				continue;
			}

			++ptr;

			/* Just a normal "escaped" character, pull the rest of the line
			 * back one. '$\n' will be handled later */
			if(*ptr != '\n')
				memmove(ptr-1, ptr, strlen(ptr)+1);
			else
				++ptr;
		}

reparse:
		reverse = 0;
		shh = 0;

		/* Get the next line */
		if(!nextline)
		{
			int in_quotes = 0;

			nextline = linebuf;
			while(*nextline && *nextline != '\n')
			{
				if(*nextline == '"')
					in_quotes ^= 1;
				if(!in_quotes)
				{
					if(*nextline == '\'')
					{
						nextline = strchr(nextline+1, '\'');
						if(!nextline)
							break;
					}
					if(nextline[0] == '$' && nextline[1] == '\n')
						memmove(nextline, nextline+1, strlen(nextline));
				}
				++nextline;
			}
			if(nextline)
			{
				if(*nextline)
					*(nextline++) = 0;
				if(!(*nextline))
					nextline = NULL;
			}
		}

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

		/* Find the end of the command name */
		ptr = linebuf;
		while(*ptr && !isspace(*ptr))
			++ptr;

		if(*ptr)
		{
			/* If found, NULL it and get the beginning of the option */
			tmp = *ptr;
			*(ptr++) = 0;
			while(isspace(*ptr) && *ptr)
				++ptr;
		}

		/* Check for special 'level'ing commands */
		/* The 'do' command pushes us up a level, and checks for the next
		 * if-type command, which itself will let us know if we should start
		 * ignoring commands */
		if(strcasecmp("do", linebuf) == 0)
		{
			has_do = 1;
			++do_level;
			memmove(linebuf, ptr, strlen(ptr)+1);
			goto reparse;
		}
		/* 'else' toggles ignoring commands at the current level */
		if(strcasecmp("else", linebuf) == 0)
		{
			if(do_level <= 0)
			{
				fprintf(stderr, "\n\n!!! Error, line %d !!!\n"
				                "'else' statement encountered without 'do'!\n",
				                curr_line);
				goto end;
			}

			/* TODO: allow another if-type command to follow an else on the
			 * same level */
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
			if(do_level <= 0)
			{
				fprintf(stderr, "\n\n!!! Error, line %d !!!\n"
				                "'done' statement encountered without 'do'!\n",
				                curr_line);
				goto end;
			}
			wait_for_done &= ~(1<<(--do_level));
			did_else &= ~(1<<do_level);
			did_cmds &= ~(1<<do_level);
			continue;
		}

		if((wait_for_done & ((1<<do_level)-1)))
			continue;

		/* Set an environment variable. The value is space sensitive, so if you
		 * wish to use spaces, encapsulate the value in ''s. Using '?=' instead
		 * of '=' will only set the variable if it isn't already set. */
		if(strchr(linebuf, '=') > linebuf || strncmp(ptr, "+=", 2) == 0 ||
		   strncmp(ptr, "-=", 2) == 0 || strncmp(ptr, "?=", 2) == 0 ||
		   ptr[0] == '=')
		{
			char *val = strchr(linebuf, '=');
			int ovr = 1;

			if(!val)
			{
				val = ptr;
				if(val[0] != '=')
					++val;
			}
			ptr = linebuf;

			/* Restore stolen space */
			ptr[strlen(ptr)] = tmp;

			if(*(val-1) == '+')
			{
				*(val-1) = 0;
				++val;
				while(*val && isspace(*val))
					++val;
				extract_word(&ptr);
				extract_word(&val);
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

				extract_word(&ptr);
				extract_word(&val);
				len = strlen(val);

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
				++val;
				extract_word(&ptr);
				if(*getvar(ptr))
					ovr = 0;
			}
			else
			{
				*(val++) = 0;
				extract_word(&ptr);
			}
			while(*val && isspace(*val))
				++val;
			extract_word(&val);
			if(*val)
				setenv(ptr, val, ovr);
			else if(ovr)
				unsetenv(ptr);
			continue;
		}

		if(!(*ptr))
		{
			fprintf(stderr, "Malformed command, line #%d: '%s'\n", curr_line,
			        linebuf);
			continue;
		}

		/* The if command checks if 'val1 = val2' is true and processes the
		 * rest of the line if so. */
		if(strcasecmp("if", linebuf) == 0)
value_check:
		{
			char *next, *var2 = ptr;

			var2 = extract_word(&ptr);
			var2 = strchr(var2, '=');
			if(!var2)
			{
				printf("\n\n!!! Error, line %d !!!\n"
				       "Malformed 'if' command!\n\n", curr_line);
				ret = 1;
				goto end;
			}
			*(var2++) = 0;
			while(*var2 && isspace(*var2))
				++var2;
			extract_word(&ptr);
			next = extract_word(&var2);
			if(strcmp(ptr, var2) == 0)
				nextcmd = next;

			if(reverse)
				nextcmd = (char*)((long)nextcmd ^ (long)next);
			if(has_do)
				wait_for_done |= (nextcmd?0:1)<<(do_level-1);
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
			next = extract_word(&ptr);

			if(atoi(ptr) == ret)
				nextcmd = next;

			if(reverse)
				nextcmd = (char*)((long)nextcmd ^ (long)next);
			if(has_do)
				wait_for_done |= (nextcmd?0:1)<<(do_level-1);
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
			char *next;
			next = extract_word(&ptr);
#ifdef _WIN32
			if(strcasecmp("win32", ptr) == 0)
				nextcmd = next;
#endif
#ifdef __unix__
			if(strcasecmp("unix", ptr) == 0)
				nextcmd = next;
#endif
			if(reverse)
				nextcmd = (char*)((long)nextcmd ^ (long)next);
			if(has_do)
				wait_for_done |= (nextcmd?0:1)<<(do_level-1);
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
			char *next;
			next = extract_word(&ptr);

			for(i = 1;i < argc;++i)
			{
				if(strcasecmp(ptr, argv[i]) == 0)
				{
					nextcmd = next;
					break;
				}
			}

			if(reverse)
				nextcmd = (char*)((long)nextcmd ^ (long)next);
			if(has_do)
				wait_for_done |= (nextcmd?0:1)<<(do_level-1);
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

			next = extract_word(&ptr);
			if(stat(ptr, &statbuf) == 0)
				nextcmd = next;

			if(reverse)
				nextcmd = (char*)((long)nextcmd ^ (long)next);
			if(has_do)
				wait_for_done |= (nextcmd?0:1)<<(do_level-1);
			continue;
		}
		if(strcasecmp("ifnexist", linebuf) == 0)
		{
			reverse = 1;
			goto file_dir_check;
		}

		if((wait_for_done & ((1<<do_level)-1)))
			continue;

		/* Reset the return value and 'do' status */
		has_do = 0;
		ret = 0;

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
			if(!shh)
			{
				printf("%s\n", ptr);
				fflush(stdout);
			}
			if((ret=system(ptr)) != 0)
			{
				if(!ignore_err)
					goto end;
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
			snprintf(buffer, sizeof(buffer), "%s %s", argv[0],
			         ((strcmp(ptr, ".")==0)?"":ptr));
			if((ret=system(buffer)) != 0)
			{
				if(ignore_err < 2)
					printf("!!! Error, line %d !!!\n"
					       "'reinvoke' returned with exitcode %d!\n",
					       ++ignored_errors, ret);
				if(!ignore_err)
					goto end;
				if(ignore_err < 2)
					printf("--- Error %d ignored. ---\n\n", ++ignored_errors);
				fflush(stdout);
			}

			continue;
		}

		/* Invokes an alternate script within the same context, and returns
		 * control when it exits. All variables and command-line options will
		 * persists into and out of the secondary script, and an invoke'd
		 * script cannot invoke another. Errors and exit calls will cause both
		 * scripts to abort. To safely exit from an invoked script before the
		 * end of the file, and continue the original, use 'uninvoke' */
		if(strcasecmp("invoke", linebuf) == 0)
		{
			int i;
			FILE *f2;

			extract_word(&ptr);
			f2 = f;
			f = fopen(ptr, "r");
			if(!f)
			{
				f = f2;
				if(!ignore_err)
				{
					printf("Could not open script '%s'!\n", ptr);
					goto end;
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

			for(i = 0;i < INVOKE_BKP_SIZE;++i)
			{
				if(!invoke_backup[i].f)
				{
					invoke_backup[i].f = f2;

					invoke_backup[i].bkp_lbuf = strdup(linebuf);

					invoke_backup[i].bkp_nextline = (nextline ?
					                                 strdup(nextline) : NULL);

					invoke_backup[i].bkp_line = curr_line;
					invoke_backup[i].bkp_did_else = did_else;
					invoke_backup[i].bkp_did_cmds = did_cmds;
					invoke_backup[i].bkp_do_level = do_level;

					break;
				}
			}

			curr_line = 0;
			nextline = NULL;

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
			free(sources);
			sources = strdup(ptr);
			if(!sources)
			{
				fprintf(stderr, "\n\n\n** Critical Error **\n"
				                "Out of memory duplicating string '%s'\n\n",
				                ptr);
				ret = -1;
				goto end;
			}
			ptr = sources;
compile_more_sources:
			tmp = 0;
			while(ptr && *ptr)
			{
				char *src, *ext, *next, c;
				struct stat statbuf;

				next = extract_word(&ptr);
				c = *(ptr-1);
				if(c != '\'' && c != '"')
					c = ' ';

				ext = strrchr(ptr, '/');
				if(!ext) ext = ptr;
				ext = strrchr(ext, '.');
				if(!ext)
					goto next_src_file;

				*ext = 0;
				snprintf(obj, sizeof(obj), "%s/%s%s", getvar("OBJ_DIR"),
				         ptr, getvar("OBJ_EXT"));
				*ext = '.';
				if(stat(obj, &statbuf) == 0)
				{
					if(!check_obj_deps(ptr, statbuf.st_mtime))
						goto next_src_file;
				}

				src = find_src(ptr);

				i = 0;
				if(strcmp(ext+1, "c")==0)
					i += snprintf(buffer+i, sizeof(buffer)-i, "%s %s %s",
					              getvar("CC"), getvar("CPPFLAGS"),
					              getvar("CFLAGS"));
				else if(strcmp(ext+1, "cpp")==0 || strcmp(ext+1, "cc")==0 ||
				        strcmp(ext+1, "cxx")==0)
					i += snprintf(buffer+i, sizeof(buffer)-i, "%s %s %s",
					              getvar("CXX"), getvar("CPPFLAGS"),
					              getvar("CXXFLAGS"));
				else
					goto next_src_file;

				if(*getvar("DEP_OPT"))
				{
					if(ext) *ext = 0;
					i += snprintf(buffer+i, sizeof(buffer)-i, " %s\"%s/%s%s\"",
					              getvar("DEP_OPT"), getvar("DEP_DIR"), ptr,
					              getvar("DEP_EXT"));
					if(ext) *ext = '.';
				}

				i += snprintf(buffer+i, sizeof(buffer)-i, " %s\"%s\"",
				              getvar("SRC_OPT"), src);
				i += snprintf(buffer+i, sizeof(buffer)-i, " %s\"%s\"",
				              getvar("OUT_OPT"), obj);
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
					tmp = 1;
					if(!ignore_err)
						goto end;
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
				{
					*(ptr++) = c;
					if(ptr < next)
						memmove(ptr, next, strlen(next)+1);
				}
			}
			ret = tmp;
			continue;
		}

		/* Adds more source files to the list, and compiles them as above */
		if(strcasecmp("compileadd", linebuf) == 0)
		{
			int pos = 0;
			char *t;

			if(sources)
				pos = strlen(sources);
			t = realloc(sources, pos + strlen(ptr) + 2);
			if(!t)
			{
				fprintf(stderr, "\n\n\n** Critical Error **\n"
				                "Out of memory appending string '%s'\n\n",
				                ptr);
				ret = -1;
				goto end;
			}
			sources = t;
			sources[pos] = ' ';
			strcpy(sources+pos+1, ptr);
			ptr = sources+pos+1;
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
				fprintf(stderr, "\n\n!!! Error, line %d !!!\n"
				                "Trying to build %s with undefined "
				                "sources!\n\n", curr_line, ptr);
				ret = 1;
				goto end;
			}

			extract_word(&ptr);

			snprintf(obj, sizeof(obj), "%s%s", ptr, getvar("EXE_EXT"));
			if(stat(obj, &statbuf) == 0)
			{
				exe_time = statbuf.st_mtime;
				do_link = 0;
			}

			i = 0;
			i += snprintf(buffer+i, sizeof(buffer)-i, "%s", getvar("LD"));
			i += snprintf(buffer+i, sizeof(buffer)-i, " %s\"%s\"",
			              getvar("OUT_OPT"), obj);

			i += build_obj_list(buffer+i, sizeof(buffer)-i, exe_time,
			                    &do_link);

			if(!do_link)
				continue;

			i += snprintf(buffer+i, sizeof(buffer)-i, " %s",
			              getvar("LDFLAGS"));

			if(!shh)
			{
				if(verbose)
					printf("%s\n", buffer);
				else
					printf("Linking %s%s...\n", ptr, getvar("EXE_EXT"));
				fflush(stdout);
			}
			if((ret=system(buffer)) != 0)
			{
				if(!ignore_err)
					goto end;
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
				fprintf(stderr, "\n\n!!! Error, line %d !!!\n"
				                "Trying to build %s with undefined "
				                "sources!\n\n", curr_line, ptr);
				ret = 1;
				goto end;
			}

			extract_word(&ptr);

			libify_name(obj, sizeof(obj), ptr);
			if(stat(obj, &statbuf) == 0)
			{
				lib_time = statbuf.st_mtime;
				do_link = 0;
			}

			i = 0;
			i += snprintf(buffer+i, sizeof(buffer)-i, "%s %s", getvar("AR"),
			              getvar("AR_OPT"));
			i += snprintf(buffer+i, sizeof(buffer)-i, " \"%s\"", obj);

			i += build_obj_list(buffer+i, sizeof(buffer)-i, lib_time,
			                    &do_link);

			if(!do_link)
				continue;

			libify_name(obj, sizeof(obj), ptr);
			remove(obj);

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
					goto end;
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
			free(loaded_files);
			loaded_files = strdup(ptr);
			if(!loaded_files)
			{
				fprintf(stderr, "\n\n\n** Critical Error **\n"
				                "Out of memory duplicating string '%s'\n\n",
				                ptr);
				ret = -1;
				goto end;
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
					printf("\n\n!!! Error, line %d !!!\n"
				       "'loadexec' called with no files!\n\n", curr_line);
				if(!ignore_err)
					goto end;
				if(ignore_err < 2)
					printf("--- Error %d ignored. ---\n\n", ++ignored_errors);
				fflush(stdout);
				continue;
			}

			curr = loaded_files;
			tmp = 0;
			while(*curr)
			{
				char *cmd_ptr = ptr;
				char *nuller;
				char c = ' ';

				next = extract_word(&curr);
				if(curr > loaded_files && (*(curr-1) == '\'' ||
				                           *(curr-1) == '"'))
					c = *(curr-1);

				i = 0;
				while(*cmd_ptr)
				{
					do {
						nuller = strchr(cmd_ptr, '<');
						if(!nuller || (nuller[1] == '@' && nuller[2] == '>'))
							break;
						cmd_ptr = nuller+1;
					} while(1);
					if(!nuller)
						break;
					*nuller = 0;

					i += snprintf(buffer+i, sizeof(buffer)-i, "%s%s", cmd_ptr,
					              curr);

					*nuller = '<';
					cmd_ptr = nuller+3;
				}
				i += snprintf(buffer+i, sizeof(buffer)-i, "%s", cmd_ptr);

				if(!shh)
				{
					printf("%s\n", buffer);
					fflush(stdout);
				}
				if((ret=system(buffer)) != 0)
				{
					tmp = 1;
					if(!ignore_err)
						goto end;
					if(ignore_err < 2)
						printf("--- Error %d ignored. ---\n\n",
						       ++ignored_errors);
					fflush(stdout);
				}

				curr += strlen(curr);
				if(*next)
				{
					*(curr++) = c;
					if(curr < next)
						memmove(curr, next, strlen(next)+1);
				}
			}
			ret = tmp;
			continue;
		}

		/* Prints a string to the console. If a '.' char is used at the
		 * beginning of string, it will be skipped */
		if(strcasecmp("echo", linebuf) == 0)
		{
			if(ptr[0] == '.')
				++ptr;
			printf("%s\n", ptr);
			fflush(stdout);
			continue;
		}

		/* Prints a string to the console without a newline. If a '.' char is
		 * used at the beginning of string, it will be skipped */
		if(strcasecmp("put", linebuf) == 0)
		{
			if(ptr[0] == '.')
				++ptr;
			printf("%s", ptr);
			fflush(stdout);
			continue;
		}

		/* Creates/truncates a file and writes a string to it */
		if(strcasecmp("writefile", linebuf) == 0)
		{
			char *str;
			FILE *o;

			str = extract_word(&ptr);
			o = fopen(ptr, "w");
			if(!o)
			{
				ret = 1;
				if(!ignore_err)
				{
					printf("Could not create file '%s'!\n", ptr);
					goto end;
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

			if(str[0] == '.')
				++str;
			fprintf(o, "%s\n", str);
			fclose(o);
			continue;
		}

		/* Appends a string to a file */
		if(strcasecmp("appendfile", linebuf) == 0)
		{
			char *str;
			FILE *o;

			str = extract_word(&ptr);
			o = fopen(ptr, "a");
			if(!o)
			{
				ret = 1;
				if(!ignore_err)
				{
					printf("Could not create file '%s'!\n", ptr);
					goto end;
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

			if(str[0] == '.')
				++str;
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
			lbl = strdup(ptr);
			if(!lbl)
			{
				fprintf(stderr, "\n\n\n** Critical Error **\n"
				                "Out of memory duplicating string '%s'\n\n",
				                lbl);
				ret = -1;
				goto end;
			}

			rewind(f);
			src_line = curr_line;
			curr_line = 0;
			while(fgets(linebuf, sizeof(linebuf), f) != NULL)
			{
				++curr_line;
				if(linebuf[0] == '#' && linebuf[1] == ':')
				{
					ptr = strpbrk(linebuf, "\r\n");
					if(ptr) *ptr = 0;
					if(strcasecmp(linebuf+2, lbl) == 0)
					{
						free(lbl);
						nextline = NULL;
						goto main_loop_start;
					}
				}
			}
			fprintf(stderr, "\n\n!!! Error, line %d !!!\n"
			                "Label target '%s' not found!\n\n", src_line, lbl);
			free(lbl);
			ret = -1;
			goto end;
		}


		/* Stores a list of paths to look for source files in. Passing only
		 * '.' clears the list. */
		if(strcasecmp("src_paths", linebuf) == 0)
		{
			unsigned int count = 0;
			char *next;

			while(src_paths[count] && count < SRC_PATH_SIZE)
			{
				free(src_paths[count]);
				src_paths[count] = NULL;
				++count;
			}

			if(strcmp(ptr, ".") == 0)
				continue;

			count = 0;
			while(*ptr)
			{
				if(count >= SRC_PATH_SIZE)
				{
					printf("\n\n!!! Error, line %d !!!\n"
					       "Too many source paths specified!\n\n",
					       ++curr_line);
					ret = -1;
					goto end;
				}

				next = extract_word(&ptr);
				src_paths[count] = strdup(ptr);
				if(!src_paths[count++])
				{
					fprintf(stderr, "\n\n\n** Critical Error **\n"
					                "Out of memory duplicating string "
					                "'%s'\n\n", ptr);
					ret = -1;
					goto end;
				}
				ptr = next;
			}
			continue;
		}

		/* Deletes the specified executables, appending 'EXE_EXT' to the names
		 * as needed */
		if(strcasecmp("rmexec", linebuf) == 0)
		{
			char *next;
			do {
				next = extract_word(&ptr);
				snprintf(buffer, sizeof(buffer), "%s%s", ptr,
				         getvar("EXE_EXT"));

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
						goto end;
					if(ignore_err < 2)
						printf("--- Error %d ignored. ---\n\n",
						       ++ignored_errors);
					fflush(stdout);
				}
			} while(*(ptr=next));

			continue;
		}

		/* Deletes the specified libraries, prepending 'LIB_PRE' to the
		 * filename portions and appending with 'LIB_EXT' */
		if(strcasecmp("rmlib", linebuf) == 0)
		{
			char *next;
			do {
				next = extract_word(&ptr);
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
						goto end;
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
			char *next;
			do {
				next = extract_word(&ptr);

				ext = strrchr(ptr, '/');
				if(!ext) ext = ptr;
				ext = strrchr(ext, '.');

				if(ext) *ext = 0;
				snprintf(buffer, sizeof(buffer), "%s/%s%s", getvar("OBJ_DIR"),
				         ptr, getvar("OBJ_EXT"));
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
							goto end;
						if(ignore_err < 2)
							printf("--- Error %d ignored. ---\n\n",
							       ++ignored_errors);
					}
				}

				if(ext) *ext = 0;
				snprintf(buffer, sizeof(buffer), "%s/%s%s", getvar("DEP_DIR"),
				         ptr, getvar("DEP_EXT"));
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
						goto end;
					if(ignore_err < 2)
						printf("--- Error %d ignored. ---\n\n",
						       ++ignored_errors);
					fflush(stdout);
				}
			} while(*(ptr=next));
			continue;
		}

		/* Removes a list of files or empty directories */
		if(strcasecmp("rm", linebuf) == 0)
		{
			char *next;
			do {
				next = extract_word(&ptr);

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
						goto end;
					if(ignore_err < 2)
						printf("--- Error %d ignored. ---\n\n",
						       ++ignored_errors);
					fflush(stdout);
				}
			} while(*(ptr=next));
			continue;
		}

		/* Creates a directory (with mode 700 in Unix) */
		if(strcasecmp("mkdir", linebuf) == 0)
		{
			extract_word(&ptr);

			if(!shh)
			{
				if(!verbose) printf("Creating directory %s/...\n", ptr);
#ifdef _WIN32
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
					goto end;
				if(ignore_err < 2)
					printf("--- Error %d ignored. ---\n\n", ++ignored_errors);
				fflush(stdout);
			}
			continue;
		}

		/* Enables/disables command verboseness */
		if(strcasecmp("verbose", linebuf) == 0)
		{
			verbose = (atoi(ptr) != 0);
			continue;
		}

		/* Leaves the current script, falling back to the previous if the
		 * current was started with the invoke command. Otherwise, it behaves
		 * like exit */
		if(strcasecmp("uninvoke", linebuf) == 0)
		{
			ret = atoi(ptr);
			if(!invoke_backup[0].f)
				goto end;

			for(i = 1;i <= INVOKE_BKP_SIZE;++i)
			{
				if(i == INVOKE_BKP_SIZE || !invoke_backup[i].f)
				{
					--i;
					fclose(f);
					f = invoke_backup[i].f;
					invoke_backup[i].f = NULL;

					strcpy(linebuf, invoke_backup[i].bkp_lbuf);
					free(invoke_backup[i].bkp_lbuf);

					if(invoke_backup[i].bkp_nextline)
					{
						nextline = linebuf+strlen(linebuf)+1;
						strcpy(nextline, invoke_backup[i].bkp_nextline);
						free(invoke_backup[i].bkp_nextline);
					}

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
					goto end;
				if(ignore_err < 2)
					printf("--- Error %d ignored. ---\n\n", ++ignored_errors);
				fflush(stdout);
			}

			continue;
		}

		/* Copies a file */
		if(strcasecmp("copy", linebuf) == 0)
		{
			char *dfile;

			dfile = extract_word(&ptr);
			if(!(*dfile))
			{
				if(ignore_err < 2)
					printf("\n\n!!! Error, line %d !!! \n"
					       "Improper arguments to 'copy'!\n", curr_line);
				if(!ignore_err)
					goto end;
				if(ignore_err < 2)
					printf("--- Error %d ignored. ---\n", ++ignored_errors);
				fflush(stdout);
				continue;
			}
			extract_word(&dfile);
			if(dfile[strlen(dfile)-1] == '/')
			{
				char *fn = strrchr(ptr, '/');
				snprintf(obj, sizeof(obj), "%s%s", dfile, (fn?(fn+1):ptr));
				dfile = obj;
			}
			if(!shh)
			{
				if(verbose) printf("copy_file(\"%s\", \"%s\");\n", ptr, dfile);
				else        printf("Copying '%s' to '%s'...\n", ptr, dfile);
				fflush(stdout);
			}
			if(copy_file(ptr, dfile) != 0)
			{
				if(ignore_err < 2)
					printf("!!! Could not copy !!!\n");
				if(!ignore_err)
					goto end;
				if(ignore_err < 2)
					printf("--- Error %d ignored. ---\n\n", ++ignored_errors);
				fflush(stdout);
			}
			continue;
		}

		/* Copies a library file, prepending and appending the names as
		 * necesarry */
		if(strcasecmp("copylib", linebuf) == 0)
		{
			char *dfile;

			dfile = extract_word(&ptr);
			if(!(*dfile))
			{
				if(ignore_err < 2)
					printf("\n\n!!! Error, line %d !!! \n"
					       "Improper arguments to 'copylib'!\n", curr_line);
				if(!ignore_err)
					goto end;
				if(ignore_err < 2)
					printf("--- Error %d ignored. ---\n", ++ignored_errors);
				fflush(stdout);
				continue;
			}
			extract_word(&dfile);
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
			if(copy_file(obj, buffer) != 0)
			{
				if(ignore_err < 2)
					printf("!!! Could not copy !!!\n");
				if(!ignore_err)
					goto end;
				if(ignore_err < 2)
					printf("--- Error %d ignored. ---\n\n", ++ignored_errors);
				fflush(stdout);
			}
			continue;
		}

		/* Copies an executable file, appending the names as necesarry */
		if(strcasecmp("copyexec", linebuf) == 0)
		{
			char *dfile;

			dfile = extract_word(&ptr);
			if(!(*dfile))
			{
				if(ignore_err < 2)
					printf("\n\n!!! Error, line %d !!! \n"
					       "Improper arguments to 'copyexec'!\n", curr_line);
				if(!ignore_err)
					goto end;
				if(ignore_err < 2)
					printf("--- Error %d ignored. ---\n", ++ignored_errors);
				fflush(stdout);
				continue;
			}
			extract_word(&dfile);
			if(dfile[strlen(dfile)-1] == '/')
			{
				char *fn = strrchr(ptr, '/');
				snprintf(obj, sizeof(obj), "%s%s", dfile, (fn?(fn+1):ptr));
				snprintf(buffer, sizeof(buffer), "%s%s", obj,
				         getvar("EXE_EXT"));
			}
			else
				snprintf(buffer, sizeof(buffer), "%s%s", dfile,
				         getvar("EXE_EXT"));

			libify_name(obj, sizeof(obj), ptr);
			snprintf(obj, sizeof(obj), "%s%s", ptr, getvar("EXE_EXT"));

			if(!shh)
			{
				if(verbose) printf("copy_file(\"%s\", \"%s\");\n", obj, buffer);
				else        printf("Copying '%s' to '%s'...\n", obj, buffer);
				fflush(stdout);
			}
			if(copy_file(obj, buffer) != 0)
			{
				if(ignore_err < 2)
					printf("!!! Could not copy !!!\n");
				if(!ignore_err)
					goto end;
				if(ignore_err < 2)
					printf("--- Error %d ignored. ---\n\n", ++ignored_errors);
				fflush(stdout);
			}
			continue;
		}

		/* Exits the script with the specified exitcode */
		if(strcasecmp("exit", linebuf) == 0)
		{
			ret = atoi(ptr);
			goto end;
		}

		printf("\n\n!!! Error, line %d !!!\n"
		       "Unknown command '%s'\n\n", curr_line, linebuf);
		break;
	}

end:
	fflush(stdout);
	i = 0;
	while(src_paths[i] && i < SRC_PATH_SIZE)
		free(src_paths[i++]);
	i = 0;
	while(i < INVOKE_BKP_SIZE && invoke_backup[i].f)
	{
		fclose(invoke_backup[i].f);
		free(invoke_backup[i].bkp_lbuf);
		free(invoke_backup[i].bkp_nextline);
		++i;
	}
	free(loaded_files);
	free(sources);
	fclose(f);
	return ret;
}
