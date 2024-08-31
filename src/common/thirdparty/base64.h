//
//  base64 encoding and decoding with C++.
//  Version: 1.01.00
//

#ifndef BASE64_H_C0CE2A47_D10E_42C9_A27C_C883944E704A
#define BASE64_H_C0CE2A47_D10E_42C9_A27C_C883944E704A

#include "tarray.h"

TArray<uint8_t> base64_encode(unsigned char const* bytes_to_encode, size_t in_len);
void base64_decode(void* memory, size_t len, const char* encoded_string);

#endif /* BASE64_H_C0CE2A47_D10E_42C9_A27C_C883944E704A */
