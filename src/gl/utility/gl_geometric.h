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

#endif
