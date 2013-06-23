#ifndef __GL_GEOM
#define __GL_GEOM

#include "math.h"
#include "r_defs.h"

class Vector
{
public:
	Vector()
	{
	   SetX(0.f);
	   SetY(1.f);
	   SetZ(0.f);
	   m_length = 1.f;
	}

	Vector(float x, float y, float z)
	{
	   SetX(x);
	   SetY(y);
	   SetZ(z);
	   m_length=-1.0f;
	}

	Vector(float *v)
	{
	   SetX(v[0]);
	   SetY(v[1]);
	   SetZ(v[2]);
	   m_length=-1.0f;
	}

	Vector(vertex_t * v)
	{
		SetX(v->fx);
		SetY(v->fy);
		SetZ(0);
	}

	void Normalize()
	{
	   float l = 1.f / Length();

	   SetX(X() * l);
	   SetY(Y() * l);
	   SetZ(Z() * l);
	   m_length=1.0f;
	}

	void UpdateLength()
	{
	   m_length = sqrtf((X() * X()) + (Y() * Y()) + (Z() * Z()));
	}

	void Set(float *v)
	{
	   SetX(v[0]);
	   SetY(v[1]);
	   SetZ(v[2]);
	   m_length=-1.0f;
	}

	void Set(float x, float y, float z)
	{
	   SetX(x);
	   SetY(y);
	   SetZ(z);
	   m_length=-1.0f;
	}

	float Length()
	{
		if (m_length<0.0f) UpdateLength();
		return m_length;
	}

	float Dist(Vector &v)
	{
	   Vector t(X() - v.X(), Y() - v.Y(), Z() - v.Z());

	   return t.Length();
	}

	float Dot(Vector &v)
	{
	   return (X() * v.X()) + (Y() * v.Y()) + (Z() * v.Z());
	}
	
	Vector Cross(Vector &v);
	Vector operator- (Vector &v);
	Vector operator+ (Vector &v);
	Vector operator* (float f);
	Vector operator/ (float f);
	bool operator== (Vector &v);
	bool operator!= (Vector &v) { return !((*this) == v); }
	
	void GetRightUp(Vector &up, Vector &right);
	float operator[] (int index) const { return m_vec[index]; }
	float &operator[] (int index) { return m_vec[index]; }
	float X() const { return m_vec[0]; }
	float Y() const { return m_vec[1]; }
	float Z() const { return m_vec[2]; }
	void SetX(float x) { m_vec[0] = x; }
	void SetY(float y) { m_vec[1] = y; }
	void SetZ(float z) { m_vec[2] = z; }
	void Scale(float scale);
	
	Vector ProjectVector(Vector &a);
	Vector ProjectPlane(Vector &right, Vector &up);
protected:
	float m_vec[3];
	float m_length;
};


class Plane
{
public:
	Plane()
	{
	   m_normal.Set(0.f, 1.f, 0.f);
	   m_d = 0.f;
	}
   void Init(float *v1, float *v2, float *v3);
   void Init(float a, float b, float c, float d);
   void Init(float *verts, int numVerts);
   void Set(secplane_t &plane);
   float DistToPoint(float x, float y, float z);
   bool PointOnSide(float x, float y, float z);
   bool PointOnSide(Vector &v) { return PointOnSide(v.X(), v.Y(), v.Z()); }
   bool ValidNormal() { return m_normal.Length() == 1.f; }

   float A() { return m_normal.X(); }
   float B() { return m_normal.Y(); }
   float C() { return m_normal.Z(); }
   float D() { return m_d; }

   const Vector &Normal() const { return m_normal; }
protected:
   Vector m_normal;
   float m_d;
};

class Matrix3x4	// used like a 4x4 matrix with the last row always being (0,0,0,1)
{
	float m[3][4];

public:

	void MakeIdentity()
	{
		memset(m, 0, sizeof(m));
		m[0][0] = m[1][1] = m[2][2] = 1.f;
	}

	void Translate(float x, float y, float z)
	{
		m[0][3] = m[0][0]*x + m[0][1]*y + m[0][2]*z + m[0][3];
		m[1][3] = m[1][0]*x + m[1][1]*y + m[1][2]*z + m[1][3];
		m[2][3] = m[2][0]*x + m[2][1]*y + m[2][2]*z + m[2][3];
	}

	void Scale(float x, float y, float z)
	{
		m[0][0] *=x;
		m[1][0] *=x;
		m[2][0] *=x;
			
		m[0][1] *=y;
		m[1][1] *=y;
		m[2][1] *=y;
			
		m[0][2] *=z;
		m[1][2] *=z;
		m[2][2] *=z;
	}

	void Rotate(float ax, float ay, float az, float angle)
	{
		Matrix3x4 m1;

		Vector axis(ax, ay, az);
		axis.Normalize();
		double c = cos(angle * M_PI/180.), s = sin(angle * M_PI/180.), t = 1 - c;
		double sx = s*axis.X(), sy = s*axis.Y(), sz = s*axis.Z();
		double tx, ty, txx, tyy, u, v;

		tx = t*axis.X();
		m1.m[0][0] = float( (txx=tx*axis.X()) + c );
		m1.m[0][1] = float(   (u=tx*axis.Y()) - sz);
		m1.m[0][2] = float(   (v=tx*axis.Z()) + sy);

		ty = t*axis.Y();
		m1.m[1][0] = float(              u    + sz);
		m1.m[1][1] = float( (tyy=ty*axis.Y()) + c );
		m1.m[1][2] = float(   (u=ty*axis.Z()) - sx);

		m1.m[2][0] = float(              v  - sy);
		m1.m[2][1] = float(              u  + sx);
		m1.m[2][2] = float(     (t-txx-tyy) + c );

		m1.m[0][3] = 0.f;
		m1.m[1][3] = 0.f;
		m1.m[2][3] = 0.f;

		*this = (*this) * m1;
	}

	Matrix3x4 operator *(const Matrix3x4 &other)
	{
		Matrix3x4 result;

		result.m[0][0] = m[0][0]*other.m[0][0] + m[0][1]*other.m[1][0] + m[0][2]*other.m[2][0];
		result.m[0][1] = m[0][0]*other.m[0][1] + m[0][1]*other.m[1][1] + m[0][2]*other.m[2][1];
		result.m[0][2] = m[0][0]*other.m[0][2] + m[0][1]*other.m[1][2] + m[0][2]*other.m[2][2];
		result.m[0][3] = m[0][0]*other.m[0][3] + m[0][1]*other.m[1][3] + m[0][2]*other.m[2][3] + m[0][3];

		result.m[1][0] = m[1][0]*other.m[0][0] + m[1][1]*other.m[1][0] + m[1][2]*other.m[2][0];
		result.m[1][1] = m[1][0]*other.m[0][1] + m[1][1]*other.m[1][1] + m[1][2]*other.m[2][1];
		result.m[1][2] = m[1][0]*other.m[0][2] + m[1][1]*other.m[1][2] + m[1][2]*other.m[2][2];
		result.m[1][3] = m[1][0]*other.m[0][3] + m[1][1]*other.m[1][3] + m[1][2]*other.m[2][3] + m[1][3];

		result.m[2][0] = m[2][0]*other.m[0][0] + m[2][1]*other.m[1][0] + m[2][2]*other.m[2][0];
		result.m[2][1] = m[2][0]*other.m[0][1] + m[2][1]*other.m[1][1] + m[2][2]*other.m[2][1];
		result.m[2][2] = m[2][0]*other.m[0][2] + m[2][1]*other.m[1][2] + m[2][2]*other.m[2][2];
		result.m[2][3] = m[2][0]*other.m[0][3] + m[2][1]*other.m[1][3] + m[2][2]*other.m[2][3] + m[2][3];

		return result;
	}

	Vector operator *(const Vector &vec)
	{
		Vector result;

		result.SetX(vec.X()*m[0][0] + vec.Y()*m[0][1] + vec.Z()*m[0][2] + m[0][3]);
		result.SetY(vec.X()*m[1][0] + vec.Y()*m[1][1] + vec.Z()*m[1][2] + m[1][3]);
		result.SetZ(vec.X()*m[2][0] + vec.Y()*m[2][1] + vec.Z()*m[2][2] + m[2][3]);
		return result;
	}

	void MultiplyVector(float *f3 , float *f3o)
	{
		float x = f3[0] * m[0][0] + f3[1] * m[0][1] + f3[2] * m[0][2] + m[0][3];
		float y = f3[0] * m[1][0] + f3[1] * m[1][1] + f3[2] * m[1][2] + m[1][3];
		float z = f3[0] * m[2][0] + f3[1] * m[2][1] + f3[2] * m[2][2] + m[2][3];
		f3o[2] = z; f3o[1] = y; f3o[0] = x;
	}
};

#endif
