/*
**  Hardpoly renderer
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

#include <stdlib.h>
#include "gpu_types.h"
#include "doomtype.h"
#include <cmath>
#include "dobject.h"
#include "vm.h"
#include "serializer.h"

Mat4f Mat4f::Null()
{
	Mat4f m;
	memset(m.Matrix, 0, sizeof(m.Matrix));
	return m;
}

Mat4f Mat4f::Identity()
{
	Mat4f m = Null();
	m.Matrix[0] = 1.0f;
	m.Matrix[5] = 1.0f;
	m.Matrix[10] = 1.0f;
	m.Matrix[15] = 1.0f;
	return m;
}

Mat4f Mat4f::FromValues(float *matrix)
{
	Mat4f m;
	memcpy(m.Matrix, matrix, sizeof(m.Matrix));
	return m;
}

Mat4f Mat4f::Transpose(const Mat4f &matrix)
{
	Mat4f m;
	for (int y = 0; y < 4; y++)
		for (int x = 0; x < 4; x++)
			m.Matrix[x + y * 4] = matrix.Matrix[y + x * 4];
	return m;
}

Mat4f Mat4f::Translate(float x, float y, float z)
{
	Mat4f m = Identity();
	m.Matrix[0 + 3 * 4] = x;
	m.Matrix[1 + 3 * 4] = y;
	m.Matrix[2 + 3 * 4] = z;
	return m;
}

Mat4f Mat4f::Scale(float x, float y, float z)
{
	Mat4f m = Null();
	m.Matrix[0 + 0 * 4] = x;
	m.Matrix[1 + 1 * 4] = y;
	m.Matrix[2 + 2 * 4] = z;
	m.Matrix[3 + 3 * 4] = 1;
	return m;
}

Mat4f Mat4f::Rotate(float angle, float x, float y, float z)
{
	float c = cosf(angle);
	float s = sinf(angle);
	Mat4f m = Null();
	m.Matrix[0 + 0 * 4] = (x*x*(1.0f - c) + c);
	m.Matrix[0 + 1 * 4] = (x*y*(1.0f - c) - z*s);
	m.Matrix[0 + 2 * 4] = (x*z*(1.0f - c) + y*s);
	m.Matrix[1 + 0 * 4] = (y*x*(1.0f - c) + z*s);
	m.Matrix[1 + 1 * 4] = (y*y*(1.0f - c) + c);
	m.Matrix[1 + 2 * 4] = (y*z*(1.0f - c) - x*s);
	m.Matrix[2 + 0 * 4] = (x*z*(1.0f - c) - y*s);
	m.Matrix[2 + 1 * 4] = (y*z*(1.0f - c) + x*s);
	m.Matrix[2 + 2 * 4] = (z*z*(1.0f - c) + c);
	m.Matrix[3 + 3 * 4] = 1.0f;
	return m;
}

Mat4f Mat4f::SwapYZ()
{
	Mat4f m = Null();
	m.Matrix[0 + 0 * 4] = 1.0f;
	m.Matrix[1 + 2 * 4] = 1.0f;
	m.Matrix[2 + 1 * 4] = -1.0f;
	m.Matrix[3 + 3 * 4] = 1.0f;
	return m;
}

Mat4f Mat4f::Perspective(float fovy, float aspect, float z_near, float z_far, Handedness handedness, ClipZRange clipZ)
{
	float f = (float)(1.0 / tan(fovy * M_PI / 360.0));
	Mat4f m = Null();
	m.Matrix[0 + 0 * 4] = f / aspect;
	m.Matrix[1 + 1 * 4] = f;
	m.Matrix[2 + 2 * 4] = (z_far + z_near) / (z_near - z_far);
	m.Matrix[2 + 3 * 4] = (2.0f * z_far * z_near) / (z_near - z_far);
	m.Matrix[3 + 2 * 4] = -1.0f;

	if (handedness == Handedness::Left)
	{
		m = m * Mat4f::Scale(1.0f, 1.0f, -1.0f);
	}

	if (clipZ == ClipZRange::ZeroPositiveW)
	{
		Mat4f scale_translate = Identity();
		scale_translate.Matrix[2 + 2 * 4] = 0.5f;
		scale_translate.Matrix[2 + 3 * 4] = 0.5f;
		m = scale_translate * m;
	}

	return m;
}

Mat4f Mat4f::Frustum(float left, float right, float bottom, float top, float near, float far, Handedness handedness, ClipZRange clipZ)
{
	float a = (right + left) / (right - left);
	float b = (top + bottom) / (top - bottom);
	float c = -(far + near) / (far - near);
	float d = -(2.0f * far) / (far - near);
	Mat4f m = Null();
	m.Matrix[0 + 0 * 4] = 2.0f * near / (right - left);
	m.Matrix[1 + 1 * 4] = 2.0f * near / (top - bottom);
	m.Matrix[0 + 2 * 4] = a;
	m.Matrix[1 + 2 * 4] = b;
	m.Matrix[2 + 2 * 4] = c;
	m.Matrix[2 + 3 * 4] = d;
	m.Matrix[3 + 2 * 4] = -1;

	if (handedness == Handedness::Left)
	{
		m = m * Mat4f::Scale(1.0f, 1.0f, -1.0f);
	}

	if (clipZ == ClipZRange::ZeroPositiveW)
	{
		Mat4f scale_translate = Identity();
		scale_translate.Matrix[2 + 2 * 4] = 0.5f;
		scale_translate.Matrix[2 + 3 * 4] = 0.5f;
		m = scale_translate * m;
	}

	return m;
}

Mat4f Mat4f::operator*(const Mat4f &mult) const
{
	Mat4f result;
	for (int x = 0; x < 4; x++)
	{
		for (int y = 0; y < 4; y++)
		{
			result.Matrix[x + y * 4] =
				Matrix[0 * 4 + x] * mult.Matrix[y * 4 + 0] +
				Matrix[1 * 4 + x] * mult.Matrix[y * 4 + 1] +
				Matrix[2 * 4 + x] * mult.Matrix[y * 4 + 2] +
				Matrix[3 * 4 + x] * mult.Matrix[y * 4 + 3];
		}
	}
	return result;
}

Vec4f Mat4f::operator*(const Vec4f &v) const
{
	Vec4f result;
#ifdef NO_SSE
	result.X = Matrix[0 * 4 + 0] * v.X + Matrix[1 * 4 + 0] * v.Y + Matrix[2 * 4 + 0] * v.Z + Matrix[3 * 4 + 0] * v.W;
	result.Y = Matrix[0 * 4 + 1] * v.X + Matrix[1 * 4 + 1] * v.Y + Matrix[2 * 4 + 1] * v.Z + Matrix[3 * 4 + 1] * v.W;
	result.Z = Matrix[0 * 4 + 2] * v.X + Matrix[1 * 4 + 2] * v.Y + Matrix[2 * 4 + 2] * v.Z + Matrix[3 * 4 + 2] * v.W;
	result.W = Matrix[0 * 4 + 3] * v.X + Matrix[1 * 4 + 3] * v.Y + Matrix[2 * 4 + 3] * v.Z + Matrix[3 * 4 + 3] * v.W;
#else
	__m128 m0 = _mm_loadu_ps(Matrix);
	__m128 m1 = _mm_loadu_ps(Matrix + 4);
	__m128 m2 = _mm_loadu_ps(Matrix + 8);
	__m128 m3 = _mm_loadu_ps(Matrix + 12);
	__m128 mv = _mm_loadu_ps(&v.X);
	m0 = _mm_mul_ps(m0, _mm_shuffle_ps(mv, mv, _MM_SHUFFLE(0, 0, 0, 0)));
	m1 = _mm_mul_ps(m1, _mm_shuffle_ps(mv, mv, _MM_SHUFFLE(1, 1, 1, 1)));
	m2 = _mm_mul_ps(m2, _mm_shuffle_ps(mv, mv, _MM_SHUFFLE(2, 2, 2, 2)));
	m3 = _mm_mul_ps(m3, _mm_shuffle_ps(mv, mv, _MM_SHUFFLE(3, 3, 3, 3)));
	mv = _mm_add_ps(_mm_add_ps(_mm_add_ps(m0, m1), m2), m3);
	Vec4f sv;
	_mm_storeu_ps(&result.X, mv);
#endif
	return result;
}

/////////////////////////////////////////////////////////////////////////////

Mat3f::Mat3f(const Mat4f &matrix)
{
	for (int y = 0; y < 3; y++)
		for (int x = 0; x < 3; x++)
			Matrix[x + y * 3] = matrix.Matrix[x + y * 4];
}

Mat3f Mat3f::Null()
{
	Mat3f m;
	memset(m.Matrix, 0, sizeof(m.Matrix));
	return m;
}

Mat3f Mat3f::Identity()
{
	Mat3f m = Null();
	m.Matrix[0] = 1.0f;
	m.Matrix[4] = 1.0f;
	m.Matrix[8] = 1.0f;
	return m;
}

Mat3f Mat3f::FromValues(float *matrix)
{
	Mat3f m;
	memcpy(m.Matrix, matrix, sizeof(m.Matrix));
	return m;
}

Mat3f Mat3f::Transpose(const Mat3f &matrix)
{
	Mat3f m;
	for (int y = 0; y < 3; y++)
		for (int x = 0; x < 3; x++)
			m.Matrix[x + y * 3] = matrix.Matrix[y + x * 3];
	return m;
}

Mat3f Mat3f::operator*(const Mat3f &mult) const
{
	Mat3f result;
	for (int x = 0; x < 3; x++)
	{
		for (int y = 0; y < 3; y++)
		{
			result.Matrix[x + y * 3] =
				Matrix[0 * 3 + x] * mult.Matrix[y * 3 + 0] +
				Matrix[1 * 3 + x] * mult.Matrix[y * 3 + 1] +
				Matrix[2 * 3 + x] * mult.Matrix[y * 3 + 2];
		}
	}
	return result;
}

Vec3f Mat3f::operator*(const Vec3f &v) const
{
	Vec3f result;
	result.X = Matrix[0 * 3 + 0] * v.X + Matrix[1 * 3 + 0] * v.Y + Matrix[2 * 3 + 0] * v.Z;
	result.Y = Matrix[0 * 3 + 1] * v.X + Matrix[1 * 3 + 1] * v.Y + Matrix[2 * 3 + 1] * v.Z;
	result.Z = Matrix[0 * 3 + 2] * v.X + Matrix[1 * 3 + 2] * v.Y + Matrix[2 * 3 + 2] * v.Z;
	return result;
}

//=============================================================================
//
// ZScript Export
//
//=============================================================================
IMPLEMENT_CLASS(DMatrix4, false, false)

void DMatrix4::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	arc.Array("values", Matrix, 16);
}
/////////////////////////////////////////////////////////////////////////////

DEFINE_ACTION_FUNCTION(DMatrix4, Empty)
{
	PARAM_PROLOGUE;
	ACTION_RETURN_OBJECT(Create<DMatrix4> (Mat4f::Null ()));
}

DEFINE_ACTION_FUNCTION(DMatrix4, Identity)
{
	PARAM_PROLOGUE;
	ACTION_RETURN_OBJECT(Create<DMatrix4> (Mat4f::Identity ()));
}

DEFINE_ACTION_FUNCTION(DMatrix4, AngleAxis)
{
	PARAM_PROLOGUE;
	PARAM_ANGLE(angle);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(z);

	ACTION_RETURN_OBJECT(Create<DMatrix4> (Mat4f::Rotate(angle.Radians(), x, y, z)));
}

DEFINE_ACTION_FUNCTION(DMatrix4, Transpose)
{
	PARAM_SELF_PROLOGUE(DMatrix4);
	ACTION_RETURN_OBJECT(Create<DMatrix4> (Mat4f::Transpose (*self)));
}

DEFINE_ACTION_FUNCTION(DMatrix4, Translate)
{
	PARAM_SELF_PROLOGUE(DMatrix4);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(z);
	ACTION_RETURN_OBJECT(Create<DMatrix4> (*self * Mat4f::Translate (x, y, z)));
}

DEFINE_ACTION_FUNCTION(DMatrix4, Scale)
{
	PARAM_SELF_PROLOGUE(DMatrix4);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(z);
	ACTION_RETURN_OBJECT(Create<DMatrix4> (*self * Mat4f::Scale (x, y, z)));
}

DEFINE_ACTION_FUNCTION(DMatrix4, Rotate)
{
	PARAM_SELF_PROLOGUE(DMatrix4);
	PARAM_ANGLE(angle);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(z);

	ACTION_RETURN_OBJECT(Create<DMatrix4> (*self * Mat4f::Rotate (angle.Radians(), x, y, z)));
}

DEFINE_ACTION_FUNCTION(DMatrix4, SwapYZ)
{
	PARAM_SELF_PROLOGUE(DMatrix4);
	ACTION_RETURN_OBJECT(Create<DMatrix4> (*self * Mat4f::SwapYZ ()));
}

DEFINE_ACTION_FUNCTION(DMatrix4, Perspective)
{
	PARAM_SELF_PROLOGUE(DMatrix4);
	PARAM_FLOAT(fovy);
	PARAM_FLOAT(aspect);
	PARAM_FLOAT(near);
	PARAM_FLOAT(far);
	ACTION_RETURN_OBJECT(Create<DMatrix4> (*self * Mat4f::Perspective (fovy, aspect, near, far, Handedness::Right, ClipZRange::NegativePositiveW)));
}

DEFINE_ACTION_FUNCTION(DMatrix4, Frustum)
{
	PARAM_SELF_PROLOGUE(DMatrix4);
	PARAM_FLOAT(left);
	PARAM_FLOAT(right);
	PARAM_FLOAT(bottom);
	PARAM_FLOAT(top);
	PARAM_FLOAT(near);
	PARAM_FLOAT(far);
	ACTION_RETURN_OBJECT(Create<DMatrix4> (*self * Mat4f::Frustum (left, right, bottom, top, near, far, Handedness::Right, ClipZRange::NegativePositiveW)));
}

DEFINE_ACTION_FUNCTION(DMatrix4, Multiply)
{
	PARAM_SELF_PROLOGUE(DMatrix4);
	PARAM_OBJECT(matrix, DMatrix4);
	ACTION_RETURN_OBJECT(Create<DMatrix4> (*self * *matrix));
}

DEFINE_ACTION_FUNCTION(DMatrix4, MultiplyPoint)
{
	PARAM_SELF_PROLOGUE(DMatrix4);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(z);
	Vec4f result = *self * Vec4f(x, y, z, 1);
	ACTION_RETURN_VEC3(DVector3 (result.X , result.Y, result.Z) / result.W);
}

DEFINE_ACTION_FUNCTION(DMatrix4, MultiplyVector)
{
	PARAM_SELF_PROLOGUE(DMatrix4);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(z);
	Vec4f result = *self * Vec4f(x, y, z, 0);
	ACTION_RETURN_VEC3(DVector3 (result.X, result.Y, result.Z) / result.W);
}

DEFINE_ACTION_FUNCTION(DMatrix4, Set)
{
	PARAM_SELF_PROLOGUE(DMatrix4);
	PARAM_INT(row);
	PARAM_INT(column);
	PARAM_FLOAT(value);
	self->Matrix[row + column * 4] = value;
	return 0;
}

DEFINE_ACTION_FUNCTION(DMatrix4, Get)
{
	PARAM_SELF_PROLOGUE(DMatrix4);
	PARAM_INT(row);
	PARAM_INT(column);
	ACTION_RETURN_FLOAT(self->Matrix[row + column * 4]);
}