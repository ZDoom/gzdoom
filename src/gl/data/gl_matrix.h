
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

#ifdef USE_DOUBLE
typedef double FLOATTYPE;
#else
typedef float FLOATTYPE;
#endif

class VSMatrix {

	public:

		VSMatrix()
		{
		}
		
		VSMatrix(int)
		{
			loadIdentity();
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

		void matrixToGL(int location);	
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

#endif