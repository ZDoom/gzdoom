
#include "doomtype.h"
#include "bicubic_interpolation.h"

void BicubicInterpolation::ScaleImage(uint32_t *dest_data, int dest_width, int dest_height, const uint32_t *src_data, int src_width, int src_height)
{
	if (dest_width <= 0 || dest_height <= 0 || src_width <= 0 || src_height <= 0)
		return;

	// Scale factor as a rational number r = n / d
	int n = dest_width;
	int d = src_width;

	const unsigned char *src_ptr = (const unsigned char *)src_data;
	unsigned char *dest_ptr = (unsigned char *)dest_data;

	scale(n, d, src_width, src_width * 4, src_height, src_ptr + 0, dest_width, dest_width * 4, dest_height, dest_ptr + 0);
	scale(n, d, src_width, src_width * 4, src_height, src_ptr + 1, dest_width, dest_width * 4, dest_height, dest_ptr + 1);
	scale(n, d, src_width, src_width * 4, src_height, src_ptr + 2, dest_width, dest_width * 4, dest_height, dest_ptr + 2);
	scale(n, d, src_width, src_width * 4, src_height, src_ptr + 3, dest_width, dest_width * 4, dest_height, dest_ptr + 3);
}

void BicubicInterpolation::scale(int n, int d, int in_width, int in_pitch, int in_height, const unsigned char *f, int out_width, int out_pitch, int out_height, unsigned char *g)
{
	// Implementation of Michael J. Aramini's Efficient Image Magnification by Bicubic Spline Interpolation

	int dimension_size = (out_width > out_height) ? out_width : out_height;
	L_vector.resize(dimension_size);

	for (int i=0;i<4;i++)
		c_vector[i].resize(dimension_size);
	h_vector.resize(in_width);

	int larger_out_dimension;
	int j, k, l, m, index;
	int *L = &L_vector[0];
	float x;
	float *c[4] = { &c_vector[0][0], &c_vector[1][0], &c_vector[2][0], &c_vector[3][0] };
	float *h = &h_vector[0];

	larger_out_dimension = (out_width > out_height) ? out_width : out_height;

	for (k = 0; k < larger_out_dimension; k++)
		L[k] = (k * d) / n;

	for (k = 0; k < n; k++)
	{
		x = (float)((k * d) % n) / (float)n;
		c[0][k] = C0(x);
		c[1][k] = C1(x);
		c[2][k] = C2(x);
		c[3][k] = C3(x);
	}
	for (k = n; k < larger_out_dimension; k++)
		for (l = 0; l < 4; l++)
			c[l][k] = c[l][k % n];

	for (k = 0; k < out_height; k++)
	{
		for (j = 0; j < in_width; j++)
		{
			h[j] = 0.0f;
			for (l = 0; l < 4; l++)
			{
				index = L[k] + l - 1;
				if ((index >= 0) && (index < in_height))
					h[j] += f[index*in_pitch+j*4] * c[3 - l][k];
			}
		}
		for (m = 0; m < out_width; m++)
		{
			x = 0.5f;
			for (l = 0; l < 4; l++)
			{
				index = L[m] + l - 1;
				if ((index >= 0) && (index < in_width))
					x += h[index] * c[3 - l][m];
			}
			if (x <= 0.0f)
				g[k*out_pitch+m*4] = 0;
			else if (x >= 255)
				g[k*out_pitch+m*4] = 255;
			else
				g[k*out_pitch+m*4] = (unsigned char)x;
		}
	}
}

inline float BicubicInterpolation::C0(float t)
{
	return -a * t * t * t + a * t * t;
}

inline float BicubicInterpolation::C1(float t)
{
	return -(a + 2.0f) * t * t * t + (2.0f * a + 3.0f) * t * t - a * t;
}

inline float BicubicInterpolation::C2(float t)
{
	return (a + 2.0f) * t * t * t - (a + 3.0f) * t * t + 1.0f;
}

inline float BicubicInterpolation::C3(float t)
{
	return a * t * t * t - 2.0f * a * t * t + a * t;
}
