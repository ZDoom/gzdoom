/* _[v]snprintf() from msvcrt.dll might not nul terminate */
/* OpenWatcom-provided versions seem to behave the same... */

#include "common.h"

#if defined(USE_LIBXMP_SNPRINTF)

#undef snprintf
#undef vsnprintf

int libxmp_vsnprintf(char *str, size_t sz, const char *fmt, va_list ap)
{
	int rc = _vsnprintf(str, sz, fmt, ap);
	if (sz != 0) {
		if (rc < 0) rc = (int)sz;
		if ((size_t)rc >= sz) str[sz - 1] = '\0';
	}
	return rc;
}

int libxmp_snprintf (char *str, size_t sz, const char *fmt, ...)
{
	va_list ap;
	int rc;

	va_start (ap, fmt);
	rc = _vsnprintf(str, sz, fmt, ap);
	va_end (ap);

	return rc;
}

#endif

/* Win32 debug message helper by Mirko Buffoni */
#if defined(_MSC_VER) && defined(DEBUG)
void libxmp_msvc_dbgprint(const char *format, ...)
{
	va_list argptr;

	/* do the output */
	va_start(argptr, format);
	vprintf(format, argptr);
	printf("\n");
	va_end(argptr);
}
#endif
