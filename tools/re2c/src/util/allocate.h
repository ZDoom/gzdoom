#ifndef _RE2C_UTIL_ALLOCATE_
#define _RE2C_UTIL_ALLOCATE_

#include <stddef.h> // size_t

namespace re2c {

// useful fof allocation of arrays of POD objects
// 'new []' invokes default constructor for each object
// this can be unacceptable for performance reasons
template <typename T> T * allocate (size_t n)
{
	void * p = operator new (n * sizeof (T));
	return static_cast<T *> (p);
}

} // namespace re2c

#endif // _RE2C_UTIL_ALLOCATE_
