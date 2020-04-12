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
#include "xs_Float.h"
#include "math/cmath.h"


#define EQUAL_EPSILON (1/65536.)

// make this a local inline function to avoid any dependencies on other headers and not pollute the global namespace
namespace pi
{
	inline constexpr double pi() { return 3.14159265358979323846; }
	inline constexpr double pif() { return 3.14159265358979323846f; }
}



template<class vec_t> struct TVector3;
template<class vec_t> struct TRotator;
template<class vec_t> struct TAngle;

template<class vec_t>
struct TVector2
{
	vec_t X, Y;

	TVector2() = default;

	TVector2 (vec_t a, vec_t b)
		: X(a), Y(b)
	{
	}

	TVector2(const TVector2 &other) = default;

	TVector2 (const TVector3<vec_t> &other)	// Copy the X and Y from the 3D vector and discard the Z
		: X(other.X), Y(other.Y)
	{
	}

	void Zero()
	{
		Y = X = 0;
	}

	bool isZero() const
	{
		return X == 0 && Y == 0;
	}

	TVector2 &operator= (const TVector2 &other) = default;

	// Access X and Y as an array
	vec_t &operator[] (int index)
	{
		return index == 0 ? X : Y;
	}

	const vec_t &operator[] (int index) const
	{
		return index == 0 ? X : Y;
	}

	// Test for equality
	bool operator== (const TVector2 &other) const
	{
		return X == other.X && Y == other.Y;
	}

	// Test for inequality
	bool operator!= (const TVector2 &other) const
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
	TVector2 operator- () const
	{
		return TVector2(-X, -Y);
	}

	// Scalar addition
	TVector2 &operator+= (double scalar)
	{
		X += scalar, Y += scalar;
		return *this;
	}

	friend TVector2 operator+ (const TVector2 &v, vec_t scalar)
	{
		return TVector2(v.X + scalar, v.Y + scalar);
	}

	friend TVector2 operator+ (vec_t scalar, const TVector2 &v)
	{
		return TVector2(v.X + scalar, v.Y + scalar);
	}

	// Scalar subtraction
	TVector2 &operator-= (vec_t scalar)
	{
		X -= scalar, Y -= scalar;
		return *this;
	}

	TVector2 operator- (vec_t scalar) const
	{
		return TVector2(X - scalar, Y - scalar);
	}

	// Scalar multiplication
	TVector2 &operator*= (vec_t scalar)
	{
		X *= scalar, Y *= scalar;
		return *this;
	}

	friend TVector2 operator* (const TVector2 &v, vec_t scalar)
	{
		return TVector2(v.X * scalar, v.Y * scalar);
	}

	friend TVector2 operator* (vec_t scalar, const TVector2 &v)
	{
		return TVector2(v.X * scalar, v.Y * scalar);
	}

	// Scalar division
	TVector2 &operator/= (vec_t scalar)
	{
		scalar = 1 / scalar, X *= scalar, Y *= scalar;
		return *this;
	}

	TVector2 operator/ (vec_t scalar) const
	{
		scalar = 1 / scalar;
		return TVector2(X * scalar, Y * scalar);
	}

	// Vector addition
	TVector2 &operator+= (const TVector2 &other)
	{
		X += other.X, Y += other.Y;
		return *this;
	}

	TVector2 operator+ (const TVector2 &other) const
	{
		return TVector2(X + other.X, Y + other.Y);
	}

	// Vector subtraction
	TVector2 &operator-= (const TVector2 &other)
	{
		X -= other.X, Y -= other.Y;
		return *this;
	}

	TVector2 operator- (const TVector2 &other) const
	{
		return TVector2(X - other.X, Y - other.Y);
	}

	// Vector length
	vec_t Length() const
	{
		return (vec_t)g_sqrt (X*X + Y*Y);
	}

	vec_t LengthSquared() const
	{
		return X*X + Y*Y;
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


	// Dot product
	vec_t operator | (const TVector2 &other) const
	{
		return X*other.X + Y*other.Y;
	}

	// Returns the angle that the ray (0,0)-(X,Y) faces
	TAngle<vec_t> Angle() const;

	// Returns a rotated vector. angle is in degrees.
	TVector2 Rotated (double angle)
	{
		double cosval = g_cosdeg (angle);
		double sinval = g_sindeg (angle);
		return TVector2(X*cosval - Y*sinval, Y*cosval + X*sinval);
	}

	// Returns a rotated vector. angle is in degrees.
	template<class T>
	TVector2 Rotated(TAngle<T> angle)
	{
		double cosval = angle.Cos();
		double sinval = angle.Sin();
		return TVector2(X*cosval - Y*sinval, Y*cosval + X*sinval);
	}

	// Returns a vector rotated 90 degrees clockwise.
	TVector2 Rotated90CW()
	{
		return TVector2(Y, -X);
	}

	// Returns a vector rotated 90 degrees counterclockwise.
	TVector2 Rotated90CCW()
	{
		return TVector2(-Y, X);
	}
};

template<class vec_t>
struct TVector3
{
	typedef TVector2<vec_t> Vector2;

	vec_t X, Y, Z;

	TVector3() = default;

	TVector3 (vec_t a, vec_t b, vec_t c)
		: X(a), Y(b), Z(c)
	{
	}

	TVector3(vec_t *o)
		: X(o[0]), Y(o[1]), Z(o[2])
	{
	}

	TVector3(std::nullptr_t nul) = delete;

	TVector3(const TVector3 &other) = default;

	TVector3 (const Vector2 &xy, vec_t z)
		: X(xy.X), Y(xy.Y), Z(z)
	{
	}

	TVector3 (const TRotator<vec_t> &rot);

	void Zero()
	{
		Z = Y = X = 0;
	}

	bool isZero() const
	{
		return X == 0 && Y == 0 && Z == 0;
	}

	TVector3 &operator= (const TVector3 &other) = default;

	// Access X and Y and Z as an array
	vec_t &operator[] (int index)
	{
		return index == 0 ? X : index == 1 ? Y : Z;
	}

	const vec_t &operator[] (int index) const
	{
		return index == 0 ? X : index == 1 ? Y : Z;
	}

	// Test for equality
	bool operator== (const TVector3 &other) const
	{
		return X == other.X && Y == other.Y && Z == other.Z;
	}

	// Test for inequality
	bool operator!= (const TVector3 &other) const
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
	TVector3 operator- () const
	{
		return TVector3(-X, -Y, -Z);
	}

	// Scalar addition
	TVector3 &operator+= (vec_t scalar)
	{
		X += scalar, Y += scalar, Z += scalar;
		return *this;
	}

	friend TVector3 operator+ (const TVector3 &v, vec_t scalar)
	{
		return TVector3(v.X + scalar, v.Y + scalar, v.Z + scalar);
	}

	friend TVector3 operator+ (vec_t scalar, const TVector3 &v)
	{
		return TVector3(v.X + scalar, v.Y + scalar, v.Z + scalar);
	}

	// Scalar subtraction
	TVector3 &operator-= (vec_t scalar)
	{
		X -= scalar, Y -= scalar, Z -= scalar;
		return *this;
	}

	TVector3 operator- (vec_t scalar) const
	{
		return TVector3(X - scalar, Y - scalar, Z - scalar);
	}

	// Scalar multiplication
	TVector3 &operator*= (vec_t scalar)
	{
		X = vec_t(X *scalar), Y = vec_t(Y * scalar), Z = vec_t(Z * scalar);
		return *this;
	}

	friend TVector3 operator* (const TVector3 &v, vec_t scalar)
	{
		return TVector3(v.X * scalar, v.Y * scalar, v.Z * scalar);
	}

	friend TVector3 operator* (vec_t scalar, const TVector3 &v)
	{
		return TVector3(v.X * scalar, v.Y * scalar, v.Z * scalar);
	}

	// Scalar division
	TVector3 &operator/= (vec_t scalar)
	{
		scalar = 1 / scalar, X = vec_t(X * scalar), Y = vec_t(Y * scalar), Z = vec_t(Z * scalar);
		return *this;
	}

	TVector3 operator/ (vec_t scalar) const
	{
		scalar = 1 / scalar;
		return TVector3(X * scalar, Y * scalar, Z * scalar);
	}

	// Vector addition
	TVector3 &operator+= (const TVector3 &other)
	{
		X += other.X, Y += other.Y, Z += other.Z;
		return *this;
	}

	TVector3 operator+ (const TVector3 &other) const
	{
		return TVector3(X + other.X, Y + other.Y, Z + other.Z);
	}

	// Vector subtraction
	TVector3 &operator-= (const TVector3 &other)
	{
		X -= other.X, Y -= other.Y, Z -= other.Z;
		return *this;
	}

	TVector3 operator- (const TVector3 &other) const
	{
		return TVector3(X - other.X, Y - other.Y, Z - other.Z);
	}

	// Add a 2D vector to this 3D vector, leaving Z unchanged.
	TVector3 &operator+= (const Vector2 &other)
	{
		X += other.X, Y += other.Y;
		return *this;
	}

	// Subtract a 2D vector from this 3D vector, leaving Z unchanged.
	TVector3 &operator-= (const Vector2 &other)
	{
		X -= other.X, Y -= other.Y;
		return *this;
	}

	// returns the XY fields as a 2D-vector.
	Vector2 XY() const
	{
		return{ X, Y };
	}

	// Add a 3D vector and a 2D vector.
	friend TVector3 operator+ (const TVector3 &v3, const Vector2 &v2)
	{
		return TVector3(v3.X + v2.X, v3.Y + v2.Y, v3.Z);
	}

	friend TVector3 operator- (const TVector3 &v3, const Vector2 &v2)
	{
		return TVector3(v3.X - v2.X, v3.Y - v2.Y, v3.Z);
	}

	friend Vector2 operator+ (const Vector2 &v2, const TVector3 &v3)
	{
		return Vector2(v2.X + v3.X, v2.Y + v3.Y);
	}

	// Subtract a 3D vector and a 2D vector.
	// Discards the Z component of the 3D vector and returns a 2D vector.
	friend Vector2 operator- (const TVector2<vec_t> &v2, const TVector3 &v3)
	{
		return Vector2(v2.X - v3.X, v2.Y - v3.Y);
	}

	void GetRightUp(TVector3 &right, TVector3 &up)
	{
		TVector3 n(X, Y, Z);
		TVector3 fn(fabs(n.X), fabs(n.Y), fabs(n.Z));
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
		right.MakeUnit();;
		up.MakeUnit();
	}


	// Returns the angle (in radians) that the ray (0,0)-(X,Y) faces
	TAngle<vec_t> Angle() const;
	TAngle<vec_t> Pitch() const;

	// Vector length
	double Length() const
	{
		return g_sqrt (X*X + Y*Y + Z*Z);
	}

	double LengthSquared() const
	{
		return X*X + Y*Y + Z*Z;
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

	TVector3 Resized(double len)
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
	vec_t operator | (const TVector3 &other) const
	{
		return X*other.X + Y*other.Y + Z*other.Z;
	}

	// Cross product
	TVector3 operator ^ (const TVector3 &other) const
	{
		return TVector3(Y*other.Z - Z*other.Y,
					   Z*other.X - X*other.Z,
					   X*other.Y - Y*other.X);
	}

	TVector3 &operator ^= (const TVector3 &other)
	{
		*this = *this ^ other;
		return *this;
	}
};

template<class vec_t>
struct TVector4
{
	typedef TVector3<vec_t> Vector3;

	vec_t X, Y, Z, W;

	TVector4() = default;

	TVector4(vec_t a, vec_t b, vec_t c, vec_t d)
		: X(a), Y(b), Z(c), W(d)
	{
	}

	TVector4(vec_t *o)
		: X(o[0]), Y(o[1]), Z(o[2]), W(o[3])
	{
	}

	TVector4(const TVector4 &other) = default;

	TVector4(const Vector3 &xyz, vec_t w)
		: X(xyz.X), Y(xyz.Y), Z(xyz.Z), W(w)
	{
	}

	void Zero()
	{
		Z = Y = X = W = 0;
	}

	bool isZero() const
	{
		return X == 0 && Y == 0 && Z == 0 && W == 0;
	}

	TVector4 &operator= (const TVector4 &other) = default;

	// Access X and Y and Z as an array
	vec_t &operator[] (int index)
	{
		return (&X)[index];
	}

	const vec_t &operator[] (int index) const
	{
		return (&X)[index];
	}

	// Test for equality
	bool operator== (const TVector4 &other) const
	{
		return X == other.X && Y == other.Y && Z == other.Z && W == other.W;
	}

	// Test for inequality
	bool operator!= (const TVector4 &other) const
	{
		return X != other.X || Y != other.Y || Z != other.Z || W != other.W;
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
	TVector4 operator- () const
	{
		return TVector4(-X, -Y, -Z, -W);
	}

	// Scalar addition
	TVector4 &operator+= (vec_t scalar)
	{
		X += scalar, Y += scalar, Z += scalar; W += scalar;
		return *this;
	}

	friend TVector4 operator+ (const TVector4 &v, vec_t scalar)
	{
		return TVector4(v.X + scalar, v.Y + scalar, v.Z + scalar, v.W + scalar);
	}

	friend TVector4 operator+ (vec_t scalar, const TVector4 &v)
	{
		return TVector4(v.X + scalar, v.Y + scalar, v.Z + scalar, v.W + scalar);
	}

	// Scalar subtraction
	TVector4 &operator-= (vec_t scalar)
	{
		X -= scalar, Y -= scalar, Z -= scalar, W -= scalar;
		return *this;
	}

	TVector4 operator- (vec_t scalar) const
	{
		return TVector4(X - scalar, Y - scalar, Z - scalar, W - scalar);
	}

	// Scalar multiplication
	TVector4 &operator*= (vec_t scalar)
	{
		X = vec_t(X *scalar), Y = vec_t(Y * scalar), Z = vec_t(Z * scalar), W = vec_t(W * scalar);
		return *this;
	}

	friend TVector4 operator* (const TVector4 &v, vec_t scalar)
	{
		return TVector4(v.X * scalar, v.Y * scalar, v.Z * scalar, v.W * scalar);
	}

	friend TVector4 operator* (vec_t scalar, const TVector4 &v)
	{
		return TVector4(v.X * scalar, v.Y * scalar, v.Z * scalar, v.W * scalar);
	}

	// Scalar division
	TVector4 &operator/= (vec_t scalar)
	{
		scalar = 1 / scalar, X = vec_t(X * scalar), Y = vec_t(Y * scalar), Z = vec_t(Z * scalar), W = vec_t(W * scalar);
		return *this;
	}

	TVector4 operator/ (vec_t scalar) const
	{
		scalar = 1 / scalar;
		return TVector4(X * scalar, Y * scalar, Z * scalar, W * scalar);
	}

	// Vector addition
	TVector4 &operator+= (const TVector4 &other)
	{
		X += other.X, Y += other.Y, Z += other.Z, W += other.W;
		return *this;
	}

	TVector4 operator+ (const TVector4 &other) const
	{
		return TVector4(X + other.X, Y + other.Y, Z + other.Z, W + other.W);
	}

	// Vector subtraction
	TVector4 &operator-= (const TVector4 &other)
	{
		X -= other.X, Y -= other.Y, Z -= other.Z, W -= other.W;
		return *this;
	}

	TVector4 operator- (const TVector4 &other) const
	{
		return TVector4(X - other.X, Y - other.Y, Z - other.Z, W - other.W);
	}

	// Add a 3D vector to this 4D vector, leaving W unchanged.
	TVector4 &operator+= (const Vector3 &other)
	{
		X += other.X, Y += other.Y, Z += other.Z;
		return *this;
	}

	// Subtract a 3D vector from this 4D vector, leaving W unchanged.
	TVector4 &operator-= (const Vector3 &other)
	{
		X -= other.X, Y -= other.Y, Z -= other.Z;
		return *this;
	}

	// returns the XYZ fields as a 3D-vector.
	Vector3 XYZ() const
	{
		return{ X, Y, Z };
	}

	// Add a 4D vector and a 3D vector.
	friend TVector4 operator+ (const TVector4 &v4, const Vector3 &v3)
	{
		return TVector4(v4.X + v3.X, v4.Y + v3.Y, v4.Z + v3.Z, v4.W);
	}

	friend TVector4 operator- (const TVector4 &v4, const Vector3 &v3)
	{
		return TVector4(v4.X - v3.X, v4.Y - v3.Y, v4.Z - v3.Z, v4.W);
	}

	friend Vector3 operator+ (const Vector3 &v3, const TVector4 &v4)
	{
		return Vector3(v3.X + v4.X, v3.Y + v4.Y, v3.Z + v4.Z);
	}

	// Subtract a 4D vector and a 3D vector.
	// Discards the W component of the 4D vector and returns a 3D vector.
	friend Vector3 operator- (const TVector3<vec_t> &v3, const TVector4 &v4)
	{
		return Vector3(v3.X - v4.X, v3.Y - v4.Y, v3.Z - v4.Z);
	}

	// Vector length
	double Length() const
	{
		return g_sqrt(X*X + Y*Y + Z*Z + W*W);
	}

	double LengthSquared() const
	{
		return X*X + Y*Y + Z*Z + W*W;
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

	TVector4 Resized(double len)
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
	vec_t operator | (const TVector4 &other) const
	{
		return X*other.X + Y*other.Y + Z*other.Z + W*other.W;
	}
};

template<class vec_t>
struct TMatrix3x3
{
	typedef TVector3<vec_t> Vector3;

	vec_t Cells[3][3];

	TMatrix3x3() = default;
	TMatrix3x3(const TMatrix3x3 &other) = default;

	TMatrix3x3(const Vector3 &row1, const Vector3 &row2, const Vector3 &row3)
	{
		(*this)[0] = row1;
		(*this)[1] = row2;
		(*this)[2] = row3;
	}

	// Construct a rotation matrix about an arbitrary axis.
	// (The axis vector must be normalized.)
	TMatrix3x3(const Vector3 &axis, double radians)
	{
		double c = g_cos(radians), s = g_sin(radians), t = 1 - c;
/* In comments: A more readable version of the matrix setup.
This was found in Diana Gruber's article "The Mathematics of the
3D Rotation Matrix" at <http://www.makegames.com/3drotation/> and is
attributed to Graphics Gems (Glassner, Academic Press, 1990).

		Cells[0][0] = t*axis.X*axis.X + c;
		Cells[0][1] = t*axis.X*axis.Y - s*axis.Z;
		Cells[0][2] = t*axis.X*axis.Z + s*axis.Y;

		Cells[1][0] = t*axis.Y*axis.X + s*axis.Z;
		Cells[1][1] = t*axis.Y*axis.Y + c;
		Cells[1][2] = t*axis.Y*axis.Z - s*axis.X;

		Cells[2][0] = t*axis.Z*axis.X - s*axis.Y;
		Cells[2][1] = t*axis.Z*axis.Y + s*axis.X;
		Cells[2][2] = t*axis.Z*axis.Z + c;

Outside comments: A faster version with only 10 (not 24) multiplies.
*/
		double sx = s*axis.X, sy = s*axis.Y, sz = s*axis.Z;
		double tx, ty, txx, tyy, u, v;

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

	TMatrix3x3(const Vector3 &axis, double c/*cosine*/, double s/*sine*/)
	{
		double t = 1 - c;
		double sx = s*axis.X, sy = s*axis.Y, sz = s*axis.Z;
		double tx, ty, txx, tyy, u, v;

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

	static TMatrix3x3 Rotate2D(double radians)
	{
		double c = g_cos(radians);
		double s = g_sin(radians);
		TMatrix3x3 ret;
		ret.Cells[0][0] = c; ret.Cells[0][1] = -s; ret.Cells[0][2] = 0;
		ret.Cells[1][0] = s; ret.Cells[1][1] =  c; ret.Cells[1][2] = 0;
		ret.Cells[2][0] = 0; ret.Cells[2][1] =  0; ret.Cells[2][2] = 1;
		return ret;
	}

	static TMatrix3x3 Scale2D(TVector2<vec_t> scaleVec)
	{
		TMatrix3x3 ret;
		ret.Cells[0][0] = scaleVec.X; ret.Cells[0][1] =          0; ret.Cells[0][2] = 0;
		ret.Cells[1][0] =          0; ret.Cells[1][1] = scaleVec.Y; ret.Cells[1][2] = 0;
		ret.Cells[2][0] =          0; ret.Cells[2][1] =          0; ret.Cells[2][2] = 1;
		return ret;
	}

	static TMatrix3x3 Translate2D(TVector2<vec_t> translateVec)
	{
		TMatrix3x3 ret;
		ret.Cells[0][0] = 1; ret.Cells[0][1] = 0; ret.Cells[0][2] = translateVec.X;
		ret.Cells[1][0] = 0; ret.Cells[1][1] = 1; ret.Cells[1][2] = translateVec.Y;
		ret.Cells[2][0] = 0; ret.Cells[2][1] = 0; ret.Cells[2][2] =              1;
		return ret;
	}

	void Zero()
	{
		memset (this, 0, sizeof *this);
	}

	void Identity()
	{
		Cells[0][0] = 1; Cells[0][1] = 0; Cells[0][2] = 0;
		Cells[1][0] = 0; Cells[1][1] = 1; Cells[1][2] = 0;
		Cells[2][0] = 0; Cells[2][1] = 0; Cells[2][2] = 1;
	}

	Vector3 &operator[] (int index)
	{
		return *((Vector3 *)&Cells[index]);
	}

	const Vector3 &operator[] (int index) const
	{
		return *((Vector3 *)&Cells[index]);
	}

	// Multiply a scalar
	TMatrix3x3 &operator*= (double scalar)
	{
		(*this)[0] *= scalar;
		(*this)[1] *= scalar;
		(*this)[2] *= scalar;
		return *this;
	}

	friend TMatrix3x3 operator* (double s, const TMatrix3x3 &m)
	{
		return TMatrix3x3(m[0]*s, m[1]*s, m[2]*s);
	}

	TMatrix3x3 operator* (double s) const
	{
		return TMatrix3x3((*this)[0]*s, (*this)[1]*s, (*this)[2]*s);
	}

	// Divide a scalar
	TMatrix3x3 &operator/= (double scalar)
	{
		return *this *= 1 / scalar;
	}

	TMatrix3x3 operator/ (double s) const
	{
		return *this * (1 / s);
	}

	// Add two 3x3 matrices together
	TMatrix3x3 &operator+= (const TMatrix3x3 &o)
	{
		(*this)[0] += o[0];
		(*this)[1] += o[1];
		(*this)[2] += o[2];
		return *this;
	}

	TMatrix3x3 operator+ (const TMatrix3x3 &o) const
	{
		return TMatrix3x3((*this)[0] + o[0], (*this)[1] + o[1], (*this)[2] + o[2]);
	}

	// Subtract two 3x3 matrices
	TMatrix3x3 &operator-= (const TMatrix3x3 &o)
	{
		(*this)[0] -= o[0];
		(*this)[1] -= o[1];
		(*this)[2] -= o[2];
		return *this;
	}

	TMatrix3x3 operator- (const TMatrix3x3 &o) const
	{
		return TMatrix3x3((*this)[0] - o[0], (*this)[1] - o[1], (*this)[2] - o[2]);
	}

	// Concatenate two 3x3 matrices
	TMatrix3x3 &operator*= (const TMatrix3x3 &o)
	{
		return *this = *this * o;
	}

	TMatrix3x3 operator* (const TMatrix3x3 &o) const
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
	friend Vector3 operator* (const Vector3 &v, const TMatrix3x3 &m)
	{
		return Vector3(m[0] | v, m[1] | v, m[2] | v);
	}

	friend Vector3 operator* (const TMatrix3x3 &m, const Vector3 &v)
	{
		return Vector3(m[0] | v, m[1] | v, m[2] | v);
	}
};

#define BAM_FACTOR (90. / 0x40000000)

template<class vec_t>
struct TAngle
{
	vec_t Degrees;

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

	TAngle (vec_t amt)
		: Degrees(amt)
	{
	}

	TAngle(const TAngle &other) = default;
	TAngle &operator= (const TAngle &other) = default;

	TAngle &operator= (double other)
	{
		Degrees = (decltype(Degrees))other;
		return *this;
	}

	// intentionally disabled so that common math functions cannot be accidentally called with a TAngle.
	//operator vec_t() const { return Degrees; }

	TAngle operator- () const
	{
		return TAngle(-Degrees);
	}

	TAngle &operator+= (TAngle other)
	{
		Degrees += other.Degrees;
		return *this;
	}

	TAngle &operator-= (TAngle other)
	{
		Degrees -= other.Degrees;
		return *this;
	}

	TAngle &operator*= (TAngle other)
	{
		Degrees *= other.Degrees;
		return *this;
	}

	TAngle &operator/= (TAngle other)
	{
		Degrees /= other.Degrees;
		return *this;
	}

	TAngle operator+ (TAngle other) const
	{
		return Degrees + other.Degrees;
	}

	TAngle operator- (TAngle other) const
	{
		return Degrees - other.Degrees;
	}

	TAngle operator* (TAngle other) const
	{
		return Degrees * other.Degrees;
	}

	TAngle operator/ (TAngle other) const
	{
		return Degrees / other.Degrees;
	}

	TAngle &operator+= (vec_t other)
	{
		Degrees = Degrees + other;
		return *this;
	}

	TAngle &operator-= (vec_t other)
	{
		Degrees = Degrees - other;
		return *this;
	}

	TAngle &operator*= (vec_t other)
	{
		Degrees = Degrees * other;
		return *this;
	}

	TAngle &operator/= (vec_t other)
	{
		Degrees = Degrees / other;
		return *this;
	}

	TAngle operator+ (vec_t other) const
	{
		return Degrees + other;
	}

	TAngle operator- (vec_t other) const
	{
		return Degrees - other;
	}

	friend TAngle operator- (vec_t o1, TAngle o2)
	{
		return TAngle(o1 - o2.Degrees);
	}

	TAngle operator* (vec_t other) const
	{
		return Degrees * other;
	}

	TAngle operator/ (vec_t other) const
	{
		return Degrees / other;
	}

	// Should the comparisons consider an epsilon value?
	bool operator< (TAngle other) const
	{
		return Degrees < other.Degrees;
	}

	bool operator> (TAngle other) const
	{
		return Degrees > other.Degrees;
	}

	bool operator<= (TAngle other) const
	{
		return Degrees <= other.Degrees;
	}

	bool operator>= (TAngle other) const
	{
		return Degrees >= other.Degrees;
	}

	bool operator== (TAngle other) const
	{
		return Degrees == other.Degrees;
	}

	bool operator!= (TAngle other) const
	{
		return Degrees != other.Degrees;
	}

	bool operator< (vec_t other) const
	{
		return Degrees < other;
	}

	bool operator> (vec_t other) const
	{
		return Degrees > other;
	}

	bool operator<= (vec_t other) const
	{
		return Degrees <= other;
	}

	bool operator>= (vec_t other) const
	{
		return Degrees >= other;
	}

	bool operator== (vec_t other) const
	{
		return Degrees == other;
	}

	bool operator!= (vec_t other) const
	{
		return Degrees != other;
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

	vec_t Radians() const
	{
		return vec_t(Degrees * (pi::pi() / 180.0));
	}

	unsigned BAMs() const
	{
		return xs_CRoundToInt(Degrees * (0x40000000 / 90.));
	}

	TVector2<vec_t> ToVector(vec_t length = 1) const
	{
		return TVector2<vec_t>(length * Cos(), length * Sin());
	}

	vec_t Cos() const
	{
		return vec_t(g_cosdeg(Degrees));
	}

	vec_t Sin() const
	{
		return vec_t(g_sindeg(Degrees));
	}

	double Tan() const
	{
		return vec_t(g_tan(Radians()));
	}

	// This is for calculating vertical velocity. For high pitches the tangent will become too large to be useful.
	double TanClamped(double max = 5.) const
	{
		return clamp(Tan(), -max, max);
	}

	static inline TAngle ToDegrees(double rad)
	{
		return TAngle(double(rad * (180.0 / pi::pi())));
	}

};

// Emulates the old floatbob offset table with direct calls to trig functions.
inline double BobSin(double fb)
{
	return TAngle<double>(double(fb * (180.0 / 32))).Sin() * 8;
}

template<class T>
inline TAngle<T> fabs (const TAngle<T> &deg)
{
	return TAngle<T>(fabs(deg.Degrees));
}

template<class T>
inline TAngle<T> deltaangle(const TAngle<T> &a1, const TAngle<T> &a2)
{
	return (a2 - a1).Normalized180();
}

template<class T>
inline TAngle<T> deltaangle(const TAngle<T> &a1, double a2)
{
	return (a2 - a1).Normalized180();
}

template<class T>
inline TAngle<T> deltaangle(double a1, const TAngle<T> &a2)
{
	return (a2 - a1).Normalized180();
}

template<class T>
inline TAngle<T> absangle(const TAngle<T> &a1, const TAngle<T> &a2)
{
	return fabs((a1 - a2).Normalized180());
}

template<class T>
inline TAngle<T> absangle(const TAngle<T> &a1, double a2)
{
	return fabs((a1 - a2).Normalized180());
}

inline TAngle<double> VecToAngle(double x, double y)
{
	return g_atan2(y, x) * (180.0 / pi::pi());
}

template<class T>
inline TAngle<T> VecToAngle (const TVector2<T> &vec)
{
	return (T)g_atan2(vec.Y, vec.X) * (180.0 / pi::pi());
}

template<class T>
inline TAngle<T> VecToAngle (const TVector3<T> &vec)
{
	return (T)g_atan2(vec.Y, vec.X) * (180.0 / pi::pi());
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
	return -VecToAngle(TVector2<T>(X, Y).Length(), Z);
}

// Much of this is copied from TVector3. Is all that functionality really appropriate?
template<class vec_t>
struct TRotator
{
	typedef TAngle<vec_t> Angle;

	Angle Pitch;	// up/down
	Angle Yaw;		// left/right
	Angle Roll;		// rotation about the forward axis.
	Angle CamRoll;	// Roll specific to actor cameras. Used by quakes.

	TRotator() = default;

	TRotator (const Angle &p, const Angle &y, const Angle &r)
		: Pitch(p), Yaw(y), Roll(r)
	{
	}

	TRotator(const TRotator &other) = default;
	TRotator &operator= (const TRotator &other) = default;

	// Access angles as an array
	Angle &operator[] (int index)
	{
		return *(&Pitch + index);
	}

	const Angle &operator[] (int index) const
	{
		return *(&Pitch + index);
	}

	// Test for equality
	bool operator== (const TRotator &other) const
	{
		return fabs(Pitch - other.Pitch) < Angle(EQUAL_EPSILON) && fabs(Yaw - other.Yaw) < Angle(EQUAL_EPSILON) && fabs(Roll - other.Roll) < Angle(EQUAL_EPSILON);
	}

	// Test for inequality
	bool operator!= (const TRotator &other) const
	{
		return fabs(Pitch - other.Pitch) >= Angle(EQUAL_EPSILON) && fabs(Yaw - other.Yaw) >= Angle(EQUAL_EPSILON) && fabs(Roll - other.Roll) >= Angle(EQUAL_EPSILON);
	}

	// Unary negation
	TRotator operator- () const
	{
		return TRotator(-Pitch, -Yaw, -Roll);
	}

	// Scalar addition
	TRotator &operator+= (const Angle &scalar)
	{
		Pitch += scalar, Yaw += scalar, Roll += scalar;
		return *this;
	}

	friend TRotator operator+ (const TRotator &v, const Angle &scalar)
	{
		return TRotator(v.Pitch + scalar, v.Yaw + scalar, v.Roll + scalar);
	}

	friend TRotator operator+ (const Angle &scalar, const TRotator &v)
	{
		return TRotator(v.Pitch + scalar, v.Yaw + scalar, v.Roll + scalar);
	}

	// Scalar subtraction
	TRotator &operator-= (const Angle &scalar)
	{
		Pitch -= scalar, Yaw -= scalar, Roll -= scalar;
		return *this;
	}

	TRotator operator- (const Angle &scalar) const
	{
		return TRotator(Pitch - scalar, Yaw - scalar, Roll - scalar);
	}

	// Scalar multiplication
	TRotator &operator*= (const Angle &scalar)
	{
		Pitch *= scalar, Yaw *= scalar, Roll *= scalar;
		return *this;
	}

	friend TRotator operator* (const TRotator &v, const Angle &scalar)
	{
		return TRotator(v.Pitch * scalar, v.Yaw * scalar, v.Roll * scalar);
	}

	friend TRotator operator* (const Angle &scalar, const TRotator &v)
	{
		return TRotator(v.Pitch * scalar, v.Yaw * scalar, v.Roll * scalar);
	}

	// Scalar division
	TRotator &operator/= (const Angle &scalar)
	{
		Angle mul(1 / scalar.Degrees);
		Pitch *= scalar, Yaw *= scalar, Roll *= scalar;
		return *this;
	}

	TRotator operator/ (const Angle &scalar) const
	{
		Angle mul(1 / scalar.Degrees);
		return TRotator(Pitch * mul, Yaw * mul, Roll * mul);
	}

	// Vector addition
	TRotator &operator+= (const TRotator &other)
	{
		Pitch += other.Pitch, Yaw += other.Yaw, Roll += other.Roll;
		return *this;
	}

	TRotator operator+ (const TRotator &other) const
	{
		return TRotator(Pitch + other.Pitch, Yaw + other.Yaw, Roll + other.Roll);
	}

	// Vector subtraction
	TRotator &operator-= (const TRotator &other)
	{
		Pitch -= other.Pitch, Yaw -= other.Yaw, Roll -= other.Roll;
		return *this;
	}

	TRotator operator- (const TRotator &other) const
	{
		return TRotator(Pitch - other.Pitch, Yaw - other.Yaw, Roll - other.Roll);
	}
};

// Create a forward vector from a rotation (ignoring roll)

template<class T>
inline TVector3<T>::TVector3 (const TRotator<T> &rot)
{
	double pcos = rot.Pitch.Cos();
	X = pcos * rot.Yaw.Cos();
	Y = pcos * rot.Yaw.Sin();
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
typedef TAngle<float>		FAngle;

typedef TVector2<double>		DVector2;
typedef TVector3<double>		DVector3;
typedef TVector4<double>		DVector4;
typedef TRotator<double>		DRotator;
typedef TMatrix3x3<double>		DMatrix3x3;
typedef TAngle<double>			DAngle;


class Plane
{
public:
	void Set(FVector3 normal, float d)
	{
		m_normal = normal;
		m_d = d;
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

	bool PointOnSide(FVector3 &v) { return PointOnSide(v.X, v.Y, v.Z); }

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
