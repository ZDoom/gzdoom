// Some libraries expect these functions, for which Visual Studio (pre-2013) falls down on the job.

#include <stdio.h>
#include <math.h>

#ifndef _MSC_VER
# include <stdint.h>
int64_t _ftelli64(
   FILE *stream
);
int _fseeki64(
   FILE *stream,
   int64_t offset,
   int origin
);
#endif

int fseeko(FILE *fp, off_t offset, int whence)
{
    return _fseeki64(fp, (int64_t)offset, whence);
}
int fseeko64(FILE *fp, off64_t offset, int whence)
{
    return _fseeki64(fp, (int64_t)offset, whence);
}

off_t ftello(FILE *stream)
{
    return (off_t)_ftelli64(stream);
}
off64_t ftello64(FILE *stream)
{
    return (off64_t)_ftelli64(stream);
}

long lround(double d)
{
    return (long)(d > 0 ? d + 0.5 : ceil(d - 0.5));
}
