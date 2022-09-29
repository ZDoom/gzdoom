/**
 * This file has no copyright assigned and is placed in the Public Domain.
 * This file is part of the w64 mingw-runtime package.
 * No warranty is given; refer to the file DISCLAIMER.PD within this package.
 */
#ifndef _dbg_LOAD_IMAGE_h
#define _dbg_LOAD_IMAGE_h

#ifndef WINAPI
#define WINAPI __stdcall
#endif

#define IMAGEAPI DECLSPEC_IMPORT WINAPI
#define DBHLP_DEPRECIATED __declspec(deprecated)

#define DBHLPAPI IMAGEAPI

#ifndef EBACKTRACE_MINGW32

#define IMAGE_SEPARATION (64*1024)

  typedef struct _LOADED_IMAGE {
    PSTR ModuleName;
    HANDLE hFile;
    PUCHAR MappedAddress;
#ifdef _IMAGEHLP64
    PIMAGE_NT_HEADERS64 FileHeader;
#else
    PIMAGE_NT_HEADERS32 FileHeader;
#endif
    PIMAGE_SECTION_HEADER LastRvaSection;
    ULONG NumberOfSections;
    PIMAGE_SECTION_HEADER Sections;
    ULONG Characteristics;
    BOOLEAN fSystemImage;
    BOOLEAN fDOSImage;
    BOOLEAN fReadOnly;
    UCHAR Version;
    LIST_ENTRY Links;
    ULONG SizeOfImage;
  } LOADED_IMAGE,*PLOADED_IMAGE;

#endif

#define MAX_SYM_NAME 2000

  typedef struct _MODLOAD_DATA {
    DWORD ssize;
    DWORD ssig;
    PVOID data;
    DWORD size;
    DWORD flags;
  } MODLOAD_DATA,*PMODLOAD_DATA;

#endif
