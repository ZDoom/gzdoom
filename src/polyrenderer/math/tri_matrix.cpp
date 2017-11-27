/*
**  Polygon Doom software renderer
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
**
*/

#include <stddef.h>
#include "templates.h"
#include "doomdef.h"
#include "i_system.h"
#include "w_wad.h"
#include "v_video.h"
#include "doomstat.h"
#include "st_stuff.h"
#include "g_game.h"
#include "g_level.h"
#include "r_data/r_translate.h"
#include "v_palette.h"
#include "r_data/colormaps.h"
#include "r_utility.h"
#include "tri_matrix.h"
#include "polyrenderer/drawers/poly_triangle.h"
#include "swrenderer/drawers/r_draw_rgba.h"
#include "swrenderer/viewport/r_viewport.h"

TriMatrix TriMatrix::null()
{
	TriMatrix m;
	memset(m.matrix, 0, sizeof(m.matrix));
	return m;
}

TriMatrix TriMatrix::identity()
{
	TriMatrix m = null();
	m.matrix[0] = 1.0f;
	m.matrix[5] = 1.0f;
	m.matrix[10] = 1.0f;
	m.matrix[15] = 1.0f;
	return m;
}

TriMatrix TriMatrix::translate(float x, float y, float z)
{
	TriMatrix m = identity();
	m.matrix[0 + 3 * 4] = x;
	m.matrix[1 + 3 * 4] = y;
	m.matrix[2 + 3 * 4] = z;
	return m;
}

TriMatrix TriMatrix::scale(float x, float y, float z)
{
	TriMatrix m = null();
	m.matrix[0 + 0 * 4] = x;
	m.matrix[1 + 1 * 4] = y;
	m.matrix[2 + 2 * 4] = z;
	m.matrix[3 + 3 * 4] = 1;
	return m;
}

TriMatrix TriMatrix::rotate(float angle, float x, float y, float z)
{
	float c = cosf(angle);
	float s = sinf(angle);
	TriMatrix m = null();
	m.matrix[0 + 0 * 4] = (x*x*(1.0f - c) + c);
	m.matrix[0 + 1 * 4] = (x*y*(1.0f - c) - z*s);
	m.matrix[0 + 2 * 4] = (x*z*(1.0f - c) + y*s);
	m.matrix[1 + 0 * 4] = (y*x*(1.0f - c) + z*s);
	m.matrix[1 + 1 * 4] = (y*y*(1.0f - c) + c);
	m.matrix[1 + 2 * 4] = (y*z*(1.0f - c) - x*s);
	m.matrix[2 + 0 * 4] = (x*z*(1.0f - c) - y*s);
	m.matrix[2 + 1 * 4] = (y*z*(1.0f - c) + x*s);
	m.matrix[2 + 2 * 4] = (z*z*(1.0f - c) + c);
	m.matrix[3 + 3 * 4] = 1.0f;
	return m;
}

TriMatrix TriMatrix::swapYZ()
{
	TriMatrix m = null();
	m.matrix[0 + 0 * 4] = 1.0f;
	m.matrix[1 + 2 * 4] = 1.0f;
	m.matrix[2 + 1 * 4] = -1.0f;
	m.matrix[3 + 3 * 4] = 1.0f;
	return m;
}

TriMatrix TriMatrix::perspective(float fovy, float aspect, float z_near, float z_far)
{
	float f = (float)(1.0 / tan(fovy * M_PI / 360.0));
	TriMatrix m = null();
	m.matrix[0 + 0 * 4] = f / aspect;
	m.matrix[1 + 1 * 4] = f;
	m.matrix[2 + 2 * 4] = (z_far + z_near) / (z_near - z_far);
	m.matrix[2 + 3 * 4] = (2.0f * z_far * z_near) / (z_near - z_far);
	m.matrix[3 + 2 * 4] = -1.0f;
	return m;
}

TriMatrix TriMatrix::frustum(float left, float right, float bottom, float top, float near, float far)
{
	float a = (right + left) / (right - left);
	float b = (top + bottom) / (top - bottom);
	float c = -(far + near) / (far - near);
	float d = -(2.0f * far) / (far - near);
	TriMatrix m = null();
	m.matrix[0 + 0 * 4] = 2.0f * near / (right - left);
	m.matrix[1 + 1 * 4] = 2.0f * near / (top - bottom);
	m.matrix[0 + 2 * 4] = a;
	m.matrix[1 + 2 * 4] = b;
	m.matrix[2 + 2 * 4] = c;
	m.matrix[2 + 3 * 4] = d;
	m.matrix[3 + 2 * 4] = -1;
	return m;
}

TriMatrix TriMatrix::worldToView(const FRenderViewpoint &viewpoint)
{
	TriMatrix m = null();
	m.matrix[0 + 0 * 4] = (float)viewpoint.Sin;
	m.matrix[0 + 1 * 4] = (float)-viewpoint.Cos;
	m.matrix[1 + 2 * 4] = 1.0f;
	m.matrix[2 + 0 * 4] = (float)-viewpoint.Cos;
	m.matrix[2 + 1 * 4] = (float)-viewpoint.Sin;
	m.matrix[3 + 3 * 4] = 1.0f;
	return m * translate((float)-viewpoint.Pos.X, (float)-viewpoint.Pos.Y, (float)-viewpoint.Pos.Z);
}

TriMatrix TriMatrix::viewToClip(double focalTangent, double centerY, double YaspectMul)
{
	float near = 5.0f;
	float far = 65536.0f;
	float width = (float)(focalTangent * near);
	float top = (float)(centerY / viewheight * YaspectMul * near);
	float bottom = (float)(top - YaspectMul * near);
	return frustum(-width, width, bottom, top, near, far);
}

TriMatrix TriMatrix::operator*(const TriMatrix &mult) const
{
	TriMatrix result;
	for (int x = 0; x < 4; x++)
	{
		for (int y = 0; y < 4; y++)
		{
			result.matrix[x + y * 4] =
				matrix[0 * 4 + x] * mult.matrix[y * 4 + 0] +
				matrix[1 * 4 + x] * mult.matrix[y * 4 + 1] +
				matrix[2 * 4 + x] * mult.matrix[y * 4 + 2] +
				matrix[3 * 4 + x] * mult.matrix[y * 4 + 3];
		}
	}
	return result;
}

FVector4 TriMatrix::operator*(const FVector4 &v) const
{
#ifdef NO_SSE
	float vx = matrix[0 * 4 + 0] * v.X + matrix[1 * 4 + 0] * v.Y + matrix[2 * 4 + 0] * v.Z + matrix[3 * 4 + 0] * v.W;
	float vy = matrix[0 * 4 + 1] * v.X + matrix[1 * 4 + 1] * v.Y + matrix[2 * 4 + 1] * v.Z + matrix[3 * 4 + 1] * v.W;
	float vz = matrix[0 * 4 + 2] * v.X + matrix[1 * 4 + 2] * v.Y + matrix[2 * 4 + 2] * v.Z + matrix[3 * 4 + 2] * v.W;
	float vw = matrix[0 * 4 + 3] * v.X + matrix[1 * 4 + 3] * v.Y + matrix[2 * 4 + 3] * v.Z + matrix[3 * 4 + 3] * v.W;
	return{ vx, vy, vz, vw };
#else
	__m128 m0 = _mm_loadu_ps(matrix);
	__m128 m1 = _mm_loadu_ps(matrix + 4);
	__m128 m2 = _mm_loadu_ps(matrix + 8);
	__m128 m3 = _mm_loadu_ps(matrix + 12);
	__m128 mv = _mm_loadu_ps(&v.X);
	m0 = _mm_mul_ps(m0, _mm_shuffle_ps(mv, mv, _MM_SHUFFLE(0, 0, 0, 0)));
	m1 = _mm_mul_ps(m1, _mm_shuffle_ps(mv, mv, _MM_SHUFFLE(1, 1, 1, 1)));
	m2 = _mm_mul_ps(m2, _mm_shuffle_ps(mv, mv, _MM_SHUFFLE(2, 2, 2, 2)));
	m3 = _mm_mul_ps(m3, _mm_shuffle_ps(mv, mv, _MM_SHUFFLE(3, 3, 3, 3)));
	mv = _mm_add_ps(_mm_add_ps(_mm_add_ps(m0, m1), m2), m3);
	FVector4 sv;
	_mm_storeu_ps(&sv.X, mv);
	return sv;
#endif
}
