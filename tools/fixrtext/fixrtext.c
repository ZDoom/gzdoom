/* fixrtext.c
**
** Given a coff-win32 object file, search for an .rtext section header and
** set its IMAGE_SCN_MEM_WRITE flag if it isn't already set. This gets
** around an NASM deficiency that prevents creating such files with
** "execute read write" sections.
**
** The author of this program disclaims copyright.
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

#ifndef _MSC_VER
#include <errno.h>

int fopen_s (FILE **pFile, const char *filename, const char *mode)
{
	if ((*pFile = fopen (filename, mode)) == NULL)
	{
		return errno;
	}
	return 0;
}
#endif

int main (int argc, char **argv)
{
	FILE *f;
	IMAGE_FILE_HEADER filehead;
	IMAGE_SECTION_HEADER secthead;
	int i;

	if (argc != 2)
		return 1;

	if (fopen_s (&f, argv[1], "r+b"))
	{
		fprintf (stderr, "Could not open %s\n", argv[1]);
		return 1;
	}

	if (fread (&filehead, sizeof filehead, 1, f) != 1 ||
		filehead.Machine != IMAGE_FILE_MACHINE_I386)
	{
		fprintf (stderr, "%s is not an x86 object file\n", argv[1]);
		fclose (f);
		return 1;
	}

	for (i = 0; i < filehead.NumberOfSections; ++i)
	{
		if (fread (&secthead, sizeof secthead, 1, f) != 1)
		{
			fprintf (stderr, "Could not read section header %d\n", i + 1);
			fclose (f);
			return 1;
		}
		if (memcmp (secthead.Name, ".rtext\0", IMAGE_SIZEOF_SHORT_NAME) == 0)
		{
			if (secthead.Characteristics & IMAGE_SCN_MEM_WRITE)
			{
				fprintf (stderr, "The .rtext section in %s is already writeable\n", argv[1]);
				fclose (f);
				return 0;
			}
			secthead.Characteristics |= IMAGE_SCN_MEM_WRITE;
			if (fseek (f, -(long)sizeof secthead, SEEK_CUR))
			{
				fprintf (stderr, "Failed to seek back to start of .rtext section header\n");
				fclose (f);
				return 1;
			}
			if (fwrite (&secthead, sizeof secthead, 1, f) != 1)
			{
				fprintf (stderr, "Failed to rewrite .rtext section header\n");
				fclose (f);
				return 1;
			}
/*			fprintf (stderr, "The .rtext section in %s was successfully made writeable\n", argv[1]);	*/
			fclose (f);
			return 0;
		}
	}
	fclose (f);
	return 0;
}
