// makewad.c
//
// Reads a wad specification from stdin.
// Produces a wad file -or- writes a Makefile to stdout.

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#define MAX_LUMPS	4096

typedef struct
{
	char		 magic[4];
	unsigned int numlumps;
	int			 infotableofs;

} wadinfo_t;


typedef struct
{
	int			 filepos;
	unsigned int size;
	char		 name[8];

} filelump_t;

int appendlump (FILE *wadfile, char *filename)
{
	char readbuf[64*1024];
	FILE *lumpfile;
	size_t readlen;
	int ret = 0;

	lumpfile = fopen (filename, "rb");
	if (lumpfile == NULL)
	{
		fprintf (stderr, "Could not open %s: %s\n", filename, strerror(errno));
		return 1;
	}
	while (lumpfile != NULL)
	{
		readlen = fread (readbuf, 1, sizeof(readbuf), lumpfile);
		if (readlen < sizeof(readbuf))
		{
			if (ferror (lumpfile))
			{
				fprintf (stderr, "Error reading %s: %s\n", filename, strerror(errno));
				ret |= 1;
			}
			fclose (lumpfile);
			lumpfile = NULL;
		}
		if (fwrite (readbuf, 1, readlen, wadfile) < readlen)
		{
			fprintf (stderr, "Error writing to wad: %s\n", strerror(errno));
			fclose (lumpfile);
			lumpfile = NULL;
			ret |= 1;
		}
	}
	return ret;
}

int buildwad (FILE *listfile, char *listfilename, char *makecmd, char *makefile)
{
	wadinfo_t header;
	filelump_t directory[MAX_LUMPS];
	char str[256];
	FILE *wadfile = NULL;
	char *pt;
	char *lumpname, *filename;
	int lineno = 0;
	int ret = 0;
	int i;

	strncpy (header.magic, "PWAD", 4);
	header.infotableofs = 0;
	header.numlumps = 0;
	memset (directory, 0, sizeof(directory));

	while (fgets (str, sizeof(str), listfile))
	{
		lineno++;

		// Strip comments
		pt = strchr (str, '#');
		if (pt) *pt = 0;

		// Strip trailing whitespace
		pt = str + strlen (str) - 1;
		while (pt >= str && isspace(*pt)) pt--;
		if (pt < str) continue;
		pt[1] = 0;

		// Skip leading whitespace
		pt = str;
		while (isspace(*pt)) pt++;

		if (*pt == '@')
		{ // Rest of line is wadfile to create
			if (wadfile != NULL)
			{
				fprintf (stderr, "Line %d: Tried to reopen wadfile as %s.\n", lineno, pt + 1);
				fclose (wadfile);
				return 1;
			}
			filename = makefile ? makefile : pt+1;
			wadfile = fopen (filename, makefile ? "w" : "wb");
			if (wadfile == NULL)
			{
				fprintf (stderr, "Line %d: Could not open %s: %s\n", lineno, filename, strerror(errno));
				return 1;
			}
			if (makefile)
			{ // Write out the only rule the makefile has
				fprintf (wadfile, "%s: %s", pt+1, listfilename);
			}
			else
			{
				// The correct header will be written once the wad is complete
				fwrite (&header, sizeof(header), 1, wadfile);
			}
			continue;
		}

		// Everything up to the next whitespace is the lump name
		lumpname = pt;
		filename = NULL;
		while (*pt && !isspace(*pt)) pt++;

		// If there is more, skip whitespace and use the remainder of the line
		// as the file to insert into the lump. If no filename is given, then
		// a 0-length marker lump is inserted.
		if (*pt)
		{
			*pt = 0;
			filename = pt + 1;
			while (*filename && isspace(*filename)) filename++;
			if (*filename == 0) filename = NULL;
		}


		if (wadfile == NULL)
		{
			fprintf (stderr, "Line %d: No wad specified before lumps.\n", lineno);
			return 1;
		}

		if (makefile)
		{
			if (filename != NULL)
			{
				if (strchr (filename, ' '))
				{
					fprintf (wadfile, " \\\n\t\"%s\"", filename);
				}
				else
				{
					fprintf (wadfile, " \\\n\t%s", filename);
				}
			}
		}
		else
		{
			for (i = 0; lumpname[i]; ++i)
			{
				lumpname[i] = toupper(lumpname[i]);
			}
			strncpy (directory[header.numlumps].name, lumpname, 8);
			directory[header.numlumps].filepos = ftell (wadfile);
			if (filename != NULL)
			{
				ret |= appendlump (wadfile, filename);
				directory[header.numlumps].size = ftell (wadfile) - directory[header.numlumps].filepos;
			}
			header.numlumps++;
		}
	}
	if (wadfile != NULL)
	{
		if (makefile)
		{
			fprintf (wadfile, "\n\t%s %s\n", makecmd, listfilename);
		}
		else
		{
			header.infotableofs = ftell (wadfile);
#ifdef WORDS_BIGENDIAN
#define SWAP(x) 	((((x)>>24)|(((x)>>8) & 0xff00)|(((x)<<8) & 0xff0000)|((x)<<24)))

			for (i = 0; i < header.numlumps; ++i)
			{
				directory[i].filepos = SWAP(directory[i].filepos);
				directory[i].size = SWAP(directory[i].size);
			}
#endif
			if (fwrite (directory, sizeof(directory[0]), header.numlumps, wadfile) != header.numlumps)
			{
				fprintf (stderr, "Error writing to wad: %s\n", strerror(errno));
				ret |= 1;
			}
			else
			{
#ifdef WORDS_BIGENDIAN
				SWAP(header.infotableofs);
				SWAP(header.numlumps);
#endif
				fseek (wadfile, 0, SEEK_SET);
				if (fwrite (&header, sizeof(header), 1, wadfile) != 1)
				{
					fprintf (stderr, "Error writing to wad: %s\n", strerror(errno));
					ret |= 1;
				}
			}
		}
		fclose (wadfile);
	}
	return ret;
}

int main (int argc, char **argv)
{
	FILE *listfile = NULL;
	char *listfilename = NULL;
	char *makefile = NULL;
	int ret;
	int i;

	for (i = 1; i < argc; ++i)
	{
		if (strcmp (argv[i], "-make") == 0)
		{
			if (i >= argc-1)
			{
				goto baduse;
			}
			makefile = argv[++i];
		}
		else if (listfile == NULL)
		{
			listfilename = argv[i];
			listfile = fopen (listfilename, "r");
			if (listfile == NULL)
			{
				fprintf (stderr, "Can't open %s: %s\n", listfilename, strerror(errno));
				return 1;
			}
		}
		else
		{
			fclose (listfile);
			goto baduse;
		}
	}
	if (makefile != NULL && listfile == NULL)
	{
		fprintf (stderr, "You must specify a listfile if you want a makefile\n");
		return 1;
	}

	ret = buildwad (listfile ? listfile : stdin, listfilename, argv[0], makefile);
	if (listfile != NULL)
	{
		fclose (listfile);
	}
	return ret;

baduse:
	fprintf (stderr, "Usage: makewad [-make makecommand makefile] [listfile]\n");
	return 1;
}
