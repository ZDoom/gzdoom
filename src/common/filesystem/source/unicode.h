#pragma once

#include <stdint.h>
#include <vector>
namespace FileSys {
	
void utf16_to_utf8(const unsigned short* in, std::vector<char>& buffer);
void ibm437_to_utf8(const char* in, std::vector<char>& buffer);
int unicode_tolower(int c);
char *tolower_normalize(const char *str);

}
