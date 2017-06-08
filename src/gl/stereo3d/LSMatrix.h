//
//---------------------------------------------------------------------------
//
// Copyright(C) 2016-2017 Christopher Bruns
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//
/*
** LSMatrix.h
** less-simple matrix class
*/

#ifndef VR_LS_MATRIX_H_
#define VR_LS_MATRIX_H_

#include "gl/data/gl_matrix.h"
#include "openvr.h"

namespace vr {
	HmdMatrix34_t;
}

class LSVec3
{
public:
	LSVec3(FLOATTYPE x, FLOATTYPE y, FLOATTYPE z, FLOATTYPE w=1.0f) 
		: x(mVec[0]), y(mVec[1]), z(mVec[2]), w(mVec[3])
	{
		mVec[0] = x;
		mVec[1] = y;
		mVec[2] = z;
		mVec[3] = w;
	}

	LSVec3(const LSVec3& rhs) 
		: x(mVec[0]), y(mVec[1]), z(mVec[2]), w(mVec[3])
	{
		*this = rhs;
	}

	LSVec3& operator=(const LSVec3& rhs) {
		LSVec3& lhs = *this;
		for (int i = 0; i < 4; ++i)
			lhs[i] = rhs[i];
		return *this;
	}

	const FLOATTYPE& operator[](int i) const {return mVec[i];}
	FLOATTYPE& operator[](int i) { return mVec[i]; }

	LSVec3 operator-(const LSVec3& rhs) const {
		const LSVec3& lhs = *this;
		LSVec3 result = *this;
		for (int i = 0; i < 4; ++i)
			result[i] -= rhs[i];
		return result;
	}

	FLOATTYPE mVec[4];
	FLOATTYPE& x;
	FLOATTYPE& y;
	FLOATTYPE& z;
	FLOATTYPE& w;
};

LSVec3 operator*(FLOATTYPE s, const LSVec3& rhs) {
	LSVec3 result = rhs;
	for (int i = 0; i < 4; ++i)
		result[i] *= s;
	return result;
}

class LSMatrix44 : public VSMatrix
{
public:
	LSMatrix44() 
	{
		loadIdentity();
	}
		
	LSMatrix44(const vr::HmdMatrix34_t& m) {
		loadIdentity();
		for (int i = 0; i < 3; ++i) {
			for (int j = 0; j < 4; ++j) {
				(*this)[i][j] = m.m[i][j];
			}
		}
	}

	LSMatrix44(const VSMatrix& m) {
		m.copy(mMatrix);
	}

	// overload bracket operator to return one row of the matrix, so you can invoke, say, m[2][3]
	FLOATTYPE* operator[](int i) {return &mMatrix[i*4];}
	const FLOATTYPE* operator[](int i) const { return &mMatrix[i * 4]; }

	LSMatrix44 operator*(const VSMatrix& rhs) const {
		LSMatrix44 result(*this);
		result.multMatrix(rhs);
		return result;
	}

	LSVec3 operator*(const LSVec3& rhs) const {
		const LSMatrix44& lhs = *this;
		LSVec3 result(0, 0, 0, 0);
		for (int i = 0; i < 4; ++i) {
			for (int j = 0; j < 4; ++j) {
				result[i] += lhs[i][j] * rhs[j];
			}
		}
		return result;
	}

	LSMatrix44 getWithoutTranslation() const {
		LSMatrix44 m = *this;
		// Remove translation component
		m[3][3] = 1.0f;
		m[3][2] = m[3][1] = m[3][0] = 0.0f;
		m[2][3] = m[1][3] = m[0][3] = 0.0f;
		return m;
	}

	LSMatrix44 transpose() const {
		LSMatrix44 result;
		for (int i = 0; i < 3; ++i) {
			for (int j = 0; j < 4; ++j) {
				result[i][j] = (*this)[j][i];
			}
		}
		return result;
	}

};

#endif // VR_LS_MATRIX_H_


