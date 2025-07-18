/*
** vectors.h
** Vector math routines.
**
**---------------------------------------------------------------------------
** Copyright 2005-2007 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
** Since C++ doesn't let me add completely new operators, the following two
** are overloaded for vectors:
**
**    |  dot product
**    ^  cross product
*/

#ifndef VECTORS_H
#define VECTORS_H

#include <cstddef>
#include <math.h>
#include <float.h>
#include <string.h>
#include <algorithm>

#include "common/utility/basics.h"

// this is needed to properly normalize angles. We cannot do that with compiler provided conversions because they differ too much
#include "xs_Float.h"

// make this a local inline function to avoid any dependencies on other headers and not pollute the global namespace
namespace pi
{
	inline constexpr double pi() { return 3.14159265358979323846; }
	inline constexpr float pif() { return 3.14159265358979323846f; }
}

// optionally use reliable math routines if reproducability across hardware is important, but let this still compile without them.
#if __has_include("math/cmath.h")
#include "math/cmath.h"
#else
inline double g_cosdeg(double v) { return cos(v * (pi::pi() / 180.)); }
inline double g_sindeg(double v) { return sin(v * (pi::pi() / 180.)); }
inline double g_cos(double v) { return cos(v); }
inline double g_sin(double v) { return sin(v); }
inline double g_sqrt(double v) { return sqrt(v); }
inline double g_atan2(double v, double w) { return atan2(v, w); }
#endif


#define EQUAL_EPSILON (1/65536.)

template<class vec_t> struct TVector3;
template<class vec_t> struct TRotator;
template<class vec_t> struct TAngle;

template<class vec_t>
struct TVector2
{
	vec_t X, Y;

	constexpr TVector2() = default;

	constexpr TVector2 (vec_t a, vec_t b)
		: X(a), Y(b)
	{
	}

	constexpr TVector2(const TVector2 &other) = default;

	constexpr TVector2(vec_t *o)
		: X(o[0]), Y(o[1])
	{
	}

	template<typename U>
	constexpr explicit operator TVector2<U> () const noexcept {
		return TVector2<U>(static_cast<U>(X), static_cast<U>(Y));
	}

	constexpr void Zero()
	{
		Y = X = 0;
	}

	constexpr bool isZero() const
	{
		return X == 0 && Y == 0;
	}

	constexpr TVector2 &operator= (const TVector2 &other) = default;

	// Access X and Y as an array
	constexpr vec_t &operator[] (int index)
	{
		return index == 0 ? X : Y;
	}

	constexpr const vec_t &operator[] (int index) const
	{
		return index == 0 ? X : Y;
	}

	// Test for equality
	constexpr bool operator== (const TVector2 &other) const
	{
		return X == other.X && Y == other.Y;
	}

	// Test for inequality
	constexpr bool operator!= (const TVector2 &other) const
	{
		return X != other.X || Y != other.Y;
	}

	// Test for approximate equality
	bool ApproximatelyEquals (const TVector2 &other) const
	{
		return fabs(X - other.X) < EQUAL_EPSILON && fabs(Y - other.Y) < EQUAL_EPSILON;
	}

	// Test for approximate inequality
	bool DoesNotApproximatelyEqual (const TVector2 &other) const
	{
		return fabs(X - other.X) >= EQUAL_EPSILON || fabs(Y - other.Y) >= EQUAL_EPSILON;
	}

	// Unary negation
	constexpr TVector2 operator- () const
	{
		return TVector2(-X, -Y);
	}

	// Scalar addition
#if 0
	constexpr TVector2 &operator+= (double scalar)
	{
		X += scalar, Y += scalar;
		return *this;
	}
#endif

	constexpr friend TVector2 operator+ (const TVector2 &v, vec_t scalar)
	{
		return TVector2(v.X + scalar, v.Y + scalar);
	}

	constexpr friend TVector2 operator+ (vec_t scalar, const TVector2 &v)
	{
		return TVector2(v.X + scalar, v.Y + scalar);
	}

	// Scalar subtraction
	constexpr TVector2 &operator-= (vec_t scalar)
	{
		X -= scalar, Y -= scalar;
		return *this;
	}

	constexpr TVector2 operator- (vec_t scalar) const
	{
		return TVector2(X - scalar, Y - scalar);
	}

	// Scalar multiplication
	constexpr TVector2 &operator*= (vec_t scalar)
	{
		X *= scalar, Y *= scalar;
		return *this;
	}

	constexpr friend TVector2 operator* (const TVector2 &v, vec_t scalar)
	{
		return TVector2(v.X * scalar, v.Y * scalar);
	}

	constexpr friend TVector2 operator* (vec_t scalar, const TVector2 &v)
	{
		return TVector2(v.X * scalar, v.Y * scalar);
	}

	// Scalar division
	constexpr TVector2 &operator/= (vec_t scalar)
	{
		scalar = 1 / scalar, X *= scalar, Y *= scalar;
		return *this;
	}

	constexpr TVector2 operator/ (vec_t scalar) const
	{
		scalar = 1 / scalar;
		return TVector2(X * scalar, Y * scalar);
	}

	// Vector addition
	constexpr TVector2 &operator+= (const TVector2 &other)
	{
		X += other.X, Y += other.Y;
		return *this;
	}

	constexpr TVector2 operator+ (const TVector2 &other) const
	{
		return TVector2(X + other.X, Y + other.Y);
	}

	// Vector subtraction
	constexpr TVector2 &operator-= (const TVector2 &other)
	{
		X -= other.X, Y -= other.Y;
		return *this;
	}

	constexpr TVector2 operator- (const TVector2 &other) const
	{
		return TVector2(X - other.X, Y - other.Y);
	}

	// Vector length
	vec_t Length() const
	{
		return (vec_t)g_sqrt (LengthSquared());
	}

	constexpr vec_t LengthSquared() const
	{
		return X*X + Y*Y;
	}
	
	double Sum() const
	{
		return abs(X) + abs(Y);
	}


	// Return a unit vector facing the same direction as this one
	TVector2 Unit() const
	{
		vec_t len = Length();
		if (len != 0) len = 1 / len;
		return *this * len;
	}

	// Scales this vector into a unit vector. Returns the old length
	vec_t MakeUnit()
	{
		vec_t len, ilen;
		len = ilen = Length();
		if (ilen != 0) ilen = 1 / ilen;
		*this *= ilen;
		return len;
	}

	// Resizes this vector to be the specified length (if it is not 0)
	TVector2 &MakeResize(double len)
	{
		double scale = len / Length();
		X = vec_t(X * scale);
		Y = vec_t(Y * scale);
		return *this;
	}

	TVector2 Resized(double len) const
	{
		double vlen = Length();
		if (vlen != 0.)
		{
			double scale = len / vlen;
			return{ vec_t(X * scale), vec_t(Y * scale) };
		}
		else
		{
			return *this;
		}
	}

	// Dot product
	constexpr vec_t operator | (const TVector2 &other) const
	{
		return X*other.X + Y*other.Y;
	}

	constexpr vec_t dot(const TVector2 &other) const
	{
		return X*other.X + Y*other.Y;
	}

	// Returns the angle that the ray (0,0)-(X,Y) faces
	TAngle<vec_t> Angle() const;

	// Returns a rotated vector. angle is in degrees.
	TVector2 Rotated (double angle) const
	{
		double cosval = g_cosdeg (angle);
		double sinval = g_sindeg (angle);
		return TVector2(X*cosval - Y*sinval, Y*cosval + X*sinval);
	}

	// Returns a rotated vector. angle is in degrees.
	template<class T>
	TVector2 Rotated(TAngle<T> angle) const
	{
		double cosval = angle.Cos();
		double sinval = angle.Sin();
		return TVector2(X*cosval - Y*sinval, Y*cosval + X*sinval);
	}

	// Returns a rotated vector. angle is in degrees.
	constexpr TVector2 Rotated(const double cosval, const double sinval) const
	{
		return TVector2(X*cosval - Y*sinval, Y*cosval + X*sinval);
	}

	// Returns a vector rotated 90 degrees clockwise.
	constexpr TVector2 Rotated90CW() const
	{
		return TVector2(Y, -X);
	}

	// Returns a vector rotated 90 degrees counterclockwise.
	constexpr TVector2 Rotated90CCW() const
	{
		return TVector2(-Y, X);
	}
};

template<class vec_t>
struct TVector3
{
	typedef TVector2<vec_t> Vector2;
	// this does not compile - should be true on all relevant hardware.
	//static_assert(myoffsetof(TVector3, X) == myoffsetof(Vector2, X) && myoffsetof(TVector3, Y) == myoffsetof(Vector2, Y), "TVector2 and TVector3 are not aligned");

	vec_t X, Y, Z;

	constexpr TVector3() = default;

	constexpr TVector3 (vec_t a, vec_t b, vec_t c)
		: X(a), Y(b), Z(c)
	{
	}

	constexpr TVector3(vec_t *o)
		: X(o[0]), Y(o[1]), Z(o[2])
	{
	}

	constexpr TVector3(std::nullptr_t nul) = delete;

	constexpr TVector3(const TVector3 &other) = default;

	constexpr TVector3 (const Vector2 &xy, vec_t z)
		: X(xy.X), Y(xy.Y), Z(z)
	{
	}

	TVector3 (const TRotator<vec_t> &rot);
	
	template<typename U>
	constexpr explicit operator TVector3<U> () const noexcept {
		return TVector3<U>(static_cast<U>(X), static_cast<U>(Y), static_cast<U>(Z));
	}

	constexpr void Zero()
	{
		Z = Y = X = 0;
	}

	constexpr bool isZero() const
	{
		return X == 0 && Y == 0 && Z == 0;
	}

	constexpr TVector3 plusZ(double z) const
	{
		return { X, Y, Z + z };
	}

	constexpr TVector3 &operator= (const TVector3 &other) = default;

	// Access X and Y and Z as an array
	constexpr vec_t &operator[] (int index)
	{
		return index == 0 ? X : index == 1 ? Y : Z;
	}

	constexpr const vec_t &operator[] (int index) const
	{
		return index == 0 ? X : index == 1 ? Y : Z;
	}

	// Test for equality
	constexpr bool operator== (const TVector3 &other) const
	{
		return X == other.X && Y == other.Y && Z == other.Z;
	}

	// Test for inequality
	constexpr bool operator!= (const TVector3 &other) const
	{
		return X != other.X || Y != other.Y || Z != other.Z;
	}

	// Test for approximate equality
	bool ApproximatelyEquals (const TVector3 &other) const
	{
		return fabs(X - other.X) < EQUAL_EPSILON && fabs(Y - other.Y) < EQUAL_EPSILON && fabs(Z - other.Z) < EQUAL_EPSILON;
	}

	// Test for approximate inequality
	bool DoesNotApproximatelyEqual (const TVector3 &other) const
	{
		return fabs(X - other.X) >= EQUAL_EPSILON || fabs(Y - other.Y) >= EQUAL_EPSILON || fabs(Z - other.Z) >= EQUAL_EPSILON;
	}

	// Unary negation
	constexpr TVector3 operator- () const
	{
		return TVector3(-X, -Y, -Z);
	}

	// Scalar addition
#if 0
	constexpr TVector3 &operator+= (vec_t scalar)
	{
		X += scalar, Y += scalar, Z += scalar;
		return *this;
	}
#endif

	constexpr friend TVector3 operator+ (const TVector3 &v, vec_t scalar)
	{
		return TVector3(v.X + scalar, v.Y + scalar, v.Z + scalar);
	}

	constexpr friend TVector3 operator+ (vec_t scalar, const TVector3 &v)
	{
		return TVector3(v.X + scalar, v.Y + scalar, v.Z + scalar);
	}

	// Scalar subtraction
	constexpr TVector3 &operator-= (vec_t scalar)
	{
		X -= scalar, Y -= scalar, Z -= scalar;
		return *this;
	}

	constexpr TVector3 operator- (vec_t scalar) const
	{
		return TVector3(X - scalar, Y - scalar, Z - scalar);
	}

	// Scalar multiplication
	constexpr TVector3 &operator*= (vec_t scalar)
	{
		X = vec_t(X *scalar), Y = vec_t(Y * scalar), Z = vec_t(Z * scalar);
		return *this;
	}

	constexpr friend TVector3 operator* (const TVector3 &v, vec_t scalar)
	{
		return TVector3(v.X * scalar, v.Y * scalar, v.Z * scalar);
	}

	constexpr friend TVector3 operator* (vec_t scalar, const TVector3 &v)
	{
		return TVector3(v.X * scalar, v.Y * scalar, v.Z * scalar);
	}

	// Scalar division
	constexpr TVector3 &operator/= (vec_t scalar)
	{
		scalar = 1 / scalar, X = vec_t(X * scalar), Y = vec_t(Y * scalar), Z = vec_t(Z * scalar);
		return *this;
	}

	constexpr TVector3 operator/ (vec_t scalar) const
	{
		scalar = 1 / scalar;
		return TVector3(X * scalar, Y * scalar, Z * scalar);
	}

	// Vector addition
	constexpr TVector3 &operator+= (const TVector3 &other)
	{
		X += other.X, Y += other.Y, Z += other.Z;
		return *this;
	}

	constexpr TVector3 operator+ (const TVector3 &other) const
	{
		return TVector3(X + other.X, Y + other.Y, Z + other.Z);
	}

	// Vector subtraction
	constexpr TVector3 &operator-= (const TVector3 &other)
	{
		X -= other.X, Y -= other.Y, Z -= other.Z;
		return *this;
	}

	constexpr TVector3 operator- (const TVector3 &other) const
	{
		return TVector3(X - other.X, Y - other.Y, Z - other.Z);
	}

	// Add a 2D vector to this 3D vector, leaving Z unchanged.
	constexpr TVector3 &operator+= (const Vector2 &other)
	{
		X += other.X, Y += other.Y;
		return *this;
	}

	// Subtract a 2D vector from this 3D vector, leaving Z unchanged.
	constexpr TVector3 &operator-= (const Vector2 &other)
	{
		X -= other.X, Y -= other.Y;
		return *this;
	}

	constexpr Vector2 XY() const
	{
		return Vector2(X, Y);
	}

	// Add a 3D vector and a 2D vector.
	constexpr friend TVector3 operator+ (const TVector3 &v3, const Vector2 &v2)
	{
		return TVector3(v3.X + v2.X, v3.Y + v2.Y, v3.Z);
	}

	constexpr friend TVector3 operator- (const TVector3 &v3, const Vector2 &v2)
	{
		return TVector3(v3.X - v2.X, v3.Y - v2.Y, v3.Z);
	}

	constexpr friend Vector2 operator+ (const Vector2 &v2, const TVector3 &v3)
	{
		return Vector2(v2.X + v3.X, v2.Y + v3.Y);
	}

	// Subtract a 3D vector and a 2D vector.
	// Discards the Z component of the 3D vector and returns a 2D vector.
	constexpr friend Vector2 operator- (const TVector2<vec_t> &v2, const TVector3 &v3)
	{
		return Vector2(v2.X - v3.X, v2.Y - v3.Y);
	}

	void GetRightUp(TVector3 &right, TVector3 &up)
	{
		TVector3 n(X, Y, Z);
		TVector3 fn((vec_t)fabs(n.X), (vec_t)fabs(n.Y), (vec_t)fabs(n.Z));
		int major = 0;

		if (fn[1] > fn[major]) major = 1;
		if (fn[2] > fn[major]) major = 2;

		// build right vector by hand
		if (fabs(fn[0] - 1.0f) < FLT_EPSILON || fabs(fn[1] - 1.0f) < FLT_EPSILON || fabs(fn[2] - 1.0f) < FLT_EPSILON)
		{
			if (major == 0 && n[0] > 0.f)
			{
				right = { 0.f, 0.f, -1.f };
			}
			else if (major == 0)
			{
				right = { 0.f, 0.f, 1.f };
			}
			else if (major == 1 || (major == 2 && n[2] > 0.f))
			{
				right = { 1.f, 0.f, 0.f };
			}
			// Unconditional to ease static analysis
			else // major == 2 && n[2] <= 0.0f		
			{
				right = { -1.f, 0.f, 0.f };
			}
		}
		else
		{
			static TVector3 axis[3] =
			{
				{ 1.0f, 0.0f, 0.0f },
				{ 0.0f, 1.0f, 0.0f },
				{ 0.0f, 0.0f, 1.0f }
			};

			right = axis[major] ^ n;
		}

		up = n ^right;
		right.MakeUnit();
		up.MakeUnit();
	}


	// Returns the angle (in radians) that the ray (0,0)-(X,Y) faces
	TAngle<vec_t> Angle() const;
	TAngle<vec_t> Pitch() const;

	// Vector length
	double Length() const
	{
		return g_sqrt (LengthSquared());
	}

	constexpr double LengthSquared() const
	{
		return X*X + Y*Y + Z*Z;
	}
	
	double Sum() const
	{
		return abs(X) + abs(Y) + abs(Z);
	}


	// Return a unit vector facing the same direction as this one
	TVector3 Unit() const
	{
		double len = Length();
		if (len != 0) len = 1 / len;
		return *this * (vec_t)len;
	}

	// Scales this vector into a unit vector
	void MakeUnit()
	{
		double len = Length();
		if (len != 0) len = 1 / len;
		*this *= (vec_t)len;
	}

	// Resizes this vector to be the specified length (if it is not 0)
	TVector3 &MakeResize(double len)
	{
		double vlen = Length();
		if (vlen != 0.)
		{
			double scale = len / vlen;
			X = vec_t(X * scale);
			Y = vec_t(Y * scale);
			Z = vec_t(Z * scale);
		}
		return *this;
	}

	TVector3 Resized(double len) const
	{
		double vlen = Length();
		if (vlen != 0.)
		{
			double scale = len / vlen;
			return{ vec_t(X * scale), vec_t(Y * scale), vec_t(Z * scale) };
		}
		else
		{
			return *this;
		}
	}

	// Dot product
	constexpr vec_t operator | (const TVector3 &other) const
	{
		return X*other.X + Y*other.Y + Z*other.Z;
	}

	constexpr vec_t dot (const TVector3& other) const
	{
		return X * other.X + Y * other.Y + Z * other.Z;
	}

	// Cross product
	constexpr TVector3 operator ^ (const TVector3 &other) const
	{
		return TVector3(Y*other.Z - Z*other.Y,
					   Z*other.X - X*other.Z,
					   X*other.Y - Y*other.X);
	}

	constexpr TVector3 &operator ^= (const TVector3 &other)
	{
		*this = *this ^ other;
		return *this;
	}

	constexpr TVector3 ScaleXYZ (const TVector3 &scaling)
	{
		return TVector3(X * scaling.X, Y * scaling.Y, Z * scaling.Z);
	}
};

template<class vec_t>
struct TVector4
{
	typedef TVector2<vec_t> Vector2;
	typedef TVector3<vec_t> Vector3;

	vec_t X, Y, Z, W;

	constexpr TVector4() = default;

	constexpr TVector4(vec_t a, vec_t b, vec_t c, vec_t d)
		: X(a), Y(b), Z(c), W(d)
	{
	}

	constexpr TVector4(vec_t *o)
		: X(o[0]), Y(o[1]), Z(o[2]), W(o[3])
	{
	}

	constexpr TVector4(const TVector4 &other) = default;

	constexpr TVector4(const Vector3 &xyz, vec_t w)
		: X(xyz.X), Y(xyz.Y), Z(xyz.Z), W(w)
	{
	}

	constexpr TVector4(const vec_t v[4])
		: TVector4(v[0], v[1], v[2], v[3])
	{
	}

	template<typename U>
	constexpr explicit operator TVector4<U> () const noexcept {
		return TVector4<U>(static_cast<U>(X), static_cast<U>(Y), static_cast<U>(Z), static_cast<U>(W));
	}

	constexpr void Zero()
	{
		Z = Y = X = W = 0;
	}

	constexpr bool isZero() const
	{
		return X == 0 && Y == 0 && Z == 0 && W == 0;
	}

	constexpr TVector4 &operator= (const TVector4 &other) = default;

	// Access X and Y and Z as an array
	constexpr vec_t &operator[] (int index)
	{
		return (&X)[index];
	}

	constexpr const vec_t &operator[] (int index) const
	{
		return (&X)[index];
	}

	// Test for equality
	constexpr bool operator== (const TVector4 &other) const
	{
		return X == other.X && Y == other.Y && Z == other.Z && W == other.W;
	}

	// Test for inequality
	constexpr bool operator!= (const TVector4 &other) const
	{
		return X != other.X || Y != other.Y || Z != other.Z || W != other.W;
	}

	// returns the XY fields as a 2D-vector.
	constexpr Vector2 XY() const
	{
		return Vector2(X, Y);
	}

	constexpr Vector3 XYZ() const
	{
		return Vector3(X, Y, Z);
	}

	// Test for approximate equality
	bool ApproximatelyEquals(const TVector4 &other) const
	{
		return fabs(X - other.X) < EQUAL_EPSILON && fabs(Y - other.Y) < EQUAL_EPSILON && fabs(Z - other.Z) < EQUAL_EPSILON && fabs(W - other.W) < EQUAL_EPSILON;
	}

	// Test for approximate inequality
	bool DoesNotApproximatelyEqual(const TVector4 &other) const
	{
		return fabs(X - other.X) >= EQUAL_EPSILON || fabs(Y - other.Y) >= EQUAL_EPSILON || fabs(Z - other.Z) >= EQUAL_EPSILON || fabs(W - other.W) >= EQUAL_EPSILON;
	}

	// Unary negation
	constexpr TVector4 operator- () const
	{
		return TVector4(-X, -Y, -Z, -W);
	}

	// Scalar addition
	constexpr TVector4 &operator+= (vec_t scalar)
	{
		X += scalar, Y += scalar, Z += scalar; W += scalar;
		return *this;
	}

	constexpr friend TVector4 operator+ (const TVector4 &v, vec_t scalar)
	{
		return TVector4(v.X + scalar, v.Y + scalar, v.Z + scalar, v.W + scalar);
	}

	constexpr friend TVector4 operator+ (vec_t scalar, const TVector4 &v)
	{
		return TVector4(v.X + scalar, v.Y + scalar, v.Z + scalar, v.W + scalar);
	}

	// Scalar subtraction
	constexpr TVector4 &operator-= (vec_t scalar)
	{
		X -= scalar, Y -= scalar, Z -= scalar, W -= scalar;
		return *this;
	}

	constexpr TVector4 operator- (vec_t scalar) const
	{
		return TVector4(X - scalar, Y - scalar, Z - scalar, W - scalar);
	}

	// Scalar multiplication
	constexpr TVector4 &operator*= (vec_t scalar)
	{
		X = vec_t(X *scalar), Y = vec_t(Y * scalar), Z = vec_t(Z * scalar), W = vec_t(W * scalar);
		return *this;
	}

	constexpr friend TVector4 operator* (const TVector4 &v, vec_t scalar)
	{
		return TVector4(v.X * scalar, v.Y * scalar, v.Z * scalar, v.W * scalar);
	}

	constexpr friend TVector4 operator* (vec_t scalar, const TVector4 &v)
	{
		return TVector4(v.X * scalar, v.Y * scalar, v.Z * scalar, v.W * scalar);
	}

	// Scalar division
	constexpr TVector4 &operator/= (vec_t scalar)
	{
		scalar = 1 / scalar, X = vec_t(X * scalar), Y = vec_t(Y * scalar), Z = vec_t(Z * scalar), W = vec_t(W * scalar);
		return *this;
	}

	constexpr TVector4 operator/ (vec_t scalar) const
	{
		scalar = 1 / scalar;
		return TVector4(X * scalar, Y * scalar, Z * scalar, W * scalar);
	}

	// Vector addition
	constexpr TVector4 &operator+= (const TVector4 &other)
	{
		X += other.X, Y += other.Y, Z += other.Z, W += other.W;
		return *this;
	}

	constexpr TVector4 operator+ (const TVector4 &other) const
	{
		return TVector4(X + other.X, Y + other.Y, Z + other.Z, W + other.W);
	}

	// Vector subtraction
	constexpr TVector4 &operator-= (const TVector4 &other)
	{
		X -= other.X, Y -= other.Y, Z -= other.Z, W -= other.W;
		return *this;
	}

	constexpr TVector4 operator- (const TVector4 &other) const
	{
		return TVector4(X - other.X, Y - other.Y, Z - other.Z, W - other.W);
	}

	// Add a 3D vector to this 4D vector, leaving W unchanged.
	constexpr TVector4 &operator+= (const Vector3 &other)
	{
		X += other.X, Y += other.Y, Z += other.Z;
		return *this;
	}

	// Subtract a 3D vector from this 4D vector, leaving W unchanged.
	constexpr TVector4 &operator-= (const Vector3 &other)
	{
		X -= other.X, Y -= other.Y, Z -= other.Z;
		return *this;
	}

	// Add a 4D vector and a 3D vector.
	constexpr friend TVector4 operator+ (const TVector4 &v4, const Vector3 &v3)
	{
		return TVector4(v4.X + v3.X, v4.Y + v3.Y, v4.Z + v3.Z, v4.W);
	}

	constexpr friend TVector4 operator- (const TVector4 &v4, const Vector3 &v3)
	{
		return TVector4(v4.X - v3.X, v4.Y - v3.Y, v4.Z - v3.Z, v4.W);
	}

	constexpr friend Vector3 operator+ (const Vector3 &v3, const TVector4 &v4)
	{
		return Vector3(v3.X + v4.X, v3.Y + v4.Y, v3.Z + v4.Z);
	}

	// Subtract a 4D vector and a 3D vector.
	// Discards the W component of the 4D vector and returns a 3D vector.
	constexpr friend Vector3 operator- (const TVector3<vec_t> &v3, const TVector4 &v4)
	{
		return Vector3(v3.X - v4.X, v3.Y - v4.Y, v3.Z - v4.Z);
	}

	// Vector length
	double Length() const
	{
		return g_sqrt(LengthSquared());
	}

	constexpr double LengthSquared() const
	{
		return X*X + Y*Y + Z*Z + W*W;
	}
	
	double Sum() const
	{
		return abs(X) + abs(Y) + abs(Z) + abs(W);
	}
	

	// Return a unit vector facing the same direction as this one
	TVector4 Unit() const
	{
		double len = Length();
		if (len != 0) len = 1 / len;
		return *this * (vec_t)len;
	}

	// Scales this vector into a unit vector
	void MakeUnit()
	{
		double len = Length();
		if (len != 0) len = 1 / len;
		*this *= (vec_t)len;
	}

	// Resizes this vector to be the specified length (if it is not 0)
	TVector4 &MakeResize(double len)
	{
		double vlen = Length();
		if (vlen != 0.)
		{
			double scale = len / vlen;
			X = vec_t(X * scale);
			Y = vec_t(Y * scale);
			Z = vec_t(Z * scale);
			W = vec_t(W * scale);
		}
		return *this;
	}

	TVector4 Resized(double len) const 
	{
		double vlen = Length();
		if (vlen != 0.)
		{
			double scale = len / vlen;
			return{ vec_t(X * scale), vec_t(Y * scale), vec_t(Z * scale), vec_t(W * scale) };
		}
		else
		{
			return *this;
		}
	}

	// Dot product
	constexpr vec_t operator | (const TVector4 &other) const
	{
		return X*other.X + Y*other.Y + Z*other.Z + W*other.W;
	}

	constexpr vec_t dot(const TVector4 &other) const
	{
		return X*other.X + Y*other.Y + Z*other.Z + W*other.W;
	}
};

inline void ZeroSubnormalsF(double& num)
{
	if (fabs(num) < FLT_MIN) num = 0;
}

inline void ZeroSubnormals(double& num)
{
	if (fabs(num) < DBL_MIN) num = 0;
}

inline void ZeroSubnormals(float& num)
{
	if (fabsf(num) < FLT_MIN) num = 0;
}

template<typename T>
inline void ZeroSubnormals(TVector2<T>& vec)
{
	ZeroSubnormals(vec.X);
	ZeroSubnormals(vec.Y);
}

template<typename T>
inline void ZeroSubnormals(TVector3<T>& vec)
{
	ZeroSubnormals(vec.X);
	ZeroSubnormals(vec.Y);
	ZeroSubnormals(vec.Z);
}

template<typename T>
inline void ZeroSubnormals(TVector4<T>& vec)
{
	ZeroSubnormals(vec.X);
	ZeroSubnormals(vec.Y);
	ZeroSubnormals(vec.Z);
	ZeroSubnormals(vec.W);
}

template<class vec_t>
struct TMatrix3x3
{
	typedef TVector3<vec_t> Vector3;

	vec_t Cells[3][3];

	constexpr TMatrix3x3() = default;
	constexpr TMatrix3x3(const TMatrix3x3 &other) = default;
	constexpr TMatrix3x3& operator=(const TMatrix3x3& other) = default;

	constexpr TMatrix3x3(const Vector3 &row1, const Vector3 &row2, const Vector3 &row3)
	{
		(*this)[0] = row1;
		(*this)[1] = row2;
		(*this)[2] = row3;
	}

	// Construct a rotation matrix about an arbitrary axis.
	// (The axis vector must be normalized.)
	constexpr TMatrix3x3(const Vector3 &axis, double degrees)
		: TMatrix3x3(axis, g_sindeg(degrees), g_cosdeg(degrees))
	{
	}

	constexpr TMatrix3x3(const Vector3 &axis, double c/*cosine*/, double s/*sine*/)
	{
		double t = 1 - c;
		double sx = s*axis.X, sy = s*axis.Y, sz = s*axis.Z;
		double tx = 0, ty = 0, txx = 0, tyy = 0, u = 0, v = 0;

		tx = t*axis.X;
		Cells[0][0] = vec_t( (txx=tx*axis.X) + c );
		Cells[0][1] = vec_t(   (u=tx*axis.Y) - sz);
		Cells[0][2] = vec_t(   (v=tx*axis.Z) + sy);

		ty = t*axis.Y;
		Cells[1][0] = vec_t(              u  + sz);
		Cells[1][1] = vec_t( (tyy=ty*axis.Y) + c );
		Cells[1][2] = vec_t(   (u=ty*axis.Z) - sx);

		Cells[2][0] = vec_t(              v  - sy);
		Cells[2][1] = vec_t(              u  + sx);
		Cells[2][2] = vec_t(     (t-txx-tyy) + c );
	}

	TMatrix3x3(const Vector3 &axis, TAngle<vec_t> degrees);

	static TMatrix3x3 Rotate2D(double degrees)
	{
		double c = g_cosdeg(degrees);
		double s = g_sindeg(degrees);
		TMatrix3x3 ret;
		ret.Cells[0][0] = c; ret.Cells[0][1] = -s; ret.Cells[0][2] = 0;
		ret.Cells[1][0] = s; ret.Cells[1][1] =  c; ret.Cells[1][2] = 0;
		ret.Cells[2][0] = 0; ret.Cells[2][1] =  0; ret.Cells[2][2] = 1;
		return ret;
	}

	constexpr static TMatrix3x3 Scale2D(TVector2<vec_t> scaleVec)
	{
		TMatrix3x3 ret;
		ret.Cells[0][0] = scaleVec.X; ret.Cells[0][1] =          0; ret.Cells[0][2] = 0;
		ret.Cells[1][0] =          0; ret.Cells[1][1] = scaleVec.Y; ret.Cells[1][2] = 0;
		ret.Cells[2][0] =          0; ret.Cells[2][1] =          0; ret.Cells[2][2] = 1;
		return ret;
	}

	constexpr static TMatrix3x3 Translate2D(TVector2<vec_t> translateVec)
	{
		TMatrix3x3 ret;
		ret.Cells[0][0] = 1; ret.Cells[0][1] = 0; ret.Cells[0][2] = translateVec.X;
		ret.Cells[1][0] = 0; ret.Cells[1][1] = 1; ret.Cells[1][2] = translateVec.Y;
		ret.Cells[2][0] = 0; ret.Cells[2][1] = 0; ret.Cells[2][2] =              1;
		return ret;
	}

	constexpr void Zero()
	{
		memset (this, 0, sizeof *this);
	}

	constexpr void Identity()
	{
		Cells[0][0] = 1; Cells[0][1] = 0; Cells[0][2] = 0;
		Cells[1][0] = 0; Cells[1][1] = 1; Cells[1][2] = 0;
		Cells[2][0] = 0; Cells[2][1] = 0; Cells[2][2] = 1;
	}

	constexpr Vector3 &operator[] (int index)
	{
		return *((Vector3 *)&Cells[index]);
	}

	constexpr const Vector3 &operator[] (int index) const
	{
		return *((Vector3 *)&Cells[index]);
	}

	// Multiply a scalar
	constexpr TMatrix3x3 &operator*= (double scalar)
	{
		(*this)[0] *= scalar;
		(*this)[1] *= scalar;
		(*this)[2] *= scalar;
		return *this;
	}

	constexpr friend TMatrix3x3 operator* (double s, const TMatrix3x3 &m)
	{
		return TMatrix3x3(m[0]*s, m[1]*s, m[2]*s);
	}

	constexpr TMatrix3x3 operator* (double s) const
	{
		return TMatrix3x3((*this)[0]*s, (*this)[1]*s, (*this)[2]*s);
	}

	// Divide a scalar
	constexpr TMatrix3x3 &operator/= (double scalar)
	{
		return *this *= 1 / scalar;
	}

	constexpr TMatrix3x3 operator/ (double s) const
	{
		return *this * (1 / s);
	}

	// Add two 3x3 matrices together
	constexpr TMatrix3x3 &operator+= (const TMatrix3x3 &o)
	{
		(*this)[0] += o[0];
		(*this)[1] += o[1];
		(*this)[2] += o[2];
		return *this;
	}

	constexpr TMatrix3x3 operator+ (const TMatrix3x3 &o) const
	{
		return TMatrix3x3((*this)[0] + o[0], (*this)[1] + o[1], (*this)[2] + o[2]);
	}

	// Subtract two 3x3 matrices
	constexpr TMatrix3x3 &operator-= (const TMatrix3x3 &o)
	{
		(*this)[0] -= o[0];
		(*this)[1] -= o[1];
		(*this)[2] -= o[2];
		return *this;
	}

	constexpr TMatrix3x3 operator- (const TMatrix3x3 &o) const
	{
		return TMatrix3x3((*this)[0] - o[0], (*this)[1] - o[1], (*this)[2] - o[2]);
	}

	// Concatenate two 3x3 matrices
	constexpr TMatrix3x3 &operator*= (const TMatrix3x3 &o)
	{
		return *this = *this * o;
	}

	constexpr TMatrix3x3 operator* (const TMatrix3x3 &o) const
	{
		return TMatrix3x3(
			Vector3(Cells[0][0]*o[0][0] + Cells[0][1]*o[1][0] + Cells[0][2]*o[2][0],
					Cells[0][0]*o[0][1] + Cells[0][1]*o[1][1] + Cells[0][2]*o[2][1],
					Cells[0][0]*o[0][2] + Cells[0][1]*o[1][2] + Cells[0][2]*o[2][2]),
			Vector3(Cells[1][0]*o[0][0] + Cells[1][1]*o[1][0] + Cells[1][2]*o[2][0],
					Cells[1][0]*o[0][1] + Cells[1][1]*o[1][1] + Cells[1][2]*o[2][1],
					Cells[1][0]*o[0][2] + Cells[1][1]*o[1][2] + Cells[1][2]*o[2][2]),
			Vector3(Cells[2][0]*o[0][0] + Cells[2][1]*o[1][0] + Cells[2][2]*o[2][0],
					Cells[2][0]*o[0][1] + Cells[2][1]*o[1][1] + Cells[2][2]*o[2][1],
					Cells[2][0]*o[0][2] + Cells[2][1]*o[1][2] + Cells[2][2]*o[2][2]));
	}

	// Multiply a 3D vector by a rotation matrix
	constexpr friend Vector3 operator* (const Vector3 &v, const TMatrix3x3 &m)
	{
		return Vector3(m[0] | v, m[1] | v, m[2] | v);
	}

	constexpr friend Vector3 operator* (const TMatrix3x3 &m, const Vector3 &v)
	{
		return Vector3(m[0] | v, m[1] | v, m[2] | v);
	}
};

#define BAM_FACTOR (90. / 0x40000000)

template<class vec_t>
struct TAngle
{
	vec_t Degrees_;

	// This is to catch any accidental attempt to assign an angle_t to this type. Any explicit exception will require a type cast.
	TAngle(int) = delete;
	TAngle(unsigned int) = delete;
	TAngle(long) = delete;
	TAngle(unsigned long) = delete;
	TAngle &operator= (int other) = delete;
	TAngle &operator= (unsigned other) = delete;
	TAngle &operator= (long other) = delete;
	TAngle &operator= (unsigned long other) = delete;

	TAngle() = default;

private:
	// Both constructors are needed to avoid unnecessary conversions when assigning to FAngle.
	constexpr TAngle (float amt)
		: Degrees_((vec_t)amt)
	{
	}
	constexpr TAngle (double amt)
		: Degrees_((vec_t)amt)
	{
	}
public:

	constexpr vec_t& Degrees__() { return Degrees_; }
	
	static constexpr TAngle fromDeg(float deg)
	{
		return TAngle(deg);
	}
	static constexpr TAngle fromDeg(double deg)
	{
		return TAngle(deg);
	}
	static constexpr TAngle fromDeg(int deg)
	{
		return TAngle((vec_t)deg);
	}
	static constexpr TAngle fromDeg(unsigned deg)
	{
		return TAngle((vec_t)deg);
	}

	static constexpr TAngle fromRad(float rad)
	{
		return TAngle(float(rad * (180.0f / pi::pif())));
	}
	static constexpr TAngle fromRad(double rad)
	{
		return TAngle(double(rad * (180.0 / pi::pi())));
	}

	static constexpr TAngle fromCos(double cos)
	{
		return fromRad(g_acos(cos));
	}

	static constexpr TAngle fromSin(double sin)
	{
		return fromRad(g_asin(sin));
	}

	static constexpr TAngle fromTan(double tan)
	{
		return fromRad(g_atan(tan));
	}

	static constexpr TAngle fromBam(int f)
	{
		return TAngle(f * (90. / 0x40000000));
	}
	static constexpr TAngle fromBam(unsigned f)
	{
		return TAngle(f * (90. / 0x40000000));
	}

	static constexpr TAngle fromBuild(double bang)
	{
		return TAngle(bang * (90. / 512));
	}

	static constexpr TAngle fromQ16(int bang)
	{
		return TAngle(bang * (90. / 16384));
	}

	constexpr TAngle(const TAngle &other) = default;
	constexpr TAngle &operator= (const TAngle &other) = default;

	constexpr TAngle operator- () const
	{
		return TAngle(-Degrees_);
	}

	constexpr TAngle &operator+= (TAngle other)
	{
		Degrees_ += other.Degrees_;
		return *this;
	}

	constexpr TAngle &operator-= (TAngle other)
	{
		Degrees_ -= other.Degrees_;
		return *this;
	}

	constexpr TAngle operator+ (TAngle other) const
	{
		return Degrees_ + other.Degrees_;
	}

	constexpr TAngle operator- (TAngle other) const
	{
		return Degrees_ - other.Degrees_;
	}

	constexpr TAngle &operator*= (vec_t other)
	{
		Degrees_ = Degrees_ * other;
		return *this;
	}

	constexpr TAngle &operator/= (vec_t other)
	{
		Degrees_ = Degrees_ / other;
		return *this;
	}

	constexpr TAngle operator* (vec_t other) const
	{
		return Degrees_ * other;
	}

	constexpr TAngle operator* (TAngle other) const
	{
		return Degrees_ * other.Degrees_;
	}

	constexpr TAngle operator/ (vec_t other) const
	{
		return Degrees_ / other;
	}

	constexpr double operator/ (TAngle other) const
	{
		return Degrees_ / other.Degrees_;
	}

	// Should the comparisons consider an epsilon value?
	constexpr bool operator< (TAngle other) const
	{
		return Degrees_ < other.Degrees_;
	}

	constexpr bool operator> (TAngle other) const
	{
		return Degrees_ > other.Degrees_;
	}

	constexpr bool operator<= (TAngle other) const
	{
		return Degrees_ <= other.Degrees_;
	}

	constexpr bool operator>= (TAngle other) const
	{
		return Degrees_ >= other.Degrees_;
	}

	constexpr bool operator== (TAngle other) const
	{
		return Degrees_ == other.Degrees_;
	}

	constexpr bool operator!= (TAngle other) const
	{
		return Degrees_ != other.Degrees_;
	}

	// Ensure the angle is between [0.0,360.0) degrees
	TAngle Normalized360() const
	{
		// Normalizing the angle converts it to a BAM, which masks it, and converts it back to a float.
		// Note: We MUST use xs_Float here because it is the only method that guarantees reliable wraparound.
		return (vec_t)(BAM_FACTOR * BAMs());
	}

	// Ensures the angle is between (-180.0,180.0] degrees
	TAngle Normalized180() const
	{
		return (vec_t)(BAM_FACTOR * (signed int)BAMs());
	}

	constexpr vec_t Radians() const
	{
		return vec_t(Degrees_ * (pi::pi() / 180.0));
	}

	unsigned BAMs() const
	{
		return xs_CRoundToInt(Degrees_ * (0x40000000 / 90.));
	}

	constexpr vec_t Degrees() const
	{
		return Degrees_;
	}

	constexpr int Buildang() const
	{
		return int(Degrees_ * (512 / 90.0));
	}

	constexpr int Q16() const
	{
		return int(Degrees_ * (16384 / 90.0));
	}

	TVector2<vec_t> ToVector(vec_t length = 1) const
	{
		return TVector2<vec_t>(length * Cos(), length * Sin());
	}

	vec_t Cos() const
	{
		return vec_t(g_cosdeg(Degrees_));
	}

	vec_t Sin() const
	{
		return vec_t(g_sindeg(Degrees_));
	}

	double Tan() const
	{
		// use an optimized approach if we have a sine table. If not just call the CRT's tan function.
#if __has_include("math/cmath.h")
		const auto bam = BAMs();
		return g_sinbam(bam) / g_cosbam(bam);
#else
		return vec_t(tan(Radians()));
#endif
	}

	// This is for calculating vertical velocity. For high pitches the tangent will become too large to be useful.
	double TanClamped(double max = 5.) const
	{
		return clamp(Tan(), -max, max);
	}

	// returns sign of the NORMALIZED angle.
	int Sgn() const
	{
		auto val = int(BAMs());
		return (val > 0) - (val < 0);
	}
};

typedef TAngle<float>		FAngle;
typedef TAngle<double>		DAngle;

constexpr DAngle nullAngle = DAngle::fromDeg(0.);
constexpr FAngle nullFAngle = FAngle::fromDeg(0.);
constexpr DAngle minAngle = DAngle::fromDeg(1. / 65536.);
constexpr FAngle minFAngle = FAngle::fromDeg(1. / 65536.);

constexpr DAngle DAngle1 = DAngle::fromDeg(1);
constexpr DAngle DAngle15 = DAngle::fromDeg(15);
constexpr DAngle DAngle22_5 = DAngle::fromDeg(22.5);
constexpr DAngle DAngle45 = DAngle::fromDeg(45);
constexpr DAngle DAngle60 = DAngle::fromDeg(60);
constexpr DAngle DAngle90 = DAngle::fromDeg(90);
constexpr DAngle DAngle180 = DAngle::fromDeg(180);
constexpr DAngle DAngle270 = DAngle::fromDeg(270);
constexpr DAngle DAngle360 = DAngle::fromDeg(360);

template<class T>
inline TAngle<T> fabs (const TAngle<T> &deg)
{
	return TAngle<T>::fromDeg(fabs(deg.Degrees()));
}

template<class T>
inline TAngle<T> abs (const TAngle<T> &deg)
{
	return TAngle<T>::fromDeg(fabs(deg.Degrees()));
}

template<class T>
inline TAngle<T> deltaangle(const TAngle<T> &a1, const TAngle<T> &a2)
{
	return (a2 - a1).Normalized180();
}

template<class T>
inline TAngle<T> absangle(const TAngle<T> &a1, const TAngle<T> &a2)
{
	return fabs(deltaangle(a2, a1));
}

inline TAngle<double> VecToAngle(double x, double y)
{
	return TAngle<double>::fromRad(g_atan2(y, x));
}

template<class T>
inline TAngle<T> VecToAngle (const TVector2<T> &vec)
{
	return TAngle<T>::fromRad(g_atan2(vec.Y, vec.X));
}

template<class T>
inline TAngle<T> VecToAngle (const TVector3<T> &vec)
{
	return TAngle<T>::fromRad(g_atan2(vec.Y, vec.X));
}

template<class T>
TAngle<T> TVector2<T>::Angle() const
{
	return VecToAngle(X, Y);
}

template<class T>
TAngle<T> TVector3<T>::Angle() const
{
	return VecToAngle(X, Y);
}

template<class T>
TAngle<T> TVector3<T>::Pitch() const
{
	return -VecToAngle(XY().Length(), Z);
}

template<class T>
constexpr inline TVector2<T> clamp(const TVector2<T> &vec, const TVector2<T> &min, const TVector2<T> &max)
{
	return TVector2<T>(clamp(vec.X, min.X, max.X), clamp(vec.Y, min.Y, max.Y));
}

template<class T>
constexpr inline TVector3<T> clamp(const TVector3<T> &vec, const TVector3<T> &min, const TVector3<T> &max)
{
	return TVector3<T>(clamp<T>(vec.X, min.X, max.X), clamp<T>(vec.Y, min.Y, max.Y), clamp<T>(vec.Z, min.Z, max.Z));
}

template<class T>
constexpr inline TRotator<T> clamp(const TRotator<T> &rot, const TRotator<T> &min, const TRotator<T> &max)
{
	return TRotator<T>(clamp(rot.Pitch, min.Pitch, max.Pitch), clamp(rot.Yaw, min.Yaw, max.Yaw), clamp(rot.Roll, min.Roll, max.Roll));
}

template<class T>
inline TAngle<T> interpolatedvalue(const TAngle<T> &oang, const TAngle<T> &ang, const double interpfrac)
{
	return oang + (deltaangle(oang, ang) * interpfrac);
}

template<class T>
inline TRotator<T> interpolatedvalue(const TRotator<T> &oang, const TRotator<T> &ang, const double interpfrac)
{
	return TRotator<T>(
		interpolatedvalue(oang.Pitch, ang.Pitch, interpfrac),
		interpolatedvalue(oang.Yaw, ang.Yaw, interpfrac),
		interpolatedvalue(oang.Roll, ang.Roll, interpfrac)
	);
}

template <class T>
constexpr inline T interpolatedvalue(const T& oval, const T& val, const double interpfrac)
{
	return T(oval + (val - oval) * interpfrac);
}

// Much of this is copied from TVector3. Is all that functionality really appropriate?
template<class vec_t>
struct TRotator
{
	typedef TAngle<vec_t> Angle;

	Angle Pitch;	// up/down
	Angle Yaw;		// left/right
	Angle Roll;		// rotation about the forward axis.

	constexpr TRotator() = default;

	constexpr TRotator (const Angle &p, const Angle &y, const Angle &r)
		: Pitch(p), Yaw(y), Roll(r)
	{
	}

	constexpr TRotator(const TRotator &other) = default;
	constexpr TRotator &operator= (const TRotator &other) = default;

	constexpr void Zero()
	{
		Roll = Yaw = Pitch = nullAngle;
	}

	constexpr bool isZero() const
	{
		return Pitch == nullAngle && Yaw == nullAngle && Roll == nullAngle;
	}

	// Access angles as an array
	constexpr Angle &operator[] (int index)
	{
		return *(&Pitch + index);
	}

	constexpr const Angle &operator[] (int index) const
	{
		return *(&Pitch + index);
	}

	// Test for equality
	constexpr bool operator== (const TRotator &other) const
	{
		return Pitch == other.Pitch && Yaw == other.Yaw && Roll == other.Roll;
	}

	// Test for inequality
	constexpr bool operator!= (const TRotator &other) const
	{
		return Pitch != other.Pitch || Yaw != other.Yaw || Roll != other.Roll;
	}

	// Test for approximate equality
	bool ApproximatelyEquals (const TRotator &other) const
	{
		constexpr auto epsilon = Angle(EQUAL_EPSILON);
		return fabs(Pitch - other.Pitch) < epsilon && fabs(Yaw - other.Yaw) < epsilon && fabs(Roll - other.Roll) < epsilon;
	}

	// Test for approximate inequality
	bool DoesNotApproximatelyEqual (const TRotator &other) const
	{
		constexpr auto epsilon = Angle(EQUAL_EPSILON);
		return fabs(Pitch - other.Pitch) >= epsilon && fabs(Yaw - other.Yaw) >= epsilon && fabs(Roll - other.Roll) >= epsilon;
	}

	// Unary negation
	constexpr TRotator operator- () const
	{
		return TRotator(-Pitch, -Yaw, -Roll);
	}

	// Scalar addition
	constexpr TRotator &operator+= (const Angle &scalar)
	{
		Pitch += scalar, Yaw += scalar, Roll += scalar;
		return *this;
	}

	constexpr friend TRotator operator+ (const TRotator &v, const Angle &scalar)
	{
		return TRotator(v.Pitch + scalar, v.Yaw + scalar, v.Roll + scalar);
	}

	constexpr friend TRotator operator+ (const Angle &scalar, const TRotator &v)
	{
		return TRotator(v.Pitch + scalar, v.Yaw + scalar, v.Roll + scalar);
	}

	// Scalar subtraction
	constexpr TRotator &operator-= (const Angle &scalar)
	{
		Pitch -= scalar, Yaw -= scalar, Roll -= scalar;
		return *this;
	}

	constexpr TRotator operator- (const Angle &scalar) const
	{
		return TRotator(Pitch - scalar, Yaw - scalar, Roll - scalar);
	}

	// Scalar multiplication
	constexpr TRotator &operator*= (const Angle &scalar)
	{
		Pitch *= scalar, Yaw *= scalar, Roll *= scalar;
		return *this;
	}

	constexpr friend TRotator operator* (const TRotator &v, const Angle &scalar)
	{
		return TRotator(v.Pitch * scalar, v.Yaw * scalar, v.Roll * scalar);
	}

	constexpr friend TRotator operator* (const Angle &scalar, const TRotator &v)
	{
		return TRotator(v.Pitch * scalar, v.Yaw * scalar, v.Roll * scalar);
	}

	// Scalar division
	constexpr TRotator &operator/= (const Angle &scalar)
	{
		Angle mul(1 / scalar.Degrees_);
		Pitch *= mul, Yaw *= mul, Roll *= mul;
		return *this;
	}

	constexpr TRotator &operator/= (const vec_t &scalar)
	{
		const auto mul = 1. / scalar;
		Pitch *= mul, Yaw *= mul, Roll *= mul;
		return *this;
	}

	constexpr TRotator operator/ (const Angle &scalar) const
	{
		Angle mul(1 / scalar.Degrees_);
		return TRotator(Pitch * mul, Yaw * mul, Roll * mul);
	}

	constexpr TRotator operator/ (const vec_t &scalar) const
	{
		const auto mul = 1. / scalar;
		return TRotator(Pitch * mul, Yaw * mul, Roll * mul);
	}

	// Vector addition
	constexpr TRotator &operator+= (const TRotator &other)
	{
		Pitch += other.Pitch, Yaw += other.Yaw, Roll += other.Roll;
		return *this;
	}

	constexpr TRotator operator+ (const TRotator &other) const
	{
		return TRotator(Pitch + other.Pitch, Yaw + other.Yaw, Roll + other.Roll);
	}

	// Vector subtraction
	constexpr TRotator &operator-= (const TRotator &other)
	{
		Pitch -= other.Pitch, Yaw -= other.Yaw, Roll -= other.Roll;
		return *this;
	}

	constexpr TRotator operator- (const TRotator &other) const
	{
		return TRotator(Pitch - other.Pitch, Yaw - other.Yaw, Roll - other.Roll);
	}
};

// Create a forward vector from a rotation (ignoring roll)
template<class T>
inline TVector3<T>::TVector3 (const TRotator<T> &rot)
{
	auto XY = rot.Pitch.Cos() * rot.Yaw.ToVector();
	X = XY.X;
	Y = XY.Y;
	Z = rot.Pitch.Sin();
}

template<class T>
inline TMatrix3x3<T>::TMatrix3x3(const TVector3<T> &axis, TAngle<T> degrees)
{
	double c = degrees.Cos(), s = degrees.Sin(), t = 1 - c;
	double sx = s*axis.X, sy = s*axis.Y, sz = s*axis.Z;
	double tx, ty, txx, tyy, u, v;

	tx = t*axis.X;
	Cells[0][0] = T( (txx=tx*axis.X) + c  );
	Cells[0][1] = T(   (u=tx*axis.Y) - sz );
	Cells[0][2] = T(   (v=tx*axis.Z) + sy );

	ty = t*axis.Y;
	Cells[1][0] = T(              u  + sz );
	Cells[1][1] = T( (tyy=ty*axis.Y) + c  );
	Cells[1][2] = T(   (u=ty*axis.Z) - sx );

	Cells[2][0] = T(              v  - sy );
	Cells[2][1] = T(              u  + sx );
	Cells[2][2] = T(     (t-txx-tyy) + c  );
}

typedef TVector2<float>		FVector2;
typedef TVector3<float>		FVector3;
typedef TVector4<float>		FVector4;
typedef TRotator<float>		FRotator;
typedef TMatrix3x3<float>	FMatrix3x3;

typedef TVector2<double>		DVector2;
typedef TVector3<double>		DVector3;
typedef TVector4<double>		DVector4;
typedef TRotator<double>		DRotator;
typedef TMatrix3x3<double>		DMatrix3x3;

class Plane
{
public:
	void Set(FVector3 normal, float d)
	{
		m_normal = normal;
		m_d = d;
	}

	void Init(const FVector3& p1, const FVector3& p2, const FVector3& p3)
	{
		m_normal = ((p2 - p1) ^ (p3 - p1)).Unit();
		m_d = -(p3 |m_normal);
	}

	// same for a play-vector. Note that y and z are inversed.
	void Set(DVector3 normal, double d)
	{
		m_normal = { (float)normal.X, (float)normal.Z, (float)normal.Y };
		m_d = (float)d;
	}

	float DistToPoint(float x, float y, float z)
	{
		FVector3 p(x, y, z);

		return (m_normal | p) + m_d;
	}


	bool PointOnSide(float x, float y, float z)
	{
		return DistToPoint(x, y, z) < 0.f;
	}

	bool PointOnSide(const FVector3 &v) { return PointOnSide(v.X, v.Y, v.Z); }

	float A() { return m_normal.X; }
	float B() { return m_normal.Y; }
	float C() { return m_normal.Z; }
	float D() { return m_d; }

	const FVector3 &Normal() const { return m_normal; }
protected:
	FVector3 m_normal;
	float m_d;
};

#endif
