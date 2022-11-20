#pragma once

#include "vectors.h"

template<typename vec_t>
class TQuaternion
{
public:
	typedef TVector2<vec_t> Vector2;
	typedef TVector3<vec_t> Vector3;

	vec_t X, Y, Z, W;

	TQuaternion() = default;

	TQuaternion(vec_t x, vec_t y, vec_t z, vec_t w)
		: X(x), Y(y), Z(z), W(w)
	{
	}

	TQuaternion(vec_t *o)
		: X(o[0]), Y(o[1]), Z(o[2]), W(o[3])
	{
	}

	TQuaternion(const TQuaternion &other) = default;

	TQuaternion(const Vector3 &v, vec_t s)
		: X(v.X), Y(v.Y), Z(v.Z), W(s)
	{
	}

	TQuaternion(const vec_t v[4])
		: TQuaternion(v[0], v[1], v[2], v[3])
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

	TQuaternion &operator= (const TQuaternion &other) = default;

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
	bool operator== (const TQuaternion &other) const
	{
		return X == other.X && Y == other.Y && Z == other.Z && W == other.W;
	}

	// Test for inequality
	bool operator!= (const TQuaternion &other) const
	{
		return X != other.X || Y != other.Y || Z != other.Z || W != other.W;
	}

	// returns the XY fields as a 2D-vector.
	const Vector2& XY() const
	{
		return *reinterpret_cast<const Vector2*>(this);
	}

	Vector2& XY()
	{
		return *reinterpret_cast<Vector2*>(this);
	}

	// returns the XY fields as a 2D-vector.
	const Vector3& XYZ() const
	{
		return *reinterpret_cast<const Vector3*>(this);
	}

	Vector3& XYZ()
	{
		return *reinterpret_cast<Vector3*>(this);
	}


	// Test for approximate equality
	bool ApproximatelyEquals(const TQuaternion &other) const
	{
		return fabs(X - other.X) < EQUAL_EPSILON && fabs(Y - other.Y) < EQUAL_EPSILON && fabs(Z - other.Z) < EQUAL_EPSILON && fabs(W - other.W) < EQUAL_EPSILON;
	}

	// Test for approximate inequality
	bool DoesNotApproximatelyEqual(const TQuaternion &other) const
	{
		return fabs(X - other.X) >= EQUAL_EPSILON || fabs(Y - other.Y) >= EQUAL_EPSILON || fabs(Z - other.Z) >= EQUAL_EPSILON || fabs(W - other.W) >= EQUAL_EPSILON;
	}

	// Unary negation
	TQuaternion operator- () const
	{
		return TQuaternion(-X, -Y, -Z, -W);
	}

	// Scalar addition
	TQuaternion &operator+= (vec_t scalar)
	{
		X += scalar, Y += scalar, Z += scalar; W += scalar;
		return *this;
	}

	friend TQuaternion operator+ (const TQuaternion &v, vec_t scalar)
	{
		return TQuaternion(v.X + scalar, v.Y + scalar, v.Z + scalar, v.W + scalar);
	}

	friend TQuaternion operator+ (vec_t scalar, const TQuaternion &v)
	{
		return TQuaternion(v.X + scalar, v.Y + scalar, v.Z + scalar, v.W + scalar);
	}

	// Scalar subtraction
	TQuaternion &operator-= (vec_t scalar)
	{
		X -= scalar, Y -= scalar, Z -= scalar, W -= scalar;
		return *this;
	}

	TQuaternion operator- (vec_t scalar) const
	{
		return TQuaternion(X - scalar, Y - scalar, Z - scalar, W - scalar);
	}

	// Scalar multiplication
	TQuaternion &operator*= (vec_t scalar)
	{
		X = vec_t(X *scalar), Y = vec_t(Y * scalar), Z = vec_t(Z * scalar), W = vec_t(W * scalar);
		return *this;
	}

	friend TQuaternion operator* (const TQuaternion &v, vec_t scalar)
	{
		return TQuaternion(v.X * scalar, v.Y * scalar, v.Z * scalar, v.W * scalar);
	}

	friend TQuaternion operator* (vec_t scalar, const TQuaternion &v)
	{
		return TQuaternion(v.X * scalar, v.Y * scalar, v.Z * scalar, v.W * scalar);
	}

	// Scalar division
	TQuaternion &operator/= (vec_t scalar)
	{
		scalar = 1 / scalar, X = vec_t(X * scalar), Y = vec_t(Y * scalar), Z = vec_t(Z * scalar), W = vec_t(W * scalar);
		return *this;
	}

	TQuaternion operator/ (vec_t scalar) const
	{
		scalar = 1 / scalar;
		return TQuaternion(X * scalar, Y * scalar, Z * scalar, W * scalar);
	}

	// Vector addition
	TQuaternion &operator+= (const TQuaternion &other)
	{
		X += other.X, Y += other.Y, Z += other.Z, W += other.W;
		return *this;
	}

	TQuaternion operator+ (const TQuaternion &other) const
	{
		return TQuaternion(X + other.X, Y + other.Y, Z + other.Z, W + other.W);
	}

	// Vector subtraction
	TQuaternion &operator-= (const TQuaternion &other)
	{
		X -= other.X, Y -= other.Y, Z -= other.Z, W -= other.W;
		return *this;
	}

	TQuaternion operator- (const TQuaternion &other) const
	{
		return TQuaternion(X - other.X, Y - other.Y, Z - other.Z, W - other.W);
	}

	// Quaternion length
	double Length() const
	{
		return g_sqrt(X*X + Y*Y + Z*Z + W*W);
	}

	double LengthSquared() const
	{
		return X*X + Y*Y + Z*Z + W*W;
	}
	
	double Sum() const
	{
		return abs(X) + abs(Y) + abs(Z) + abs(W);
	}
	

	// Return a unit vector facing the same direction as this one
	TQuaternion Unit() const
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
	TQuaternion &MakeResize(double len)
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

	TQuaternion Resized(double len) const 
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
	vec_t operator | (const TQuaternion &other) const
	{
		return X*other.X + Y*other.Y + Z*other.Z + W*other.W;
	}

	vec_t dot(const TQuaternion &other) const
	{
		return X*other.X + Y*other.Y + Z*other.Z + W*other.W;
	}

	TQuaternion& operator*= (const TQuaternion& q)
	{
		*this = *this * q;
		return *this;
	}

	friend TQuaternion<vec_t> operator* (const TQuaternion<vec_t>& q1, const TQuaternion<vec_t>& q2)
	{
		return TQuaternion(
			q1.W * q2.X + q1.X * q2.W + q1.Y * q2.Z - q1.Z * q2.Y,
			q1.W * q2.Y - q1.X * q2.Z + q1.Y * q2.W + q1.Z * q2.X,
			q1.W * q2.Z + q1.X * q2.Y - q1.Y * q2.X + q1.Z * q2.W,
			q1.W * q2.W - q1.X * q2.X - q1.Y * q2.Y - q1.Z * q2.Z
		);
	}

	// Rotate Vector3 by Quaternion q
	friend TVector3<vec_t> operator* (const TQuaternion<vec_t>& q, const TVector3<vec_t>& v)
	{
		auto r = TQuaternion({ v.X, v.Y, v.Z, 0 }) * TQuaternion({ -q.X, -q.Y, -q.Z, q.W });
		r = q * r;
		return TVector3(r.X, r.Y, r.Z);
	}

	TQuaternion<vec_t> Conjugate()
	{
		return TQuaternion(-X, -Y, -Z, +W);
	}
	TQuaternion<vec_t> Inverse()
	{
		return Conjugate() / LengthSquared();
	}

	static TQuaternion<vec_t> AxisAngle(TVector3<vec_t> axis, TAngle<vec_t> angle)
	{
		auto lengthSquared = axis.LengthSquared();
		auto halfAngle = angle * 0.5;
		auto sinTheta = halfAngle.Sin();
		auto cosTheta = halfAngle.Cos();
		auto factor = sinTheta / g_sqrt(lengthSquared);
		TQuaternion<vec_t> ret;
		ret.W = cosTheta;
		ret.XYZ() = factor * axis;
		return ret;
	}
	static TQuaternion<vec_t> FromAngles(TAngle<vec_t> yaw, TAngle<vec_t> pitch, TAngle<vec_t> roll)
	{
		auto zRotation = TQuaternion::AxisAngle(Vector3(vec_t{0.0}, vec_t{0.0}, vec_t{1.0}), yaw);
		auto yRotation = TQuaternion::AxisAngle(Vector3(vec_t{0.0}, vec_t{1.0}, vec_t{0.0}), pitch);
		auto xRotation = TQuaternion::AxisAngle(Vector3(vec_t{1.0}, vec_t{0.0}, vec_t{0.0}), roll);
		return zRotation * yRotation * xRotation;
	}

	static TQuaternion<vec_t> NLerp(TQuaternion<vec_t> from, TQuaternion<vec_t> to, vec_t t)
	{
		return (from * (vec_t{1.0} - t) + to * t).Unit();
	}
	static TQuaternion<vec_t> SLerp(TQuaternion<vec_t> from, TQuaternion<vec_t> to, vec_t t)
	{
		auto dot = from.dot(to);
		const auto dotThreshold = vec_t{0.9995};
		if (dot < vec_t{0.0})
		{
			to = -to;
			dot = -dot;
		}
		if (dot > dotThreshold)
		{
			return NLerp(from, to, t);
		}
		else
		{
			auto robustDot = clamp(dot, vec_t{-1.0}, vec_t{1.0});
			auto theta = TAngle<vec_t>::fromRad(g_acos(robustDot));
			auto scale0 = (theta * (vec_t{1.0} - t)).Sin();
			auto scale1 = (theta * t).Sin();
			return (from * scale0 + to * scale1).Unit();
		}
	}
};

typedef TQuaternion<float>	FQuaternion;
typedef TQuaternion<double>		DQuaternion;
