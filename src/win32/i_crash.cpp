/*
** i_crash.cpp
** Gathers exception information when the program crashes.
**
**---------------------------------------------------------------------------
** Copyright 2002 Randy Heit
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
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winnt.h>
#include <tlhelp32.h>
#include "resource.h"

#define MAX_CRASH_REPORT (256*1024)

typedef BOOL (WINAPI *THREADWALK) (HANDLE, LPTHREADENTRY32);
typedef BOOL (WINAPI *MODULEWALK) (HANDLE, LPMODULEENTRY32);
typedef HANDLE (WINAPI *CREATESNAPSHOT) (DWORD, DWORD);

extern HINSTANCE g_hInst;
extern HWND Window;

EXCEPTION_POINTERS CrashPointers;
DWORD *TopOfStack;

	   char *CrashText;
static char *CrashMax;
static char *CrashPtr;
static void *DumpAddress;

static DWORD CrashCode;
static PVOID CrashAddress;

static void DumpBytes ();
static void AddStackInfo ();
static void StackWalk ();
static void AddToolHelp ();

void GetTopOfStack (void *top)
{
	MEMORY_BASIC_INFORMATION memInfo;

	if (VirtualQuery (top, &memInfo, sizeof(memInfo)))
	{
		TopOfStack = (DWORD *)((BYTE *)memInfo.BaseAddress + memInfo.RegionSize);
	}
	else
	{
		TopOfStack = NULL;
	}
}

void CreateCrashLog (char *(*userCrashInfo)(char *text, char *maxtext))
{
	static const struct
	{
		DWORD Code; const char *Text;
	}
	exceptions[] =
	{
		{ EXCEPTION_ACCESS_VIOLATION, "ACCESS_VIOLATION" },
		{ EXCEPTION_ARRAY_BOUNDS_EXCEEDED, "ARRAY_BOUNDS_EXCEEDED" },
		{ EXCEPTION_BREAKPOINT, "BREAKPOINT" },
		{ EXCEPTION_DATATYPE_MISALIGNMENT, "DATATYPE_MISALIGNMENT" },
		{ EXCEPTION_FLT_DENORMAL_OPERAND, "FLT_DENORMAL_OPERAND." },
		{ EXCEPTION_FLT_DIVIDE_BY_ZERO, "FLT_DIVIDE_BY_ZERO" },
		{ EXCEPTION_FLT_INEXACT_RESULT, "FLT_INEXACT_RESULT" },
		{ EXCEPTION_FLT_INVALID_OPERATION, "FLT_INVALID_OPERATION" },
		{ EXCEPTION_FLT_OVERFLOW, "FLT_OVERFLOW" },
		{ EXCEPTION_FLT_STACK_CHECK, "FLT_STACK_CHECK" },
		{ EXCEPTION_FLT_UNDERFLOW, "FLT_UNDERFLOW" },
		{ EXCEPTION_ILLEGAL_INSTRUCTION, "ILLEGAL_INSTRUCTION" },
		{ EXCEPTION_IN_PAGE_ERROR, "IN_PAGE_ERROR" },
		{ EXCEPTION_INT_DIVIDE_BY_ZERO, "INT_DIVIDE_BY_ZERO" },
		{ EXCEPTION_INT_OVERFLOW, "INT_OVERFLOW" },
		{ EXCEPTION_INVALID_DISPOSITION, "INVALID_DISPOSITION" },
		{ EXCEPTION_NONCONTINUABLE_EXCEPTION, "NONCONTINUABLE_EXCEPTION" },
		{ EXCEPTION_PRIV_INSTRUCTION, "PRIV_INSTRUCTION" },
		{ EXCEPTION_SINGLE_STEP, "SINGLE_STEP" },
		{ EXCEPTION_STACK_OVERFLOW, "STACK_OVERFLOW" }
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


	int i;

	// Do not collect information more than once.
	if (CrashText != NULL)
	{
		return;
	}

	CrashText = (char *)HeapAlloc (GetProcessHeap(), 0, MAX_CRASH_REPORT);

	CrashCode = CrashPointers.ExceptionRecord->ExceptionCode;
	CrashAddress = CrashPointers.ExceptionRecord->ExceptionAddress;

	if (CrashText == NULL)
		return;
	CrashMax = CrashText + MAX_CRASH_REPORT;

	for (i = 0; (size_t)i < sizeof(exceptions)/sizeof(exceptions[0]); ++i)
	{
		if (exceptions[i].Code == CrashPointers.ExceptionRecord->ExceptionCode)
		{
			break;
		}
	}
	if ((size_t)i < sizeof(exceptions)/sizeof(exceptions[0]))
	{
		CrashPtr = CrashText + wsprintf (CrashText, "Code: %s\r\n", exceptions[i].Text);
	}
	else
	{
		CrashPtr = CrashText + wsprintf (CrashText, "Code: %08x\r\n", CrashPointers.ExceptionRecord->ExceptionCode);
	}
	if (CrashPointers.ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION)
	{
		CrashPtr += wsprintf (CrashPtr, "Tried to %s address %08x\r\n",
			CrashPointers.ExceptionRecord->ExceptionInformation[0] ? "write" : "read",
			CrashPointers.ExceptionRecord->ExceptionInformation[1]);
	}
	CrashPtr += wsprintf (CrashPtr, "Flags: %08x\r\nAddress: %08x\r\n\r\n", CrashPointers.ExceptionRecord->ExceptionFlags,
		CrashPointers.ExceptionRecord->ExceptionAddress);

	OSVERSIONINFO info = { sizeof(info) };
	GetVersionEx (&info);

	CrashPtr += wsprintf (CrashPtr, "Windows %s %d.%d Build %d %s\r\n\r\n",
		info.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS ? "9x" : "NT",
		info.dwMajorVersion, info.dwMinorVersion, info.dwBuildNumber, info.szCSDVersion);

	CONTEXT *ctxt = CrashPointers.ContextRecord;

	if (ctxt->ContextFlags & CONTEXT_SEGMENTS)
	{
		CrashPtr += wsprintf (CrashPtr, "GS=%04x  FS=%04x  ES=%04x  DS=%04x\r\n",
			ctxt->SegGs, ctxt->SegFs, ctxt->SegEs, ctxt->SegDs);
	}

	if (ctxt->ContextFlags & CONTEXT_INTEGER)
	{
		CrashPtr += wsprintf (CrashPtr, "EAX=%08x  EBX=%08x  ECX=%08x  EDX=%08x\r\nESI=%08x  EDI=%08x\r\n",
			ctxt->Eax, ctxt->Ebx, ctxt->Ecx, ctxt->Edx, ctxt->Esi, ctxt->Edi);
	}

	if (ctxt->ContextFlags & CONTEXT_CONTROL)
	{
		CrashPtr += wsprintf (CrashPtr, "EBP=%08x  EIP=%08x  ESP=%08x  CS=%04x  SS=%04x\r\nEFlags=%08x\r\n",
			ctxt->Ebp, ctxt->Eip, ctxt->Esp, ctxt->SegCs, ctxt->SegSs, ctxt->EFlags);

		DWORD j;

		for (i = 0, j = 1; (size_t)i < sizeof(eflagsBits)/sizeof(eflagsBits[0]); j <<= 1, ++i)
		{
			if (eflagsBits[i][0] != 'x')
			{
				CrashPtr += wsprintf (CrashPtr, " %c%c%c", eflagsBits[i][0], eflagsBits[i][1],
					ctxt->EFlags & j ? '+' : '-');
			}
		}
		*CrashPtr++ = '\r';
		*CrashPtr++ = '\n';
		*CrashPtr = 0;
	}

	if (ctxt->ContextFlags & CONTEXT_FLOATING_POINT)
	{
		CrashPtr += wsprintf (CrashPtr,
			"\r\nFPU State:\r\n ControlWord=%04x StatusWord=%04x TagWord=%04x\r\n"
			" ErrorOffset=%08x\r\n ErrorSelector=%08x\r\n DataOffset=%08x\r\n DataSelector=%08x\r\n"
			" Cr0NpxState=%08x\r\n\r\n",
			(WORD)ctxt->FloatSave.ControlWord, (WORD)ctxt->FloatSave.StatusWord, (WORD)ctxt->FloatSave.TagWord,
			ctxt->FloatSave.ErrorOffset, ctxt->FloatSave.ErrorSelector, ctxt->FloatSave.DataOffset,
			ctxt->FloatSave.DataSelector, ctxt->FloatSave.Cr0NpxState);

		for (i = 0; i < 8; ++i)
		{
			CrashPtr += wsprintf (CrashPtr, "MM%d=%08x%08x\r\n", i, *(DWORD *)(&ctxt->FloatSave.RegisterArea[20*i+4]),
				*(DWORD *)(&ctxt->FloatSave.RegisterArea[20*i]));
		}
	}

	AddToolHelp ();

	CrashPtr += wsprintf (CrashPtr, "\r\nBytes near EIP:");
	DumpAddress = (BYTE *)CrashPointers.ExceptionRecord->ExceptionAddress-16;
	DumpBytes ();

	CrashPtr = userCrashInfo (CrashPtr, CrashMax);

	if (ctxt->ContextFlags & CONTEXT_CONTROL)
	{
		DumpAddress = (void *)CrashPointers.ContextRecord->Esp;
		AddStackInfo ();
	}
}

static void AddToolHelp ()
{
	if (CrashPtr + 80 >= CrashMax)
		return;

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
	pModule32First = (MODULEWALK)GetProcAddress (kernel, "Module32First");
	pModule32Next = (MODULEWALK)GetProcAddress (kernel, "Module32Next");

	if (!(pCreateToolhelp32Snapshot && pThread32First && pThread32Next &&
		  pModule32First && pModule32Next))
	{
		CrashPtr += wsprintf (CrashPtr, "\r\nTool Help unavailable\r\n");
		FreeLibrary (kernel);
		return;
	}

	HANDLE snapshot = pCreateToolhelp32Snapshot (TH32CS_SNAPMODULE|TH32CS_SNAPTHREAD, 0);
	if (snapshot == INVALID_HANDLE_VALUE)
	{
		CrashPtr += wsprintf (CrashPtr, "\r\nTool Help snapshot unavailable\r\n");
		FreeLibrary (kernel);
		return;
	}

	THREADENTRY32 thread = { sizeof(thread) };

	if (CrashPtr + 80 < CrashMax)
	{
		CrashPtr += wsprintf (CrashPtr, "\r\nRunning threads:\r\n");

		if (pThread32First (snapshot, &thread))
		{
			do
			{
				if (CrashPtr + 24 >= CrashMax)
					break;
				if (thread.th32OwnerProcessID == GetCurrentProcessId())
				{
					CrashPtr += wsprintf (CrashPtr, "%08x", thread.th32ThreadID);

					if (thread.th32ThreadID == GetCurrentThreadId())
					{
						CrashPtr += wsprintf (CrashPtr, " at %08x*", CrashAddress);
					}
#if 0
// OpenThread was first introduced in ME/2000, and I don't think this is worthwhile
// enough to bother looking it up.
					else
					{
						// Determine EIP for the thread
						HANDLE thandle = OpenThread (THREAD_GET_CONTEXT|THREAD_QUERY_INFORMATION|THREAD_SUSPEND_RESUME, FALSE, thread.th32ThreadID);
						if (thandle != INVALID_HANDLE_VALUE)
						{
							if (SuspendThread (thandle) != -1)
							{
								CONTEXT con = { CONTEXT_CONTROL };
								if (GetThreadContext (thandle, &con))
								{
									CrashPtr += wsprintf (CrashPtr, " at %08x", con.Eip);
								}
								ResumeThread (thandle);
							}
							CloseHandle (thandle);
						}
					}
#endif
					*CrashPtr++ = '\r';
					*CrashPtr++ = '\n';
					*CrashPtr = 0;
				}
			} while (pThread32Next (snapshot, &thread));
		}
	}

	MODULEENTRY32 module = { sizeof(module) };

	if (CrashPtr + 80 < CrashMax)
	{
		CrashPtr += wsprintf (CrashPtr, "\r\nLoaded modules:\r\n");

		if (pModule32First (snapshot, &module))
		{
			do
			{
				if (strlen (module.szModule) + CrashPtr + 24 >= CrashMax)
					break;
				CrashPtr += wsprintf (CrashPtr, "%08x - %08x %c%s\r\n",
					module.modBaseAddr, module.modBaseAddr + module.modBaseSize - 1,
					module.modBaseAddr <= CrashPointers.ExceptionRecord->ExceptionAddress &&
					module.modBaseAddr + module.modBaseSize > CrashPointers.ExceptionRecord->ExceptionAddress
					? '*' : ' ',
					module.szModule);
			} while (pModule32Next (snapshot, &module));
		}
	}

	CloseHandle (snapshot);
	FreeLibrary (kernel);
}

static void AddStackInfo ()
{
	DWORD *addr = (DWORD *)DumpAddress;

	GetTopOfStack (DumpAddress);

	if (CrashPtr + 40 >= CrashMax)
		return;

	StackWalk ();

	if (CrashPtr + 256 >= CrashMax)
		return;

	CrashPtr += wsprintf (CrashPtr, "\r\nStack Contents:\r\n");
	DWORD *scan;
	for (scan = addr; scan < TopOfStack && CrashPtr < CrashMax-112; scan += 4)
	{
		int i;
		int max;

		if (TopOfStack - scan < 4)
		{
			max = TopOfStack - scan;
		}
		else
		{
			max = 4;
		}

		CrashPtr += wsprintf (CrashPtr, "%08x:", (DWORD)scan);
		__try
		{
			for (i = 0; i < max; ++i)
			{
				CrashPtr += wsprintf (CrashPtr, " %08x", scan[i]);
			}
		}
		__except(EXCEPTION_EXECUTE_HANDLER)
		{
		}
		for (; i < 4; ++i)
		{
			CrashPtr += wsprintf (CrashPtr, "         ");
		}
		*CrashPtr++ = ' ';
		*CrashPtr++ = ' ';
		__try
		{
			for (i = 0; i < max*4; ++i)
			{
				BYTE code = ((BYTE *)scan)[i];
				if (code >= 0x20 && code <= 0x7f)
				{
					*CrashPtr++ = code;
				}
				else
				{
					*CrashPtr++ = '\xb7';
				}
			}
		}
		__except(EXCEPTION_EXECUTE_HANDLER)
		{
		}
		*CrashPtr++ = '\r';
		*CrashPtr++ = '\n';
		*CrashPtr = 0;
	}
	if (scan < TopOfStack)
	{
		CrashPtr += wsprintf (CrashPtr, "** Output Truncated **\r\n");
	}
}

static void StackWalk ()
{
	DWORD *addr = (DWORD *)DumpAddress;
	DWORD_PTR *seh_stack;

#ifdef __GNUC__
    asm volatile ("movl %%fs:($0),%%eax;" : "=a" (seh_stack));
#else
	__asm
	{
		mov eax, fs:[0]
		mov seh_stack, eax
	}
#endif

	BYTE *pBaseOfImage = (BYTE *)GetModuleHandle (0);
	IMAGE_OPTIONAL_HEADER *pHeader = (IMAGE_OPTIONAL_HEADER *)(pBaseOfImage +
		((IMAGE_DOS_HEADER*)pBaseOfImage)->e_lfanew +
		sizeof(IMAGE_NT_SIGNATURE) + sizeof(IMAGE_FILE_HEADER));

	DWORD_PTR codeStart = (DWORD_PTR)pBaseOfImage + pHeader->BaseOfCode;
	DWORD_PTR codeEnd = codeStart + pHeader->SizeOfCode;

	CrashPtr += wsprintf (CrashPtr, "\r\nPossible call trace:\r\n %08x  BOOM", CrashAddress);
	__try
	{
		for (DWORD_PTR *scan = addr; scan < TopOfStack; ++scan)
		{
			if (*scan >= codeStart && *scan < codeEnd)
			{
				static const char regNames[][4] =
				{
					"eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi"
				};

				// If inside an SEH record, don't report this address
				DWORD_PTR *seh = seh_stack;
				while (seh != (DWORD_PTR *)0xffffffff)
				{
					if (scan >= seh && scan < seh+2)
						break;
					seh = (DWORD_PTR *)*seh;
				}
				if (seh == (DWORD_PTR *)0xffffffff)
				{
					CrashPtr += wsprintf (CrashPtr, "\r\n %08x", *scan);
					if (CrashPtr + 16 >= CrashMax)
						goto done;
					// Check if address is after a call statement. Print what was called if it is.
					__try
					{
						const BYTE *bytep = (BYTE *)(*scan);
						const DWORD *dwordp = (DWORD *)(*scan);

						if (*(bytep - 5) == 0xe8)
						{
							CrashPtr += wsprintf (CrashPtr, "  call %08x", *(dwordp - 1) + *scan);
						}
						else if ((*(bytep - 2) == 0xff) && ((*(bytep - 1) & 0xf7) == 0xd0))
						{
							CrashPtr += wsprintf (CrashPtr, "  call %s", regNames[*(bytep - 1) & 7]);
						}
						else
						{
							int i;

							for (i = 2; i < 7; ++i)
							{
								if (*(bytep - i) == 0xff && ((*(bytep - i + 1) & 070) == 020))
								{
									break;
								}
							}
							if (i < 7)
							{
								int mod, rm, basereg = -1, indexreg = -1;
								int scale = 1, offset = 0;

								bytep -= i - 1;
								mod = *bytep >> 6;
								rm = *bytep & 7;
								bytep++;

								if (mod == 0 && rm == 5)
								{
									mod = 2;
								}
								else if (rm == 4)
								{
									int index, base;

									scale = 1 << (*bytep >> 6);
									index = (*bytep >> 3) & 7;
									base = *bytep & 7;
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
									offset = (signed char)*bytep++;
								}
								else
								{
									offset = *(DWORD *)bytep;
									bytep += 4;
								}
								if ((DWORD_PTR)bytep == *scan)
								{
									CrashPtr += wsprintf (CrashPtr, "  call [");
									if (basereg >= 0)
									{
										CrashPtr += wsprintf (CrashPtr, "%s", regNames[basereg]);
									}
									if (indexreg >= 0)
									{
										CrashPtr += wsprintf (CrashPtr, "%s%s", basereg >= 0 ? "+" : "", regNames[indexreg]);
										if (scale > 1)
										{
											CrashPtr += wsprintf (CrashPtr, "*%d", scale);
										}
									}
									if (offset != 0)
									{
										if (indexreg < 0 && basereg < 0)
										{
											CrashPtr += wsprintf (CrashPtr, "%08x", offset);
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
											CrashPtr += wsprintf (CrashPtr, "%c0x%x", sign, offset);
										}
									}
									CrashPtr += wsprintf (CrashPtr, "]");
								}
							}
						}
					}
					__except(EXCEPTION_EXECUTE_HANDLER)
					{
					}
					if (CrashPtr + 16 >= CrashMax)
						goto done;
				}
			}
		}
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
	}
done:
	*CrashPtr++ = '\r';
	*CrashPtr++ = '\n';
	*CrashPtr = 0;
}

static void DumpBytes ()
{
	BYTE *address = (BYTE *)DumpAddress;

	for (int i = 0; i < 16*3; ++i)
	{
		if ((i & 15) == 0)
		{
			CrashPtr += wsprintf (CrashPtr, "\r\n%08x:", address);
		}
		__try
		{
			CrashPtr += wsprintf (CrashPtr, " %02x", *address);
		}
		__except(EXCEPTION_EXECUTE_HANDLER)
		{
			CrashPtr += wsprintf (CrashPtr, " --");
		}
		address++;
	}
	*CrashPtr++ = '\r';
	*CrashPtr++ = '\n';
	*CrashPtr = 0;
}

static BOOL CALLBACK CrashCallback (HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	HWND edit;
	HGDIOBJ font;

	switch (message)
	{
	case WM_INITDIALOG:
		edit = GetDlgItem (hDlg, IDC_CRASHINFO);
		font = GetStockObject (ANSI_FIXED_FONT);
		if (font != INVALID_HANDLE_VALUE)
		{
			SendMessage (edit, WM_SETFONT, (WPARAM)font, FALSE);
		}
		SendMessage (edit, WM_SETTEXT, 0, (LPARAM)CrashText);
		break;

	case WM_COMMAND:
		if (LOWORD(wParam) != IDC_CRASHINFO)
		{
			EndDialog (hDlg, 0);
		}
	}
	return FALSE;
}

void DisplayCrashLog ()
{
	if (CrashText == NULL)
	{
		char ohPoo[] = "ZDoom crashed but did not have any memory available\n"
			"to record detailed information about the crash.\n"
			"\nThis is all that is available:\n\nCode=XXXXXXXX\nAddr=XXXXXXXX";
		wsprintf (ohPoo + sizeof(ohPoo) - 23, "%08X\nAddr=%08X", CrashCode, CrashAddress);
		MessageBox (Window, ohPoo,
			"ZDoom Very Fatal Error", MB_OK|MB_ICONSTOP);
	}
	else
	{
		DialogBox (g_hInst, MAKEINTRESOURCE(IDD_CRASHDIALOG), Window, (DLGPROC)CrashCallback);
		HeapFree (GetProcessHeap(), 0, CrashText);
		CrashText = NULL;
	}
}
