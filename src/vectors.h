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

#include <math.h>
#include <string.h>

#ifndef PI
#define PI				3.14159265358979323846		// matches value in gcc v2 math.h
#endif

#define EQUAL_EPSILON (1/65536.f)


#define DEG2RAD(d) ((d)*PI/180.f)
#define RAD2DEG(r) ((r)*180.f/PI)

template<class vec_t> struct TVector3;
template<class vec_t> struct TRotator;
template<class vec_t> struct TAngle;

template<class vec_t>
struct TVector2
{
	vec_t X, Y;

	TVector2 ()
	{
	}

	TVector2 (double a, double b)
		: X(vec_t(a)), Y(vec_t(b))
	{
	}

	TVector2 (const TVector2 &other)
		: X(other.X), Y(other.Y)
	{
	}

	TVector2 (const TVector3<vec_t> &other)	// Copy the X and Y from the 3D vector and discard the Z
		: X(other.X), Y(other.Y)
	{
	}

	void Zero()
	{
		Y = X = 0;
	}

	TVector2 &operator= (const TVector2 &other)
	{
		// This might seem backwards, but this helps produce smaller code when a newly
		// created vector is assigned, because the components can just be popped off
		// the FPU stack in order without the need for fxch. For platforms with a
		// more sensible registered-based FPU, of course, the order doesn't matter.
		// (And, yes, I know fxch can improve performance in the right circumstances,
		// but this isn't one of those times. Here, it's little more than a no-op that
		// makes the exe 2 bytes larger whenever you assign one vector to another.)
		Y = other.Y, X = other.X;
		return *this;
	}

	// Access X and Y as an array
	vec_t &operator[] (int index)
	{
		return *(&X + index);
	}

	const vec_t &operator[] (int index) const
	{
		return *(&X + index);
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

	friend TVector2 operator+ (const TVector2 &v, double scalar)
	{
		return TVector2(v.X + scalar, v.Y + scalar);
	}

	friend TVector2 operator+ (double scalar, const TVector2 &v)
	{
		return TVector2(v.X + scalar, v.Y + scalar);
	}

	// Scalar subtraction
	TVector2 &operator-= (double scalar)
	{
		X -= scalar, Y -= scalar;
		return *this;
	}

	TVector2 operator- (double scalar) const
	{
		return TVector2(X - scalar, Y - scalar);
	}

	// Scalar multiplication
	TVector2 &operator*= (double scalar)
	{
		X *= scalar, Y *= scalar;
		return *this;
	}

	friend TVector2 operator* (const TVector2 &v, double scalar)
	{
		return TVector2(v.X * scalar, v.Y * scalar);
	}

	friend TVector2 operator* (double scalar, const TVector2 &v)
	{
		return TVector2(v.X * scalar, v.Y * scalar);
	}

	// Scalar division
	TVector2 &operator/= (double scalar)
	{
		scalar = 1 / scalar, X *= scalar, Y *= scalar;
		return *this;
	}

	TVector2 operator/ (double scalar) const
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
	double Length() const
	{
		return sqrt (X*X + Y*Y);
	}

	double LengthSquared() const
	{
		return X*X + Y*Y;
	}

	// Return a unit vector facing the same direction as this one
	TVector2 Unit() const
	{
		double len = Length();
		if (len != 0) len = 1 / len;
		return *this * len;
	}

	// Scales this vector into a unit vector. Returns the old length
	double MakeUnit()
	{
		double len, ilen;
		len = ilen = Length();
		if (ilen != 0) ilen = 1 / ilen;
		*this *= ilen;
		return len;
	}

	// Dot product
	double operator | (const TVector2 &other) const
	{
		return X*other.X + Y*other.Y;
	}

	// Returns the angle (in radians) that the ray (0,0)-(X,Y) faces
	double Angle() const
	{
		return atan2 (X, Y);
	}

	// Returns a rotated vector. TAngle is in radians.
	TVector2 Rotated (double angle)
	{
		double cosval = cos (angle);
		double sinval = sin (angle);
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

	TVector3 ()
	{
	}

	TVector3 (double a, double b, double c)
		: X(vec_t(a)), Y(vec_t(b)), Z(vec_t(c))
	{
	}

	TVector3 (const TVector3 &other)
		: X(other.X), Y(other.Y), Z(other.Z)
	{
	}

	TVector3 (const Vector2 &xy, double z)
		: X(xy.X), Y(xy.Y), Z(z)
	{
	}

	TVector3 (const TRotator<vec_t> &rot);

	void Zero()
	{
		Z = Y = X = 0;
	}

	TVector3 &operator= (const TVector3 &other)
	{
		Z = other.Z, Y = other.Y, X = other.X;
		return *this;
	}

	// Access X and Y and Z as an array
	vec_t &operator[] (int index)
	{
		return *(&X + index);
	}

	const vec_t &operator[] (int index) const
	{
		return *(&X + index);
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
	TVector3 &operator+= (double scalar)
	{
		X += scalar, Y += scalar, Z += scalar;
		return *this;
	}

	friend TVector3 operator+ (const TVector3 &v, double scalar)
	{
		return TVector3(v.X + scalar, v.Y + scalar, v.Z + scalar);
	}

	friend TVector3 operator+ (double scalar, const TVector3 &v)
	{
		return TVector3(v.X + scalar, v.Y + scalar, v.Z + scalar);
	}

	// Scalar subtraction
	TVector3 &operator-= (double scalar)
	{
		X -= scalar, Y -= scalar, Z -= scalar;
		return *this;
	}

	TVector3 operator- (double scalar) const
	{
		return TVector3(X - scalar, Y - scalar, Z - scalar);
	}

	// Scalar multiplication
	TVector3 &operator*= (double scalar)
	{
		X = vec_t(X *scalar), Y = vec_t(Y * scalar), Z = vec_t(Z * scalar);
		return *this;
	}

	friend TVector3 operator* (const TVector3 &v, double scalar)
	{
		return TVector3(v.X * scalar, v.Y * scalar, v.Z * scalar);
	}

	friend TVector3 operator* (double scalar, const TVector3 &v)
	{
		return TVector3(v.X * scalar, v.Y * scalar, v.Z * scalar);
	}

	// Scalar division
	TVector3 &operator/= (double scalar)
	{
		scalar = 1 / scalar, X = vec_t(X * scalar), Y = vec_t(Y * scalar), Z = vec_t(Z * scalar);
		return *this;
	}

	TVector3 operator/ (double scalar) const
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

	// Add a 3D vector and a 2D vector.
	// Discards the Z component of the 3D vector and returns a 2D vector.
	friend Vector2 operator+ (const TVector3 &v3, const Vector2 &v2)
	{
		return Vector2(v3.X + v2.X, v3.Y + v2.Y);
	}

	friend Vector2 operator+ (const Vector2 &v2, const TVector3 &v3)
	{
		return Vector2(v2.X + v3.X, v2.Y + v3.Y);
	}

	// Subtract a 3D vector and a 2D vector.
	// Discards the Z component of the 3D vector and returns a 2D vector.
	friend Vector2 operator- (const TVector3 &v3, const Vector2 &v2)
	{
		return Vector2(v3.X - v2.X, v3.Y - v2.Y);
	}

	friend Vector2 operator- (const TVector2<vec_t> &v2, const TVector3 &v3)
	{
		return Vector2(v2.X - v3.X, v2.Y - v3.Y);
	}

	// Vector length
	double Length() const
	{
		return sqrt (X*X + Y*Y + Z*Z);
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
		return *this * len;
	}

	// Scales this vector into a unit vector
	void MakeUnit()
	{
		double len = Length();
		if (len != 0) len = 1 / len;
		*this *= len;
	}

	// Resizes this vector to be the specified length (if it is not 0)
	TVector3 &Resize(double len)
	{
		double nowlen = Length();
		X = vec_t(X * (len /= nowlen));
		Y = vec_t(Y * len);
		Z = vec_t(Z * len);
		return *this;
	}

	// Dot product
	double operator | (const TVector3 &other) const
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
	}
};

template<class vec_t>
struct TMatrix3x3
{
	typedef TVector3<vec_t> Vector3;

	vec_t Cells[3][3];

	TMatrix3x3()
	{
	}

	TMatrix3x3(const TMatrix3x3 &other)
	{
		(*this)[0] = other[0];
		(*this)[1] = other[1];
		(*this)[2] = other[2];
	}

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
		double c = cos(radians), s = sin(radians), t = 1 - c;
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

template<class vec_t>
struct TAngle
{
	vec_t Degrees;

	TAngle ()
	{
	}

	TAngle (float amt)
		: Degrees(amt)
	{
	}

	TAngle (double amt)
		: Degrees(vec_t(amt))
	{
	}

	TAngle (int amt)
		: Degrees(vec_t(amt))
	{
	}

	TAngle (const TAngle &other)
		: Degrees(other.Degrees)
	{
	}

	TAngle &operator= (const TAngle &other)
	{
		Degrees = other.Degrees;
		return *this;
	}

	TAngle &operator= (double other)
	{
		Degrees = other;
		return *this;
	}

	operator float() const { return Degrees; }
	operator double() const { return Degrees; }

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

	TAngle &operator+= (double other)
	{
		Degrees = vec_t(Degrees + other);
		return *this;
	}

	TAngle &operator-= (double other)
	{
		Degrees = vec_t(Degrees - other);
		return *this;
	}

	TAngle &operator*= (double other)
	{
		Degrees = vec_t(Degrees * other);
		return *this;
	}

	TAngle &operator/= (double other)
	{
		Degrees = vec_t(Degrees / other);
		return *this;
	}

	TAngle operator+ (double other) const
	{
		return Degrees + other;
	}

	TAngle operator- (double other) const
	{
		return Degrees - other;
	}

	friend TAngle operator- (double o1, TAngle o2)
	{
		return TAngle(o1 - o2.Degrees);
	}

	TAngle operator* (double other) const
	{
		return Degrees * vec_t(other);
	}

	TAngle operator/ (double other) const
	{
		return Degrees / vec_t(other);
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

	bool operator< (double other) const
	{
		return Degrees < other;
	}

	bool operator> (double other) const
	{
		return Degrees > other;
	}

	bool operator<= (double other) const
	{
		return Degrees <= other;
	}

	bool operator>= (double other) const
	{
		return Degrees >= other;
	}

	bool operator== (double other) const
	{
		return Degrees == other;
	}

	bool operator!= (double other) const
	{
		return Degrees != other;
	}

	// Ensure the angle is between [0.0,360.0) degrees
	TAngle &Normalize360()
	{
		// Normalizing the angle converts it to a BAM, masks it, and converts it back to a float.

		// This could have been kept entirely in floating point using fmod(), but the MSVCRT has lots
		// of overhead for that function, despite the x87 offering the FPREM instruction which does
		// exactly what fmod() is supposed to do. So fmod ends up being an order of magnitude slower
		// than casting to and from an int.

		// Casting Degrees to a volatile ensures that the compiler will not try to evaluate an expression
		// such as "TAngle a(360*100+24); a.Normalize360();" at compile time. Normally, it would see that
		// this expression is constant and attempt to remove the Normalize360() call entirely and store
		// the result of the function in the TAngle directly. Unfortunately, it does not do the casting
		// properly and will overflow, producing an incorrect result. So we need to make sure it always
		// evaluates Normalize360 at run time and never at compile time. (This applies to VC++. I don't
		// know if other compilers suffer similarly).
		Degrees = vec_t((int(*(volatile vec_t *)&Degrees * ((1<<30)/360.0)) & ((1<<30)-1)) * (360.f/(1<<30)));
		return *this;
	}

	// Ensures the angle is between (-180.0,180.0] degrees
	TAngle &Normalize180()
	{
		Degrees = vec_t((((int(*(volatile vec_t *)&Degrees * ((1<<30)/360.0))+(1<<29)-1) & ((1<<30)-1)) - (1<<29)+1) * (360.f/(1<<30)));
		return *this;
	}

	// Like Normalize360(), except the integer value is not converted back to a float.
	// The steps parameter must be a power of 2.
	int Quantize(int steps)
	{
		return int(*(volatile vec_t *)&Degrees * (steps/360.0)) & (steps-1);
	}
};

template<class T>
inline double ToRadians (const TAngle<T> &deg)
{
	return double(deg.Degrees * (PI / 180.0));
}

template<class T>
inline TAngle<T> ToDegrees (double rad)
{
	return TAngle<T> (double(rad * (180.0 / PI)));
}

template<class T>
inline double cos (const TAngle<T> &deg)
{
	return cos(ToRadians(deg));
}

template<class T>
inline double sin (const TAngle<T> &deg)
{
	return sin(ToRadians(deg));
}

template<class T>
inline double tan (const TAngle<T> &deg)
{
	return tan(ToRadians(deg));
}

template<class T>
inline TAngle<T> fabs (const TAngle<T> &deg)
{
	return TAngle<T>(fabs(deg.Degrees));
}

template<class T>
inline TAngle<T> vectoyaw (const TVector2<T> &vec)
{
	return atan2(vec.Y, vec.X) * (180.0 / PI);
}

template<class T>
inline TAngle<T> vectoyaw (const TVector3<T> &vec)
{
	return atan2(vec.Y, vec.X) * (180.0 / PI);
}

// Much of this is copied from TVector3. Is all that functionality really appropriate?
template<class vec_t>
struct TRotator
{
	typedef TAngle<vec_t> Angle;

	Angle Pitch;	// up/down
	Angle Yaw;		// left/right
	Angle Roll;		// rotation about the forward axis

	TRotator ()
	{
	}

	TRotator (const Angle &p, const Angle &y, const Angle &r)
		: Pitch(p), Yaw(y), Roll(r)
	{
	}

	TRotator (const TRotator &other)
		: Pitch(other.Pitch), Yaw(other.Yaw), Roll(other.Roll)
	{
	}

	TRotator &operator= (const TRotator &other)
	{
		Roll = other.Roll, Yaw = other.Yaw, Pitch = other.Pitch;
		return *this;
	}

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

	// Normalize each component
	TRotator &Normalize180 ()
	{
		for (int i = -3; i; ++i)
		{
			(*this)[i+3].Normalize180();
		}
		return *this;
	}

	TRotator &Normalize360 ()
	{
		for (int i = -3; i; ++i)
		{
			(*this)[i+3].Normalize360();
		}
		return *this;
	}
};

// Create a forward vector from a rotation (ignoring roll)

template<class T>
inline TVector3<T>::TVector3 (const TRotator<T> &rot)
: X(cos(rot.Pitch)*cos(rot.Yaw)), Y(cos(rot.Pitch)*sin(rot.Yaw)), Z(-sin(rot.Pitch))
{
}

template<class T>
inline TMatrix3x3<T>::TMatrix3x3(const TVector3<T> &axis, TAngle<T> degrees)
{
	double c = cos(degrees), s = sin(degrees), t = 1 - c;
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
typedef TRotator<float>		FRotator;
typedef TMatrix3x3<float>	FMatrix3x3;
typedef TAngle<float>		FAngle;

#endif
