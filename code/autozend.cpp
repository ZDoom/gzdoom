// This file contains the tails of lists stored in special data segments

#include "autosegs.h"

#if defined(_MSC_VER)

#pragma data_seg(".areg$z")
void *ARegTail = 0;

#pragma data_seg(".creg$z")
void *CRegTail = 0;

#pragma data_seg(".greg$z")
void *GRegTail = 0;

#pragma data_seg(".sreg$z")
void *SRegTail = 0;

#pragma data_seg()



#elif defined(__GNUC__)

void *ARegTail __attribute__((section("areg"))) = 0;
void *CRegTail __attribute__((section("creg"))) = 0;
void *GRegTail __attribute__((section("greg"))) = 0;
void *SRegTail __attribute__((section("sreg"))) = 0;


#elif

#error Please fix autozend.cpp for your compiler

#endif
