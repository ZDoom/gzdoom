#ifndef _RE2C_UTIL_FORBID_COPY_
#define _RE2C_UTIL_FORBID_COPY_

// must be used at the end of class definition
// (since this macro changes scope to private)
#define FORBID_COPY(type) \
	private: \
		type (const type &); \
		type & operator = (const type &)

#endif // _RE2C_UTIL_FORBID_COPY_
