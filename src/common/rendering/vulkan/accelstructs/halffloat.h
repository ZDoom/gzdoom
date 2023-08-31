/*
**
**  This software is provided 'as-is', without any express or implied
**  warranty.  In no event will the authors be held liable for any damages
**  arising from the use of this software.
**
**  Permission is granted to anyone to use this software for any purpose,
**  including commercial applications, and to alter it and redistribute it
**  freely, subject to the following restrictions:
**
**  1. The origin of this software must not be misrepresented; you must not
**     claim that you wrote the original software. If you use this software
**     in a product, an acknowledgment in the product documentation would be
**     appreciated but is not required.
**  2. Altered source versions must be plainly marked as such, and must not be
**     misrepresented as being the original software.
**  3. This notice may not be removed or altered from any source distribution.
**
**  Note: Some of the libraries UICore may link to may have additional
**  requirements or restrictions.
**
**  Based on the paper "Fast Half Float Conversions" by Jeroen van der Zijp.
*/

#pragma once

namespace HalfFloatTables
{
	extern unsigned int mantissa_table[2048];
	extern unsigned int exponent_table[64];
	extern unsigned short offset_table[64];
	extern unsigned short base_table[512];
	extern unsigned char shift_table[512];
};

/// Convert half-float to float. Only works for 'normal' half-float values
inline float halfToFloatSimple(unsigned short hf)
{
	unsigned int float_value = ((hf & 0x8000) << 16) | (((hf & 0x7c00) + 0x1C000) << 13) | ((hf & 0x03FF) << 13);
	void *ptr = static_cast<void*>(&float_value);
	return *static_cast<float*>(ptr);
}

/// Convert float to half-float. Only works for 'normal' half-float values
inline unsigned short floatToHalfSimple(float float_value)
{
	void *ptr = static_cast<void*>(&float_value);
	unsigned int f = *static_cast<unsigned int*>(ptr);
	return ((f >> 16) & 0x8000) | ((((f & 0x7f800000) - 0x38000000) >> 13) & 0x7c00) | ((f >> 13) & 0x03ff);
}

/// Convert half-float to float
inline float halfToFloat(unsigned short hf)
{
	using namespace HalfFloatTables;
	unsigned int float_value = mantissa_table[offset_table[hf >> 10] + (hf & 0x3ff)] + exponent_table[hf >> 10];
	void *ptr = static_cast<void*>(&float_value);
	return *static_cast<float*>(ptr);
}

/// Convert float to half-float
inline unsigned short floatToHalf(float float_value)
{
	using namespace HalfFloatTables;
	void *ptr = static_cast<void*>(&float_value);
	unsigned int f = *static_cast<unsigned int*>(ptr);
	return base_table[(f >> 23) & 0x1ff] + ((f & 0x007fffff) >> shift_table[(f >> 23) & 0x1ff]);
}
