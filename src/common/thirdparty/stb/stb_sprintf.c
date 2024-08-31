#define STB_SPRINTF_IMPLEMENTATION
#define STB_SPRINTF_UTF8_CHARS
#include "stb_sprintf.h"

// We still need our own wrappers because they use a size_t for count, not an int.
int mysnprintf(char* buf, size_t count, char const* fmt, ...)
{
	int result;
	va_list va;
	va_start(va, fmt);

	result = stbsp_vsnprintf(buf, (int)count, fmt, va);
	va_end(va);

	return result;
}

int myvsnprintf(char* buf, size_t count, const char* fmt, va_list va)
{
	return stbsp_vsnprintf(buf, (int)count, fmt, va);
}
