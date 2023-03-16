/*
** i_crash.cpp
** Gathers exception information when the program crashes.
**
**---------------------------------------------------------------------------
** Copyright 2002-2006 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.s
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

// HEADER FILES ------------------------------------------------------------

#pragma warning(disable:4996)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <richedit.h>
#include <tlhelp32.h>
#ifndef _M_IX86
#include <winternl.h>
#endif
#ifndef __GNUC__
#if _MSC_VER
#pragma warning(disable:4091)	// this silences a warning for a bogus definition in the Windows 8.1 SDK.
#endif
#include <dbghelp.h>
#if _MSC_VER
#pragma warning(default:4091)
#endif
#endif
#include <commctrl.h>
#include <commdlg.h>
#include <uxtheme.h>
#include <shellapi.h>

#include <stdint.h>
#include <stdio.h>
#include "resource.h"
#include "version.h"
#include "m_swap.h"
#include "basics.h"
#include "zstring.h"
#include "printf.h"
#include "cmdlib.h"
#include "i_mainwindow.h"

#include <time.h>
#include <miniz.h>

// MACROS ------------------------------------------------------------------

// Maximum number of files that might appear in a crash report.
#define MAX_FILES 5

#define ZIP_LOCALFILE	MAKE_ID('P','K',3,4)
#define ZIP_CENTRALFILE	MAKE_ID('P','K',1,2)
#define ZIP_ENDOFDIR	MAKE_ID('P','K',5,6)

// DBGHELP.H ---------------------------------------------------------------

// w32api does not include dbghelp.h, so if I don't include these here,
// a person using GCC will need to download the Platform SDK, grab dbghelp.h
// from it, and edit it so it works with GCC.
#ifdef __GNUC__
typedef enum _MINIDUMP_TYPE
{
    MiniDumpNormal
	// Other types omitted.
} MINIDUMP_TYPE;

typedef struct _MINIDUMP_EXCEPTION_INFORMATION {
    DWORD ThreadId;
    PEXCEPTION_POINTERS ExceptionPointers;
    BOOL ClientPointers;
} MINIDUMP_EXCEPTION_INFORMATION, *PMINIDUMP_EXCEPTION_INFORMATION;

typedef struct _MINIDUMP_USER_STREAM_INFORMATION {
    ULONG UserStreamCount;
    void *UserStreamArray;			// Not really void *
} MINIDUMP_USER_STREAM_INFORMATION, *PMINIDUMP_USER_STREAM_INFORMATION;

typedef BOOL (WINAPI * MINIDUMP_CALLBACK_ROUTINE) (
    IN PVOID CallbackParam,
    IN CONST void *CallbackInput,	// Not really void *
    IN OUT void *CallbackOutput		// Not really void *
    );

typedef struct _MINIDUMP_CALLBACK_INFORMATION {
    MINIDUMP_CALLBACK_ROUTINE CallbackRoutine;
    PVOID CallbackParam;
} MINIDUMP_CALLBACK_INFORMATION, *PMINIDUMP_CALLBACK_INFORMATION;
#endif

// Dbghelp.dll is loaded at runtime so we don't create any needless
// dependencies on it.
typedef BOOL (WINAPI *THREADWALK) (HANDLE, LPTHREADENTRY32);
typedef BOOL (WINAPI *MODULEWALK) (HANDLE, LPMODULEENTRY32);
typedef HANDLE (WINAPI *CREATESNAPSHOT) (DWORD, DWORD);
typedef BOOL (WINAPI *WRITEDUMP) (HANDLE, DWORD, HANDLE, int,
								  PMINIDUMP_EXCEPTION_INFORMATION,
								  PMINIDUMP_USER_STREAM_INFORMATION,
								  PMINIDUMP_CALLBACK_INFORMATION);

// TYPES -------------------------------------------------------------------

#ifdef _M_X64
typedef PRUNTIME_FUNCTION (WINAPI *RTLLOOKUPFUNCTIONENTRY)
	(ULONG64 ControlPc, PULONG64 ImageBase, void *HistoryTable);
#endif

// Damn Microsoft for doing Get/SetWindowLongPtr half-assed. Instead of
// giving them proper prototypes under Win32, they are just macros for
// Get/SetWindowLong, meaning they take LONGs and not LONG_PTRs.
#ifdef _WIN64
typedef LONG_PTR WLONG_PTR;
#else
typedef LONG WLONG_PTR;
#endif

namespace zip
{
#pragma pack(push,1)
	struct LocalFileHeader
	{
		uint32_t	Magic;						// 0
		uint8_t	VersionToExtract[2];		// 4
		uint16_t	Flags;						// 6
		uint16_t	Method;						// 8
		uint16_t	ModTime;					// 10
		uint16_t	ModDate;					// 12
		uint32_t	CRC32;						// 14
		uint32_t	CompressedSize;				// 18
		uint32_t	UncompressedSize;			// 22
		uint16_t	NameLength;					// 26
		uint16_t	ExtraLength;				// 28
	};

	struct CentralDirectoryEntry
	{
		uint32_t	Magic;
		uint8_t		VersionMadeBy[2];
		uint8_t		VersionToExtract[2];
		uint16_t	Flags;
		uint16_t	Method;
		uint16_t	ModTime;
		uint16_t	ModDate;
		uint32_t	CRC32;
		uint32_t	CompressedSize;
		uint32_t	UncompressedSize;
		uint16_t	NameLength;
		uint16_t	ExtraLength;
		uint16_t	CommentLength;
		uint16_t	StartingDiskNumber;
		uint16_t	InternalAttributes;
		uint32_t	ExternalAttributes;
		uint32_t	LocalHeaderOffset;
	};

	struct EndOfCentralDirectory
	{
		uint32_t	Magic;
		uint16_t	DiskNumber;
		uint16_t	FirstDisk;
		uint16_t	NumEntries;
		uint16_t	NumEntriesOnAllDisks;
		uint32_t	DirectorySize;
		uint32_t	DirectoryOffset;
		uint16_t	ZipCommentLength;
	};
#pragma pack(pop)
}

struct TarFile
{
	HANDLE		File;
	const char *Filename;
	int			ZipOffset;
	uint32_t	UncompressedSize;
	uint32_t	CompressedSize;
	uint32_t	CRC32;
	bool		Deflated;
};

struct MiniDumpThreadData
{
	HANDLE							File;
	WRITEDUMP						pMiniDumpWriteDump;
	MINIDUMP_EXCEPTION_INFORMATION *Exceptor;
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void AddFile (HANDLE file, const char *filename);
static void CloseTarFiles ();
static HANDLE MakeZip ();
static void AddZipFile (HANDLE ziphandle, TarFile *whichfile, short dosdate, short dostime);
static HANDLE CreateTempFile ();

static void DumpBytes (HANDLE file, uint8_t *address);
static void AddStackInfo (HANDLE file, void *dumpaddress, DWORD code, CONTEXT *ctxt);
static void StackWalk (HANDLE file, void *dumpaddress, DWORD *topOfStack, DWORD *jump, CONTEXT *ctxt);
static void AddToolHelp (HANDLE file);

static HANDLE WriteTextReport ();
static DWORD WINAPI WriteMiniDumpInAnotherThread (LPVOID lpParam);

static INT_PTR CALLBACK DetailsDlgProc (HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
static void SetEditControl (HWND control, HWND sizedisplay, int filenum);

static BOOL UploadReport (HANDLE report, bool gziped);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern HINSTANCE g_hInst;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

EXCEPTION_POINTERS CrashPointers;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static HRESULT (__stdcall *pEnableThemeDialogTexture) (HWND hwnd, DWORD dwFlags);

static TarFile TarFiles[MAX_FILES];

static int NumFiles;

static HANDLE DbgProcess;
static DWORD DbgProcessID;
static DWORD DbgThreadID;
static DWORD CrashCode;
static PVOID CrashAddress;
static bool NeedDbgHelp;
static char CrashSummary[256];

static WNDPROC StdStaticProc;

// CODE --------------------------------------------------------------------

//==========================================================================
//
// SafeReadMemory
//
// Returns true if the memory was succussfully read and false if not.
//
//==========================================================================

static bool SafeReadMemory (const void *base, void *buffer, size_t len)
{
#ifdef __GNUC__
	// GCC version: Test the memory beforehand and hope it doesn't become
	// unreadable while we read it. GCC is the only Windows compiler (that
	// I know of) that can't do SEH.

	if (IsBadReadPtr (base, len))
	{
		return false;
	}
	memcpy (buffer, base, len);
	return true;
#else
	// Non-GCC version: Use SEH to catch any bad reads immediately when
	// they happen instead of trying to catch them beforehand.

	bool success = false;

	__try
	{
		memcpy (buffer, base, len);
		success = true;
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
	}
	return success;
#endif
}

//==========================================================================
//
// GetTopOfStack
//
// Given a pointer to some address on the stack, returns a pointer to
// the top of the stack.
//
//==========================================================================

DWORD *GetTopOfStack (void *top)
{
	MEMORY_BASIC_INFORMATION memInfo;

	if (VirtualQuery (top, &memInfo, sizeof(memInfo)))
	{
		return (DWORD *)((uint8_t *)memInfo.BaseAddress + memInfo.RegionSize);
	}
	else
	{
		return NULL;
	}
}

//==========================================================================
//
// WriteMyMiniDump
//
// Writes a minidump if dbghelp.dll is present and adds it to the tarball.
// Otherwise, it just returns INVALID_HANDLE_VALUE.
//
//==========================================================================

static HANDLE WriteMyMiniDump (void)
{
	MINIDUMP_EXCEPTION_INFORMATION exceptor = { DbgThreadID, &CrashPointers, FALSE };
	WCHAR dbghelpPath[MAX_PATH+12], *bs;
	WRITEDUMP pMiniDumpWriteDump;
	HANDLE file = INVALID_HANDLE_VALUE;
	BOOL good = FALSE;
	HMODULE dbghelp = NULL;

	// Make sure dbghelp.dll and MiniDumpWriteDump are available
	// Try loading from the application directory first, then from the search path.
	GetModuleFileNameW (NULL, dbghelpPath, MAX_PATH);
	dbghelpPath[MAX_PATH] = 0;
	bs = wcsrchr (dbghelpPath, '\\');
	if (bs != NULL)
	{
		wcscpy (bs + 1, L"dbghelp.dll");
		dbghelp = LoadLibraryW (dbghelpPath);
	}
	if (dbghelp == NULL)
	{
		dbghelp = LoadLibraryA ("dbghelp.dll");
		if (dbghelp == NULL)
		{
			NeedDbgHelp = true;
			return INVALID_HANDLE_VALUE;
		}
	}
	pMiniDumpWriteDump = (WRITEDUMP)GetProcAddress (dbghelp, "MiniDumpWriteDump");
	if (pMiniDumpWriteDump != NULL)
	{
		file = CreateTempFile ();
		if (file != INVALID_HANDLE_VALUE)
		{
			if (CrashPointers.ExceptionRecord->ExceptionCode != EXCEPTION_STACK_OVERFLOW)
			{
				good = pMiniDumpWriteDump (DbgProcess, DbgProcessID, file,
#if 1
					MiniDumpNormal
#else
					MiniDumpWithDataSegs|MiniDumpWithIndirectlyReferencedMemory|MiniDumpWithPrivateReadWriteMemory
#endif
					, &exceptor, NULL, NULL);
			}
			else
			{
				MiniDumpThreadData dumpdata = { file, pMiniDumpWriteDump, &exceptor };
				DWORD id;
				HANDLE thread = CreateThread (NULL, 0, WriteMiniDumpInAnotherThread,
					&dumpdata, 0, &id);
				WaitForSingleObject (thread, INFINITE);
				if (GetExitCodeThread (thread, &id))
				{
					good = id;
				}
			}
		}
	}
	else
	{
		NeedDbgHelp = true;
	}
	return good ? file : INVALID_HANDLE_VALUE;
}

//==========================================================================
//
// WriteMiniDumpInAnotherThread
//
// When a stack overflow occurs, there isn't enough room left on the stack
// for MiniDumpWriteDump to do its thing, so we create a new thread with
// a new stack to do the work.
//
//==========================================================================

static DWORD WINAPI WriteMiniDumpInAnotherThread (LPVOID lpParam)
{
	MiniDumpThreadData *dumpdata = (MiniDumpThreadData *)lpParam;
	return dumpdata->pMiniDumpWriteDump (DbgProcess, DbgProcessID,
		dumpdata->File, MiniDumpNormal, dumpdata->Exceptor, NULL, NULL);
}

//==========================================================================
//
// Writef
//
// Like printf, but for Win32 file HANDLEs.
//
//==========================================================================

void Writef (HANDLE file, const char *format, ...)
{
	char buffer[1024];
	va_list args;
	DWORD len;

	va_start (args, format);
	len = myvsnprintf (buffer, sizeof buffer, format, args);
	va_end (args);
	WriteFile (file, buffer, len, &len, NULL);
}

//==========================================================================
//
// WriteLogFileStreamer
//
// The callback function to stream a Rich Edit's contents to a file.
//
//==========================================================================


//==========================================================================
//
// WriteLogFile
//
// Writes the contents of a Rich Edit control to a file.
//
//==========================================================================

HANDLE WriteLogFile()
{
	HANDLE file;

	file = CreateTempFile();
	if (file != INVALID_HANDLE_VALUE)
	{
		auto writeFile = [&](const void* data, uint32_t size, uint32_t& written) -> bool
		{
			DWORD tmp = 0;
			BOOL result = WriteFile(file, data, size, &tmp, nullptr);
			written = tmp;
			return result == TRUE;
		};
		mainwindow.GetLog(writeFile);
	}
	return file;
}

//==========================================================================
//
// CreateCrashLog
//
// Creates all the files needed for a crash report.
//
//==========================================================================

void CreateCrashLog (const char *custominfo, DWORD customsize)
{
	// Do not collect information more than once.
	if (NumFiles != 0)
	{
		return;
	}

	DbgThreadID = GetCurrentThreadId();
	DbgProcessID = GetCurrentProcessId();
	DbgProcess = GetCurrentProcess();

	CrashCode = CrashPointers.ExceptionRecord->ExceptionCode;
	CrashAddress = CrashPointers.ExceptionRecord->ExceptionAddress;

	AddFile (WriteTextReport(), "report.txt");
	AddFile (WriteMyMiniDump(), "minidump.mdmp");

	// Copy app-specific information out of the crashing process's memory
	// and into a new temporary file.
	if (customsize != 0)
	{
		HANDLE file = CreateTempFile();
		DWORD wrote;

		if (file != INVALID_HANDLE_VALUE)
		{
			uint8_t buffer[512];
			DWORD left;

			for (;; customsize -= left, custominfo += left)
			{
				left = customsize > 512 ? 512 : customsize;
				if (left == 0)
				{
					break;
				}
				if (SafeReadMemory (custominfo, buffer, left))
				{
					WriteFile (file, buffer, left, &wrote, NULL);
				}
				else
				{
					Writef (file, "Failed reading remaining %lu bytes\r\n", customsize);
					break;
				}
			}
			AddFile (file, "local.txt");
		}
	}
	AddFile (WriteLogFile(), "log.rtf");
	CloseHandle (DbgProcess);
}

//==========================================================================
//
// WriteTextReport
//
// This is the classic plain-text report ZDoom has generated since it
// added crash gathering abilities in 2002.
//
//==========================================================================

HANDLE WriteTextReport ()
{
	HANDLE file;

	static const struct
	{
		DWORD Code; const char *Text;
	}
	exceptions[] =
	{
		{ EXCEPTION_ACCESS_VIOLATION, "Access Violation" },
		{ EXCEPTION_ARRAY_BOUNDS_EXCEEDED, "Array Bounds Exceeded" },
		{ EXCEPTION_BREAKPOINT, "Breakpoint" },
		{ EXCEPTION_DATATYPE_MISALIGNMENT, "Data Type Misalignment" },
		{ EXCEPTION_FLT_DENORMAL_OPERAND, "Floating Point Denormal Operand." },
		{ EXCEPTION_FLT_DIVIDE_BY_ZERO, "Floating Point Divide By Zero" },
		{ EXCEPTION_FLT_INEXACT_RESULT, "Floating Point Inexact Result" },
		{ EXCEPTION_FLT_INVALID_OPERATION, "Floating Point Invalid Operation" },
		{ EXCEPTION_FLT_OVERFLOW, "Floating Point Overflow" },
		{ EXCEPTION_FLT_STACK_CHECK, "Floating Point Stack Check" },
		{ EXCEPTION_FLT_UNDERFLOW, "Floating Point Underflow" },
		{ EXCEPTION_ILLEGAL_INSTRUCTION, "Illegal Instruction" },
		{ EXCEPTION_IN_PAGE_ERROR, "In Page Error" },
		{ EXCEPTION_INT_DIVIDE_BY_ZERO, "Integer Divide By Zero" },
		{ EXCEPTION_INT_OVERFLOW, "Integer Overflow" },
		{ EXCEPTION_INVALID_DISPOSITION, "Invalid Disposition" },
		{ EXCEPTION_NONCONTINUABLE_EXCEPTION, "Noncontinuable Exception" },
		{ EXCEPTION_PRIV_INSTRUCTION, "Priviledged Instruction" },
		{ EXCEPTION_SINGLE_STEP, "Single Step" },
		{ EXCEPTION_STACK_OVERFLOW, "Stack Overflow" }
	};
	static const char eflagsBits[][2] =
	{
		{ 'C','F' },	// Carry Flag
		{ 'x','1' },	// Always one
		{ 'P','F' },	// Parity Flag
		{ 'x','0' },	// Always zero
		{ 'A','F' },	// Auxilliary Carry Flag
		{ 'x','0' },	// Always zero
		{ 'Z','F' },	// Zero Flag
		{ 'S','F' },	// Sign Flag
		{ 'T','F' },	// Trap Flag
		{ 'I','F' },	// Interrupt Enable Flag
		{ 'D','F' },	// Direction Flag
		{ 'O','F' },	// Overflow Flag
		{ 'x','x' },	// IOPL low bit
		{ 'x','x' },	// IOPL high bit
		{ 'N','T' },	// Nested Task
		{ 'x','0' },	// Always zero
		{ 'R','F' },	// Resume Flag
		{ 'V','M' },	// Virtual-8086 Mode
		{ 'A','C' },	// Alignment Check
		{ 'V','I' },	// Virtual Interrupt Flag
		{ 'V','P' }		// Virtual Interrupt Pending
	};


	int i, j;

	file = CreateTempFile ();
	if (file == INVALID_HANDLE_VALUE)
	{
		return file;
	}

	OSVERSIONINFO verinfo = { sizeof(verinfo) };
	GetVersionEx (&verinfo);

	for (i = 0; (size_t)i < sizeof(exceptions)/sizeof(exceptions[0]); ++i)
	{
		if (exceptions[i].Code == CrashPointers.ExceptionRecord->ExceptionCode)
		{
			break;
		}
	}
	j = mysnprintf (CrashSummary, countof(CrashSummary), "Code: %08lX", CrashPointers.ExceptionRecord->ExceptionCode);
	if ((size_t)i < sizeof(exceptions)/sizeof(exceptions[0]))
	{
		j += mysnprintf (CrashSummary + j, countof(CrashSummary) - j, " (%s", exceptions[i].Text);
		if (CrashPointers.ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION)
		{
			// Pre-NT kernels do not seem to provide this information.
			if (verinfo.dwPlatformId != VER_PLATFORM_WIN32_WINDOWS)
			{
				j += mysnprintf (CrashSummary + j, countof(CrashSummary) - j,
					" - tried to %s address %p",
					CrashPointers.ExceptionRecord->ExceptionInformation[0] ? "write" : "read",
					(void *)CrashPointers.ExceptionRecord->ExceptionInformation[1]);
			}
		}
		CrashSummary[j++] = ')';
	}
	j += mysnprintf (CrashSummary + j, countof(CrashSummary) - j, "\r\nAddress: %p", CrashPointers.ExceptionRecord->ExceptionAddress);
	Writef (file, "%s\r\nFlags: %08X\r\n\r\n", CrashSummary, CrashPointers.ExceptionRecord->ExceptionFlags);

	Writef (file, "Windows %s %d.%d Build %d %s\r\n\r\n",
		verinfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS ? "9x" : "NT",
		verinfo.dwMajorVersion, verinfo.dwMinorVersion, verinfo.dwBuildNumber, verinfo.szCSDVersion);

	CONTEXT *ctxt = CrashPointers.ContextRecord;

	if (ctxt->ContextFlags & CONTEXT_SEGMENTS)
	{
		Writef (file, "GS=%04x  FS=%04x  ES=%04x  DS=%04x\r\n",
			ctxt->SegGs, ctxt->SegFs, ctxt->SegEs, ctxt->SegDs);
	}

	if (ctxt->ContextFlags & CONTEXT_INTEGER)
	{
#ifndef _M_X64
		Writef (file, "EAX=%08x  EBX=%08x  ECX=%08x  EDX=%08x\r\nESI=%08x  EDI=%08x\r\n",
			ctxt->Eax, ctxt->Ebx, ctxt->Ecx, ctxt->Edx, ctxt->Esi, ctxt->Edi);
#else
		Writef (file, "RAX=%016I64x  RBX=%016I64x  RCX=%016I64x\r\n"
					  "RDX=%016I64x  RSI=%016I64x  RDI=%016I64x\r\n"
					  "RBP=%016I64x   R8=%016I64x   R9=%016I64x\r\n"
					  "R10=%016I64x  R11=%016I64x  R12=%016I64x\r\n"
					  "R13=%016I64x  R14=%016I64x  R15=%016I64x\r\n",
			ctxt->Rax, ctxt->Rbx, ctxt->Rcx, ctxt->Rdx, ctxt->Rsi, ctxt->Rdi, ctxt->Rbp,
			ctxt->R8, ctxt->R9, ctxt->R10, ctxt->R11, ctxt->R12, ctxt->R13, ctxt->R14, ctxt->R15);
#endif
	}

	if (ctxt->ContextFlags & CONTEXT_CONTROL)
	{
#ifndef _M_X64
		Writef (file, "EBP=%08x  EIP=%08x  ESP=%08x  CS=%04x  SS=%04x\r\nEFlags=%08x\r\n",
			ctxt->Ebp, ctxt->Eip, ctxt->Esp, ctxt->SegCs, ctxt->SegSs, ctxt->EFlags);
#else
		Writef (file, "RIP=%016I64x  RSP=%016I64x\r\nCS=%04x  SS=%04x  EFlags=%08x\r\n",
			ctxt->Rip, ctxt->Rsp, ctxt->SegCs, ctxt->SegSs, ctxt->EFlags);
#endif

		DWORD dw;

		for (i = 0, dw = 1; (size_t)i < sizeof(eflagsBits)/sizeof(eflagsBits[0]); dw <<= 1, ++i)
		{
			if (eflagsBits[i][0] != 'x')
			{
				Writef (file, " %c%c%c", eflagsBits[i][0], eflagsBits[i][1],
					ctxt->EFlags & dw ? '+' : '-');
			}
		}
		Writef (file, "\r\n");
	}

	if (ctxt->ContextFlags & CONTEXT_FLOATING_POINT)
	{
#ifndef _M_X64
		Writef (file,
			"\r\nFPU State:\r\n ControlWord=%04x StatusWord=%04x TagWord=%04x\r\n"
			" ErrorOffset=%08x\r\n ErrorSelector=%08x\r\n DataOffset=%08x\r\n DataSelector=%08x\r\n"
			// Cr0NpxState was renamed in recent Windows headers so better skip it here. Its meaning is unknown anyway.
			//" Cr0NpxState=%08x\r\n"
			"\r\n"
			,
			(uint16_t)ctxt->FloatSave.ControlWord, (uint16_t)ctxt->FloatSave.StatusWord, (uint16_t)ctxt->FloatSave.TagWord,
			ctxt->FloatSave.ErrorOffset, ctxt->FloatSave.ErrorSelector, ctxt->FloatSave.DataOffset,
			ctxt->FloatSave.DataSelector
			//, ctxt->FloatSave.Cr0NpxState
			);

		for (i = 0; i < 8; ++i)
		{
			DWORD d0, d1;
			memcpy(&d0, &ctxt->FloatSave.RegisterArea[20*i+4], sizeof(DWORD));
			memcpy(&d1, &ctxt->FloatSave.RegisterArea[20*i], sizeof(DWORD));
			Writef (file, "MM%d=%08x%08x\r\n", i, d0, d1);
		}
#else
		for (i = 0; i < 8; ++i)
		{
			Writef (file, "MM%d=%016I64x\r\n", i, ctxt->Legacy[i].Low);
		}
		for (i = 0; i < 16; ++i)
		{
			Writef (file, "XMM%d=%016I64x%016I64x\r\n", i, ctxt->FltSave.XmmRegisters[i].High, ctxt->FltSave.XmmRegisters[i].Low);
		}
#endif
	}

	AddToolHelp (file);

#ifdef _M_IX86
	Writef (file, "\r\nBytes near EIP:");
#else
	Writef (file, "\r\nBytes near RIP:");
#endif
	DumpBytes (file, (uint8_t *)CrashPointers.ExceptionRecord->ExceptionAddress-16);

	if (ctxt->ContextFlags & CONTEXT_CONTROL)
	{
#ifndef _M_X64
		AddStackInfo (file, (void *)(size_t)CrashPointers.ContextRecord->Esp,
			CrashPointers.ExceptionRecord->ExceptionCode, ctxt);
#else
		AddStackInfo (file, (void *)CrashPointers.ContextRecord->Rsp,
			CrashPointers.ExceptionRecord->ExceptionCode, ctxt);
#endif
	}

	return file;
}

//==========================================================================
//
// AddToolHelp
//
// Adds the information supplied by the tool help functions to the text
// report. This includes the list of threads for this process and the
// loaded modules.
//
//==========================================================================

static void AddToolHelp (HANDLE file)
{
	HMODULE kernel = GetModuleHandleA ("kernel32.dll");
	if (kernel == NULL)
		return;

	CREATESNAPSHOT pCreateToolhelp32Snapshot;
	THREADWALK pThread32First;
	THREADWALK pThread32Next;
	MODULEWALK pModule32First;
	MODULEWALK pModule32Next;

	pCreateToolhelp32Snapshot = (CREATESNAPSHOT)GetProcAddress (kernel, "CreateToolhelp32Snapshot");
	pThread32First = (THREADWALK)GetProcAddress (kernel, "Thread32First");
	pThread32Next = (THREADWALK)GetProcAddress (kernel, "Thread32Next");
	pModule32First = (MODULEWALK)GetProcAddress (kernel, "Module32FirstW");
	pModule32Next = (MODULEWALK)GetProcAddress (kernel, "Module32NextW");

	if (!(pCreateToolhelp32Snapshot && pThread32First && pThread32Next &&
		  pModule32First && pModule32Next))
	{
		Writef (file, "\r\nTool Help unavailable\r\n");
		return;
	}

	HANDLE snapshot = pCreateToolhelp32Snapshot (TH32CS_SNAPMODULE|TH32CS_SNAPTHREAD, 0);
	if (snapshot == INVALID_HANDLE_VALUE)
	{
		Writef (file, "\r\nTool Help snapshot unavailable\r\n");
		return;
	}

	THREADENTRY32 thread = { sizeof(thread) };

	Writef (file, "\r\nRunning threads:\r\n");

	if (pThread32First (snapshot, &thread))
	{
		do
		{
			if (thread.th32OwnerProcessID == DbgProcessID)
			{
				Writef (file, "%08x", thread.th32ThreadID);

				if (thread.th32ThreadID == DbgThreadID)
				{
					Writef (file, " at %p*", CrashAddress);
				}
				Writef (file, "\r\n");
			}
		} while (pThread32Next (snapshot, &thread));
	}

	MODULEENTRY32 module = { sizeof(module) };

	Writef (file, "\r\nLoaded modules:\r\n");

	if (pModule32First (snapshot, &module))
	{
		do
		{
			auto amod = FString(module.szModule);
			Writef (file, "%p - %p %c%s\r\n",
				module.modBaseAddr, module.modBaseAddr + module.modBaseSize - 1,
				module.modBaseAddr <= CrashPointers.ExceptionRecord->ExceptionAddress &&
				module.modBaseAddr + module.modBaseSize > CrashPointers.ExceptionRecord->ExceptionAddress
				? '*' : ' ',
				amod.GetChars());
		} while (pModule32Next (snapshot, &module));
	}

	CloseHandle (snapshot);
}

//==========================================================================
//
// AddStackInfo
//
// Writes a stack dump to the text report.
//
//==========================================================================

static void AddStackInfo (HANDLE file, void *dumpaddress, DWORD code, CONTEXT *ctxt)
{
	DWORD *addr = (DWORD *)dumpaddress, *jump;
	DWORD *topOfStack = GetTopOfStack (dumpaddress);
	uint8_t peekb;
#ifdef _M_IX86
	DWORD peekd;
#else
	uint64_t peekq;
#endif

	jump = topOfStack;
	if (code == EXCEPTION_STACK_OVERFLOW)
	{
		// If the stack overflowed, only dump the first and last 16KB of it.
		if (topOfStack - addr > 32768/4)
		{
			jump = addr + 16384/4;
		}
	}

	StackWalk (file, dumpaddress, topOfStack, jump, ctxt);

	Writef (file, "\r\nStack Contents:\r\n");
	DWORD *scan;
	for (scan = addr; scan < topOfStack; scan += 4)
	{
		int i;
		ptrdiff_t max;

		if (scan == jump)
		{
			scan = topOfStack - 16384/4;
			Writef (file, "\r\n . . . Snip . . .\r\n\r\n");
		}

		Writef (file, "%p:", scan);
#ifdef _M_IX86
		if (topOfStack - scan < 4)
		{
			max = topOfStack - scan;
		}
		else
		{
			max = 4;
		}

		for (i = 0; i < max; ++i)
		{
			if (!SafeReadMemory (&scan[i], &peekd, 4))
			{
				break;
			}
			Writef (file, " %08x", peekd);
		}
		for (; i < 4; ++i)
		{
			Writef (file, "         ");
		}
#else
		if ((uint64_t *)topOfStack - (uint64_t *)scan < 2)
		{
			max = (uint64_t *)topOfStack - (uint64_t *)scan;
		}
		else
		{
			max = 2;
		}

		for (i = 0; i < max; ++i)
		{
			if (!SafeReadMemory (&scan[i], &peekq, 8))
			{
				break;
			}
			Writef (file, " %016x", peekq);
		}
		if (i < 2)
		{
			Writef (file, "                 ");
		}
#endif
		Writef (file, "  ");
		for (i = 0; i < int(max*sizeof(void*)); ++i)
		{
			if (!SafeReadMemory ((uint8_t *)scan + i, &peekb, 1))
			{
				break;
			}
			Writef (file, "%c", peekb >= 0x20 && peekb <= 0x7f ? peekb : '\xb7');
		}
		Writef (file, "\r\n");
	}
}

#ifdef _M_IX86

//==========================================================================
//
// StackWalk
// Win32 version
//
// Lists a possible call trace for the crashing thread to the text report.
// This is not very specific and just lists any pointers into the
// main executable region, whether they are part of the real trace or not.
//
//==========================================================================

static void StackWalk (HANDLE file, void *dumpaddress, DWORD *topOfStack, DWORD *jump, CONTEXT *ctxt)
{
	DWORD *addr = (DWORD *)dumpaddress;

	uint8_t *pBaseOfImage = (uint8_t *)GetModuleHandle (0);
	IMAGE_OPTIONAL_HEADER *pHeader = (IMAGE_OPTIONAL_HEADER *)(pBaseOfImage +
		((IMAGE_DOS_HEADER*)pBaseOfImage)->e_lfanew +
		sizeof(IMAGE_NT_SIGNATURE) + sizeof(IMAGE_FILE_HEADER));

	DWORD_PTR codeStart = (DWORD_PTR)pBaseOfImage + pHeader->BaseOfCode;
	DWORD_PTR codeEnd = codeStart + pHeader->SizeOfCode;

	Writef (file, "\r\nPossible call trace:\r\n %08x  BOOM", CrashAddress);
	for (DWORD *scan = addr; scan < topOfStack; ++scan)
	{
		if (scan == jump)
		{
			scan = topOfStack - 16384/4;
			Writef (file, "\r\n\r\n  . . . . Snip . . . .\r\n");
		}

		DWORD_PTR code;

		if (SafeReadMemory (scan, &code, sizeof(code)) &&
			code >= codeStart && code < codeEnd)
		{
			static const char regNames[][4] =
			{
				"eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi"
			};

			// Check if address is after a call statement. Print what was called if it is.
			const uint8_t *bytep = (uint8_t *)code;
			uint8_t peekb;

#define chkbyte(x,m,v) (SafeReadMemory(x, &peekb, 1) && ((peekb & m) == v))

			if (chkbyte(bytep - 5, 0xFF, 0xE8) || chkbyte(bytep - 5, 0xFF, 0xE9))
			{
				DWORD peekd;
				if (SafeReadMemory (bytep - 4, &peekd, 4))
				{
					DWORD_PTR jumpaddr = peekd + code;
					Writef (file, "\r\n %p  %s %p", code - 5,
						peekb == 0xE9 ? "jmp " : "call", jumpaddr);
					if (chkbyte((LPCVOID)jumpaddr, 0xFF, 0xE9) &&
						SafeReadMemory ((LPCVOID)(jumpaddr + 1), &peekd, 4))
					{
						Writef (file," => jmp  %p", peekd + jumpaddr + 5);
					}
				}
				else
				{
					Writef (file, "\r\n %p  %s ????????", code - 5,
						peekb == 0xE9 ? "jmp " : "call");
				}
			}
			else if (chkbyte(bytep - 2, 0xFF, 0xFF) && chkbyte(bytep - 1, 0xF7, 0xD0))
			{
				Writef (file, "\r\n %p  call %s", code - 2, regNames[peekb & 7]);
			}
			else
			{
				int i;

				for (i = 2; i < 7; ++i)
				{
					if (chkbyte(bytep - i, 0xFF, 0xFF) && chkbyte(bytep - i + 1, 070, 020))
					{
						break;
					}
				}
				if (i >= 7)
				{
					Writef (file, "\r\n %08x", code);
				}
				else
				{
					int mod, rm, basereg = -1, indexreg = -1;
					int scale = 1, offset = 0;

					Writef (file, "\r\n %08x", bytep - i);
					bytep -= i - 1;
					SafeReadMemory (bytep, &peekb, 1);
					mod = peekb >> 6;
					rm = peekb & 7;
					bytep++;

					if (mod == 0 && rm == 5)
					{
						mod = 2;
					}
					else if (rm == 4)
					{
						int index, base;

						SafeReadMemory (bytep, &peekb, 1);
						scale = 1 << (peekb >> 6);
						index = (peekb >> 3) & 7;
						base = peekb & 7;
						bytep++;

						if (index != 4)
						{
							indexreg = index;
						}
						if (base != 5)
						{
							basereg = base;
						}
						else
						{
							if (mod == 0)
							{
								mod = 2;
							}
							else
							{
								basereg = 5;
							}
						}
					}
					else
					{
						basereg = rm;
					}

					if (mod == 1)
					{
						signed char miniofs;
						SafeReadMemory (bytep++, &miniofs, 1);
						offset = miniofs;
					}
					else
					{
						SafeReadMemory (bytep, &offset, 4);
						bytep += 4;
					}
					if ((DWORD_PTR)bytep == code)
					{
						Writef (file, "  call [", bytep - i + 1);
						if (basereg >= 0)
						{
							Writef (file, "%s", regNames[basereg]);
						}
						if (indexreg >= 0)
						{
							Writef (file, "%s%s", basereg >= 0 ? "+" : "", regNames[indexreg]);
							if (scale > 1)
							{
								Writef (file, "*%d", scale);
							}
						}
						if (offset != 0)
						{
							if (indexreg < 0 && basereg < 0)
							{
								Writef (file, "%08x", offset);
							}
							else
							{
								char sign;
								if (offset < 0)
								{
									sign = '-';
									offset = -offset;
								}
								else
								{
									sign = '+';
								}
								Writef (file, "%c0x%x", sign, offset);
							}
						}
						Writef (file, "]");
					}
				}
			}
		}
	}
	Writef (file, "\r\n");
}

#else

//==========================================================================
//
// StackWalk
// Win64 version
//
// Walks the stack for the crashing thread and dumps the trace to the text
// report. Unlike the Win32 version, Win64 provides facilities for
// doing a 100% exact walk.
//
// See http://www.nynaeve.net/?p=113 for more information, and
// http://www.nynaeve.net/Code/StackWalk64.cpp in particular.
//
//==========================================================================

static void StackWalk (HANDLE file, void *dumpaddress, DWORD *topOfStack, DWORD *jump, CONTEXT *ctxt)
{
	RTLLOOKUPFUNCTIONENTRY RtlLookupFunctionEntry;
	HMODULE kernel;
	CONTEXT context;
	KNONVOLATILE_CONTEXT_POINTERS nv_context;
	PRUNTIME_FUNCTION function;
	PVOID handler_data;
	ULONG64 establisher_frame;
	ULONG64 image_base;

	Writef (file, "\r\nCall trace:\r\n  rip=%p  <- Here it dies.\r\n", CrashAddress);

	kernel = GetModuleHandleA("kernel32.dll");
	if (kernel == NULL || NULL == (RtlLookupFunctionEntry =
		(RTLLOOKUPFUNCTIONENTRY)GetProcAddress(kernel, "RtlLookupFunctionEntry")))
	{
		Writef (file, "  Unavailable: Could not get address of RtlLookupFunctionEntry\r\n");
		return;
	}
	// Get the caller's context
	context = *ctxt;

	// This unwind loop intentionally skips the first call frame, as it
	// shall correspond to the call to StackTrace64, which we aren't
	// interested in.

	for (ULONG frame = 0; ; ++frame)
	{
		// Try to look up unwind metadata for the current function.
		function = RtlLookupFunctionEntry(context.Rip, &image_base, NULL);
		memset(&nv_context, 0, sizeof(nv_context));
		if (function == NULL)
		{
			// If we don't have a RUNTIME_FUNCTION, then we've encountered
			// a leaf function. Adjust the stack appropriately.
			context.Rip  = (ULONG64)(*(PULONG64)context.Rsp);
			context.Rsp += 8;
			Writef(file, "  Leaf function\r\n\r\n");
		}
		else
		{
			// Note that there is not a one-to-one correspondance between
			// runtime functions and source functions. One source function
			// may be broken into multiple runtime functions. This loop walks
			// backward from the current runtime function for however many
			// consecutive runtime functions precede it. There is a slight
			// chance that this will walk across different source functions.
			// (Or maybe not, depending on whether or not the compiler
			// guarantees that there will be empty space between functions;
			// it looks like VC++ might.) In practice, this seems to work
			// quite well for identifying the exact address to search for in
			// a map file to determine the function name.

			PRUNTIME_FUNCTION function2 = function;
			ULONG64 base = image_base;

			while (function2 != NULL)
			{
				Writef(file, "  Function range: %p -> %p\r\n",
					(void *)(base + function2->BeginAddress),
					(void *)(base + function2->EndAddress));
				function2 = RtlLookupFunctionEntry(base + function2->BeginAddress - 1, &base, NULL);
			}
			Writef(file, "\r\n");

			// Use RtlVirtualUnwind to execute the unwind for us.
			RtlVirtualUnwind(0/*UNW_FLAG_NHANDLER*/, image_base, context.Rip,
				function, &context, &handler_data, &establisher_frame,
				&nv_context);
		}
		// If we reach a RIP of zero, this means we've walked off the end of
		// the call stack and are done.
		if (context.Rip == 0)
		{
			break;
		}

		// Display the context. Note that we don't bother showing the XMM
		// context, although we have the nonvolatile portion of it.
		Writef(file, " FRAME %02d:\r\n  rip=%p rsp=%p rbp=%p\r\n",
			frame, context.Rip, context.Rsp, context.Rbp);
		Writef(file, "  r12=%p r13=%p r14=%p\r\n"
					 "  rdi=%p rsi=%p rbx=%p\r\n",
			context.R12, context.R13, context.R14,
			context.Rdi, context.Rsi, context.Rbx);

		static const char reg_names[16][4] =
		{
			"rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
			"r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"
		};

		// If we have stack-based register stores, then display them here.
		for (int i = 0; i < 16; ++i)
		{
			if (nv_context.IntegerContext[i])
			{
				Writef(file, "   -> '%s' saved on stack at %p (=> %p)\r\n",
					reg_names[i], nv_context.IntegerContext[i], *nv_context.IntegerContext[i]);
			}
		}
	}
}

#endif

//==========================================================================
//
// DumpBytes
//
// Writes the bytes around EIP to the text report.
//
//==========================================================================

static void DumpBytes (HANDLE file, uint8_t *address)
{
	char line[68*3], *line_p = line;
	DWORD len;
	uint8_t peek;

	for (int i = 0; i < 16*3; ++i)
	{
		if ((i & 15) == 0)
		{
			line_p += mysnprintf (line_p, countof(line) - (line_p - line), "\r\n%p:", address);
		}
		if (SafeReadMemory (address, &peek, 1))
		{
			line_p += mysnprintf (line_p, countof(line) - (line_p - line), " %02x", *address);
		}
		else
		{
			line_p += mysnprintf (line_p, countof(line) - (line_p - line), " --");
		}
		address++;
	}
	*line_p++ = '\r';
	*line_p++ = '\n';
	WriteFile (file, line, DWORD(line_p - line), &len, NULL);
}

//==========================================================================
//
// CreateTempFile
//
// Creates a new temporary with read/write access. The file will be deleted
// automatically when it is closed.
//
//==========================================================================

static HANDLE CreateTempFile ()
{
	WCHAR temppath[MAX_PATH-13];
	WCHAR tempname[MAX_PATH];
	if (!GetTempPathW (countof(temppath), temppath))
	{
		temppath[0] = '.';
		temppath[1] = '\0';
	}
	if (!GetTempFileNameW (temppath, L"zdo", 0, tempname))
	{
		return INVALID_HANDLE_VALUE;
	}
	return CreateFileW (tempname, GENERIC_WRITE|GENERIC_READ, 0, NULL, CREATE_ALWAYS,
		FILE_ATTRIBUTE_TEMPORARY|FILE_FLAG_DELETE_ON_CLOSE|FILE_FLAG_SEQUENTIAL_SCAN,
		NULL);
}

//==========================================================================
//
// AddFile
//
// Add another file to be written to the tar.
//
//==========================================================================

static void AddFile (HANDLE file, const char *filename)
{
	if (NumFiles == MAX_FILES || file == INVALID_HANDLE_VALUE)
	{
		return;
	}
	TarFiles[NumFiles].File = file;
	TarFiles[NumFiles].Filename = filename;
	NumFiles++;
}

//==========================================================================
//
// CloseTarFiles
//
// Close all files that were previously registerd with AddFile(). They
// must still be open.
//
//==========================================================================

static void CloseTarFiles ()
{
	for (int i = 0; i < NumFiles; ++i)
	{
		CloseHandle (TarFiles[i].File);
	}
	NumFiles = 0;
}

//==========================================================================
//
// WriteBlock
//
// This is a wrapper around WriteFile. If stream is non-NULL, then the data
// is compressed before writing it to the file. outbuf must be 1024 bytes.
//
//==========================================================================

static DWORD WriteBlock (HANDLE file, LPCVOID buffer, DWORD bytes, z_stream *stream, Bytef *outbuf)
{
	if (stream == NULL)
	{
		WriteFile (file, buffer, bytes, &bytes, NULL);
		return bytes;
	}
	else
	{
		DWORD wrote = 0;

		stream->next_in = (Bytef *)buffer;
		stream->avail_in = bytes;

		while (stream->avail_in != 0)
		{
			if (stream->avail_out == 0)
			{
				stream->next_out = outbuf;
				stream->avail_out = 1024;
				wrote += 1024;
				WriteFile (file, outbuf, 1024, &bytes, NULL);
			}
			deflate (stream, Z_NO_FLUSH);
		}
		return wrote;
	}
}

//==========================================================================
//
// WriteZip
//
// Writes a .zip/pk3 file. A HANDLE to the file is returned. This file is
// delete-on-close.
//
// The archive contains all the files previously passed to AddFile(). They
// must still be open, because MakeZip() does not open them itself.
//
//==========================================================================

static HANDLE MakeZip ()
{
	zip::CentralDirectoryEntry central = { ZIP_CENTRALFILE, { 20, 0 }, { 20, 0 }, };
	zip::EndOfCentralDirectory dirend = { ZIP_ENDOFDIR, };
	short dosdate, dostime;
	time_t now;
	struct tm *nowtm;
	int i, numfiles;
	HANDLE file;
	DWORD len;
	uint32_t dirsize;
	size_t namelen;

	if (NumFiles == 0)
	{
		return INVALID_HANDLE_VALUE;
	}

	// Open the zip file.
	file = CreateTempFile ();
	if (file == INVALID_HANDLE_VALUE)
	{
		return file;
	}

	time (&now);
	nowtm = localtime (&now);

	if (nowtm == NULL || nowtm->tm_year < 80)
	{
		dosdate = dostime = 0;
	}
	else
	{
		dosdate = (nowtm->tm_year - 80) * 512 + (nowtm->tm_mon + 1) * 32 + nowtm->tm_mday;
		dostime = nowtm->tm_hour * 2048 + nowtm->tm_min * 32 + nowtm->tm_sec / 2;
		dosdate = LittleShort(dosdate);
		dostime = LittleShort(dostime);
	}

	// Write out the zip archive, one file at a time
	for (i = 0; i < NumFiles; ++i)
	{
		AddZipFile (file, &TarFiles[i], dosdate, dostime);
	}

	// Write the central directory
	central.ModTime = dostime;
	central.ModDate = dosdate;

	dirend.DirectoryOffset = LittleLong((uint32_t)SetFilePointer (file, 0, NULL, FILE_CURRENT));

	for (i = 0, numfiles = 0, dirsize = 0; i < NumFiles; ++i)
	{
		// Skip empty files
		if (TarFiles[i].UncompressedSize == 0)
		{
			continue;
		}
		numfiles++;
		if (TarFiles[i].Deflated)
		{
			central.Flags = LittleShort((uint16_t)2);
			central.Method = LittleShort((uint16_t)8);
		}
		else
		{
			central.Flags = 0;
			central.Method = 0;
		}
		namelen = strlen(TarFiles[i].Filename);
		central.InternalAttributes = 0;
		if (namelen > 4 && stricmp(TarFiles[i].Filename - 4, ".txt") == 0)
		{ // Bit 0 set indicates this is probably a text file. But do any tools use it?
			central.InternalAttributes = LittleShort((uint16_t)1);
		}
		central.CRC32 = LittleLong(TarFiles[i].CRC32);
		central.CompressedSize = LittleLong(TarFiles[i].CompressedSize);
		central.UncompressedSize = LittleLong(TarFiles[i].UncompressedSize);
		central.NameLength = LittleShort((uint16_t)namelen);
		central.LocalHeaderOffset = LittleLong(TarFiles[i].ZipOffset);
		WriteFile (file, &central, sizeof(central), &len, NULL);
		WriteFile (file, TarFiles[i].Filename, (DWORD)namelen, &len, NULL);
		dirsize += DWORD(sizeof(central) + namelen);
	}

	// Write the directory terminator
	dirend.NumEntriesOnAllDisks = dirend.NumEntries = LittleShort((uint16_t)numfiles);
	dirend.DirectorySize = LittleLong(dirsize);
	WriteFile (file, &dirend, sizeof(dirend), &len, NULL);

	return file;
}

//==========================================================================
//
// AddZipFile
//
// Adds a file to the opened zip.
//
//==========================================================================

static void AddZipFile (HANDLE ziphandle, TarFile *whichfile, short dosdate, short dostime)
{
	zip::LocalFileHeader local = { ZIP_LOCALFILE, };
	Bytef outbuf[1024], inbuf[1024];
	z_stream stream;
	DWORD wrote, len, k;
	int err;
	bool gzip;

	whichfile->UncompressedSize = GetFileSize (whichfile->File, NULL);
	whichfile->CompressedSize = 0;
	whichfile->ZipOffset = 0;
	whichfile->Deflated = false;

	// Skip empty files.
	if (whichfile->UncompressedSize == 0)
	{
		return;
	}

	stream.next_in = Z_NULL;
	stream.avail_in = 0;
	stream.zalloc = Z_NULL;
	stream.zfree = Z_NULL;
	err = deflateInit2 (&stream, Z_BEST_COMPRESSION, Z_DEFLATED, -MAX_WBITS,
		8, Z_DEFAULT_STRATEGY);
	gzip = err == Z_OK;

	if (gzip)
	{
		local.Method = LittleShort((uint16_t)8);
		whichfile->Deflated = true;
		stream.next_out = outbuf;
		stream.avail_out = sizeof(outbuf);
	}

	// Write out the header and filename.
	local.VersionToExtract[0] = 20;
	local.Flags = gzip ? LittleShort((uint16_t)2) : 0;
	local.ModTime = dostime;
	local.ModDate = dosdate;
	local.UncompressedSize = LittleLong(whichfile->UncompressedSize);
	local.NameLength = LittleShort((uint16_t)strlen(whichfile->Filename));

	whichfile->ZipOffset = SetFilePointer (ziphandle, 0, NULL, FILE_CURRENT);
	WriteFile (ziphandle, &local, sizeof(local), &wrote, NULL);
	WriteFile (ziphandle, whichfile->Filename, (DWORD)strlen(whichfile->Filename), &wrote, NULL);

	// Write the file itself and calculate its CRC.
	SetFilePointer (whichfile->File, 0, NULL, FILE_BEGIN);
	for (k = 0; k < whichfile->UncompressedSize; )
	{
		len = whichfile->UncompressedSize - k;
		if (len > 1024)
		{
			len = 1024;
		}
		k += len;
		ReadFile (whichfile->File, &inbuf, len, &len, NULL);
		whichfile->CRC32 = crc32 (whichfile->CRC32, inbuf, len);
		whichfile->CompressedSize += WriteBlock (ziphandle, inbuf, len, gzip ? &stream : NULL, outbuf);
	}

	// Flush the zlib stream buffer.
	if (gzip)
	{
		for (bool done = false;;)
		{
			len = sizeof(outbuf) - stream.avail_out;
			if (len != 0)
			{
				whichfile->CompressedSize += len;
				WriteFile (ziphandle, outbuf, len, &wrote, NULL);
				stream.next_out = outbuf;
				stream.avail_out = sizeof(outbuf);
			}
			if (done)
			{
				break;
			}
			err = deflate (&stream, Z_FINISH);
			done = stream.avail_out != 0 || err == Z_STREAM_END;
			if (err != Z_STREAM_END && err != Z_OK)
			{
				break;
			}
		}
		deflateEnd (&stream);
	}

	// Fill in fields we didn't know when we wrote the local header.
	SetFilePointer (ziphandle, whichfile->ZipOffset + 14, NULL, FILE_BEGIN);
	k = LittleLong(whichfile->CRC32);
	WriteFile (ziphandle, &k, 4, &wrote, NULL);
	k = LittleLong(whichfile->CompressedSize);
	WriteFile (ziphandle, &k, 4, &wrote, NULL);

	SetFilePointer (ziphandle, 0, NULL, FILE_END);
}

//==========================================================================
//
// DrawTransparentBitmap
//
// The replacement for TransparentBlt that does not leak memory under
// Windows 98/ME. As seen in the MSKB.
//
//==========================================================================

void DrawTransparentBitmap(HDC hdc, HBITMAP hBitmap, short xStart,
						   short yStart, COLORREF cTransparentColor)
{
	BITMAP     bm;
	COLORREF   cColor;
	HBITMAP    bmAndBack, bmAndObject, bmAndMem, bmSave;
	HBITMAP    bmBackOld, bmObjectOld, bmMemOld, bmSaveOld;
	HDC        hdcMem, hdcBack, hdcObject, hdcTemp, hdcSave;
	POINT      ptSize;

	hdcTemp = CreateCompatibleDC(hdc);
	SelectObject(hdcTemp, hBitmap);   // Select the bitmap

	GetObject(hBitmap, sizeof(BITMAP), (LPSTR)&bm);
	ptSize.x = bm.bmWidth;            // Get width of bitmap
	ptSize.y = bm.bmHeight;           // Get height of bitmap
	DPtoLP(hdcTemp, &ptSize, 1);      // Convert from device

	// to logical points

	// Create some DCs to hold temporary data.
	hdcBack   = CreateCompatibleDC(hdc);
	hdcObject = CreateCompatibleDC(hdc);
	hdcMem    = CreateCompatibleDC(hdc);
	hdcSave   = CreateCompatibleDC(hdc);

	// Create a bitmap for each DC. DCs are required for a number of
	// GDI functions.

	// Monochrome DC
	bmAndBack   = CreateBitmap(ptSize.x, ptSize.y, 1, 1, NULL);

	// Monochrome DC
	bmAndObject = CreateBitmap(ptSize.x, ptSize.y, 1, 1, NULL);

	bmAndMem    = CreateCompatibleBitmap(hdc, ptSize.x, ptSize.y);
	bmSave      = CreateCompatibleBitmap(hdc, ptSize.x, ptSize.y);

	// Each DC must select a bitmap object to store pixel data.
	bmBackOld   = (HBITMAP)SelectObject(hdcBack, bmAndBack);
	bmObjectOld = (HBITMAP)SelectObject(hdcObject, bmAndObject);
	bmMemOld    = (HBITMAP)SelectObject(hdcMem, bmAndMem);
	bmSaveOld   = (HBITMAP)SelectObject(hdcSave, bmSave);

	// Set proper mapping mode.
	SetMapMode(hdcTemp, GetMapMode(hdc));

	// Save the bitmap sent here, because it will be overwritten.
	BitBlt(hdcSave, 0, 0, ptSize.x, ptSize.y, hdcTemp, 0, 0, SRCCOPY);

	// Set the background color of the source DC to the color.
	// contained in the parts of the bitmap that should be transparent
	cColor = SetBkColor(hdcTemp, cTransparentColor);

	// Create the object mask for the bitmap by performing a BitBlt
	// from the source bitmap to a monochrome bitmap.
	BitBlt(hdcObject, 0, 0, ptSize.x, ptSize.y, hdcTemp, 0, 0,
		SRCCOPY);

	// Set the background color of the source DC back to the original
	// color.
	SetBkColor(hdcTemp, cColor);

	// Create the inverse of the object mask.
	BitBlt(hdcBack, 0, 0, ptSize.x, ptSize.y, hdcObject, 0, 0,
		NOTSRCCOPY);

	// Copy the background of the main DC to the destination.
	BitBlt(hdcMem, 0, 0, ptSize.x, ptSize.y, hdc, xStart, yStart,
		SRCCOPY);

	// Mask out the places where the bitmap will be placed.
	BitBlt(hdcMem, 0, 0, ptSize.x, ptSize.y, hdcObject, 0, 0, SRCAND);

	// Mask out the transparent colored pixels on the bitmap.
	BitBlt(hdcTemp, 0, 0, ptSize.x, ptSize.y, hdcBack, 0, 0, SRCAND);

	// XOR the bitmap with the background on the destination DC.
	BitBlt(hdcMem, 0, 0, ptSize.x, ptSize.y, hdcTemp, 0, 0, SRCPAINT);

	// Copy the destination to the screen.
	BitBlt(hdc, xStart, yStart, ptSize.x, ptSize.y, hdcMem, 0, 0,
		SRCCOPY);

	// Place the original bitmap back into the bitmap sent here.
	BitBlt(hdcTemp, 0, 0, ptSize.x, ptSize.y, hdcSave, 0, 0, SRCCOPY);

	// Delete the memory bitmaps.
	DeleteObject(SelectObject(hdcBack, bmBackOld));
	DeleteObject(SelectObject(hdcObject, bmObjectOld));
	DeleteObject(SelectObject(hdcMem, bmMemOld));
	DeleteObject(SelectObject(hdcSave, bmSaveOld));

	// Delete the memory DCs.
	DeleteDC(hdcMem);
	DeleteDC(hdcBack);
	DeleteDC(hdcObject);
	DeleteDC(hdcSave);
	DeleteDC(hdcTemp);
}

//==========================================================================
//
// TransparentStaticProc
//
// A special window procedure for the static control displaying the dying
// Doom Guy. It draws the bitmap with magenta as transparent. I could have
// made it an owner-draw control instead, but then I wouldn't get any of
// the standard bitmap graphic handling (like automatic sizing).
//
//==========================================================================

static LRESULT CALLBACK TransparentStaticProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	RECT rect;
	PAINTSTRUCT paint;
	HDC dc;

	if (uMsg == WM_PAINT)
	{
		if (GetUpdateRect (hWnd, &rect, FALSE))
		{
			dc = BeginPaint (hWnd, &paint);
			if (dc != NULL)
			{
				HBITMAP bitmap = (HBITMAP)SendMessage (hWnd, STM_GETIMAGE, IMAGE_BITMAP, 0);
				if (bitmap != NULL)
				{
					DrawTransparentBitmap (dc, bitmap, 0, 0, RGB(255,0,255));
				}
				EndPaint (hWnd, &paint);
			}
		}
		return 0;
	}
	return CallWindowProc (StdStaticProc, hWnd, uMsg, wParam, lParam);
}

static HMODULE WinHlp32;

//==========================================================================
//
// OverviewDlgProc
//
// The DialogProc for the crash overview page.
//
//==========================================================================

static INT_PTR CALLBACK OverviewDlgProc (HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	CHARFORMAT charFormat;
	HWND edit;
	ENLINK *link;

	switch (message)
	{
	case WM_INITDIALOG:
	{
		if (pEnableThemeDialogTexture != NULL)
		{
			pEnableThemeDialogTexture(hDlg, ETDT_ENABLETAB);
		}

		// Setup the header at the top of the page.
		edit = GetDlgItem(hDlg, IDC_CRASHHEADER);
		SetWindowTextW(edit, WGAMENAME" has encountered a problem and needs to close.\n"
			"We are sorry for the inconvenience.");

		// Setup a bold version of the standard dialog font and make the header bold.
		charFormat.cbSize = sizeof(charFormat);
		SendMessageW(edit, EM_GETCHARFORMAT, SCF_DEFAULT, (LPARAM)&charFormat);
		charFormat.dwEffects = CFE_BOLD;
		SendMessageW(edit, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&charFormat);

		// Setup the drawing routine for the dying guy's bitmap.
		edit = GetDlgItem(hDlg, IDC_DEADGUYVIEWER);
		StdStaticProc = (WNDPROC)(LONG_PTR)SetWindowLongPtr(edit, GWLP_WNDPROC, (WLONG_PTR)(LONG_PTR)TransparentStaticProc);

		// Fill in all the text just below the heading.
		edit = GetDlgItem(hDlg, IDC_PLEASETELLUS);
		SendMessageW(edit, EM_AUTOURLDETECT, TRUE, 0);
		SetWindowTextW(edit, L"Please tell us about this problem.\n"
			"The information will NOT be sent to Microsoft.\n\n"
			"An error report has been created that you can submit to help improve " GAMENAME ". "
			/*"You can either save it to disk and make a report in the bugs forum at " FORUM_URL ", "
			"or you can send it directly without letting other people know about it."*/);
		SendMessageW(edit, EM_SETSEL, 0, 81);
		SendMessageW(edit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&charFormat);
		SendMessageW(edit, EM_SETEVENTMASK, 0, ENM_LINK);

		// Assign a default invalid file handle to the user's edit control.
		edit = GetDlgItem(hDlg, IDC_CRASHINFO);
		SetWindowLongPtrW(edit, GWLP_USERDATA, (LONG_PTR)INVALID_HANDLE_VALUE);

		// Fill in the summary text at the bottom of the page.
		edit = GetDlgItem(hDlg, IDC_CRASHSUMMARY);
		auto wsum = WideString(CrashSummary);
		SetWindowTextW(edit, wsum.c_str());
		return TRUE;
	}

	case WM_NOTIFY:
		// When the link in the "please tell us" edit control is clicked, open
		// the bugs forum in a browser window. ShellExecute is used to do this,
		// so the default browser, whatever it may be, will open it.
		link = (ENLINK *)lParam;
		if (link->nmhdr.idFrom == IDC_PLEASETELLUS && link->nmhdr.code == EN_LINK)
		{
			if (link->msg == WM_LBUTTONDOWN)
			{
				//ShellExecuteA (NULL, "open", BUGS_FORUM_URL, NULL, NULL, 0);
				SetWindowLongPtrW (hDlg, DWLP_MSGRESULT, 1);
				return TRUE;
			}
		}
		return FALSE;
		break;
	}
	return FALSE;
}

//==========================================================================
//
// CrashDlgProc
//
// The DialogProc for the main crash dialog.
//
//==========================================================================

static INT_PTR CALLBACK CrashDlgProc (HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	static WCHAR overview[] = L"Overview";
	static WCHAR details[] = L"Details";
	HWND edit;
	TCITEM tcitem;
	RECT tabrect, tcrect;
	LPNMHDR nmhdr;

	switch (message)
	{
	case WM_INITDIALOG:
		// Set up the tab control
		tcitem.mask = TCIF_TEXT | TCIF_PARAM;
		edit = GetDlgItem (hDlg, IDC_CRASHTAB);

		GetWindowRect (edit, &tcrect);
		ScreenToClient (hDlg, (LPPOINT)&tcrect.left);
		ScreenToClient (hDlg, (LPPOINT)&tcrect.right);

		// There are two tabs: Overview and Details. Each pane is created from a
		// dialog template, and the resultant window is stored as the lParam for
		// the corresponding tab.
		tcitem.pszText = overview;
		tcitem.lParam = (LPARAM)CreateDialogParamW (g_hInst, MAKEINTRESOURCE(IDD_CRASHOVERVIEW), hDlg, OverviewDlgProc, (LPARAM)edit);
		TabCtrl_InsertItem (edit, 0, &tcitem);
		TabCtrl_GetItemRect (edit, 0, &tabrect);
		SetWindowPos ((HWND)tcitem.lParam, HWND_TOP, tcrect.left + 3, tcrect.top + tabrect.bottom + 3,
			tcrect.right - tcrect.left - 8, tcrect.bottom - tcrect.top - tabrect.bottom - 8, 0);

		tcitem.pszText = details;
		tcitem.lParam = (LPARAM)CreateDialogParamW (g_hInst, MAKEINTRESOURCE(IDD_CRASHDETAILS), hDlg, DetailsDlgProc, (LPARAM)edit);
		TabCtrl_InsertItem (edit, 1, &tcitem);
		SetWindowPos ((HWND)tcitem.lParam, HWND_TOP, tcrect.left + 3, tcrect.top + tabrect.bottom + 3,
			tcrect.right - tcrect.left - 8, tcrect.bottom - tcrect.top - tabrect.bottom - 8, 0);
		break;

	case WM_NOTIFY:
		nmhdr = (LPNMHDR)lParam;
		if (nmhdr->idFrom == IDC_CRASHTAB)
		{
			int i = TabCtrl_GetCurSel (nmhdr->hwndFrom);
			tcitem.mask = TCIF_PARAM;
			TabCtrl_GetItem (nmhdr->hwndFrom, i, &tcitem);
			edit = (HWND)tcitem.lParam;

			// TCN_SELCHANGING is sent just before the selected tab changes.
			// TCN_SELCHANGE is sent just after the selected tab changes.
			// So, when TCN_SELCHANGING is received hide the pane for the
			// selected tab, and when TCN_SELCHANGE is received, show the pane
			// for the selected tab.
			if (nmhdr->code == TCN_SELCHANGING)
			{
				ShowWindow (edit, SW_HIDE);
				SetWindowLongPtr (hDlg, DWLP_MSGRESULT, FALSE);
				return TRUE;
			}
			else if (nmhdr->code == TCN_SELCHANGE)
			{
				ShowWindow (edit, SW_SHOW);
				return TRUE;
			}
			else
			{
				return FALSE;
			}
		}
		break;

	case WM_COMMAND:
		if (HIWORD(wParam) == BN_CLICKED)
		{
			EndDialog (hDlg, LOWORD(wParam));
		}
		break;
	}
	return FALSE;
}

//==========================================================================
//
// DetailsDlgProc
//
// The dialog procedure for when the user wants to examine the files
// in the report.
//
//==========================================================================

static INT_PTR CALLBACK DetailsDlgProc (HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	HWND ctrl;
	int i, j;

	switch (message)
	{
	case WM_INITDIALOG:
		if (pEnableThemeDialogTexture != NULL)
		{
			pEnableThemeDialogTexture (hDlg, ETDT_ENABLETAB);
		}

		// Set up the file contents display: No undos. The control's
		// userdata stores the index of the file currently displayed.
		ctrl = GetDlgItem (hDlg, IDC_CRASHFILECONTENTS);
		SendMessageW (ctrl, EM_SETUNDOLIMIT, 0, 0);
		SetWindowLongPtrW (ctrl, GWLP_USERDATA, -1);
		SetEditControl (ctrl, GetDlgItem(hDlg, IDC_CRASHFILESIZE), 0);
		SendMessageW (ctrl, LB_SETCURSEL, 0, 0);
		break;

	case WM_SHOWWINDOW:
		// When showing this pane, refresh the list of packaged files, and
		// update the contents display as necessary.
		if (wParam == TRUE)
		{
			ctrl = GetDlgItem (hDlg, IDC_CRASHFILES);
			j = (int)SendMessageW (ctrl, LB_GETCURSEL, 0, 0);
			SendMessageW (ctrl, LB_RESETCONTENT, 0, 0);
			for (i = 0; i < NumFiles; ++i)
			{
				SendMessageA (ctrl, LB_ADDSTRING, 0, (LPARAM)TarFiles[i].Filename);
			}
			if (j == LB_ERR || j >= i) j = 0;
			SendMessageW (ctrl, LB_SETCURSEL, j, 0);

			ctrl = GetDlgItem (hDlg, IDC_CRASHFILECONTENTS);
			if (j > 2) SetWindowLongPtr (ctrl, GWLP_USERDATA, -1);
			SetEditControl (ctrl, GetDlgItem(hDlg, IDC_CRASHFILESIZE), j);
		}
		break;

	case WM_COMMAND:
		// Selecting a different file makes the contents display update.
		if (HIWORD(wParam) == LBN_SELCHANGE)
		{
			i = (int)SendMessage ((HWND)lParam, LB_GETCURSEL, 0, 0);
			if (i != LB_ERR)
			{
				// Update the contents control for this file
				SetEditControl (GetDlgItem (hDlg, IDC_CRASHFILECONTENTS),
					GetDlgItem (hDlg, IDC_CRASHFILESIZE), i);
			}
		}
		break;
	}
	return FALSE;
}

//==========================================================================
//
// StreamEditText
//
// The callback function to stream a text file into a rich edit control.
//
//==========================================================================

static DWORD CALLBACK StreamEditText (DWORD_PTR cookie, LPBYTE buffer, LONG cb, LONG *pcb)
{
	DWORD wrote;
	ReadFile ((HANDLE)cookie, buffer, cb, &wrote, NULL);
	*pcb = wrote;
	return wrote == 0;
}

//==========================================================================
//
// StreamEditBinary
//
// The callback function to stream a binary file into a rich edit control.
// The binary file is converted to a RTF hex dump so that the different
// columns can be color-coded.
//
//==========================================================================

struct BinStreamInfo
{
	int Stage;
	HANDLE File;
	DWORD Pointer;
};

static DWORD CALLBACK StreamEditBinary (DWORD_PTR cookie, LPBYTE buffer, LONG cb, LONG *pcb)
{
	BinStreamInfo *info = (BinStreamInfo *)cookie;
	uint8_t buf16[16];
	DWORD read, i;
	char *buff_p = (char *)buffer;
	char *buff_end = (char *)buffer + cb;

repeat:
	switch (info->Stage)
	{
	case 0:		// Write prologue
		buff_p += mysnprintf (buff_p, buff_end - buff_p, "{\\rtf1\\ansi\\deff0"
			"{\\colortbl ;\\red0\\green0\\blue80;\\red0\\green0\\blue0;\\red80\\green0\\blue80;}"
			"\\viewkind4\\pard");
		info->Stage++;
		break;

	case 1:		// Write body
		while (cb - ((LPBYTE)buff_p - buffer) > 150)
		{
			ReadFile (info->File, buf16, 16, &read, NULL);
			if (read == 0 || info->Pointer >= 65536)
			{
				info->Stage = read == 0 ? 2 : 3;
				goto repeat;
			}
			char *linestart = buff_p;
			buff_p += mysnprintf (buff_p, buff_end - buff_p, "\\cf1 %08lx:\\cf2 ", info->Pointer);
			info->Pointer += read;

			for (i = 0; i < read;)
			{
				if (i <= read - 4)
				{
					DWORD d;
					memcpy(&d, &buf16[i], sizeof(d));
					buff_p += mysnprintf (buff_p, buff_end - buff_p, " %08lx", d);
					i += 4;
				}
				else
				{
					buff_p += mysnprintf (buff_p, buff_end - buff_p, " %02x", buf16[i]);
					i += 1;
				}
			}
			while (buff_p - linestart < 57)
			{
				*buff_p++ = ' ';
			}
			buff_p += mysnprintf (buff_p, buff_end - buff_p, "\\cf3 ");
			for (i = 0; i < read; ++i)
			{
				uint8_t code = buf16[i];
				if (code < 0x20 || code > 0x7f) code = 0xB7;
				if (code == '\\' || code == '{' || code == '}') *buff_p++ = '\\';
				*buff_p++ = code;
			}
			buff_p += mysnprintf (buff_p, buff_end - buff_p, "\\par\r\n");
		}
		break;

	case 2:		// Write epilogue
		buff_p += mysnprintf (buff_p, buff_end - buff_p, "\\cf0 }");
		info->Stage = 4;
		break;

	case 3:		// Write epilogue for truncated file
		buff_p += mysnprintf (buff_p, buff_end - buff_p, "--- Rest of file truncated ---\\cf0 }");
		info->Stage = 4;
		break;

	case 4:		// We're done
		return TRUE;
	}

	*pcb = (LONG)((LPBYTE)buff_p - buffer);
	return FALSE;
}

//==========================================================================
//
// SetEditControl
//
// Fills the edit control with the contents of the chosen file.
// If the filename ends with ".txt" the file is assumed to be text.
// Otherwise it is treated as binary, and you get a hex dump.
//
//==========================================================================

static void SetEditControl (HWND edit, HWND sizedisplay, int filenum)
{
	char sizebuf[32];
	EDITSTREAM stream;
	DWORD size;
	POINT pt = { 0, 0 };
	const char *rtf = NULL;
	HGDIOBJ font;

	// Don't refresh the control if it's already showing the file we want.
	if (GetWindowLongPtr (edit, GWLP_USERDATA) == filenum)
	{
		return;
	}

	size = GetFileSize (TarFiles[filenum].File, NULL);
	if (size < 1024)
	{
		mysnprintf (sizebuf, countof(sizebuf), "(%lu bytes)", size);
	}
	else
	{
		mysnprintf (sizebuf, countof(sizebuf), "(%lu KB)", size/1024);
	}
	SetWindowTextA (sizedisplay, sizebuf);

	SetWindowLongPtr (edit, GWLP_USERDATA, filenum);

	SetFilePointer (TarFiles[filenum].File, 0, NULL, FILE_BEGIN);
	SendMessage (edit, EM_SETSCROLLPOS, 0, (LPARAM)&pt);

	// Set the font now, in case log.rtf was previously viewed, because
	// that file changes it.
	font = GetStockObject (ANSI_FIXED_FONT);
	if (font != INVALID_HANDLE_VALUE)
	{
		SendMessage (edit, WM_SETFONT, (WPARAM)font, FALSE);
	}

	// Text files are streamed in as-is.
	// Binary files are streamed in as color-coded hex dumps.
	stream.dwError = 0;
	if (strstr (TarFiles[filenum].Filename, ".txt") != NULL ||
		(rtf = strstr (TarFiles[filenum].Filename, ".rtf")) != NULL)
	{
		CHARFORMAT beBlack;

		beBlack.cbSize = sizeof(beBlack);
		beBlack.dwMask = CFM_COLOR;
		beBlack.dwEffects = 0;
		beBlack.crTextColor = RGB(0,0,0);
		SendMessage (edit, EM_SETCHARFORMAT, 0, (LPARAM)&beBlack);
		stream.dwCookie = (DWORD_PTR)TarFiles[filenum].File;
		stream.pfnCallback = StreamEditText;
		SendMessage (edit, EM_STREAMIN, rtf ? SF_RTF : SF_TEXT | SF_USECODEPAGE | (1252 << 16), (LPARAM)&stream);
	}
	else
	{
		BinStreamInfo info = { 0, TarFiles[filenum].File, 0 };
		stream.dwCookie = (DWORD_PTR)&info;
		stream.pfnCallback = StreamEditBinary;
		SendMessage (edit, EM_EXLIMITTEXT, 0, GetFileSize(TarFiles[filenum].File, NULL)*7);
		SendMessage (edit, EM_STREAMIN, SF_RTF, (LPARAM)&stream);
	}
	SendMessage (edit, EM_SETSEL, (WPARAM)-1, 0);
}

//==========================================================================
//
// SaveReport
//
// Makes a permanent copy of the report tarball.
//
//==========================================================================

static void SaveReport (HANDLE file)
{
	OPENFILENAME ofn = {
#ifdef OPENFILENAME_SIZE_VERSION_400
		OPENFILENAME_SIZE_VERSION_400
#else
		sizeof(ofn)
#endif
		, };
	WCHAR filename[256];

	ofn.lpstrFilter = L"Zip file (*.zip)\0*.zip\0";
	wcscpy (filename, L"CrashReport.zip");
	ofn.lpstrFile = filename;
	ofn.nMaxFile = countof(filename);

	while (GetSaveFileNameW (&ofn))
	{
		HANDLE ofile = CreateFileW (ofn.lpstrFile, GENERIC_WRITE, 0, NULL,
			CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN,
			NULL);
		if (ofile == INVALID_HANDLE_VALUE)
		{
			if (MessageBoxA (NULL, "Could not open the crash report file",
				"Save As failed", MB_RETRYCANCEL) == IDRETRY)
			{
				continue;
			}
			return;
		}
		else
		{
			DWORD fileLen = GetFileSize (file, NULL), fileLeft;
			char xferbuf[1024];

			SetFilePointer (file, 0, NULL, FILE_BEGIN);
			fileLeft = fileLen;
			while (fileLeft != 0)
			{
				DWORD grab = fileLeft > sizeof(xferbuf) ? sizeof(xferbuf) : fileLeft;
				DWORD didread;

				ReadFile (file, xferbuf, grab, &didread, NULL);
				WriteFile (ofile, xferbuf, didread, &grab, NULL);
				fileLeft -= didread;
			}
			CloseHandle (ofile);
			return;
		}
	}
}

//==========================================================================
//
// DisplayCrashLog
//
// Displays the crash information and possibly submits it. CreateCrashLog()
// must have been called previously.
//
//==========================================================================

void DisplayCrashLog ()
{
	HANDLE file;

	if (NumFiles == 0)
	{
		char ohPoo[] =
			GAMENAME" crashed but was unable to produce\n"
			"detailed information about the crash.\n"
			"\nThis is all that is available:\n\nCode=XXXXXXXX\nAddr=XXXXXXXX";
		mysnprintf (ohPoo + countof(ohPoo) - 23, 23, "%08lX\nAddr=%p", CrashCode, CrashAddress);
		MessageBoxA (NULL, ohPoo, GAMENAME" Very Fatal Error", MB_OK|MB_ICONSTOP);
		if (WinHlp32 != NULL)
		{
			FreeLibrary (WinHlp32);
		}
	}
	else
	{
		HMODULE uxtheme = LoadLibraryA ("uxtheme.dll");
		if (uxtheme != NULL)
		{
			pEnableThemeDialogTexture = (HRESULT (__stdcall *)(HWND,DWORD))GetProcAddress (uxtheme, "EnableThemeDialogTexture");
		}
		INT_PTR result = DialogBox (g_hInst, MAKEINTRESOURCE(IDD_CRASHDIALOG), NULL, (DLGPROC)CrashDlgProc);

		if (result == IDC_SAVEREPORT)
		{
			file = MakeZip ();
			SaveReport (file);
			CloseHandle (file);
		}
		if (uxtheme != NULL)
		{
			FreeLibrary (uxtheme);
		}
	}
	CloseTarFiles ();
}
