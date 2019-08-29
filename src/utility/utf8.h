#pragma once

int utf8_encode(int32_t codepoint, uint8_t *buffer, int *size);
int utf8_decode(const uint8_t *src, int *size);
int GetCharFromString(const uint8_t *&string);
inline int GetCharFromString(const char32_t *&string)
{
	return *string++;
}
const char *MakeUTF8(const char *outline, int *numchars = nullptr);	// returns a pointer to a static buffer, assuming that its caller will immediately process the result. 
const char *MakeUTF8(int codepoint, int *psize = nullptr);

extern uint16_t win1252map[];
