#ifndef DUMBFILE_H
#define DUMBFILE_H

#include "../dumb.h"

struct DUMBFILE
{
    const DUMBFILE_SYSTEM *dfs;
    void *file;
    long pos;
};

#endif // DUMBFILE_H
