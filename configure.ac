#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.57)
AC_INIT(zdoom, 2.0.63, smordak@yahoo.com)
AC_CONFIG_AUX_DIR(config)
AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE(dist-bzip2)
AC_CONFIG_SRCDIR([src/r_sky.cpp])
AC_CONFIG_HEADER(config.h)
AC_PROG_CXX

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

# Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([arpa/inet.h fcntl.h float.h limits.h malloc.h netdb.h netinet/in.h stddef.h stdlib.h string.h sys/ioctl.h sys/param.h sys/socket.h sys/time.h sys/timeb.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_C_INLINE
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_STRUCT_TM
AC_C_VOLATILE
AC_CHECK_TYPES([ptrdiff_t])
AC_C_BIGENDIAN

# Checks for library functions.
AC_FUNC_ALLOCA
AC_REPLACE_FNMATCH
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_FUNC_REALLOC
AC_TYPE_SIGNAL
AC_FUNC_STAT
AC_FUNC_STRTOD
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([atexit dup2 floor getcwd gethostbyname gettimeofday memchr memmove memset mkdir modf pow realpath  socket sqrt strchr strdup strerror strrchr strstr strtol strtoul filelength strupr])

AC_CHECK_FUNC(stricmp, stricmp=yes, stricmp=no)
if test $stricmp = no; then
	AC_CHECK_FUNC(strcasecmp, 
		AC_DEFINE(stricmp, strcasecmp, [Define stricmp as strcasecmp if you have one but not the other]),
		AC_MSG_ERROR([Neither stricmp nor strcasecmp found])
	)
fi

AC_CHECK_FUNC(strnicmp, strnicmp=yes, strnicmp=no)
if test $strnicmp = no; then
	AC_CHECK_FUNC(strncasecmp, 
		AC_DEFINE(strnicmp, strncasecmp, [Define strnicmp as strncasecmp if you have one but not the other]),
		AC_MSG_ERROR([Neither strnicmp nor strncasecmp found])
	)
fi

# Checks for libraries.

# Check for SDL
dnl Check for SDL
SDL_VERSION=1.2.0
AM_PATH_SDL($SDL_VERSION,
            :,
            AC_MSG_ERROR([*** SDL version $SDL_VERSION not found!])
)
CFLAGS="$CFLAGS $SDL_CFLAGS"
CXXFLAGS="$CXXFLAGS $SDL_CFLAGS"
LIBS="$LIBS $SDL_LIBS"

# NASM is available from: http://nasm.sourceforge.net/
# This snippet is taken from SDL's configure.in
dnl Make sure we are running on an x86 platform
case $target in
    i?86*)
		dnl Check for NASM (for assembly routines)
		AC_ARG_ENABLE(nasm,
		[  --enable-nasm           use nasm routines on x86 [default=yes]],
				, enable_nasm=yes)
		if test x$enable_nasm = xyes; then
			AC_PATH_PROG(NASM, nasm)
			if test x$NASM = x -o x$NASM = x'"$NASM"'; then
				CFLAGS="$CLAGS -DNOASM"
				: # nasm isn't installed
			else
				CFLAGS="$CFLAGS -DUSEASM"
				case $ARCH in
				win32)
					NASMFLAGS="-f win32"
					;;
				openbsd)
					NASMFLAGS="-f aoutb"
					;;
				*)
				# M_TARGET_LINUX really means "target is ELF"
					NASMFLAGS="-f elf -DM_TARGET_LINUX"
					;;
				esac
				AC_SUBST(NASMFLAGS)
			fi
		else
			CFLAGS="$CFLAGS -DNOASM"
		fi
        ;;
    *)
    # Nope, then NASM is definitely not available
		CFLAGS="$CFLAGS -DNOASM"
        ;;
esac

# Check for FMOD, first in the path, then in ~/fomdapi
AC_MSG_CHECKING([a suitable version of FMOD])
LIBS="$LIBS -lfmod-3.74"
AC_LINK_IFELSE(
[AC_LANG_PROGRAM([[#include <fmod.h>]],
 [[FSOUND_Init (44100, 64, FSOUND_INIT_STREAM_FROM_MAIN_THREAD);
   FMUSIC_LoadSongEx ("dummy", 0, 100, 0, (void*)0, 0);
   FSOUND_Stream_Open ("dummy", 0, 0, 100);]])],
 AC_MSG_RESULT([yes]),
 LIBS="$LIBS -L${HOME}/fmodapi/api"
 CFLAGS="$CFLAGS -I${HOME}/fmodapi/api/inc"
 CXXFLAGS="$CXXFLAGS -I${HOME}/fmodapi/api/inc"
 AC_LINK_IFELSE(
 [AC_LANG_PROGRAM([[#include <fmod.h>]],
  [[FSOUND_Init (44100, 64, FSOUND_INIT_STREAM_FROM_MAIN_THREAD);
    FMUSIC_LoadSongEx ("dummy", 0, 100, 0, (void*)0, 0);
    FSOUND_Stream_Open ("dummy", 0, 0, 100);]])],
  AC_MSG_RESULT([in ~/fmodapi]),
  AC_MSG_RESULT([no])
  AC_MSG_FAILURE([Could not find FMOD 3.74 or a compatible version.])))

# Check for zlib
AC_CHECK_LIB(z, deflate,, AC_MSG_FAILURE([You need zlib to build ZDoom. See http://www.gzip.org/zlib/]))
# Check for libFLAC and libFLAC++
AC_CHECK_LIB(FLAC, FLAC__stream_decoder_new)

AC_LANG_CPLUSPLUS
AC_MSG_CHECKING([libFLAC++])
ac_save_LIBS=$LIBS
LIBS="-lFLAC++ $LIBS"
AC_LINK_IFELSE(
[AC_LANG_PROGRAM([[#include <FLAC++/decoder.h>

class TestStream : public FLAC::Decoder::Stream
{
public:
	TestStream () {}
protected:
	::FLAC__StreamDecoderReadStatus read_callback(FLAC__byte buffer[], unsigned *bytes)
	{
		return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
	}
	::FLAC__StreamDecoderWriteStatus write_callback(const ::FLAC__Frame *frame, const FLAC__int32 * const buffer[])
	{
		return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
	}
	void metadata_callback(const ::FLAC__StreamMetadata *metadata)
	{
	}
	void error_callback(::FLAC__StreamDecoderErrorStatus status)
	{
	}
};]],
 [[TestStream test_it;]])],
 AC_MSG_RESULT([yes]),
 AC_MSG_RESULT([no])
 LIBS=$ac_save_LIBS)

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
