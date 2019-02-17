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

#pragma once

#include <memory>

template<typename Type> class Vec4;
template<typename Type> class Vec3;

template<typename Type>
class Vec2
{
public:
	Vec2() : X(Type(0)), Y(Type(0)) { }
	Vec2(float v) : X(v), Y(v) { }
	Vec2(float x, float y) : X(x), Y(y) { }

	float X, Y;

	bool operator==(const Vec2 &other) const { return X == other.X && Y == other.Y; }
	bool operator!=(const Vec2 &other) const { return X != other.X || Y != other.Y; }
};

template<typename Type>
class Vec3
{
public:
	Vec3() : X(Type(0)), Y(Type(0)), Z(Type(0)) { }
	Vec3(float v) : X(v), Y(v), Z(v) { }
	Vec3(float x, float y, float z) : X(x), Y(y), Z(z) { }
	Vec3(const Vec2<Type> &v, float z) : X(v.X), Y(v.Y), Z(z) { }

	float X, Y, Z;

	bool operator==(const Vec3 &other) const { return X == other.X && Y == other.Y && Z == other.Z; }
	bool operator!=(const Vec3 &other) const { return X != other.X || Y != other.Y || Z != other.Z; }
};

template<typename Type>
class Vec4
{
public:
	Vec4() : X(Type(0)), Y(Type(0)), Z(Type(0)), W(Type(0)) { }
	Vec4(Type v) : X(v), Y(v), Z(v), W(v) { }
	Vec4(Type x, Type y, Type z, Type w = 1.0f) : X(x), Y(y), Z(z), W(w) { }

	Vec4(const Vec2<Type> &v, Type z, Type w) : X(v.X), Y(v.Y), Z(z), W(w) { }
	Vec4(const Vec3<Type> &v, Type w) : X(v.X), Y(v.Y), Z(v.Z), W(w) { }

	Type X, Y, Z, W;

	bool operator==(const Vec4 &other) const { return X == other.X && Y == other.Y && Z == other.Z && W == other.W; }
	bool operator!=(const Vec4 &other) const { return X != other.X || Y != other.Y || Z != other.Z || W != other.W; }
};

typedef Vec4<double> Vec4d;
typedef Vec3<double> Vec3d;
typedef Vec2<double> Vec2d;
typedef Vec4<float> Vec4f;
typedef Vec3<float> Vec3f;
typedef Vec2<float> Vec2f;
typedef Vec4<int32_t> Vec4i;
typedef Vec3<int32_t> Vec3i;
typedef Vec2<int32_t> Vec2i;
typedef Vec4<int16_t> Vec4s;
typedef Vec3<int16_t> Vec3s;
typedef Vec2<int16_t> Vec2s;
typedef Vec4<int8_t> Vec4b;
typedef Vec3<int8_t> Vec3b;
typedef Vec2<int8_t> Vec2b;
typedef Vec4<uint32_t> Vec4ui;
typedef Vec3<uint32_t> Vec3ui;
typedef Vec2<uint32_t> Vec2ui;
typedef Vec4<uint16_t> Vec4us;
typedef Vec3<uint16_t> Vec3us;
typedef Vec2<uint16_t> Vec2us;
typedef Vec4<uint8_t> Vec4ub;
typedef Vec3<uint8_t> Vec3ub;
typedef Vec2<uint8_t> Vec2ub;

enum class Handedness
{
	Left,
	Right
};

enum class ClipZRange
{
	NegativePositiveW, // OpenGL, -wclip <= zclip <= wclip
	ZeroPositiveW      // Direct3D, 0 <= zclip <= wclip
};

class Mat4f
{
public:
	static Mat4f Null();
	static Mat4f Identity();
	static Mat4f FromValues(float *matrix);
	static Mat4f Transpose(const Mat4f &matrix);
	static Mat4f Translate(float x, float y, float z);
	static Mat4f Scale(float x, float y, float z);
	static Mat4f Rotate(float angle, float x, float y, float z);
	static Mat4f SwapYZ();
	static Mat4f Perspective(float fovy, float aspect, float near, float far, Handedness handedness, ClipZRange clipZ);
	static Mat4f Frustum(float left, float right, float bottom, float top, float near, float far, Handedness handedness, ClipZRange clipZ);

	Vec4f operator*(const Vec4f &v) const;
	Mat4f operator*(const Mat4f &m) const;

	float Matrix[16];
};

class Mat3f
{
public:
	Mat3f() { }
	Mat3f(const Mat4f &matrix);

	static Mat3f Null();
	static Mat3f Identity();
	static Mat3f FromValues(float *matrix);
	static Mat3f Transpose(const Mat3f &matrix);

	Vec3f operator*(const Vec3f &v) const;
	Mat3f operator*(const Mat3f &m) const;

	float Matrix[9];
};
