// cmdlib.h

#ifndef __CMDLIB__
#define __CMDLIB__

#ifdef _WIN32
#pragma warning(disable : 4244)     // MIPS
#pragma warning(disable : 4136)     // X86
#pragma warning(disable : 4051)     // ALPHA

#pragma warning(disable : 4018)     // signed/unsigned mismatch
#pragma warning(disable : 4305)     // truncate from double to float
#endif

#include "doomtype.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <stdarg.h>

// the dec offsetof macro doesnt work very well...
#define myoffsetof(type,identifier) ((size_t)&((type *)0)->identifier)


char	*strupr (char *in);
char	*strlower (char *in);
int		Q_strncasecmp (char *s1, char *s2, int n);
int		Q_strcasecmp (char *s1, char *s2);
void	Q_getwd (char *out);

int		Q_filelength (FILE *f);

void	Q_mkdir (char *path);

extern	char	progdir[1024];
extern	char	gamedir[1024];

void	FixPathSeperator (char *path);
void	SetProgDir (void);
char	*ExpandArg (char *path);	// from cmd line
char	*ExpandPath (char *path);	// from scripts
char	*ExpandPathAndArchive (char *path);


FILE	*SafeOpenWrite (char *filename);
FILE	*SafeOpenRead (char *filename);
void	SafeRead (FILE *f, void *buffer, int count);
void	SafeWrite (FILE *f, void *buffer, int count);

int		LoadFile (char *filename, void **bufferptr);
int		TryLoadFile (char *filename, void **bufferptr);
void	SaveFile (char *filename, void *buffer, int count);
boolean	FileExists (char *filename);

void 	DefaultExtension (char *path, char *extension);
void 	DefaultPath (char *path, char *basepath);
void 	StripFilename (char *path);
void 	StripExtension (char *path);

void 	ExtractFilePath (char *path, char *dest);
void 	ExtractFileBase (char *path, char *dest);
void	ExtractFileExtension (char *path, char *dest);

int		ParseHex (char *str);
int 	ParseNum (char *str);
boolean IsNum (char *str);		// [RH] added

short	BigShort (short l);
short	LittleShort (short l);
int		BigLong (int l);
int		LittleLong (int l);
float	BigFloat (float l);
float	LittleFloat (float l);


char	*COM_Parse (char *data);

extern	char	com_token[1024];
extern	boolean	com_eof;

char	*copystring(char *s);


void	CRC_Init(unsigned short *crcvalue);
void	CRC_ProcessByte(unsigned short *crcvalue, byte data);
unsigned short CRC_Value(unsigned short crcvalue);

void	CreatePath (char *path);
void	QCopyFile (char *from, char *to);

extern	boolean		archive;
extern	char		archivedir[1024];

void	ExpandWildcards (int *argc, char ***argv);


// for compression routines
typedef struct
{
	byte	*data;
	int		count;
} cblock_t;


#endif
