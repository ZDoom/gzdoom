#ifndef _RE2C_IR_SKELETON_WAY_
#define _RE2C_IR_SKELETON_WAY_

#include "src/util/c99_stdint.h"
#include <stdio.h>
#include <utility>
#include <vector>

namespace re2c
{

typedef std::vector<std::pair<uint32_t, uint32_t> > way_arc_t;
typedef std::vector<const way_arc_t *> way_t;

bool cmp_ways (const way_t & w1, const way_t & w2);
void fprint_way (FILE * f, const way_t & p);

} // namespace re2c

#endif // _RE2C_IR_SKELETON_WAY_
