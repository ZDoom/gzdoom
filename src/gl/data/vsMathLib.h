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
		void translate(MatrixTypes aType, float x, float y, float z);

		/** Similar to gl Translate*. Applied to MODELVIEW only.
		  *
		  * \param x,y,z vector to perform the translation
		*/
		void translate(float x, float y, float z);

		/** Similar to gl Scale*.
		  *
		  * \param aType any value from MatrixTypes
		  * \param x,y,z scale factors
		*/
		void scale(MatrixTypes aType, float x, float y, float z);

		/** Similar to gl Scale*. Applied to MODELVIEW only.
		  *
		  * \param x,y,z scale factors
		*/
		void scale(float x, float y, float z);

		/** Similar to glTotate*. 
		  *
		  * \param aType any value from MatrixTypes
		  * \param angle rotation angle in degrees
		  * \param x,y,z rotation axis in degrees
		*/
		void rotate(MatrixTypes aType, float angle, float x, float y, float z);

		/** Similar to gl Rotate*. Applied to MODELVIEW only.
		  *
		  * \param angle rotation angle in degrees
		  * \param x,y,z rotation axis in degrees
		*/
		void rotate(float angle, float x, float y, float z);

		/** Similar to glLoadIdentity.
		  *
		  * \param aType any value from MatrixTypes
		*/
		void loadIdentity(MatrixTypes aType);

		/** Similar to glMultMatrix.
		  *
		  * \param aType any value from MatrixTypes
		  * \param aMatrix matrix in column major order data, float[16]
		*/
		void multMatrix(MatrixTypes aType, float *aMatrix);

		/** Similar to gLoadMatrix.
		  *
		  * \param aType any value from MatrixTypes
		  * \param aMatrix matrix in column major order data, float[16]
		*/

		void loadMatrix(MatrixTypes aType, float *aMatrix);

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
		void lookAt(float xPos, float yPos, float zPos,
					float xLook, float yLook, float zLook,
					float xUp, float yUp, float zUp);


		/** Similar to gluPerspective
		  *
		  * \param fov vertical field of view
		  * \param ratio aspect ratio of the viewport or window
		  * \param nearp,farp distance to the near and far planes
		*/
		void perspective(float fov, float ratio, float nearp, float farp);

		/** Similar to glOrtho and gluOrtho2D (just leave the 
		  * last two params blank).
		  *
		  * \param left,right coordinates for the left and right vertical 
		  * clipping planes
		  * \param bottom,top coordinates for the bottom and top horizontal 
		  * clipping planes
		  * \param nearp,farp distance to the near and far planes
		*/
		void ortho(float left, float right, float bottom, float top, 
						float nearp=-1.0f, float farp=1.0f);

		/** Similar to glFrustum
		  *
		  * \param left,right coordinates for the left and right vertical 
		  * clipping planes
		  * \param bottom,top coordinates for the bottom and top horizontal 
		  * clipping planes
		  * \param nearp,farp distance to the near and far planes
		*/
		void frustum(float left, float right, float bottom, float top, 
						float nearp, float farp);

		/** Similar to glGet
		  *
		  * \param aType any value from MatrixTypes
		  * \returns pointer to the matrix (float[16])
		*/
		float *get(MatrixTypes aType);

		/** Similar to glGet for computed matrices
		  *
		  * \param aType any value from ComputedMatrixTypes
		  * \returns pointer to the matrix (float[16] or float[9])
		*/
		float *get(ComputedMatrixTypes aType);

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
		  * \param point a float[4] representing a point
		  * \param res a float[4] res = M * point
		*/
		void multMatrixPoint(MatrixTypes aType, float *point, float *res);

		/** Computes the multiplication of a computed matrix and a point 
		  *
		  * \param aType any value from ComputedMatrixTypes
		  * \param point a float[4] representing a point
		  * \param res a float[4] res = M * point
		*/
		void multMatrixPoint(ComputedMatrixTypes aType, float *point, float *res);

		/** vector cross product res = a x b
		  * Note: memory for the result must be allocatted by the caller
		  * 
		  * \param a,b the two input float[3]
		  * \param res the ouput result, a float[3]
		*/
		static void crossProduct( float *a, float *b, float *res);

		/** vector dot product 
		  * 
		  * \param a,b the two input float[3]
		  * \returns the dot product a.b
		*/
		static float dotProduct(float *a, float * b);

		/// normalize a vec3
		static void normalize(float *a);

		/// vector subtraction res = b - a
		static void subtract( float *a, float *b, float *res);

		/// vector addition res = a + b
		static void add( float *a, float *b, float *res);

		/// vector length
		static float length(float *a);

	protected:

		/// aux variable to hold the result of vector ops
		float mPointRes[4];

		/// Matrix stacks for all matrix types
		TArray<float> mMatrixStack[COUNT_MATRICES];

		/// The storage for matrices
		float mMatrix[COUNT_MATRICES][16];
		float mCompMatrix[COUNT_COMPUTED_MATRICES][16];
		unsigned int mUpdateTick[COUNT_MATRICES];


		/// The normal matrix
		float mNormal[12];
		float mNormal3x3[9];
		float mNormalView3x3[9];		
		float mNormalView[12];
		float mNormalModel[12];
		float mNormalModel3x3[9];
		
		/// aux 3x3 matrix
		float mMat3x3[9];

		// AUX FUNCTIONS

		/** Set a float* to an identity matrix
		  *
		  * \param a float array with the matrix contents
		  * \param size the order of the matrix
		*/
		void setIdentityMatrix( float *mat, int size=4);

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
		void multMatrix(float *resMatrix, float *aMatrix);


};

extern VSMathLib VSML;

#endif