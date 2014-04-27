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
 * \version 0.2.3 (03-06-2013)
 *		Fixed a bug that appeared with nVidia drivers
 *			it only affected blocks
 *
 * \version 0.2.2
 *		Added a normal matrix based only in the view matrix
 *		Added a normal matrix based only in the model matrix
 *
 * \version 0.2.1
 *		Split MODELVIEW into MODEL and VIEW
 *			VIEW_MODEL is now a computed matrix
 *		Removed ALWAYS_SEND_TO_GL
 *
 * \version 0.2.0
 *		Added derived matrices
 *			Projection View Model
 *			Normal
 *		Added vector operations
 *		Library is now called vsMathLib
 *
 * \version 0.1.1
 *		Added multiplication of a matrix by a point
 *		Added AUX as a matrix type
 * 
 * \version 0.1.0
 *		Initial Release
 *
 *
 ---------------------------------------------------------------*/
#ifndef __VSMathLib__
#define __VSMathLib__

#include "tarray.h"


class VSMathLib {

	public:
		/// Enumeration of the matrix types
		enum MatrixTypes{ 
				MODEL,
				VIEW, 
				PROJECTION,
				AUX0,
				AUX1,
				AUX2,
				AUX3,
				COUNT_MATRICES
		} ; 
		/// Enumeration of derived matrices
		enum ComputedMatrixTypes {
			VIEW_MODEL,
			PROJ_VIEW_MODEL,
			PROJ_VIEW,
			NORMAL,
			NORMAL_VIEW,
			NORMAL_MODEL,
			COUNT_COMPUTED_MATRICES
		};


		/** Similar to gl Translate*. 
		  *
		  * \param aType any value from MatrixTypes
		  * \param x,y,z vector to perform the translation
		*/
		void translate(MatrixTypes aType, double x, double y, double z);

		/** Similar to gl Translate*. Applied to MODELVIEW only.
		  *
		  * \param x,y,z vector to perform the translation
		*/
		void translate(double x, double y, double z);

		/** Similar to gl Scale*.
		  *
		  * \param aType any value from MatrixTypes
		  * \param x,y,z scale factors
		*/
		void scale(MatrixTypes aType, double x, double y, double z);

		/** Similar to gl Scale*. Applied to MODELVIEW only.
		  *
		  * \param x,y,z scale factors
		*/
		void scale(double x, double y, double z);

		/** Similar to glTotate*. 
		  *
		  * \param aType any value from MatrixTypes
		  * \param angle rotation angle in degrees
		  * \param x,y,z rotation axis in degrees
		*/
		void rotate(MatrixTypes aType, double angle, double x, double y, double z);

		/** Similar to gl Rotate*. Applied to MODELVIEW only.
		  *
		  * \param angle rotation angle in degrees
		  * \param x,y,z rotation axis in degrees
		*/
		void rotate(double angle, double x, double y, double z);

		/** Similar to glLoadIdentity.
		  *
		  * \param aType any value from MatrixTypes
		*/
		void loadIdentity(MatrixTypes aType);

		/** Similar to glMultMatrix.
		  *
		  * \param aType any value from MatrixTypes
		  * \param aMatrix matrix in column major order data, double[16]
		*/
		void multMatrix(MatrixTypes aType, float *aMatrix);
		void multMatrix(MatrixTypes aType, double *aMatrix);

		/** Similar to gLoadMatrix.
		  *
		  * \param aType any value from MatrixTypes
		  * \param aMatrix matrix in column major order data, double[16]
		*/

		void loadMatrix(MatrixTypes aType, double *aMatrix);

		/** Similar to gl PushMatrix
		  * 
		  * \param aType any value from MatrixTypes
		*/
		void pushMatrix(MatrixTypes aType);

		/** Similar to gl PopMatrix
		  * 
		  * \param aType any value from MatrixTypes
		*/
		void popMatrix(MatrixTypes aType);

		/** Similar to gluLookAt
		  *
		  * \param xPos, yPos, zPos camera position
		  * \param xLook, yLook, zLook point to aim the camera at
		  * \param xUp, yUp, zUp camera's up vector
		*/
		void lookAt(double xPos, double yPos, double zPos,
					double xLook, double yLook, double zLook,
					double xUp, double yUp, double zUp);


		/** Similar to gluPerspective
		  *
		  * \param fov vertical field of view
		  * \param ratio aspect ratio of the viewport or window
		  * \param nearp,farp distance to the near and far planes
		*/
		void perspective(double fov, double ratio, double nearp, double farp);

		/** Similar to glOrtho and gluOrtho2D (just leave the 
		  * last two params blank).
		  *
		  * \param left,right coordinates for the left and right vertical 
		  * clipping planes
		  * \param bottom,top coordinates for the bottom and top horizontal 
		  * clipping planes
		  * \param nearp,farp distance to the near and far planes
		*/
		void ortho(double left, double right, double bottom, double top, 
						double nearp=-1.0f, double farp=1.0f);

		/** Similar to glFrustum
		  *
		  * \param left,right coordinates for the left and right vertical 
		  * clipping planes
		  * \param bottom,top coordinates for the bottom and top horizontal 
		  * clipping planes
		  * \param nearp,farp distance to the near and far planes
		*/
		void frustum(double left, double right, double bottom, double top, 
						double nearp, double farp);

		/** Similar to glGet
		  *
		  * \param aType any value from MatrixTypes
		  * \returns pointer to the matrix (double[16])
		*/
		double *get(MatrixTypes aType);

		void copy(double * pDest, MatrixTypes aType)
		{
			memcpy(pDest, mMatrix[aType], 16 * sizeof(double));
		}

		void copy(float * pDest, MatrixTypes aType)
		{
			for (int i = 0; i < 16; i++)
			{
				pDest[i] = (float)mMatrix[aType][i];
			}
		}

		/** Similar to glGet for computed matrices
		  *
		  * \param aType any value from ComputedMatrixTypes
		  * \returns pointer to the matrix (double[16] or double[9])
		*/
		double *get(ComputedMatrixTypes aType);

		/** Updates either the buffer or the uniform variables 
		  * based on if the block name has been set
		  *
		  * \param aType any value from MatrixTypes
		*/
		void matrixToGL(int location, MatrixTypes aType);	

		/** Updates either the buffer or the uniform variables 
		  * based on if the block name has been set
		  *
		  * \param aType any value from ComputedMatrixTypes
		*/
		void matrixToGL(int location, ComputedMatrixTypes aType);	


		int getLastUpdate(MatrixTypes aType) { return mUpdateTick[aType]; }


		/** Computes the multiplication of a matrix and a point 
		  *
		  * \param aType any value from MatrixTypes
		  * \param point a double[4] representing a point
		  * \param res a double[4] res = M * point
		*/
		void multMatrixPoint(MatrixTypes aType, double *point, double *res);

		/** Computes the multiplication of a computed matrix and a point 
		  *
		  * \param aType any value from ComputedMatrixTypes
		  * \param point a double[4] representing a point
		  * \param res a double[4] res = M * point
		*/
		void multMatrixPoint(ComputedMatrixTypes aType, double *point, double *res);

		/** vector cross product res = a x b
		  * Note: memory for the result must be allocatted by the caller
		  * 
		  * \param a,b the two input double[3]
		  * \param res the ouput result, a double[3]
		*/
		static void crossProduct( double *a, double *b, double *res);

		/** vector dot product 
		  * 
		  * \param a,b the two input double[3]
		  * \returns the dot product a.b
		*/
		static double dotProduct(double *a, double * b);

		/// normalize a vec3
		static void normalize(double *a);

		/// vector subtraction res = b - a
		static void subtract( double *a, double *b, double *res);

		/// vector addition res = a + b
		static void add( double *a, double *b, double *res);

		/// vector length
		static double length(double *a);

	protected:

		/// aux variable to hold the result of vector ops
		double mPointRes[4];

		/// Matrix stacks for all matrix types
		TArray<double> mMatrixStack[COUNT_MATRICES];

		/// The storage for matrices
		double mMatrix[COUNT_MATRICES][16];
		double mCompMatrix[COUNT_COMPUTED_MATRICES][16];
		unsigned int mUpdateTick[COUNT_MATRICES];


		/// The normal matrix
		double mNormal[12];
		double mNormal3x3[9];
		double mNormalView3x3[9];		
		double mNormalView[12];
		double mNormalModel[12];
		double mNormalModel3x3[9];
		
		/// aux 3x3 matrix
		double mMat3x3[9];

		// AUX FUNCTIONS

		/** Set a double* to an identity matrix
		  *
		  * \param a double array with the matrix contents
		  * \param size the order of the matrix
		*/
		void setIdentityMatrix( double *mat, int size=4);

		/// Computes the 3x4 normal matrix based on the modelview matrix
		void computeNormalMatrix();
		/// Computes the 3x3 normal matrix for use with glUniform
		void computeNormalMatrix3x3();
		/// Computes the 3x4 normal matrix considering only the view matrix
		void computeNormalViewMatrix3x3();
		/// Computes the 3x3 normal matrix for the view matrix for use with glUniform
		void computeNormalViewMatrix();
		/// Computes the 3x4 normal matrix considering only the model matrix
		void computeNormalModelMatrix3x3();
		/// Computes the 3x3 normal matrix for the model matrix for use with glUniform
		void computeNormalModelMatrix();

		/// Computes Derived Matrices (4x4)
		void computeDerivedMatrix(ComputedMatrixTypes aType);

		//resMatrix = resMatrix * aMatrix
		void multMatrix(double *resMatrix, double *aMatrix);


};

extern VSMathLib VSML;

#endif