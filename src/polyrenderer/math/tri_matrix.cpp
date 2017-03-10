/*
**  Triangle drawers
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

TriMatrix TriMatrix::worldToView()
{
	TriMatrix m = null();
	m.matrix[0 + 0 * 4] = (float)ViewSin;
	m.matrix[0 + 1 * 4] = (float)-ViewCos;
	m.matrix[1 + 2 * 4] = 1.0f;
	m.matrix[2 + 0 * 4] = (float)-ViewCos;
	m.matrix[2 + 1 * 4] = (float)-ViewSin;
	m.matrix[3 + 3 * 4] = 1.0f;
	return m * translate((float)-ViewPos.X, (float)-ViewPos.Y, (float)-ViewPos.Z);
}

TriMatrix TriMatrix::viewToClip()
{
	auto viewport = swrenderer::RenderViewport::Instance();
	float near = 5.0f;
	float far = 65536.0f;
	float width = (float)(FocalTangent * near);
	float top = (float)(viewport->CenterY / viewport->InvZtoScale * near);
	float bottom = (float)(top - viewheight / viewport->InvZtoScale * near);
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

ShadedTriVertex TriMatrix::operator*(TriVertex v) const
{
	float vx = matrix[0 * 4 + 0] * v.x + matrix[1 * 4 + 0] * v.y + matrix[2 * 4 + 0] * v.z + matrix[3 * 4 + 0] * v.w;
	float vy = matrix[0 * 4 + 1] * v.x + matrix[1 * 4 + 1] * v.y + matrix[2 * 4 + 1] * v.z + matrix[3 * 4 + 1] * v.w;
	float vz = matrix[0 * 4 + 2] * v.x + matrix[1 * 4 + 2] * v.y + matrix[2 * 4 + 2] * v.z + matrix[3 * 4 + 2] * v.w;
	float vw = matrix[0 * 4 + 3] * v.x + matrix[1 * 4 + 3] * v.y + matrix[2 * 4 + 3] * v.z + matrix[3 * 4 + 3] * v.w;
	ShadedTriVertex sv;
	sv.x = vx;
	sv.y = vy;
	sv.z = vz;
	sv.w = vw;
	for (int i = 0; i < TriVertex::NumVarying; i++)
		sv.varying[i] = v.varying[i];
	return sv;
}
