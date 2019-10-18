#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <float.h>
#include <math.h>
#include <limits.h>
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>
#include <stdint.h>
#include <zlib.h>
#include <new>
#include <algorithm>
#include <forward_list>
#include <sys/stat.h>
#include <sys/types.h>
#include <cassert>
#ifdef _MSC_VER
#include <direct.h>
#include <io.h>
#endif // _MSC_VER
#include <limits>
#include <memory>
#include <tuple>
#include <vector>
#include <utility>
#include <functional>

// These two headers get included nearly everywhere so it doesn't matter if changing them forces a few more recompiles.
// The overall savings from PCHing them are more significant.
#include "tarray.h"
#include "zstring.h"