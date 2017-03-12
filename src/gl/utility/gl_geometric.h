/*
** gl_geometric.h
**
**---------------------------------------------------------------------------
** Copyright 2003 Timothy Stump
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
*/

#ifndef __GL_GEOM
#define __GL_GEOM

#include "math.h"
#include "r_defs.h"
#include "gl/scene/gl_wall.h"

struct GLSeg;

class Plane
{
public:
	void Set(GLSeg *seg)
	{
		m_normal = seg->Normal();
		m_d = m_normal | FVector3(-seg->x1, 0, -seg->y1);
	}

	void Set(secplane_t &plane)
	{
		m_normal = { (float)plane.Normal().X, (float)plane.Normal().Z, (float)plane.Normal().Y };
		m_d = (float)plane.fD();
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
	bool ValidNormal() { return m_normal.LengthSquared() == 1.f; }

	float A() { return m_normal.X; }
	float B() { return m_normal.Y; }
	float C() { return m_normal.Z; }
	float D() { return m_d; }

	const FVector3 &Normal() const { return m_normal; }
protected:
	FVector3 m_normal;
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

		FVector3 axis(ax, ay, az);
		axis.MakeUnit();
		double c = cos(angle * M_PI/180.), s = sin(angle * M_PI/180.), t = 1 - c;
		double sx = s*axis.X, sy = s*axis.Y, sz = s*axis.Z;
		double tx, ty, txx, tyy, u, v;

		tx = t*axis.X;
		m1.m[0][0] = float( (txx=tx*axis.X) + c );
		m1.m[0][1] = float(   (u=tx*axis.Y) - sz);
		m1.m[0][2] = float(   (v=tx*axis.Z) + sy);

		ty = t*axis.Y;
		m1.m[1][0] = float(              u    + sz);
		m1.m[1][1] = float( (tyy=ty*axis.Y) + c );
		m1.m[1][2] = float(   (u=ty*axis.Z) - sx);

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

	FVector3 operator *(const FVector3 &vec)
	{
		FVector3 result;

		result.X = vec.X*m[0][0] + vec.Y*m[0][1] + vec.Z*m[0][2] + m[0][3];
		result.Y = vec.X*m[1][0] + vec.Y*m[1][1] + vec.Z*m[1][2] + m[1][3];
		result.Z = vec.X*m[2][0] + vec.Y*m[2][1] + vec.Z*m[2][2] + m[2][3];
		return result;
	}
};

#endif
