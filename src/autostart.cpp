// This file contains the heads of lists stored in special data segments
//
// This particular scheme was chosen because it's small.

// An alternative that will work with any C++ compiler is to use static
// classes to build these lists at run time. Under Visual C++, doing things
// that way can require a lot of extra space, which is why I'm doing things
// this way.
//
// In the case of TypeInfo lists (section creg), I orginally used the
// constructor to do just that, and the code for that still exists if you
// compile with something other than Visual C++ or GCC.

#include "autosegs.h"

#if defined(_MSC_VER)

#pragma comment(linker, "/merge:.areg=.data /merge:.creg=.data /merge:.greg=.data /merge:.sreg=.data")

#pragma data_seg(".areg$a")
void *ARegHead = 0;

#pragma data_seg(".creg$a")
void *CRegHead = 0;

#pragma data_seg(".greg$a")
void *GRegHead = 0;

#pragma data_seg(".sreg$a")
void *SRegHead = 0;

#pragma data_seg()



#elif defined(__GNUC__)

void *ARegHead __attribute__((section("areg"))) = 0;
void *CRegHead __attribute__((section("creg"))) = 0;
void *GRegHead __attribute__((section("greg"))) = 0;
void *SRegHead __attribute__((section("sreg"))) = 0;

#elif

#error Please fix autostart.cpp for your compiler

#endif
