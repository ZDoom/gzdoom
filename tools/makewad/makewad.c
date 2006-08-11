// makewad.c
//
// Reads a wad specification from stdin.
// Produces a wad file -or- writes a Makefile to stdout.

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include "zip.h"

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

/*
// appendlump(wadfile, filename):
//
//   open the file by filename, write all of its contents to the wadfile 
//
//   returns: 0 = success, 1 = error
*/

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
		// try to read a chunk of data
		readlen = fread (readbuf, 1, sizeof(readbuf), lumpfile);

		// if we reached the end, or hit an error
		if (readlen < sizeof(readbuf))
		{
			// if it's an error, 
			if (ferror (lumpfile))
			{
				// diagnose 
				fprintf (stderr, "Error reading %s: %s\n", filename, strerror(errno));
				// set return code to error
				ret |= 1;
			}
			// in any case, close the lump file to break the loop
			fclose (lumpfile);
			lumpfile = NULL;
		}

		// write whatever data we have in the buffer

		// if we hit an error (less bytes written than given)
		if (fwrite (readbuf, 1, readlen, wadfile) < readlen)
		{
			// diagnose
			fprintf (stderr, "Error writing to wad: %s\n", strerror(errno));
			// close the lump file to break the loop
			fclose (lumpfile);
			lumpfile = NULL;
			// set return code to error
			ret |= 1;
		}
		// if the lump file got closed, the loop exits
	}
	return ret;
}

/*
// appendtozip(zipFile, zipname, filename):
//
//   write a given lump file (filename) to the zipFile as zipname
// 
//   zipFile: zip object to be written to
//
//   zipname: name of the file inside the zip
//   filename: file to read data from
// 
// returns: 0 = success, 1 = error
*/
int appendtozip (zipFile zipfile, const char * zipname, const char *filename)
{
	char *readbuf;
	FILE *lumpfile;
	size_t readlen;
	size_t len;
	zip_fileinfo zip_inf;

	time_t currenttime;
	struct tm * ltime;

	// clear zip_inf structure
	memset(&zip_inf, 0, sizeof(zip_inf));

	// try to determine local time
	time(&currenttime);
	ltime = localtime(&currenttime);
	// if succeeded,
	if (ltime != NULL)
	{
		// put it into the zip_inf structure
		zip_inf.tmz_date.tm_sec = ltime->tm_sec;
		zip_inf.tmz_date.tm_min = ltime->tm_min;
		zip_inf.tmz_date.tm_hour = ltime->tm_hour;
		zip_inf.tmz_date.tm_mday = ltime->tm_mday;
		zip_inf.tmz_date.tm_mon = ltime->tm_mon;
		zip_inf.tmz_date.tm_year = ltime->tm_year;
	}

	// lumpfile = source file
	lumpfile = fopen (filename, "rb");
	if (lumpfile == NULL)
	{
		fprintf (stderr, "Could not open %s: %s\n", filename, strerror(errno));
		return 1;
	}
	// len = source size
	fseek (lumpfile, 0, SEEK_END);
	len = ftell(lumpfile);
	fseek (lumpfile, 0, SEEK_SET);

	// allocate a buffer for the whole source file
	readbuf = (char*)malloc(len);
	if (readbuf == NULL)
	{
		fclose(lumpfile);
		fprintf (stderr, "Could not allocate %d bytes\n", len);
		return 1;
	}
	// read the whole source file into buffer
	readlen =  fread (readbuf, 1, len, lumpfile);
	fclose(lumpfile);

	// if read less bytes than expected, 
	if (readlen != len)
	{
		// diagnose and return error
		free (readbuf);
		fprintf (stderr, "Unable to read %s\n", filename);
		return 1;
	}
	// file loaded

	// create zip entry, giving entry name and zip_inf data,
	// write data into zipfile, and close the zip entry
	if (Z_OK != zipAddFileToZip(zipfile, zipname, &zip_inf, readbuf, (unsigned)len))
	{
		free (readbuf);
		fprintf (stderr, "Unable to write %s to zip\n", filename);
		return 1;
	}
	// all done
	free (readbuf);
	return 0;
}

/*
// buildwad(listfile, listfilename, makecmd, makefile)
//
//  go through the listfile, either:
//   writing dependencies (listfile + lumps with files) into a makefile,
//   -or-
//   writing lumps into a wad/zip file
//
//  listfile: already opened source file
//  listfilename: filename, if any - only for the makefile
//  makecmd: given as argv[0] - only for the makefile
//  makefile: output filename for makefile. determines mode:
//   - if specified, we're writing deps into a makefile
//   - otherwise, we're writing lumps into a wad/zip
// 
*/
int buildwad (FILE *listfile, char *listfilename, const char *makecmd, char *makefile)
{
	// destination we're writing output into - 
	// one of these:
	zipFile zipfile = NULL;
	FILE *wadfile = NULL;

	wadinfo_t header;
	filelump_t directory[MAX_LUMPS];
	char str[256]; // buffer for reading listfile line by line
	char *pt;
	char *lumpname, *filename;
	int lineno = 0;
	int ret = 0;
	int i;

	// prepare PWAD data
	strncpy (header.magic, "PWAD", 4);
	header.infotableofs = 0;
	header.numlumps = 0;
	memset (directory, 0, sizeof(directory));

	// read the listfile line by line
	while (fgets (str, sizeof(str), listfile))
	{
		lineno++; // counting lines for diagnostic purposes

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
			if (wadfile != NULL || zipfile != NULL)
			{
				fprintf (stderr, "Line %d: Tried to reopen wadfile as %s.\n", lineno, pt + 1);
				if (wadfile != NULL) fclose (wadfile);
				if (zipfile != NULL) zipClose (zipfile, NULL);

				return 1;
			}

			if (!makefile) // if it's not a makefile,
			{
				int ln = (int)strlen(pt+1);

				filename = pt+1;
				if (ln >= 4)
				{
					// If the output file has an extension '.zip' or '.pk3' it will be in Zip format.
					if (!stricmp(filename+ln-3, "ZIP") || !stricmp(filename+ln-3, "PK3"))
					{
						zipfile = zipOpen(filename, APPEND_STATUS_CREATE);
						if (zipfile == NULL)
						{
							fprintf (stderr, "Line %d: Could not open %s: %s\n", lineno, filename, strerror(errno));
							return 1;
						}
					}
				}
			}
			else filename = makefile;

			if (!zipfile) // if we didn't end up opening zip, it's a regular file
			{
				// open wadfile
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
			}
			continue;
		} // @

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

		// must have output selected
		if (wadfile == NULL && zipfile == NULL)
		{
			fprintf (stderr, "Line %d: No wad specified before lumps.\n", lineno);
			return 1;
		}

		// if we're writing a makefile,
		if (makefile)
		{
			// and the lump has a filename,
			if (filename != NULL)
			{
				// add it as a dependency (with quotes, if needed)
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
		else // not writing a makefile
		{
			if (zipfile == NULL) // must be a wadfile
			{
				// convert lumpname to uppercase
				for (i = 0; lumpname[i]; ++i)
				{
					lumpname[i] = toupper(lumpname[i]);
				}
				// put name into directory entry
				strncpy (directory[header.numlumps].name, lumpname, 8);
				// put filepos into directory entry
				directory[header.numlumps].filepos = ftell (wadfile);
				// if filename given,
				if (filename != NULL)
				{
					// append data to the wadfile
					ret |= appendlump (wadfile, filename);
				}
				// put size into directory entry (how many bytes were just written)
				directory[header.numlumps].size = ftell (wadfile) - directory[header.numlumps].filepos;
			}
			else if (filename != NULL) // writing a zip, and filename is non-null
			{
				// convert lumpname to lowercase
				for (i = 0; lumpname[i]; ++i)
				{
					lumpname[i] = tolower(lumpname[i]);
				}
				// add lump data to the zip
				ret |= appendtozip(zipfile, lumpname, filename);
			}
			// count all lumps
			header.numlumps++;
		}

	} // end of line-by-line reading

	// the finishing touches:
	//  - makefile: terminate the line, and write the action line
	//  - unzipped wad: write the directory at the end, 
	//    and update the header with directory's position
	//  - zipped wad: just close it

	// if we were writing a plain file,
	if (wadfile != NULL)
	{
		// if it's makefile,
		if (makefile)
		{
			// terminate the dependencies line,
			// and write the action command
			fprintf (wadfile, "\n\t%s %s\n", makecmd, listfilename);
		}
		else // otherwise, it's plain wad
		{
			// the directory of lumps will be written at the end,
			// starting at the current position - put it into the header
			header.infotableofs = ftell (wadfile);

			// swap endianness, if needed
#ifdef WORDS_BIGENDIAN
#define SWAP(x) 	((((x)>>24)|(((x)>>8) & 0xff00)|(((x)<<8) & 0xff0000)|((x)<<24)))

			for (i = 0; i < header.numlumps; ++i)
			{
				directory[i].filepos = SWAP(directory[i].filepos);
				directory[i].size = SWAP(directory[i].size);
			}
#endif
			// write the whole directory of lumps
			if (fwrite (directory, sizeof(directory[0]), header.numlumps, wadfile) != header.numlumps)
			{
				// failed to write the whole directory
				fprintf (stderr, "Error writing to wad: %s\n", strerror(errno));
				ret |= 1;
			}
			else // success - seek to 0 and rewrite the header, now with offset
			{
				// endianness
#ifdef WORDS_BIGENDIAN
				SWAP(header.infotableofs);
				SWAP(header.numlumps);
#endif
				// seek to 0
				fseek (wadfile, 0, SEEK_SET);

				// rewrite the header
				if (fwrite (&header, sizeof(header), 1, wadfile) != 1)
				{
					fprintf (stderr, "Error writing to wad: %s\n", strerror(errno));
					ret |= 1;
				}
			}
		} // plain wad

		fclose (wadfile);
	} // wadfile!=NULL - wad or makefile
	else if (zipfile != NULL)
	{
		// zip - just close it
		zipClose(zipfile, NULL);
	}
	return ret;
}

// If building with GCC and not MinGW, macro __cdecl to nothing.
#if defined(__GNUC__) && !defined(__MINGW32__)
#define __cdecl
#endif

int __cdecl main (int argc, char **argv)
{
	const char *makecmd;
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

	// Hack for msys
	if ((makecmd = getenv("OSTYPE")) != NULL && strcmp (makecmd, "msys") == 0)
	{
		makecmd = "../tools/makewad/makewad.exe";
	}
	else
	{
		makecmd = argv[0];
	}
	ret = buildwad (listfile ? listfile : stdin, listfilename, makecmd, makefile);
	if (listfile != NULL)
	{
		fclose (listfile);
	}
	return ret;

baduse:
	fprintf (stderr, "Usage: makewad [-make makefile] [listfile]\n");
	return 1;
}
