#pragma once
#include <stdarg.h>

#if defined __GNUC__ || defined __clang__
# define ATTRIBUTE(attrlist) __attribute__(attrlist)
#else
# define ATTRIBUTE(attrlist)
#endif

#include "stb_sprintf.h"

// This header collects all things printf, so that this doesn't need to pull in other, far more dirty headers, just for outputting some text.
extern "C" int mysnprintf(char* buffer, size_t count, const char* format, ...) ATTRIBUTE((format(printf, 3, 4)));
extern "C" int myvsnprintf(char* buffer, size_t count, const char* format, va_list argptr) ATTRIBUTE((format(printf, 3, 0)));

#define TEXTCOLOR_ESCAPE		'\034'
#define TEXTCOLOR_ESCAPESTR		"\034"

#define TEXTCOLOR_BRICK			"\034A"
#define TEXTCOLOR_TAN			"\034B"
#define TEXTCOLOR_GRAY			"\034C"
#define TEXTCOLOR_GREY			"\034C"
#define TEXTCOLOR_GREEN			"\034D"
#define TEXTCOLOR_BROWN			"\034E"
#define TEXTCOLOR_GOLD			"\034F"
#define TEXTCOLOR_RED			"\034G"
#define TEXTCOLOR_BLUE			"\034H"
#define TEXTCOLOR_ORANGE		"\034I"
#define TEXTCOLOR_WHITE			"\034J"
#define TEXTCOLOR_YELLOW		"\034K"
#define TEXTCOLOR_UNTRANSLATED	"\034L"
#define TEXTCOLOR_BLACK			"\034M"
#define TEXTCOLOR_LIGHTBLUE		"\034N"
#define TEXTCOLOR_CREAM			"\034O"
#define TEXTCOLOR_OLIVE			"\034P"
#define TEXTCOLOR_DARKGREEN		"\034Q"
#define TEXTCOLOR_DARKRED		"\034R"
#define TEXTCOLOR_DARKBROWN		"\034S"
#define TEXTCOLOR_PURPLE		"\034T"
#define TEXTCOLOR_DARKGRAY		"\034U"
#define TEXTCOLOR_CYAN			"\034V"
#define TEXTCOLOR_ICE			"\034W"
#define TEXTCOLOR_FIRE			"\034X"
#define TEXTCOLOR_SAPPHIRE		"\034Y"
#define TEXTCOLOR_TEAL			"\034Z"

#define TEXTCOLOR_NORMAL		"\034-"
#define TEXTCOLOR_BOLD			"\034+"

#define TEXTCOLOR_CHAT			"\034*"
#define TEXTCOLOR_TEAMCHAT		"\034!"

// game print flags
enum
{
	PRINT_LOW,		// pickup messages
	PRINT_MEDIUM,	// death messages
	PRINT_HIGH,		// critical messages
	PRINT_CHAT,		// chat messages
	PRINT_TEAMCHAT,	// chat messages from a teammate
	PRINT_LOG,		// only to logfile
	PRINT_BOLD = 200,				// What Printf_Bold used
	PRINT_TYPES = 1023,		// Bitmask.
	PRINT_NONOTIFY = 1024,	// Flag - do not add to notify buffer
	PRINT_NOLOG = 2048,		// Flag - do not print to log file
	PRINT_NOTIFY = 4096,	// Flag - add to game-native notify display - messages without this only go to the generic notification buffer.
	PRINT_NODAPEVENT = 8192, // Flag - do not emit the message as DAP debugger output.
};

enum
{
	DMSG_OFF,		// no developer messages.
	DMSG_ERROR,		// general notification messages
	DMSG_WARNING,	// warnings
	DMSG_NOTIFY,	// general notification messages
	DMSG_SPAMMY,	// for those who want to see everything, regardless of its usefulness.
};


[[noreturn]] void I_Error(const char *fmt, ...) ATTRIBUTE((format(printf,1,2)));
[[noreturn]] void I_FatalError(const char* fmt, ...) ATTRIBUTE((format(printf, 1, 2)));

// This really could need some cleanup - the main problem is that it'd create
// lots of potential for merge conflicts.

int PrintString (int iprintlevel, const char *outline);
int VPrintf(int printlevel, const char* format, va_list parms);
int Printf (int printlevel, const char *format, ...) ATTRIBUTE((format(printf,2,3)));
int Printf (const char *format, ...) ATTRIBUTE((format(printf,1,2)));
int DPrintf (int level, const char *format, ...) ATTRIBUTE((format(printf,2,3)));

void I_DebugPrint(const char* cp);
void I_DebugPrintf(const char* fmt, ...);	// Prints to the debugger's log.

// flag to silence non-error output
extern bool batchrun;
