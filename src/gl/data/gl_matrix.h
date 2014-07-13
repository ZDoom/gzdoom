
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

class VSMatrix {

	public:

		VSMatrix()
		{
		}
		
		VSMatrix(int)
		{
			loadIdentity();
		}

		void translate(double x, double y, double z);
		void scale(double x, double y, double z);
		void rotate(double angle, double x, double y, double z);
		void loadIdentity();
		void multMatrix(const float *aMatrix);
		void multMatrix(const double *aMatrix);
		void multMatrix(const VSMatrix &aMatrix)
		{
			multMatrix(aMatrix.mMatrix);
		}
		void loadMatrix(const double *aMatrix);
		void loadMatrix(const float *aMatrix);
		void lookAt(double xPos, double yPos, double zPos, double xLook, double yLook, double zLook, double xUp, double yUp, double zUp);
		void perspective(double fov, double ratio, double nearp, double farp);
		void ortho(double left, double right, double bottom, double top, double nearp=-1.0f, double farp=1.0f);
		void frustum(double left, double right, double bottom, double top, double nearp, double farp);
		void copy(double * pDest)
		{
			memcpy(pDest, mMatrix, 16 * sizeof(double));
		}

		void copy(float * pDest)
		{
			for (int i = 0; i < 16; i++)
			{
				pDest[i] = (float)mMatrix[i];
			}
		}

		void matrixToGL(int location);	
		void multMatrixPoint(const double *point, double *res);

		void computeNormalMatrix(const float *aMatrix);
		void computeNormalMatrix(const double *aMatrix);
		void computeNormalMatrix(const VSMatrix &aMatrix)
		{
			computeNormalMatrix(aMatrix.mMatrix);
		}
		
	protected:
		static void crossProduct(const double *a, const double *b, double *res);
		static double dotProduct(const double *a, const double * b);
		static void normalize(double *a);
		static void subtract(const double *a, const double *b, double *res);
		static void add(const double *a, const double *b, double *res);
		static double length(const double *a);
		static void multMatrix(double *resMatrix, const double *aMatrix);

		static void setIdentityMatrix(double *mat, int size = 4);

		/// The storage for matrices
		double mMatrix[16];

};

#endif