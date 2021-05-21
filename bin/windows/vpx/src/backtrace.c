/*
    Copyright (c) 2010 ,
        Cloud Wu . All rights reserved.

        http://www.codingnow.com

    Use, modification and distribution are subject to the "New BSD License"
    as listed at <url: http://www.opensource.org/licenses/bsd-license.php >.

   filename: backtrace.c

   build command: gcc -O2 -shared -Wall -o backtrace.dll backtrace.c -lbfd -liberty -limagehlp

   how to use: Call LoadLibraryA("backtrace.dll"); at beginning of your program .

  */

/* modified from original for EDuke32 */

// warnings cleaned up, ported to 64-bit, and heavily extended by Hendricks266

#include <windows.h>
#include <excpt.h>
#include <imagehlp.h>

// Tenuous: MinGW provides _IMAGEHLP_H while MinGW-w64 defines _IMAGEHLP_.
#ifdef _IMAGEHLP_H
# define EBACKTRACE_MINGW32
#endif
#ifdef _IMAGEHLP_
# define EBACKTRACE_MINGW_W64
#endif
#if defined(EBACKTRACE_MINGW32) && !defined(EBACKTRACE_MINGW_W64)
# include "_dbg_common.h"
#endif

#ifndef PACKAGE
# define PACKAGE EBACKTRACE1
#endif
#ifndef PACKAGE_VERSION
# define PACKAGE_VERSION 1
#endif

#if defined(_M_X64) || defined(__amd64__) || defined(__x86_64__) || defined(_WIN64)
# define EBACKTRACE64
#endif

#include <bfd.h>
#include <psapi.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>

#include <sys/stat.h>
#include <time.h>

#include <stdint.h>

#ifndef DBG_PRINTEXCEPTION_C
# define DBG_PRINTEXCEPTION_C (0x40010006)
#endif
#ifndef MS_VC_EXCEPTION
# define MS_VC_EXCEPTION 1080890248
#endif

#if defined __GNUC__ || defined __clang__
# define ATTRIBUTE(attrlist) __attribute__(attrlist)
#else
# define ATTRIBUTE(attrlist)
#endif

#define BUFFER_MAX (16*1024)

struct bfd_ctx {
    bfd * handle;
    asymbol ** symbol;
};

struct bfd_set {
    char * name;
    struct bfd_ctx * bc;
    struct bfd_set *next;
};

struct find_info {
    asymbol **symbol;
    bfd_vma counter;
    const char *file;
    const char *func;
    unsigned line;
};

struct output_buffer {
    char * buf;
    size_t sz;
    size_t ptr;
};

static void
output_init(struct output_buffer *ob, char * buf, size_t sz)
{
    ob->buf = buf;
    ob->sz = sz;
    ob->ptr = 0;
    ob->buf[0] = '\0';
}

static void
output_print(struct output_buffer *ob, const char * format, ...)
{
    va_list ap;

    if (ob->sz == ob->ptr)
        return;
    ob->buf[ob->ptr] = '\0';
    va_start(ap,format);
    vsnprintf(ob->buf + ob->ptr , ob->sz - ob->ptr , format, ap);
    va_end(ap);

    ob->ptr = strlen(ob->buf + ob->ptr) + ob->ptr;
}

static void
lookup_section(bfd *abfd, asection *sec, void *opaque_data)
{
    struct find_info *data = opaque_data;
    bfd_vma vma;

    if (data->func)
        return;

    if (!(bfd_get_section_flags(abfd, sec) & SEC_ALLOC))
        return;

    vma = bfd_get_section_vma(abfd, sec);
    if (data->counter < vma || vma + bfd_get_section_size(sec) <= data->counter)
        return;

    bfd_find_nearest_line(abfd, sec, data->symbol, data->counter - vma, &(data->file), &(data->func), &(data->line));
}

static void
find(struct bfd_ctx * b, DWORD offset, const char **file, const char **func, unsigned *line)
{
    struct find_info data;
    data.func = NULL;
    data.symbol = b->symbol;
    data.counter = offset;
    data.file = NULL;
    data.func = NULL;
    data.line = 0;

    bfd_map_over_sections(b->handle, &lookup_section, &data);
    if (file) {
        *file = data.file;
    }
    if (func) {
        *func = data.func;
    }
    if (line) {
        *line = data.line;
    }
}

static int
init_bfd_ctx(struct bfd_ctx *bc, const char * procname, struct output_buffer *ob)
{
    int r1, r2, r3;
    bfd *b;
    void *symbol_table;
    unsigned dummy = 0;
    bc->handle = NULL;
    bc->symbol = NULL;

    b = bfd_openr(procname, 0);
    if (!b) {
        output_print(ob,"Failed to open bfd from (%s)\n" , procname);
        return 1;
    }

    r1 = bfd_check_format(b, bfd_object);
    r2 = bfd_check_format_matches(b, bfd_object, NULL);
    r3 = bfd_get_file_flags(b) & HAS_SYMS;

    if (!(r1 && r2 && r3)) {
        bfd_close(b);
        if (!(r1 && r2))
            output_print(ob,"Failed to init bfd from (%s): %d %d %d\n", procname, r1, r2, r3);
        return 1;
    }

    if (bfd_read_minisymbols(b, FALSE, &symbol_table, &dummy) == 0) {
        if (bfd_read_minisymbols(b, TRUE, &symbol_table, &dummy) < 0) {
            free(symbol_table);
            bfd_close(b);
            output_print(ob,"Failed to read symbols from (%s)\n", procname);
            return 1;
        }
    }

    bc->handle = b;
    bc->symbol = symbol_table;

    return 0;
}

static void
close_bfd_ctx(struct bfd_ctx *bc)
{
    if (bc) {
        if (bc->symbol) {
            free(bc->symbol);
        }
        if (bc->handle) {
            bfd_close(bc->handle);
        }
    }
}

static struct bfd_ctx *
get_bc(struct output_buffer *ob , struct bfd_set *set , const char *procname)
{
    struct bfd_ctx bc;
    while(set->name) {
        if (strcmp(set->name , procname) == 0) {
            return set->bc;
        }
        set = set->next;
    }
    if (init_bfd_ctx(&bc, procname , ob)) {
        return NULL;
    }
    set->next = calloc(1, sizeof(*set));
    set->bc = malloc(sizeof(struct bfd_ctx));
    memcpy(set->bc, &bc, sizeof(bc));
    set->name = strdup(procname);

    return set->bc;
}

static void
release_set(struct bfd_set *set)
{
    while(set) {
        struct bfd_set * temp = set->next;
        if (set->name)
            free(set->name);
        close_bfd_ctx(set->bc);
        free(set);
        set = temp;
    }
}

static char procname[MAX_PATH];

#ifdef EBACKTRACE64
# define MachineType IMAGE_FILE_MACHINE_AMD64
# define MAYBE64(x) x ## 64
#else
# define MachineType IMAGE_FILE_MACHINE_I386
# define MAYBE64(x) x
#endif

static void
_backtrace(struct output_buffer *ob, struct bfd_set *set, int depth , LPCONTEXT context)
{
    MAYBE64(STACKFRAME) frame;
    HANDLE process, thread;
    char symbol_buffer[sizeof(MAYBE64(IMAGEHLP_SYMBOL)) + 255];
    char module_name_raw[MAX_PATH];
    struct bfd_ctx *bc = NULL;

    GetModuleFileNameA(NULL, procname, sizeof procname);

    memset(&frame,0,sizeof(frame));

#ifdef EBACKTRACE64
    frame.AddrPC.Offset = context->Rip;
    frame.AddrStack.Offset = context->Rsp;
    frame.AddrFrame.Offset = context->Rbp;
#else
    frame.AddrPC.Offset = context->Eip;
    frame.AddrStack.Offset = context->Esp;
    frame.AddrFrame.Offset = context->Ebp;
#endif

    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrStack.Mode = AddrModeFlat;
    frame.AddrFrame.Mode = AddrModeFlat;

    process = GetCurrentProcess();
    thread = GetCurrentThread();

    while(MAYBE64(StackWalk)(MachineType,
        process,
        thread,
        &frame,
        context,
        NULL,
        MAYBE64(SymFunctionTableAccess),
        MAYBE64(SymGetModuleBase), NULL)) {
        MAYBE64(IMAGEHLP_SYMBOL) *symbol;
        MAYBE64(DWORD) module_base;
        const char * module_name = "[unknown module]";

        const char * file = NULL;
        const char * func = NULL;
        unsigned line = 0;

        --depth;
        if (depth < 0)
            break;

        symbol = (MAYBE64(IMAGEHLP_SYMBOL) *)symbol_buffer;
        symbol->SizeOfStruct = (sizeof *symbol) + 255;
        symbol->MaxNameLength = 254;

        module_base = MAYBE64(SymGetModuleBase)(process, frame.AddrPC.Offset);

        if (module_base &&
            GetModuleFileNameA((HINSTANCE)(intptr_t)module_base, module_name_raw, MAX_PATH)) {
            module_name = module_name_raw;
            bc = get_bc(ob, set, module_name);
        }

        if (bc) {
            find(bc,frame.AddrPC.Offset,&file,&func,&line);
        }

        if (file == NULL) {
            MAYBE64(DWORD) dummy = 0;
            if (MAYBE64(SymGetSymFromAddr)(process, frame.AddrPC.Offset, &dummy, symbol)) {
                file = symbol->Name;
            }
            else {
                file = "[unknown file]";
            }
        }

        output_print(ob,"0x%p : %s : %s", frame.AddrPC.Offset, module_name, file);
        if (func != NULL)
            output_print(ob, " (%d) : in function (%s)", line, func);
        output_print(ob, "\n");
    }
}

static LPTSTR FormatErrorMessage(DWORD dwMessageId)
{
    LPTSTR lpBuffer = NULL;

    // adapted from http://stackoverflow.com/a/455533
    FormatMessage(
        FORMAT_MESSAGE_FROM_SYSTEM
        |FORMAT_MESSAGE_ALLOCATE_BUFFER
        |FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dwMessageId,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US)
        (LPTSTR)&lpBuffer,
        0,
        NULL);

    return lpBuffer; // must be LocalFree()'d by caller
}

static LPTSTR FormatExceptionCodeMessage(DWORD dwMessageId)
{
    LPTSTR lpBuffer = NULL;

    FormatMessage(
        FORMAT_MESSAGE_FROM_HMODULE
        |FORMAT_MESSAGE_ALLOCATE_BUFFER
        |FORMAT_MESSAGE_IGNORE_INSERTS,
        GetModuleHandleA("ntdll.dll"),
        dwMessageId,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US)
        (LPTSTR)&lpBuffer,
        0,
        NULL);

    return lpBuffer; // must be LocalFree()'d by caller
}


// adapted from http://www.catch22.net/tuts/custom-messagebox
static HHOOK hMsgBoxHook;

LRESULT CALLBACK CBTProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode < 0)
        return CallNextHookEx(hMsgBoxHook, nCode, wParam, lParam);

    switch (nCode)
    {
    case HCBT_ACTIVATE:
    {
        // Get handle to the message box!
        HWND hwnd = (HWND)wParam;

        // Do customization!
		SetWindowTextA(GetDlgItem(hwnd, IDYES), "Quit");
		SetWindowTextA(GetDlgItem(hwnd, IDNO), "Continue");
		SetWindowTextA(GetDlgItem(hwnd, IDCANCEL), "Ignore");
        return 0;
    }
        break;
    }

    // Call the next hook, if there is one
    return CallNextHookEx(hMsgBoxHook, nCode, wParam, lParam);
}

int ExceptionMessage(TCHAR *szText, TCHAR *szCaption)
{
    int retval;

    // Install a window hook, so we can intercept the message-box
    // creation, and customize it
    hMsgBoxHook = SetWindowsHookEx(
        WH_CBT,
        CBTProc,
        NULL,
        GetCurrentThreadId()            // Only install for THIS thread!!!
        );

    // Display a standard message box
    retval = MessageBoxA(NULL, szText, szCaption, MB_YESNOCANCEL|MB_ICONERROR|MB_TASKMODAL);

    // remove the window hook
    UnhookWindowsHookEx(hMsgBoxHook);

    return retval;
}

static char crashlogfilename[MAX_PATH] = "crash.log";
static char propername[MAX_PATH] = "this application";

__declspec(dllexport) void SetTechnicalName(const char* input)
{
    snprintf(crashlogfilename, MAX_PATH, "%s.crash.log", input);
}
__declspec(dllexport) void SetProperName(const char* input)
{
    strncpy(propername, input, MAX_PATH);
}

static char * g_output = NULL;
static PVOID g_prev = NULL;

static LONG WINAPI
exception_filter(LPEXCEPTION_POINTERS info)
{
    struct output_buffer ob;
    int logfd, written, msgboxID;
    PEXCEPTION_RECORD exception;
    BOOL initialized = FALSE;
    char *ExceptionPrinted;

    for (exception = info->ExceptionRecord; exception != NULL; exception = exception->ExceptionRecord)
    {
#if 0
        if (exception->ExceptionFlags & EXCEPTION_NONCONTINUABLE)
            continuable = FALSE;
#endif

        switch (exception->ExceptionCode)
        {
        case EXCEPTION_BREAKPOINT:
        case EXCEPTION_SINGLE_STEP:
        case DBG_CONTROL_C:
        case DBG_PRINTEXCEPTION_C:
        case MS_VC_EXCEPTION:
            break;
        default:
        {
            LPTSTR ExceptionCodeMsg = FormatExceptionCodeMessage(exception->ExceptionCode);
            // The message for this exception code is broken.
            LPTSTR ExceptionText = exception->ExceptionCode == EXCEPTION_ACCESS_VIOLATION ? "Access violation." : ExceptionCodeMsg;

            if (!initialized)
            {
                output_init(&ob, g_output, BUFFER_MAX);
                initialized = TRUE;
            }

            output_print(&ob, "Caught exception 0x%08X at 0x%p: %s\n", exception->ExceptionCode, exception->ExceptionAddress, ExceptionText);

            LocalFree(ExceptionCodeMsg);
        }
            break;
        }
    }

    if (!initialized)
        return EXCEPTION_CONTINUE_SEARCH; // EXCEPTION_CONTINUE_EXECUTION

    ExceptionPrinted = (char*)calloc(strlen(g_output) + 37 + 2*MAX_PATH, sizeof(char));
    strcpy(ExceptionPrinted, g_output);
    strcat(ExceptionPrinted, "\nPlease send ");
    strcat(ExceptionPrinted, crashlogfilename);
    strcat(ExceptionPrinted, " to the maintainers of ");
    strcat(ExceptionPrinted, propername);
    strcat(ExceptionPrinted, ".");

    {
        DWORD error = 0;
        BOOL SymInitialized = SymInitialize(GetCurrentProcess(), NULL, TRUE);

        if (!SymInitialized)
        {
            LPTSTR errorText;

            error = GetLastError();
            errorText = FormatErrorMessage(error);
            output_print(&ob, "SymInitialize() failed with error %d: %s\n", error, errorText);
            LocalFree(errorText);
        }

        if (SymInitialized || error == 87)
        {
            struct bfd_set *set = calloc(1,sizeof(*set));
            bfd_init();
            _backtrace(&ob , set , 128 , info->ContextRecord);
            release_set(set);

            SymCleanup(GetCurrentProcess());
        }
    }

    logfd = open(crashlogfilename, O_APPEND | O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);

    if (logfd) {
        time_t curtime;
        struct tm *curltime;
        const char *theasctime;
        const char *finistr = "---------------\n";

        while ((written = write(logfd, g_output, strlen(g_output)))) {
            g_output += written;
        }

        curtime = time(NULL);
        curltime = localtime(&curtime);
        theasctime = curltime ? asctime(curltime) : NULL;

        if (theasctime)
            write(logfd, theasctime, strlen(theasctime));
        write(logfd, finistr, strlen(finistr));
        close(logfd);
    }

    //fputs(g_output, stderr);

    msgboxID = ExceptionMessage(ExceptionPrinted, propername);

    free(ExceptionPrinted);

    switch (msgboxID)
    {
    case IDYES:
        exit(0xBAC);
        break;
    case IDNO:
        break;
    case IDCANCEL:
        return EXCEPTION_CONTINUE_EXECUTION;
        break;
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

static void
backtrace_register(void)
{
    if (g_output == NULL) {
        g_output = malloc(BUFFER_MAX);
        g_prev = AddVectoredExceptionHandler(1, exception_filter);
    }
}

static void
backtrace_unregister(void)
{
    if (g_output) {
        free(g_output);
        RemoveVectoredExceptionHandler(g_prev);
        g_prev = NULL;
        g_output = NULL;
    }
}

BOOL WINAPI
DllMain(HINSTANCE hinstDLL ATTRIBUTE((unused)), DWORD dwReason, LPVOID lpvReserved ATTRIBUTE((unused)))
{
    switch (dwReason) {
    case DLL_PROCESS_ATTACH:
        backtrace_register();
        break;
    case DLL_PROCESS_DETACH:
        backtrace_unregister();
        break;
    }
    return TRUE;
}


/* cut dependence on libintl... libbfd needs this */
char *libintl_dgettext (const char *domain_name ATTRIBUTE((unused)), const char *msgid ATTRIBUTE((unused)))
{
    static char buf[1024] = "XXX placeholder XXX";
    return buf;
}

int __printf__ ( const char * format, ... );
int libintl_fprintf ( FILE * stream, const char * format, ... );
int libintl_sprintf ( char * str, const char * format, ... );
int libintl_snprintf ( char *buffer, int buf_size, const char *format, ... );
int libintl_vprintf ( const char * format, va_list arg );
int libintl_vfprintf ( FILE * stream, const char * format, va_list arg );
int libintl_vsprintf ( char * str, const char * format, va_list arg );

int __printf__ ( const char * format, ... )
{
    int value;
    va_list arg;
    va_start(arg, format);
    value = vprintf ( format, arg );
    va_end(arg);
    return value;
}

int libintl_fprintf ( FILE * stream, const char * format, ... )
{
    int value;
    va_list arg;
    va_start(arg, format);
    value = vfprintf ( stream, format, arg );
    va_end(arg);
    return value;
}
int libintl_sprintf ( char * str, const char * format, ... )
{
    int value;
    va_list arg;
    va_start(arg, format);
    value = vsprintf ( str, format, arg );
    va_end(arg);
    return value;
}
int libintl_snprintf ( char *buffer, int buf_size, const char *format, ... )
{
    int value;
    va_list arg;
    va_start(arg, format);
    value = vsnprintf ( buffer, buf_size, format, arg );
    va_end(arg);
    return value;
}
int libintl_vprintf ( const char * format, va_list arg )
{
    return vprintf ( format, arg );
}
int libintl_vfprintf ( FILE * stream, const char * format, va_list arg )
{
    return vfprintf ( stream, format, arg );
}
int libintl_vsprintf ( char * str, const char * format, va_list arg )
{
    return vsprintf ( str, format, arg );
}

/* cut dependence on zlib... libbfd needs this */

int compress (unsigned char *dest ATTRIBUTE((unused)), unsigned long destLen ATTRIBUTE((unused)), const unsigned char source ATTRIBUTE((unused)), unsigned long sourceLen ATTRIBUTE((unused)))
{
    return 0;
}
unsigned long compressBound (unsigned long sourceLen)
{
    return sourceLen + (sourceLen >> 12) + (sourceLen >> 14) + (sourceLen >> 25) + 13;
}
int inflateEnd(void *strm ATTRIBUTE((unused)))
{
    return 0;
}
int inflateInit_(void *strm ATTRIBUTE((unused)), const char *version ATTRIBUTE((unused)), int stream_size ATTRIBUTE((unused)))
{
    return 0;
}
int inflateReset(void *strm ATTRIBUTE((unused)))
{
    return 0;
}
int inflate(void *strm ATTRIBUTE((unused)), int flush ATTRIBUTE((unused)))
{
    return 0;
}
