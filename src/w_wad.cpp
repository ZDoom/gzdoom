
//**************************************************************************
//**
//** w_wad.c : Heretic 2 : Raven Software, Corp.
//**
//** $RCSfile: w_wad.c,v $
//** $Revision: 1.6 $
//** $Date: 95/10/06 20:56:47 $
//** $Author: cjr $
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#include "m_alloc.h"
#include "doomtype.h"
#include "m_argv.h"
#include "i_system.h"
#include "cmdlib.h"
#include "c_dispatch.h"
#include "w_wad.h"
#include "w_zip.h"
#include "m_crc32.h"
#include "v_text.h"
#include "templates.h"
#include "gi.h"

// MACROS ------------------------------------------------------------------

#define NULL_INDEX		(0xffff)

// TYPES -------------------------------------------------------------------

struct rffinfo_t
{
	// Should be "RFF\x18"
	DWORD		Magic;
	DWORD		Version;
	DWORD		DirOfs;
	DWORD		NumLumps;
};

struct rfflump_t
{
	BYTE		IDontKnow[16];
	DWORD		FilePos;
	DWORD		Size;
	BYTE		IStillDontKnow[8];
	BYTE		Flags;
	char		Extension[3];
	char		Name[8];
	BYTE		WhatIsThis[4];
};

struct grpinfo_t
{
	DWORD		Magic[3];
	DWORD		NumLumps;
};

struct grplump_t
{
	char		Name[12];
	DWORD		Size;
};

union MergedHeader
{
	DWORD magic[3];
	wadinfo_t wad;
	rffinfo_t rff;
	grpinfo_t grp;
};


//
// WADFILE I/O related stuff.
//
struct FWadCollection::LumpRecord
{
	char *		fullname;		// only valid for files loaded from a .zip file
	char		name[9];
	short		wadnum;
	WORD		flags;
	int			position;
	int			size;
	int			namespc;
	int			compressedsize;
};

class FWadCollection::WadFileRecord : public FileReader
{
public:
	WadFileRecord (FILE *file);
	WadFileRecord (const char * buffer, int length);
	~WadFileRecord ();

	long Seek (long offset, int origin);
	long Read (void *buffer, long len);

	const char * MemoryData;

	char *Name;
	DWORD FirstLump;
	DWORD LastLump;

};


// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void W_SysWadInit ();

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void PrintLastError ();

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

FWadCollection Wads;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
//
// uppercoppy
//
// [RH] Copy up to 8 chars, upper-casing them in the process
//==========================================================================

void uppercopy (char *to, const char *from)
{
	int i;

	for (i = 0; i < 8 && from[i]; i++)
		to[i] = toupper (from[i]);
	for (; i < 8; i++)
		to[i] = 0;
}

FWadCollection::FWadCollection ()
: FirstLumpIndex(NULL), NextLumpIndex(NULL),
  FirstLumpIndex_FullName(NULL), NextLumpIndex_FullName(NULL), 
  NumLumps(0)
{
}

FWadCollection::~FWadCollection ()
{
	if (FirstLumpIndex != NULL)
	{
		delete[] FirstLumpIndex;
		FirstLumpIndex = NULL;
	}
	if (NextLumpIndex != NULL)
	{
		delete[] NextLumpIndex;
		NextLumpIndex = NULL;
	}
	if (FirstLumpIndex_FullName != NULL)
	{
		delete[] FirstLumpIndex_FullName;
		FirstLumpIndex_FullName = NULL;
	}
	if (NextLumpIndex_FullName != NULL)
	{
		delete[] NextLumpIndex_FullName;
		NextLumpIndex_FullName = NULL;
	}
	for (DWORD i = 0; i < LumpInfo.Size(); ++i)
	{
		if (LumpInfo[i].fullname != NULL)
		{
			delete[] LumpInfo[i].fullname;
		}
	}
	LumpInfo.Clear();

	for (DWORD i = 0; i < Wads.Size(); ++i)
	{
		delete Wads[i];
	}
	Wads.Clear();
}

//==========================================================================
//
// W_InitMultipleFiles
//
// Pass a null terminated list of files to use. All files are optional,
// but at least one file must be found. Lump names can appear multiple
// times. The name searcher looks backwards, so a later file can
// override an earlier one.
//
//==========================================================================

void FWadCollection::InitMultipleFiles (wadlist_t **filenames)
{
	int numfiles;

	// open all the files, load headers, and count lumps
	numfiles = 0;
	Wads.Clear();
	LumpInfo.Clear();
	NumLumps = 0;

	while (*filenames)
	{
		wadlist_t *next = (*filenames)->next;
		int baselump = NumLumps;
		char name[PATH_MAX];

		// [RH] Automatically append .wad extension if none is specified.
		strcpy (name, (*filenames)->name);
		FixPathSeperator (name);
		DefaultExtension (name, ".wad");

		AddFile (name);
		M_Free (*filenames);
		*filenames = next;

		// The first two files are always zdoom.wad and the IWAD, which
		// do not contain skins.
		if (++numfiles > 2)
			SkinHack (baselump);
	}

	if (NumLumps == 0)
	{
		I_FatalError ("W_InitMultipleFiles: no files found");
	}

	// [RH] Merge sprite and flat groups.
	//		(We don't need to bother with patches, since
	//		Doom doesn't use markers to identify them.)
	RenameSprites (MergeLumps ("S_START", "S_END", ns_sprites));
	MergeLumps ("F_START", "F_END", ns_flats);
	MergeLumps ("C_START", "C_END", ns_colormaps);
	MergeLumps ("A_START", "A_END", ns_acslibrary);
	MergeLumps ("TX_START", "TX_END", ns_newtextures);
	MergeLumps ("V_START", "V_END", ns_strifevoices);
	MergeLumps ("HI_START", "HI_END", ns_hires);

	// [RH] Set up hash table
	FirstLumpIndex = new WORD[NumLumps];
	NextLumpIndex = new WORD[NumLumps];
	FirstLumpIndex_FullName = new WORD[NumLumps];
	NextLumpIndex_FullName = new WORD[NumLumps];
	InitHashChains ();
}

//-----------------------------------------------------------------------
//
// Adds an external file to the lump list but not to the hash chains
// It's just a simple means to assign a lump number to some file so that
// the texture manager can read from it.
//
//-----------------------------------------------------------------------

int FWadCollection::AddExternalFile(const char *filename)
{
	LumpRecord lump;

	lump.fullname = copystring(filename);
	memset(lump.name, 0, sizeof(lump.name));
	lump.wadnum=-1;
	lump.flags = LUMPF_EXTERNAL;
	lump.position = 0;
	lump.namespc = ns_global;
	lump.compressedsize = 0;
	return LumpInfo.Push(lump);
}

#define BUFREADCOMMENT (0x400)

//-----------------------------------------------------------------------
//
// Finds the central directory end record in the end of the file.
// Taken from Quake3 source but the file in question is not GPL'ed. ;)
//
//-----------------------------------------------------------------------

static DWORD Zip_FindCentralDir(FileReader * fin)
{
	unsigned char* buf;
	DWORD FileSize;
	DWORD uBackRead;
	DWORD uMaxBack; // maximum size of global comment
	DWORD uPosFound=0;

	fin->Seek(0, SEEK_END);

	FileSize = fin->Tell();
	uMaxBack = MIN<DWORD>(0xffff, FileSize);

	buf = (unsigned char*)malloc(BUFREADCOMMENT+4);
	if (buf == NULL) return 0;

	uBackRead = 4;
	while (uBackRead < uMaxBack)
	{
		DWORD uReadSize, uReadPos;
		int i;
		if (uBackRead +BUFREADCOMMENT > uMaxBack) 
			uBackRead = uMaxBack;
		else
			uBackRead += BUFREADCOMMENT;
		uReadPos = FileSize - uBackRead ;

		uReadSize = MIN<DWORD>((BUFREADCOMMENT+4) , (FileSize-uReadPos));

		if (fin->Seek(uReadPos,SEEK_SET) != 0) break;

		if (fin->Read(buf, (SDWORD)uReadSize) != (SDWORD)uReadSize) break;

		for (i=(int)uReadSize-3; (i--)>0;)
			if (((*(buf+i))==0x50) && ((*(buf+i+1))==0x4b) && 
				((*(buf+i+2))==0x05) && ((*(buf+i+3))==0x06))
			{
				uPosFound = uReadPos+i;
				break;
			}

			if (uPosFound!=0)
				break;
	}
	free(buf);
	return uPosFound;
}

//==========================================================================
//
// W_AddFile
//
// Files with a .wad extension are wadlink files with multiple lumps,
// other files are single lumps with the base filename for the lump name.
//
// [RH] Removed reload hack
//==========================================================================
int STACK_ARGS FWadCollection::lumpcmp(const void * a, const void * b)
{
	FWadCollection::LumpRecord * rec1 = (FWadCollection::LumpRecord *)a;
	FWadCollection::LumpRecord * rec2 = (FWadCollection::LumpRecord *)b;

	return stricmp(rec1->fullname, rec2->fullname);
}


void FWadCollection::AddFile (const char *filename, const char * data, int length)
{
	WadFileRecord	*wadinfo;
	MergedHeader	header;
	LumpRecord*		lump_p;
	unsigned		i;
	FILE*			handle;
	int				startlump;
	wadlump_t*		fileinfo = NULL, *fileinfo2free = NULL;
	wadlump_t		singleinfo;
	TArray<FZipFileInfo *> EmbeddedWADs;
	void * directory = NULL;

	if (length==-1)
	{
		// open the file and add to directory
		handle = fopen (filename, "rb");
		if (handle == NULL)
		{ // Didn't find file
			Printf (TEXTCOLOR_RED " couldn't open %s\n", filename);
			PrintLastError ();
			return;
		}

		wadinfo = new WadFileRecord (handle);
	}
	else
	{
		// This is an in-memory WAD created from a WAD inside a .zip
		wadinfo = new WadFileRecord(data, length);
	}
	Printf (" adding %s", filename);
	startlump = NumLumps;


	// [RH] Determine if file is a WAD based on its signature, not its name.
	if (wadinfo->Read (&header, sizeof(header)) == 0)
	{
		Printf (TEXTCOLOR_RED " couldn't read %s\n", filename);
		PrintLastError ();
		delete wadinfo;
		return;
	}

	wadinfo->Name = copystring (filename);

	if (header.magic[0] == IWAD_ID || header.magic[0] == PWAD_ID)
	{ // This is a WAD file

		header.wad.NumLumps = LittleLong(header.wad.NumLumps);
		header.wad.InfoTableOfs = LittleLong(header.wad.InfoTableOfs);
		fileinfo = fileinfo2free = new wadlump_t[header.wad.NumLumps];
		wadinfo->Seek (header.wad.InfoTableOfs, SEEK_SET);
		wadinfo->Read (fileinfo, header.wad.NumLumps * sizeof(wadlump_t));
		NumLumps += header.wad.NumLumps;
		Printf (" (%u lumps)", header.wad.NumLumps);
	}
	else if (header.magic[0] == RFF_ID)
	{ // This is a Blood RFF file

		rfflump_t *lumps, *rff_p;

		header.rff.NumLumps = LittleLong(header.rff.NumLumps);
		header.rff.DirOfs = LittleLong(header.rff.DirOfs);
		lumps = new rfflump_t[header.rff.NumLumps];
		wadinfo->Seek (header.rff.DirOfs, SEEK_SET);
		wadinfo->Read (lumps, header.rff.NumLumps * sizeof(rfflump_t));
		BloodCrypt (lumps, header.rff.DirOfs, header.rff.NumLumps * sizeof(rfflump_t));

		NumLumps += header.rff.NumLumps;
		LumpInfo.Resize(NumLumps);
		lump_p = &LumpInfo[startlump];

		for (i = 0, rff_p = lumps; i < header.rff.NumLumps; ++i, ++rff_p)
		{
			if (rff_p->Extension[0] == 'S' && rff_p->Extension[1] == 'F' &&
				rff_p->Extension[2] == 'X')
			{
				lump_p->namespc = ns_bloodsfx;
			}
			else if (rff_p->Extension[0] == 'R' && rff_p->Extension[1] == 'A' &&
				rff_p->Extension[2] == 'W')
			{
				lump_p->namespc = ns_bloodraw;
			}
			else
			{
				lump_p->namespc = ns_global;
			}

			uppercopy (lump_p->name, rff_p->Name);
			lump_p->name[8] = 0;
			lump_p->wadnum = (WORD)Wads.Size();
			lump_p->position = LittleLong(rff_p->FilePos);
			lump_p->size = LittleLong(rff_p->Size);
			lump_p->flags = (rff_p->Flags & 0x10) >> 4;
			lump_p->compressedsize = -1;

			// Rearrange the name and extension in a part of the lump record
			// that I don't have any use for in order to cnstruct the fullname.
			rff_p->Name[8] = '\0';
			sprintf ((char *)rff_p->IDontKnow, "%s.", rff_p->Name);
			rff_p->Name[0] = '\0';
			strcat ((char *)rff_p->IDontKnow, rff_p->Extension);
			lump_p->fullname = copystring ((char *)rff_p->IDontKnow);
			if (strstr ((char *)rff_p->IDontKnow, "TILE"))
				rff_p = rff_p;

			lump_p++;
		}
		Printf (" (%u files)", header.rff.NumLumps);
		delete[] lumps;
	}
	else if (header.magic[0] == GRP_ID_0 && header.magic[1] == GRP_ID_1 && header.magic[2] == GRP_ID_2)
	{
		grplump_t *lumps, *grp_p;
		int pos;

		header.grp.NumLumps = LittleLong(header.grp.NumLumps);
		lumps = new grplump_t[header.grp.NumLumps];
		wadinfo->Read (lumps, header.grp.NumLumps * sizeof(grplump_t));
		pos = sizeof(grpinfo_t) + header.grp.NumLumps * sizeof(grplump_t);

		NumLumps += header.grp.NumLumps;
		LumpInfo.Resize(NumLumps);
		lump_p = &LumpInfo[startlump];

		for (i = 0, grp_p = lumps; i < header.grp.NumLumps; ++i, ++grp_p)
		{
			lump_p->wadnum = (WORD)Wads.Size();
			lump_p->position = pos;
			lump_p->size = LittleLong(grp_p->Size);
			pos += lump_p->size;
			grp_p->Name[12] = '\0';	// Be sure filename is null-terminated
			lump_p->fullname = copystring(grp_p->Name);
			uppercopy (lump_p->name, grp_p->Name);
			lump_p->name[8] = 0;
			lump_p->compressedsize = -1;
			lump_p->flags = 0;
			lump_p->namespc = ns_global;
			lump_p++;
		}
		Printf (" (%u files)", header.grp.NumLumps);
		delete[] lumps;
	}
	else if (header.magic[0] == ZIP_ID)
	{
		DWORD centraldir = Zip_FindCentralDir(wadinfo);
		FZipCentralInfo info;
		int skipped = 0;

		if (centraldir==0)
		{
			Printf("\n%s: ZIP file corrupt!\n", filename);
			return;
		}

		// Read the central directory info.
		wadinfo->Seek(centraldir, SEEK_SET);
		wadinfo->Read(&info, sizeof(FZipCentralInfo));

		// No multi-disk zips!
		if (info.wEntryCount != info.wTotalEntryCount ||
			info.wNumberDiskWithCD != 0 || info.wNumberDisk != 0)
		{
			Printf("\n%s: Multipart Zip files are not supported.\n", filename);
			return;
		}

		NumLumps += LittleShort(info.wEntryCount);
		LumpInfo.Resize(NumLumps);
		lump_p = &LumpInfo[startlump];

		// Load the entire central directory. Too bad that this contains variable length entries...
		directory = malloc(LittleLong(info.dwCDSize));
		wadinfo->Seek(LittleLong(info.dwCDOffset), SEEK_SET);
		wadinfo->Read(directory, LittleLong(info.dwCDSize));

		char * dirptr =(char*)directory;
		for (int i = 0; i < LittleShort(info.wEntryCount); i++)
		{
			FZipFileInfo * zip_fh = (FZipFileInfo*)dirptr;
			char name[256];
			char base[256];

			int len = LittleShort(zip_fh->wFileNameSize);
			strncpy(name, dirptr + sizeof(FZipFileInfo), MIN<int>(len, 255));
			name[len]=0;
			dirptr += sizeof(FZipFileInfo) + 
					  LittleShort(zip_fh->wFileNameSize) + 
					  LittleShort(zip_fh->wExtraSize) + 
					  LittleShort(zip_fh->wCommentSize);
			
			// skip Directories
			if(name[len - 1] == '/' && LittleLong(zip_fh->dwSize) == 0) 
			{
				skipped++;
				continue;
			}

			// Ignore obsolete compression formats
			if(LittleShort(zip_fh->wCompression) != 0 && LittleShort(zip_fh->wCompression) != Z_DEFLATED)
			{
				Printf("\n: %s: '%s' uses an unsupported compression algorithm.\n", filename, name);
				skipped++;
				continue;
			}
			// Also ignore encrypted entries
			if(LittleShort(zip_fh->wFlags) & ZF_ENCRYPTED)
			{
				Printf("\n%s: '%s' is encrypted. Encryption is not supported.\n", filename, name);
				skipped++;
				continue;
			}

			FixPathSeperator(name);
			strlwr(name);

			// Check for embedded WADs in the root directory. 
			// They must be extracted and added separately to the lump list.
			// WADs in subdirectories are added to the lump directory.
			// Embedded .zips are ignored for now. But they should be allowed later!
			char * c = strstr(name, ".wad");
			if (c && strlen(c)==4 && !strchr(name, '/'))
			{
				EmbeddedWADs.Push(zip_fh);
				skipped++;
				continue;
			}

			//ExtractFileBase(name, base);
			char * lname=strrchr(name,'/');
			if (!lname) lname=name;
			else lname++;
			strcpy(base, lname);
			char * dot = strrchr(base,'.');
			if (dot) *dot=0;
			uppercopy(lump_p->name, base);
			lump_p->name[8] = 0;
			lump_p->fullname = copystring(name);
			lump_p->size = zip_fh->dwSize;

			// Map some directories to WAD namespaces.
			// Note that some of these namespaces don't exist in WADS.
			// CheckNumForName will handle any request for these namespaces accordingly.
			lump_p->namespc =	!strncmp(name, "flats/", 6)			? ns_flats :
								!strncmp(name, "textures/", 9)		? ns_newtextures :
								!strncmp(name, "hires/", 6)			? ns_hires :
								!strncmp(name, "sprites/", 8)		? ns_sprites :
								!strncmp(name, "colormaps/", 10)	? ns_colormaps :
								!strncmp(name, "acs/", 4)			? ns_acslibrary :
								!strncmp(name, "voices/", 7)		? ns_strifevoices :
								!strncmp(name, "patches/", 8)		? ns_patches :
								!strncmp(name, "graphics/", 9)		? ns_graphics :
								!strncmp(name, "sounds/", 7)		? ns_sounds :
								!strncmp(name, "music/", 6)			? ns_music : 
								!strchr(name, '/')					? ns_global : -1;
			
			// Anything that is not in one of these subdirectories or the main directory 
			// should not be accessible through the standard WAD functions but only through 
			// the ones which look for the full name.
			if (lump_p->namespc==-1)
			{
				memset(lump_p->name, 0, 8);
			}

			lump_p->wadnum = (WORD)Wads.Size();
			lump_p->flags = LittleShort(zip_fh->wCompression) == Z_DEFLATED? 
								LUMPF_COMPRESSED|LUMPF_ZIPFILE : LUMPF_ZIPFILE;
			lump_p->compressedsize = LittleLong(zip_fh->dwCompressedSize);

			// Since '\' can't be used as a file name's part inside a ZIP
			// we have to work around this for sprites because it is a valid
			// frame character.
			if (lump_p->namespc == ns_sprites)
			{
				char * c;

				while ((c=(char*)memchr(lump_p->name, '^', 8)))
				{
					*c='\\';
				}
			}

			// The start of the file will be determined the first time it is accessed.
			lump_p->flags |= LUMPF_NEEDFILESTART;
			lump_p->position = LittleLong(zip_fh->dwFileOffset);
			lump_p++;
		}
		// Resize the lump record array to its actual size
		NumLumps -= skipped;
		LumpInfo.Resize(NumLumps);
		
		// Entries in Zips are sorted alphabetically.
		qsort(&LumpInfo[startlump], NumLumps - startlump, sizeof(LumpRecord), lumpcmp);
	}
	else
	{ // This is just a single lump file
		fileinfo2free = NULL;
		fileinfo = &singleinfo;
		singleinfo.FilePos = 0;
		singleinfo.Size = LittleLong(wadinfo->GetLength());
		FString name(ExtractFileBase (filename));
		uppercopy(singleinfo.Name, name);
		NumLumps++;
	}
	Printf ("\n");

	// Fill in lumpinfo
	if (header.magic[0] != RFF_ID &&
		header.magic[0] != ZIP_ID &&
		(header.magic[0] != GRP_ID_0 || header.magic[1] != GRP_ID_1 || header.magic[2] != GRP_ID_2))
	{
		LumpInfo.Resize(NumLumps);
		lump_p = &LumpInfo[startlump];
		for (i = startlump; i < (unsigned)NumLumps; i++, lump_p++, fileinfo++)
		{
			// [RH] Convert name to uppercase during copy
			uppercopy (lump_p->name, fileinfo->Name);
			lump_p->name[8] = 0;
			lump_p->wadnum = (WORD)Wads.Size();
			lump_p->position = LittleLong(fileinfo->FilePos);
			lump_p->size = LittleLong(fileinfo->Size);
			lump_p->namespc = ns_global;
			lump_p->flags = 0;
			lump_p->fullname = NULL;
			lump_p->compressedsize=-1;
		}

		if (fileinfo2free)
		{
			delete[] fileinfo2free;
		}

		ScanForFlatHack (startlump);
	}

	wadinfo->FirstLump = startlump;
	wadinfo->LastLump = NumLumps - 1;
	Wads.Push(wadinfo);

	// [RH] Put the Strife Teaser voices into the voices namespace
	if (Wads.Size() == IWAD_FILENUM+1 && gameinfo.gametype == GAME_Strife && gameinfo.flags & GI_SHAREWARE)
	{
		FindStrifeTeaserVoices ();
	}

	if (EmbeddedWADs.Size())
	{
		char path[256];

		sprintf(path, "%s:", filename);
		char * wadstr = path+strlen(path);

		for(unsigned int i = 0; i < EmbeddedWADs.Size(); i++)
		{
			FZipFileInfo * zip_fh = EmbeddedWADs[i];
			FZipLocalHeader localHeader;

			*wadstr=0;
			size_t len = LittleShort(zip_fh->wFileNameSize);
			if (len+strlen(path) > 255) len = 255-strlen(path);
			strncpy(wadstr, ((char*)zip_fh) + sizeof(FZipFileInfo), len);
			wadstr[len]=0;

			DWORD size = LittleLong(zip_fh->dwSize);
			char * buffer = new char[size];

			int position = LittleLong(zip_fh->dwFileOffset) ;

			wadinfo->Seek(position, SEEK_SET);
			wadinfo->Read(&localHeader, sizeof(localHeader));
			position += LittleShort(localHeader.wExtraSize) + sizeof(FZipLocalHeader) + LittleShort(zip_fh->wFileNameSize);

			wadinfo->Seek(position, SEEK_SET);
			if (zip_fh->wCompression == Z_DEFLATED)
			{
				FileReaderZ frz(*wadinfo, true);
				frz.Read(buffer, size);
			}
			else
			{
				wadinfo->Read(buffer, size);
			}
			AddFile(path, buffer, size);
		}
	}
	if (directory != NULL) free(directory);
}

//==========================================================================
//
// W_CheckIfWadLoaded
//
// Returns true if the specified wad is loaded, false otherwise.
// If a fully-qualified path is specified, then the wad must match exactly.
// Otherwise, any wad with that name will work, whatever its path.
// Returns the wads index if found, or -1 if not.
//
//==========================================================================

int FWadCollection::CheckIfWadLoaded (const char *name)
{
	unsigned int i;

	if (strrchr (name, '/') != NULL)
	{
		for (i = 0; i < Wads.Size(); ++i)
		{
			if (stricmp (GetWadFullName (i), name) == 0)
			{
				return i;
			}
		}
	}
	else
	{
		for (i = 0; i < Wads.Size(); ++i)
		{
			if (stricmp (GetWadName (i), name) == 0)
			{
				return i;
			}
		}
	}
	return -1;
}

//==========================================================================
//
// W_NumLumps
//
//==========================================================================

int FWadCollection::GetNumLumps () const
{
	return NumLumps;
}

//==========================================================================
//
// GetNumFiles
//
//==========================================================================

int FWadCollection::GetNumWads () const
{
	return Wads.Size();
}

//==========================================================================
//
// W_CheckNumForName
//
// Returns -1 if name not found. The version with a third parameter will
// look exclusively in the specified wad for the lump.
//
// [RH] Changed to use hash lookup ala BOOM instead of a linear search
// and namespace parameter
//==========================================================================

int FWadCollection::CheckNumForName (const char *name, int space)
{
	char uname[8];
	WORD i;

	if (name == NULL)
	{
		return -1;
	}

	// Let's not search for names that are longer than 8 characters and contain path separators
	// They are almost certainly full path names passed to this function.
	if (strlen(name) > 8 && strpbrk(name, "/."))
	{
		return -1;
	}

	uppercopy (uname, name);
	i = FirstLumpIndex[LumpNameHash (uname) % NumLumps];

	while (i != NULL_INDEX)
	{
		if (*(QWORD *)&LumpInfo[i].name == *(QWORD *)&uname)
		{
			if (LumpInfo[i].namespc == space) break;
			// If the lump is from one of the special namespaces exclusive to Zips
			// the check has to be done differently:
			// If we find a lump with this name in the global namespace that does not come
			// from a Zip return that. WADs don't know these namespaces and single lumps must
			// work as well.
			if (space > ns_specialzipdirectory &&
				LumpInfo[i].namespc == ns_global && 
				!(LumpInfo[i].flags & LUMPF_ZIPFILE)) break;
		}
		i = NextLumpIndex[i];
	}

	return i != NULL_INDEX ? i : -1;
}

int FWadCollection::CheckNumForName (const char *name, int space, int wadnum, bool exact)
{
	char uname[8];
	WORD i;

	if (wadnum < 0)
	{
		return CheckNumForName (name, space);
	}

	uppercopy (uname, name);
	i = FirstLumpIndex[LumpNameHash (uname) % NumLumps];

	// If exact is true if will only find lumps in the same WAD, otherwise
	// also those in earlier WADs.
	while (i != NULL_INDEX &&
		(*(QWORD *)&LumpInfo[i].name != *(QWORD *)&uname ||
		 LumpInfo[i].namespc != space ||
		 (exact? (LumpInfo[i].wadnum != wadnum) : (LumpInfo[i].wadnum > wadnum)) ))
	{
		i = NextLumpIndex[i];
	}

	return i != NULL_INDEX ? i : -1;
}

//==========================================================================
//
// W_GetNumForName
//
// Calls W_CheckNumForName, but bombs out if not found.
//
//==========================================================================

int FWadCollection::GetNumForName (const char *name, int space)
{
	int	i;

	i = CheckNumForName (name, space);

	if (i == -1)
		I_Error ("W_GetNumForName: %s not found!", name);

	return i;
}


//==========================================================================
//
// W_CheckNumForFullName
//
// Same as above but looks for a fully qualified name from a .zip
// These don't care about namespaces though because those are part
// of the path.
//
//==========================================================================

int FWadCollection::CheckNumForFullName (const char *name, bool trynormal)
{
	WORD i;

	if (name == NULL)
	{
		return -1;
	}

	i = FirstLumpIndex_FullName[MakeKey (name) % NumLumps];

	while (i != NULL_INDEX && stricmp(name, LumpInfo[i].fullname))
	{
		i = NextLumpIndex_FullName[i];
	}

	if (i != NULL_INDEX) return i;

	if (trynormal && strlen(name) <= 8 && !strpbrk(name, "./"))
	{
		return CheckNumForName(name, ns_global);
	}
	return -1;
}

int FWadCollection::CheckNumForFullName (const char *name, int wadnum)
{
	WORD i;

	if (wadnum < 0)
	{
		return CheckNumForFullName (name);
	}

	i = FirstLumpIndex[MakeKey (name) % NumLumps];

	while (i != NULL_INDEX &&
		(stricmp(name, LumpInfo[i].fullname) || LumpInfo[i].wadnum != wadnum))
	{
		i = NextLumpIndex[i];
	}

	return i != NULL_INDEX ? i : -1;
}

//==========================================================================
//
// W_GetNumForFullName
//
// Calls W_CheckNumForFullName, but bombs out if not found.
//
//==========================================================================

int FWadCollection::GetNumForFullName (const char *name)
{
	int	i;

	i = CheckNumForFullName (name);

	if (i == -1)
		I_Error ("GetNumForFullName: %s not found!", name);

	return i;
}

//==========================================================================
//
// W_LumpLength
//
// Returns the buffer size needed to load the given lump.
//
//==========================================================================

int FWadCollection::LumpLength (int lump) const
{
	if ((size_t)lump >= NumLumps)
	{
		I_Error ("W_LumpLength: %i >= NumLumps",lump);
	}

	return LumpInfo[lump].size;
}

//==========================================================================
//
// GetLumpOffset
//
// Returns the offset from the beginning of the file to the lump.
//
//==========================================================================

int FWadCollection::GetLumpOffset (int lump)
{
	if ((size_t)lump >= NumLumps)
	{
		I_Error ("GetLumpOffset: %i >= NumLumps",lump);
	}

	if (LumpInfo[lump].flags & LUMPF_NEEDFILESTART)
	{
		SetLumpAddress(&LumpInfo[lump]);
	}

	return LumpInfo[lump].position;
}

//==========================================================================
//
// GetLumpOffset
//
//==========================================================================

int FWadCollection::GetLumpFlags (int lump)
{
	if ((size_t)lump >= NumLumps)
	{
		return 0;
	}

	return LumpInfo[lump].flags;
}

//==========================================================================
//
// W_LumpNameHash
//
// NOTE: s should already be uppercase, in contrast to the BOOM version.
//
// Hash function used for lump names.
// Must be mod'ed with table size.
// Can be used for any 8-character names.
//
//==========================================================================

DWORD FWadCollection::LumpNameHash (const char *s)
{
	const DWORD *table = GetCRCTable ();;
	DWORD hash = 0xffffffff;
	int i;

	for (i = 8; i > 0 && *s; --i, ++s)
	{
		hash = CRC1 (hash, *s, table);
	}
	return hash ^ 0xffffffff;
}

//==========================================================================
//
// W_InitHashChains
//
// Prepares the lumpinfos for hashing.
// (Hey! This looks suspiciously like something from Boom! :-)
//
//==========================================================================

void FWadCollection::InitHashChains (void)
{
	char name[8];
	unsigned int i, j;

	// Mark all buckets as empty
	memset (FirstLumpIndex, 255, NumLumps*sizeof(FirstLumpIndex[0]));
	memset (NextLumpIndex, 255, NumLumps*sizeof(NextLumpIndex[0]));
	memset (FirstLumpIndex_FullName, 255, NumLumps*sizeof(FirstLumpIndex_FullName[0]));
	memset (NextLumpIndex_FullName, 255, NumLumps*sizeof(NextLumpIndex_FullName[0]));

	// Now set up the chains
	for (i = 0; i < (unsigned)NumLumps; i++)
	{
		uppercopy (name, LumpInfo[i].name);
		j = LumpNameHash (name) % NumLumps;
		NextLumpIndex[i] = FirstLumpIndex[j];
		FirstLumpIndex[j] = i;

		// Do the same for the full paths
		if (LumpInfo[i].fullname!=NULL)
		{
			j = MakeKey(LumpInfo[i].fullname) % NumLumps;
			NextLumpIndex_FullName[i] = FirstLumpIndex_FullName[j];
			FirstLumpIndex_FullName[j] = i;
		}
	}
}

//==========================================================================
//
// IsMarker
//
// (from BOOM)
//
//==========================================================================

bool FWadCollection::IsMarker (const FWadCollection::LumpRecord *lump, const char *marker) const
{
	if (lump->namespc != ns_global || (lump->flags & LUMPF_ZIPFILE))
	{
		return false;
	}
	if (strncmp (lump->name, marker, 8) == 0)
	{
		// If the previous lump was of the form FF_END and this one is
		// of the form F_END, ignore this as a marker
		if (marker[2] == 'E' && lump > &LumpInfo[0])
		{
			if ((lump - 1)->name[0] == *marker &&
				strncmp ((lump - 1)->name + 1, marker, 7) == 0)
			{
				return false;
			}
		}
		return true;
	}
	// Treat double-character markers the same as single-character markers.
	// (So if FF_START appears in the wad, it will be treated as if it is F_START.
	// However, TTX_STAR will not be treated the same as TX_START because it
	// is not a single-character marker.)
	if (marker[1] == '_' &&
		lump->name[0] == *marker &&
		strncmp (lump->name + 1, marker, 7) == 0)
	{
		return true;
	}
	return false;
}

//==========================================================================
//
// ScanForFlatHack
//
// Try to detect wads that add extra flats by sticking an extra F_END
// at the end of the flat list without any corresponding FF_START.
// In other words, fix gothic2.wad.
//
//==========================================================================

void FWadCollection::ScanForFlatHack (int startlump)
{
	if (Args->CheckParm ("-noflathack"))
	{
		return;
	}

	for (int i = startlump; (DWORD)i < NumLumps; ++i)
	{
		if (LumpInfo[i].name[0] == 'F')
		{
			int j;

			if (strcmp (LumpInfo[i].name + 1, "_START") == 0 ||
				strncmp (LumpInfo[i].name + 1, "F_START", 7) == 0)
			{
				// If the wad has a F_START/FF_START marker, check for
				// a FF_START-flats-FF_END-flats-F_END pattern as seen
				// in darkhour.wad. At what point do I stop making hacks
				// for wads that are incorrect?
				for (i = i + 1; (DWORD)i < NumLumps; ++i)
				{
					if (LumpInfo[i].name[0] == 'F' && strcmp (LumpInfo[i].name + 1, "F_END") == 0)
					{
						// Found FF_END
						break;
					}
					if (LumpInfo[i].size != 4096)
					{
						return;
					}
				}
				if (i < (int)NumLumps)
				{
					// Look for flats-F_END
					for (j = ++i; (DWORD)j < NumLumps; ++j)
					{
						if (LumpInfo[j].name[0] == 'F' && strcmp (LumpInfo[j].name + 1, "_END") == 0)
						{
							// Found F_END, so bump all the flats between FF_END/F_END up and move the
							// FF_END so it immediately precedes F_END.
							if (i != j - 1)
							{
								for (; i < j; ++i)
								{
									LumpInfo[i - 1] = LumpInfo[i];
								}
								--i;
								strcpy (LumpInfo[i].name, "FF_END");
								LumpInfo[i].size = 0;
								LumpInfo[i].namespc = ns_global;
								LumpInfo[i].flags = 0;
							}
							return;
						}
						if (LumpInfo[j].size != 4096)
						{
							return;
						}
					}
				}
				return;
			}

			// No need to look for FF_END, because Doom doesn't. One minor
			// nitpick: Doom will look for the last F_END; this finds the first
			// one if there is more than one in the file. Too bad. If there's
			// more than one F_END, this algorithm won't be able to properly
			// determine where to put the F_START anyway.
			if (strcmp (LumpInfo[i].name + 1, "_END") == 0)
			{
				// When F_END is found, back up past any lumps of length
				// 4096, then insert an F_START marker.
				for (j = i - 1; j >= startlump && LumpInfo[j].size == 4096; --j)
				{
				}
				if (j == i - 1)
				{
					// Oh no! There are no flats immediately before F_END. Maybe they are
					// at the beginning of the wad (e.g. slipgate.wad).
					for (j = startlump; LumpInfo[j].size == 4096; ++j)
					{
					}
					if (j > startlump)
					{
						// Okay, there are probably flats at the beginning of the wad.
						// Move the F_END marker so it immediately follows them, and
						// then add an F_START marker at the start of the wad.
						for (; i > j; --i)
						{
							LumpInfo[i] = LumpInfo[i-1];
						}
						strcpy (LumpInfo[j].name, "F_END");
						LumpInfo[j].size = 0;
						LumpInfo[j].namespc = ns_global;
						LumpInfo[j].flags = 0;
						j = startlump - 1;
					}
					else
					{
						// Oh well. There won't be any flats loaded from this wad, I guess.
						j = i - 1;
					}
				}
				++NumLumps;
				LumpInfo.Resize(NumLumps);
				for (; i > j; --i)
				{
					LumpInfo[i+1] = LumpInfo[i];
				}
				++i;
				strcpy (LumpInfo[i].name, "F_START");
				LumpInfo[i].size = 0;
				LumpInfo[i].namespc = ns_global;
				LumpInfo[i].flags = 0;
				return;
			}
		}
	}
}

//==========================================================================
//
// RenameSprites
//
// Renames sprites in IWADs so that unique actors can have unique sprites,
// making it possible to import any actor from any game into any other
// game without jumping through hoops to resolve duplicate sprite names.
// You just need to know what the sprite's new name is.
//
//==========================================================================

void FWadCollection::RenameSprites (int startlump)
{
	bool renameAll;

	static const DWORD HereticRenames[] =
	{ MAKE_ID('H','E','A','D'), MAKE_ID('L','I','C','H'),		// Ironlich
	};

	static const DWORD HexenRenames[] =
	{ MAKE_ID('B','A','R','L'), MAKE_ID('Z','B','A','R'),		// ZBarrel
	  MAKE_ID('A','R','M','1'), MAKE_ID('A','R','_','1'),		// MeshArmor
	  MAKE_ID('A','R','M','2'), MAKE_ID('A','R','_','2'),		// FalconShield
	  MAKE_ID('A','R','M','3'), MAKE_ID('A','R','_','3'),		// PlatinumHelm
	  MAKE_ID('A','R','M','4'), MAKE_ID('A','R','_','4'),		// AmuletOfWarding
	  MAKE_ID('S','U','I','T'), MAKE_ID('Z','S','U','I'),		// ZSuitOfArmor and ZArmorChunk
	  MAKE_ID('T','R','E','1'), MAKE_ID('Z','T','R','E'),		// ZTree and ZTreeDead
	  MAKE_ID('T','R','E','2'), MAKE_ID('T','R','E','S'),		// ZTreeSwamp150
	  MAKE_ID('C','A','N','D'), MAKE_ID('B','C','A','N'),		// ZBlueCandle
	  MAKE_ID('R','O','C','K'), MAKE_ID('R','O','K','K'),		// rocks and dirt in a_debris.cpp
	  MAKE_ID('W','A','T','R'), MAKE_ID('H','W','A','T'),		// Strife also has WATR
	  MAKE_ID('G','I','B','S'), MAKE_ID('P','O','L','5'),		// RealGibs
	  MAKE_ID('E','G','G','M'), MAKE_ID('P','R','K','M'),		// PorkFX
	};

	static const DWORD StrifeRenames[] =
	{ MAKE_ID('M','I','S','L'), MAKE_ID('S','M','I','S'),		// lots of places
	  MAKE_ID('A','R','M','1'), MAKE_ID('A','R','M','3'),		// MetalArmor
	  MAKE_ID('A','R','M','2'), MAKE_ID('A','R','M','4'),		// LeatherArmor
	  MAKE_ID('P','M','A','P'), MAKE_ID('S','M','A','P'),		// StrifeMap
	  MAKE_ID('T','L','M','P'), MAKE_ID('T','E','C','H'),		// TechLampSilver and TechLampBrass
	  MAKE_ID('T','R','E','1'), MAKE_ID('T','R','E','T'),		// TreeStub
	  MAKE_ID('B','A','R','1'), MAKE_ID('B','A','R','C'),		// BarricadeColumn
	  MAKE_ID('S','H','T','2'), MAKE_ID('M','P','U','F'),		// MaulerPuff
	  MAKE_ID('B','A','R','L'), MAKE_ID('B','B','A','R'),		// StrifeBurningBarrel
	  MAKE_ID('T','R','C','H'), MAKE_ID('T','R','H','L'),		// SmallTorchLit
	  MAKE_ID('S','H','R','D'), MAKE_ID('S','H','A','R'),		// glass shards
	  MAKE_ID('B','L','S','T'), MAKE_ID('M','A','U','L'),		// Mauler
	  MAKE_ID('L','O','G','G'), MAKE_ID('L','O','G','W'),		// StickInWater
	  MAKE_ID('V','A','S','E'), MAKE_ID('V','A','Z','E'),		// Pot and Pitcher
	  MAKE_ID('C','N','D','L'), MAKE_ID('K','N','D','L'),		// Candle
	  MAKE_ID('P','O','T','1'), MAKE_ID('M','P','O','T'),		// MetalPot
	  MAKE_ID('S','P','I','D'), MAKE_ID('S','T','L','K'),		// Stalker
	};

	const DWORD *renames;
	int numrenames;

	switch (gameinfo.gametype)
	{
	case GAME_Doom:
	default:
		// Doom's sprites don't get renamed.
		return;

	case GAME_Heretic:
		renames = HereticRenames;
		numrenames = sizeof(HereticRenames)/8;
		break;

	case GAME_Hexen:
		renames = HexenRenames;
		numrenames = sizeof(HexenRenames)/8;
		break;

	case GAME_Strife:
		renames = StrifeRenames;
		numrenames = sizeof(StrifeRenames)/8;
		break;
	}

	renameAll = !!Args->CheckParm ("-oldsprites");

	for (DWORD i = startlump + 1;
		i < NumLumps && 
		 *(DWORD *)LumpInfo[i].name != MAKE_ID('S','_','E','N') &&
		 *(((DWORD *)LumpInfo[i].name) + 1) != MAKE_ID('D',0,0,0);
		++i)
	{
		// Only sprites in the IWAD normally get renamed
		if (renameAll || LumpInfo[i].wadnum == IWAD_FILENUM)
		{
			for (int j = 0; j < numrenames; ++j)
			{
				if (*(DWORD *)LumpInfo[i].name == renames[j*2])
				{
					*(DWORD *)LumpInfo[i].name = renames[j*2+1];
				}
			}
		}

		// When not playing Doom rename all BLOD sprites to BLUD so that
		// the same blood states can be used everywhere
		if (gameinfo.gametype != GAME_Doom)
		{
			if (*(DWORD *)LumpInfo[i].name == MAKE_ID('B', 'L', 'O', 'D'))
			{
				*(DWORD *)LumpInfo[i].name = MAKE_ID('B', 'L', 'U', 'D');
			}
		}
	}
}

//==========================================================================
//
// MergeLumps
//
// Merge multiple tagged groups into one
// Basically from BOOM, too, although I tried to write it independently.
//
//==========================================================================

int FWadCollection::MergeLumps (const char *start, const char *end, int space)
{
	char ustart[8], uend[8];
	LumpRecord *newlumpinfos;
	int newlumps, oldlumps;
	int markerpos = -1;
    unsigned int i;
	bool insideBlock;

	uppercopy (ustart, start);
	uppercopy (uend, end);

	newlumpinfos = new LumpRecord[NumLumps];

	newlumps = 0;
	oldlumps = 0;
	insideBlock = false;

	for (i = 0; i < NumLumps; i++)
	{
		if (!insideBlock)
		{
			// The lump already has the desired namespace
			// (This happens for lumps coming from .zips)
			if (LumpInfo[i].namespc == space)
			{
				// Create start marker if we haven't already
				if (!newlumps)
				{
					newlumps++;
					strncpy (newlumpinfos[0].name, ustart, 8);
					newlumpinfos[0].fullname=NULL;
					newlumpinfos[0].wadnum = -1;
					newlumpinfos[0].position =
						newlumpinfos[0].size = 0;
					newlumpinfos[0].namespc = ns_global;
				}

				newlumpinfos[newlumps++] = LumpInfo[i];
			}
			else
			// Check if this is the start of a block
			if (IsMarker (&LumpInfo[i], ustart))
			{
				insideBlock = true;
				markerpos = i;

				// Create start marker if we haven't already
				if (!newlumps)
				{
					newlumps++;
					strncpy (newlumpinfos[0].name, ustart, 8);
					newlumpinfos[0].fullname=NULL;
					newlumpinfos[0].wadnum = -1;
					newlumpinfos[0].position =
						newlumpinfos[0].size = 0;
					newlumpinfos[0].namespc = ns_global;
				}
			}
			else
			{
				// Copy lumpinfo down this list
				LumpInfo[oldlumps++] = LumpInfo[i];
			}
		}
		else
		{
			if (i && LumpInfo[i].wadnum != LumpInfo[i-1].wadnum)
			{
				// Blocks cannot span multiple files
				insideBlock = false;
				LumpInfo[oldlumps++] = LumpInfo[i];
			}
			else if (IsMarker (&LumpInfo[i], uend))
			{
				// It is the end of a block. We'll add the end marker once
				// we've processed everything.
				insideBlock = false;
			}
			else
			{
				newlumpinfos[newlumps] = LumpInfo[i];
				newlumpinfos[newlumps++].namespc = space;
			}
		}
	}

	// Now copy the merged lumps to the end of the old list
	// and create the end marker entry.

	if (newlumps)
	{
		LumpInfo.Resize(oldlumps+newlumps+1);

		memcpy (&LumpInfo[oldlumps], newlumpinfos, sizeof(LumpRecord) * newlumps);
		markerpos = oldlumps;
		NumLumps = oldlumps + newlumps;
		
		strncpy (LumpInfo[NumLumps].name, uend, 8);
		LumpInfo[NumLumps].fullname=NULL;
		LumpInfo[NumLumps].wadnum = -1;
		LumpInfo[NumLumps].position =
			LumpInfo[NumLumps].size = 0;
		LumpInfo[NumLumps].namespc = ns_global;
		NumLumps++;
	}

	delete[] newlumpinfos;
	return markerpos;
}

//==========================================================================
//
// FindStrifeTeaserVoices
//
// Strife0.wad does not have the voices between V_START/V_END markers, so
// figure out which lumps are voices based on their names.
//
//==========================================================================

void FWadCollection::FindStrifeTeaserVoices ()
{
	for (DWORD i = Wads[IWAD_FILENUM]->FirstLump; i <= Wads[IWAD_FILENUM]->LastLump; ++i)
	{
		if (LumpInfo[i].name[0] == 'V' &&
			LumpInfo[i].name[1] == 'O' &&
			LumpInfo[i].name[2] == 'C')
		{
			int j;

			for (j = 3; j < 8; ++j)
			{
				if (LumpInfo[i].name[j] != 0 && !isdigit(LumpInfo[i].name[j]))
					break;
			}
			if (j == 8)
			{
				LumpInfo[i].namespc = ns_strifevoices;
			}
		}
	}
}

//==========================================================================
//
// W_FindLump
//
// Find a named lump. Specifically allows duplicates for merging of e.g.
// SNDINFO lumps.
//
//==========================================================================

int FWadCollection::FindLump (const char *name, int *lastlump, bool anyns)
{
	char name8[8];
	LumpRecord *lump_p;

	uppercopy (name8, name);

	lump_p = &LumpInfo[*lastlump];
	while (lump_p < &LumpInfo[NumLumps])
	{
		if ((anyns || lump_p->namespc == ns_global) &&
			*(QWORD *)&lump_p->name == *(QWORD *)&name8)
		{
			int lump = lump_p - &LumpInfo[0];
			*lastlump = lump + 1;
			return lump;
		}
		lump_p++;
	}

	*lastlump = NumLumps;
	return -1;
}

//==========================================================================
//
// W_CheckLumpName
//
//==========================================================================

bool FWadCollection::CheckLumpName (int lump, const char *name)
{
	if ((size_t)lump >= NumLumps)
		return false;

	return !strnicmp (LumpInfo[lump].name, name, 8);
}

//==========================================================================
//
// W_GetLumpName
//
//==========================================================================

void FWadCollection::GetLumpName (char *to, int lump) const
{
	if ((size_t)lump >= NumLumps)
		*to = 0;
	else
		uppercopy (to, LumpInfo[lump].name);
}

//==========================================================================
//
// FWadCollection :: GetLumpFullName
//
// Returns the lump's full name if it has one or its short name if not.
//
//==========================================================================

const char *FWadCollection::GetLumpFullName (int lump) const
{
	if ((size_t)lump >= NumLumps)
		return NULL;
	else if (LumpInfo[lump].fullname != NULL)
		return LumpInfo[lump].fullname;
	else
		return LumpInfo[lump].name;
}

//==========================================================================
//
// GetLumpNamespace
//
//==========================================================================

int FWadCollection::GetLumpNamespace (int lump) const
{
	if ((size_t)lump >= NumLumps)
		return ns_global;
	else
		return LumpInfo[lump].namespc;
}

//==========================================================================
//
// W_GetLumpFile
//
//==========================================================================

int FWadCollection::GetLumpFile (int lump) const
{
	if ((size_t)lump >= LumpInfo.Size())
		return -1;
	return LumpInfo[lump].wadnum;
}

//==========================================================================
//
// W_ReadLump
//
// Loads the lump into the given buffer, which must be >= W_LumpLength().
//
//==========================================================================

void FWadCollection::ReadLump (int lump, void *dest)
{
	FWadLump lumpr = OpenLumpNum (lump);
	long size = lumpr.GetLength ();
	long numread = lumpr.Read (dest, size);

	if (numread != size)
	{
		I_Error ("W_ReadLump: only read %ld of %ld on lump %i\n",
			numread, size, lump);	
	}
}

//==========================================================================
//
// ReadLump - variant 2
//
// Loads the lump into a newly created buffer and returns it.
//
//==========================================================================

FMemLump FWadCollection::ReadLump (int lump)
{
	return FMemLump(FString(ELumpNum(lump)));
}

//==========================================================================
//
// SetLumpAddress
//
//==========================================================================

void FWadCollection::SetLumpAddress(LumpRecord *l)
{
	// This file is inside a zip and has not been opened before.
	// Position points to the start of the local file header, which we must
	// read and skip so that we can get to the actual file data.
	FZipLocalHeader localHeader;
	int skiplen;
	int address;

	WadFileRecord *wad = Wads[l->wadnum];

	address = wad->Tell();
	wad->Seek (l->position, SEEK_SET);
	wad->Read (&localHeader, sizeof(localHeader));
	skiplen = LittleShort(localHeader.wFileNameSize) + LittleShort(localHeader.wExtraSize);
	l->position += sizeof(localHeader) + skiplen;
	l->flags &= ~LUMPF_NEEDFILESTART;
}

//==========================================================================
//
// OpenLumpNum
//
// Returns a copy of the file object for a lump's wad and positions its
// file pointer at the beginning of the lump.
//
//==========================================================================

FWadLump FWadCollection::OpenLumpNum (int lump)
{
	LumpRecord *l;
	WadFileRecord *wad;

	if ((unsigned)lump >= (unsigned)LumpInfo.Size())
	{
		I_Error ("W_OpenLumpNum: %u >= NumLumps", lump);
	}

	l = &LumpInfo[lump];
	wad = l->wadnum >= 0? Wads[l->wadnum] : NULL;

	if (l->flags & LUMPF_NEEDFILESTART)
	{
		SetLumpAddress(l);
	}

	if (wad != NULL) wad->Seek (l->position, SEEK_SET);

	if (l->flags & LUMPF_COMPRESSED)
	{
		// A compressed entry in a .zip file
		char * buffer = new char[l->size+1];	// the last byte is used as a reference counter
		buffer[l->size] = 0;
		FileReaderZ frz(*wad, true);
		frz.Read(buffer, l->size);
		return FWadLump(buffer, l->size, true);
	}
	else if (l->flags & LUMPF_EXTERNAL)
	{
		static char zero = '\0';
		FILE *f;
		
		if (wad != NULL)	// The WadRecord in this case is just a means to store a path
		{
			FString name;

			name.Format("%s/%s", wad->Name, l->fullname);
			f = fopen(name, "rb");
		}
		else
		{
			f = fopen(l->fullname, "rb");
		}
		// This is an external file that cannot be kept open so we have to read
		// the complete contents into a memory buffer first
		if (f != NULL)
		{
			char *buffer = new char[l->size+1];	// the last byte is used as a reference counter
			buffer[l->size] = 0;
			fread(buffer, 1, l->size, f);
			fclose(f);
			return FWadLump(buffer, l->size, true);
		}
		// The file got deleted or worse. At least return something.
		Printf("%s: Unable to open file\n", l->fullname);
		return FWadLump(&zero, 1, false);
	}
	else if (wad->MemoryData != NULL)
	{
		// A lump from an embedded WAD
		// Handling this here creates less overhead than trying
		// to do it inside the FWadLump class.
		return FWadLump((char*)wad->MemoryData + l->position, l->size, false);
	}
	else
	{
		// An uncompressed lump in a .wad or .zip
		return FWadLump (*wad, l->size, !!(l->flags & LUMPF_BLOODCRYPT));
	}
}

//==========================================================================
//
// ReopenLumpNum
//
// Similar to OpenLumpNum, except a new, independant file object is created
// for the lump's wad. Use this when you won't read the lump's data all at
// once (e.g. for streaming an Ogg Vorbis stream from a wad as music).
//
//==========================================================================

FWadLump *FWadCollection::ReopenLumpNum (int lump)
{
	LumpRecord *l;
	WadFileRecord *wad;
	FILE *f;

	if ((unsigned)lump >= (unsigned)LumpInfo.Size())
	{
		I_Error ("W_ReopenLumpNum: %u >= NumLumps", lump);
	}

	l = &LumpInfo[lump];
	wad = l->wadnum >= 0? Wads[l->wadnum] : NULL;

	if (l->flags & LUMPF_NEEDFILESTART)
	{
		SetLumpAddress(l);
	}

	if (l->flags & LUMPF_COMPRESSED)
	{
		// A compressed entry in a .zip file
		int address = wad->Tell();			// read from the existing WadFileRecord without reopening
		char * buffer = new char[l->size+1];	// the last byte is used as a reference counter
		wad->Seek(l->position, SEEK_SET);
		FileReaderZ frz(*wad, true);
		frz.Read(buffer, l->size);
		wad->Seek(address, SEEK_SET);
		return new FWadLump(buffer, l->size, true);	//... but restore the file pointer afterward!
	}
	else if (l->flags & LUMPF_EXTERNAL)
	{
		static char zero = '\0';
		FILE *f;

		if (wad != NULL)	// The WadRecord in this case is just a means to store a path
		{
			FString name;

			name.Format("%s/%s", wad->Name, l->fullname);
			f = fopen(name, "rb");
		}
		else
		{
			f = fopen(l->fullname, "rb");
		}
		// This is an external file that cannot be kept open so we have to read
		// the complete contents into a memory buffer first
		if (f != NULL)
		{
			char *buffer = new char[l->size+1];	// the last byte is used as a reference counter
			buffer[l->size] = 0;
			fread(buffer, 1, l->size, f);
			fclose(f);
			return new FWadLump(buffer, l->size, true);
		}
		// The file got deleted or worse. At least return something.
		Printf("%s: Unable to open file\n", l->fullname);
		return new FWadLump(&zero, 1, false);
	}
	else if (wad->MemoryData!=NULL)
	{
		// A lump from an embedded WAD
		// Handling this here creates less overhead than trying
		// to do it inside the FWadLump class.

		return new FWadLump((char*)wad->MemoryData+l->position, l->size, false);
	}
	else
	{
		// An uncompressed lump in a .wad or .zip
		f = fopen (wad->Name, "rb");
		if (f == NULL)
		{
			I_Error ("Could not reopen %s\n", wad->Name);
		}

		fseek (f, l->position, SEEK_SET);
		return new FWadLump (f, l->size);
	}

}

//==========================================================================
//
// GetFileReader
//
// Retrieves the FileReader object to access the given WAD
// Careful: This is only useful for real WAD files!
//
//==========================================================================

FileReader *FWadCollection::GetFileReader(int wadnum)
{
	if ((DWORD)wadnum >= Wads.Size())
	{
		return NULL;
	}
	return Wads[wadnum];
}

//==========================================================================
//
// W_GetWadName
//
// Returns the name of the given wad.
//
//==========================================================================

const char *FWadCollection::GetWadName (int wadnum) const
{
	const char *name, *slash;

	if ((DWORD)wadnum >= Wads.Size())
	{
		return NULL;
	}

	name = Wads[wadnum]->Name;
	slash = strrchr (name, '/');
	return slash != NULL ? slash+1 : name;
}

//==========================================================================
//
// W_GetWadFullName
//
// Returns the name of the given wad, including any path
//
//==========================================================================

const char *FWadCollection::GetWadFullName (int wadnum) const
{
	if ((unsigned int)wadnum >= Wads.Size())
	{
		return NULL;
	}

	return Wads[wadnum]->Name;
}


//==========================================================================
//
// IsUncompressedFile
//
// Returns true when the lump is available as an uncompressed portion of
// a file. The music player can play such lumps by streaming but anything
// else has to be loaded into memory first.
//
//==========================================================================

bool FWadCollection::IsUncompressedFile(int lump) const
{
	if ((unsigned)lump >= (unsigned)NumLumps)
	{
		I_Error ("IsUncompressedFile: %u >= NumLumps",lump);
	}

	LumpRecord * l = &LumpInfo[lump];

	if (l->flags & (LUMPF_COMPRESSED|LUMPF_EXTERNAL)) return false;
	else if (Wads[l->wadnum]->MemoryData!=NULL) return false;
	else return true;
}

//==========================================================================
//
// IsEncryptedFile
//
// Returns true if the first 256 bytes of the lump are encrypted for Blood.
//
//==========================================================================

bool FWadCollection::IsEncryptedFile(int lump) const
{
	if ((unsigned)lump >= (unsigned)NumLumps)
	{
		return false;
	}
	return !!(LumpInfo[lump].flags & LUMPF_BLOODCRYPT);
}

//==========================================================================
//
// W_SkinHack
//
// Tests a wad file to see if it contains an S_SKIN marker. If it does,
// every lump in the wad is moved into a new namespace. Because skins are
// only supposed to replace player sprites, sounds, or faces, this should
// not be a problem. Yes, there are skins that replace more than that, but
// they are such a pain, and breaking them like this was done on purpose.
// This also renames any S_SKINxx lumps to just S_SKIN.
//
//==========================================================================

void FWadCollection::SkinHack (int baselump)
{
	bool skinned = false;
	bool hasmap = false;
	DWORD i;

	for (i = baselump; i < NumLumps; i++)
	{
		if (LumpInfo[i].name[0] == 'S' &&
			LumpInfo[i].name[1] == '_' &&
			LumpInfo[i].name[2] == 'S' &&
			LumpInfo[i].name[3] == 'K' &&
			LumpInfo[i].name[4] == 'I' &&
			LumpInfo[i].name[5] == 'N')
		{ // Wad has at least one skin.
			LumpInfo[i].name[6] = LumpInfo[i].name[7] = 0;
			if (!skinned)
			{
				skinned = true;
				DWORD j;

				for (j = baselump; j < NumLumps; j++)
				{
					// Using the baselump as the namespace is safe, because
					// zdoom.wad guarantees the first possible baselump
					// passed to this function is a largish number.
					LumpInfo[j].namespc = baselump;
				}
			}
		}
		if (LumpInfo[i].name[0] == 'M' &&
			LumpInfo[i].name[1] == 'A' &&
			LumpInfo[i].name[2] == 'P')
		{
			hasmap = true;
		}
	}
	if (skinned && hasmap)
	{
		Printf (TEXTCOLOR_BLUE
			"The maps in %s will not be loaded because it has a skin.\n"
			TEXTCOLOR_BLUE
			"You should remove the skin from the wad to play these maps.\n",
			Wads[LumpInfo[baselump].wadnum]->Name);
	}
}

//==========================================================================
//
// BloodCrypt
//
//==========================================================================

void BloodCrypt (void *data, int key, int len)
{
	int p = (BYTE)key, i;

	for (i = 0; i < len; ++i)
	{
		((BYTE *)data)[i] ^= (unsigned char)(p+(i>>1));
	}
}

// WadFileRecord ------------------------------------------------------------

FWadCollection::WadFileRecord::WadFileRecord (FILE *file)
: FileReader(file), Name(NULL), FirstLump(0), LastLump(0)
{
	MemoryData=NULL;
}

FWadCollection::WadFileRecord::WadFileRecord (const char * mem, int len)
: FileReader(), MemoryData(mem), Name(NULL), FirstLump(0), LastLump(0)
{
	Length = len;
	FilePos = StartPos = 0;
}

FWadCollection::WadFileRecord::~WadFileRecord ()
{
	if (Name != NULL)
	{
		delete[] Name;
	}
	if (MemoryData != NULL) 
	{
		delete [] MemoryData;
	}
}

long FWadCollection::WadFileRecord::Seek (long offset, int origin)
{
	if (MemoryData==NULL) return FileReader::Seek(offset, origin);
	else
	{
		switch (origin)
		{
		case SEEK_CUR:
			offset+=FilePos;
			break;

		case SEEK_END:
			offset+=Length;
			break;
		default:
			break;
		}
		if (offset<0) offset=0;
		else if (offset>Length) offset=Length;
		FilePos=offset;
		return 0;
	}
}
long FWadCollection::WadFileRecord::Read (void *buffer, long len)
{
	if (MemoryData==NULL) return FileReader::Read(buffer, len);
	else
	{
		if (FilePos+len>Length) len=Length-FilePos;
		memcpy(buffer, MemoryData+FilePos, len);
		FilePos+=len;
		return len;
	}
}

// FWadLump -----------------------------------------------------------------

// FWadLump -----------------------------------------------------------------

FWadLump::FWadLump ()
: FileReader(), SourceData(NULL), DestroySource(false), Encrypted(false)
{
}

FWadLump::FWadLump (const FWadLump &copy)
{
	// This must be defined isn't called.
	File = copy.File;
	Length = copy.Length;
	FilePos = copy.FilePos;
	StartPos = copy.StartPos;
	CloseOnDestruct = false;
	SourceData = copy.SourceData;
	DestroySource = copy.DestroySource;
	if (SourceData != NULL && DestroySource)
	{
		SourceData[copy.Length]++;
	}
}

#ifdef _DEBUG
FWadLump & FWadLump::operator= (const FWadLump &copy)
{
	// Only the debug build actually calls this!
	File = copy.File;
	Length = copy.Length;
	FilePos = copy.FilePos;
	StartPos = copy.StartPos;
	CloseOnDestruct = false;	// For WAD lumps this is always false!
	SourceData = copy.SourceData;
	DestroySource = copy.DestroySource;
	if (SourceData != NULL && DestroySource)
	{
		SourceData[copy.Length]++;
	}
	return *this;
}
#endif

FWadLump::FWadLump (const FileReader &other, long length, bool encrypted)
: FileReader(other, length), SourceData(NULL), DestroySource(false), Encrypted(encrypted)
{
}

FWadLump::FWadLump (FILE *file, long length)
: FileReader(file, length), SourceData(NULL), DestroySource(false), Encrypted(false)
{
}

FWadLump::FWadLump (char *data, long length, bool destroy)
: FileReader(), SourceData(data), DestroySource(destroy), Encrypted(false)
{
	FilePos = StartPos = 0;
	Length = length;
	if (destroy)
	{
		data[length] = 0;
	}
}

FWadLump::~FWadLump()
{
	if (SourceData && DestroySource) 
	{
		if (SourceData[Length] == 0)
		{
			delete[] SourceData;
		}
		else
		{
			SourceData[Length]--;
		}
	}
}

long FWadLump::Seek (long offset, int origin)
{
	if (SourceData)
	{
		switch (origin)
		{
		case SEEK_CUR:
			offset += FilePos;
			break;

		case SEEK_END:
			offset += Length;
			break;

		default:
			break;
		}
		FilePos = clamp<long> (offset, 0, Length);
		return 0;
	}
	return FileReader::Seek(offset, origin);
}

long FWadLump::Read (void *buffer, long len)
{
	long numread;
	long startread = FilePos;

	if (SourceData != NULL)
	{
		if (FilePos + len > Length)
		{
			len = Length - FilePos;
		}
		memcpy(buffer, SourceData + FilePos, len);
		FilePos += len;
		numread = len;
	}
	else
	{
		numread = FileReader::Read(buffer, len);
	}

	// Blood, you are so mean to me with your encryption.
	if (Encrypted && startread - StartPos < 256)
	{
		int cryptstart = startread - StartPos;
		int cryptlen = MIN<int> (FilePos - StartPos, 256);
		BYTE *data = (BYTE *)buffer - cryptstart;
		
		for (int i = cryptstart; i < cryptlen; ++i)
		{
			data[i] ^= i >> 1;
		}
	}

	return numread;
}

// FMemLump -----------------------------------------------------------------

FMemLump::FMemLump ()
{
}

FMemLump::FMemLump (const FMemLump &copy)
{
	Block = copy.Block;
}

FMemLump &FMemLump::operator = (const FMemLump &copy)
{
	Block = copy.Block;
	return *this;
}

FMemLump::FMemLump (const FString &source)
: Block (source)
{
}

FMemLump::~FMemLump ()
{
}

FString::FString (ELumpNum lumpnum)
{
	FWadLump lumpr = Wads.OpenLumpNum ((int)lumpnum);
	long size = lumpr.GetLength ();
	AllocBuffer (1 + size);
	long numread = lumpr.Read (&Chars[0], size);
	Chars[size] = '\0';

	if (numread != size)
	{
		I_Error ("ConstructStringFromLump: Only read %ld of %ld bytes on lump %i (%s)\n",
			numread, size, lumpnum, Wads.GetLumpFullName((int)lumpnum));
	}
}

//==========================================================================
//
// PrintLastError
//
//==========================================================================

#ifdef _WIN32
//#define WIN32_LEAN_AND_MEAN
//#include <windows.h>

extern "C" {
__declspec(dllimport) unsigned long __stdcall FormatMessageA(
    unsigned long dwFlags,
    const void *lpSource,
    unsigned long dwMessageId,
    unsigned long dwLanguageId,
    char **lpBuffer,
    unsigned long nSize,
    va_list *Arguments
    );
__declspec(dllimport) void * __stdcall LocalFree (void *);
__declspec(dllimport) unsigned long __stdcall GetLastError ();
}

static void PrintLastError ()
{
	char *lpMsgBuf;
	FormatMessageA(0x1300 /*FORMAT_MESSAGE_ALLOCATE_BUFFER | 
							FORMAT_MESSAGE_FROM_SYSTEM | 
							FORMAT_MESSAGE_IGNORE_INSERTS*/,
		NULL,
		GetLastError(),
		1 << 10 /*MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT)*/, // Default language
		&lpMsgBuf,
		0,
		NULL 
	);
	Printf (TEXTCOLOR_RED "  %s\n", lpMsgBuf);
	// Free the buffer.
	LocalFree( lpMsgBuf );
}
#else
static void PrintLastError ()
{
	Printf (TEXTCOLOR_RED "  %s\n", strerror(errno));
}
#endif
