#pragma once

#if defined __GNUC__ || defined __clang__
# define ATTRIBUTE(attrlist) __attribute__(attrlist)
#else
# define ATTRIBUTE(attrlist)
#endif

// This header collects all things printf, so that this doesn't need to pull in other, far more dirty headers, just for outputting some text.

extern "C" int mysnprintf(char* buffer, size_t count, const char* format, ...) ATTRIBUTE((format(printf, 3, 4)));
extern "C" int myvsnprintf(char* buffer, size_t count, const char* format, va_list argptr) ATTRIBUTE((format(printf, 3, 0)));

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
	PRINT_NOTIFY = 4096,	// Flag - add to notify buffer
};

enum
{
	DMSG_OFF,		// no developer messages.
	DMSG_ERROR,		// general notification messages
	DMSG_WARNING,	// warnings
	DMSG_NOTIFY,	// general notification messages
	DMSG_SPAMMY,	// for those who want to see everything, regardless of its usefulness.
};


void I_Error(const char *fmt, ...) ATTRIBUTE((format(printf,1,2)));
void I_FatalError(const char* fmt, ...) ATTRIBUTE((format(printf, 1, 2)));

// This really could need some cleanup - the main problem is that it'd create
// lots of potential for merge conflicts.

int PrintString (int iprintlevel, const char *outline);
int VPrintf(int printlevel, const char* format, va_list parms);
int Printf (int printlevel, const char *format, ...) ATTRIBUTE((format(printf,2,3)));
int Printf (const char *format, ...) ATTRIBUTE((format(printf,1,2)));
int DPrintf (int level, const char *format, ...) ATTRIBUTE((format(printf,2,3)));

void debugprintf(const char* f, ...);	// Prints to the debugger's log.
