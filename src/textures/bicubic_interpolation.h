/*
**  Bicubic Image Scaler
**  Copyright (c) 2016 Magnus Norddahl
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
*/

#ifndef __BICUBIC_INTERPOLATION_H__
#define __BICUBIC_INTERPOLATION_H__

#pragma once

#include <vector>

// Bicubic image scaler
class BicubicInterpolation
{
public:
	void ScaleImage(uint32_t *dest, int dest_width, int dest_height, const uint32_t *src, int src_width, int src_height);

private:
	void scale(int n, int d, int in_width, int in_pitch, int in_height, const unsigned char *in_data, int out_width, int out_pitch, int out_height, unsigned char *out_data);

	float a = -0.5f; // a is a spline parameter such that -1 <= a <= 0

	inline float C0(float t);
	inline float C1(float t);
	inline float C2(float t);
	inline float C3(float t);

	std::vector<int> L_vector;
	std::vector<float> c_vector[4];
	std::vector<float> h_vector;
};

#endif
