
// Matrix class based on code from VSML:

/** ----------------------------------------------------------
 * \class VSMathLib
 *
 * Lighthouse3D
 *
 * VSMathLib - Very Simple Matrix Library
 *
 * Full documentation at 
 * http://www.lighthouse3d.com/very-simple-libs
 *
 * This class aims at easing geometric transforms, camera
 * placement and projection definition for programmers
 * working with OpenGL core versions.
 *
 *
 ---------------------------------------------------------------*/
#ifndef __VSMatrix__
#define __VSMatrix__

#include <stdlib.h>
#include "vectors.h"

#ifdef USE_DOUBLE
typedef double FLOATTYPE;
#else
typedef float FLOATTYPE;
#endif

class VSMatrix {

	public:

		VSMatrix() = default;

		VSMatrix(int)
		{
			loadIdentity();
		}

		static VSMatrix identity()
		{
			VSMatrix m;
			m.loadIdentity();
			return m;
		}

		void translate(FLOATTYPE x, FLOATTYPE y, FLOATTYPE z);
		void scale(FLOATTYPE x, FLOATTYPE y, FLOATTYPE z);
		void rotate(FLOATTYPE angle, FLOATTYPE x, FLOATTYPE y, FLOATTYPE z);
		void loadIdentity();
#ifdef USE_DOUBLE
		void multMatrix(const float *aMatrix);
#endif
		void multVector(FLOATTYPE *aVector);
		void multMatrix(const FLOATTYPE *aMatrix);
		void multMatrix(const VSMatrix &aMatrix)
		{
			multMatrix(aMatrix.mMatrix);
		}
		void multQuaternion(const TVector4<FLOATTYPE>& q);
		void loadMatrix(const FLOATTYPE *aMatrix);
#ifdef USE_DOUBLE
		void loadMatrix(const float *aMatrix);
#endif
		void lookAt(FLOATTYPE xPos, FLOATTYPE yPos, FLOATTYPE zPos, FLOATTYPE xLook, FLOATTYPE yLook, FLOATTYPE zLook, FLOATTYPE xUp, FLOATTYPE yUp, FLOATTYPE zUp);
		void perspective(FLOATTYPE fov, FLOATTYPE ratio, FLOATTYPE nearp, FLOATTYPE farp);
		void ortho(FLOATTYPE left, FLOATTYPE right, FLOATTYPE bottom, FLOATTYPE top, FLOATTYPE nearp=-1.0f, FLOATTYPE farp=1.0f);
		void frustum(FLOATTYPE left, FLOATTYPE right, FLOATTYPE bottom, FLOATTYPE top, FLOATTYPE nearp, FLOATTYPE farp);
		void copy(FLOATTYPE * pDest)
		{
			memcpy(pDest, mMatrix, 16 * sizeof(FLOATTYPE));
		}

#ifdef USE_DOUBLE
		void copy(float * pDest)
		{
			for (int i = 0; i < 16; i++)
			{
				pDest[i] = (float)mMatrix[i];
			}
		}
#endif

		const FLOATTYPE *get() const
		{
			return mMatrix;
		}

		void multMatrixPoint(const FLOATTYPE *point, FLOATTYPE *res);

#ifdef USE_DOUBLE
		void computeNormalMatrix(const float *aMatrix);
#endif
		void computeNormalMatrix(const FLOATTYPE *aMatrix);
		void computeNormalMatrix(const VSMatrix &aMatrix)
		{
			computeNormalMatrix(aMatrix.mMatrix);
		}
		bool inverseMatrix(VSMatrix &result);
		void transpose();

	protected:
		static void crossProduct(const FLOATTYPE *a, const FLOATTYPE *b, FLOATTYPE *res);
		static FLOATTYPE dotProduct(const FLOATTYPE *a, const FLOATTYPE * b);
		static void normalize(FLOATTYPE *a);
		static void subtract(const FLOATTYPE *a, const FLOATTYPE *b, FLOATTYPE *res);
		static void add(const FLOATTYPE *a, const FLOATTYPE *b, FLOATTYPE *res);
		static FLOATTYPE length(const FLOATTYPE *a);
		static void multMatrix(FLOATTYPE *resMatrix, const FLOATTYPE *aMatrix);

		static void setIdentityMatrix(FLOATTYPE *mat, int size = 4);

		/// The storage for matrices
		FLOATTYPE mMatrix[16];

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
		double c = cos(angle * pi::pi()/180.), s = sin(angle * pi::pi()/180.), t = 1 - c;
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