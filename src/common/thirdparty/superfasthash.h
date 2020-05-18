#pragma once
#include <stdint.h>

uint32_t SuperFastHash (const char *data, size_t len);
uint32_t SuperFastHashI (const char *data, size_t len);

inline unsigned int MakeKey(const char* s)
{
	if (s == NULL)
	{
		return 0;
	}
	return SuperFastHashI(s, strlen(s));
}

inline unsigned int MakeKey(const char* s, size_t len)
{
	return SuperFastHashI(s, len);
}

