/* --------------------------------------------------

Lighthouse3D

VSMatrix - Very Simple Matrix Library

http://www.lighthouse3d.com/very-simple-libs

This is a simplified version of VSMatrix that has been adjusted for GZDoom's needs.

----------------------------------------------------*/

#include <algorithm>
#include "gl/system/gl_system.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "doomtype.h"
#include "gl/data/gl_matrix.h"

static inline FLOATTYPE 
DegToRad(FLOATTYPE degrees) 
{ 
	return (FLOATTYPE)(degrees * (M_PI / 180.0f));
};

// sets the square matrix mat to the identity matrix,
// size refers to the number of rows (or columns)
void 
VSMatrix::setIdentityMatrix( FLOATTYPE *mat, int size) {

	// fill matrix with 0s
	for (int i = 0; i < size * size; ++i)
			mat[i] = 0.0f;

	// fill diagonal with 1s
	for (int i = 0; i < size; ++i)
		mat[i + i * size] = 1.0f;
}



// gl LoadIdentity implementation
void 
VSMatrix::loadIdentity()
{
	// fill matrix with 0s
	for (int i = 0; i < 16; ++i)
			mMatrix[i] = 0.0f;

	// fill diagonal with 1s
	for (int i = 0; i < 4; ++i)
		mMatrix[i + i * 4] = 1.0f;
}


// gl MultMatrix implementation
void 
VSMatrix::multMatrix(const FLOATTYPE *aMatrix)
{
	
	FLOATTYPE res[16];

	for (int i = 0; i < 4; ++i) 
	{
		for (int j = 0; j < 4; ++j) 
		{
			res[j*4 + i] = 0.0f;
			for (int k = 0; k < 4; ++k) 
			{
				res[j*4 + i] += mMatrix[k*4 + i] * aMatrix[j*4 + k]; 
			}
		}
	}
	memcpy(mMatrix, res, 16 * sizeof(FLOATTYPE));
}

#ifdef USE_DOUBLE
// gl MultMatrix implementation
void
VSMatrix::multMatrix(const float *aMatrix)
{

	FLOATTYPE res[16];

	for (int i = 0; i < 4; ++i) 
	{
		for (int j = 0; j < 4; ++j) 
		{
			res[j * 4 + i] = 0.0f;
			for (int k = 0; k < 4; ++k) 
			{
				res[j*4 + i] += mMatrix[k*4 + i] * aMatrix[j*4 + k]; 
			}
		}
	}
	memcpy(mMatrix, res, 16 * sizeof(FLOATTYPE));
}
#endif



// gl LoadMatrix implementation
void 
VSMatrix::loadMatrix(const FLOATTYPE *aMatrix)
{
	memcpy(mMatrix, aMatrix, 16 * sizeof(FLOATTYPE));
}

#ifdef USE_DOUBLE
// gl LoadMatrix implementation
void 
VSMatrix::loadMatrix(const float *aMatrix)
{
	for (int i = 0; i < 16; ++i)
	{
		mMatrix[i] = aMatrix[i];
	}
}
#endif


// gl Translate implementation
void 
VSMatrix::translate(FLOATTYPE x, FLOATTYPE y, FLOATTYPE z) 
{
	mMatrix[12] = mMatrix[0] * x + mMatrix[4] * y + mMatrix[8] * z + mMatrix[12];
	mMatrix[13] = mMatrix[1] * x + mMatrix[5] * y + mMatrix[9] * z + mMatrix[13];
	mMatrix[14] = mMatrix[2] * x + mMatrix[6] * y + mMatrix[10] * z + mMatrix[14];
}


// gl Scale implementation
void 
VSMatrix::scale(FLOATTYPE x, FLOATTYPE y, FLOATTYPE z) 
{
	mMatrix[0] *= x;   mMatrix[1] *= x;   mMatrix[2] *= x;   mMatrix[3] *= x;
	mMatrix[4] *= y;   mMatrix[5] *= y;   mMatrix[6] *= y;   mMatrix[7] *= y;
	mMatrix[8] *= z;   mMatrix[9] *= z;   mMatrix[10] *= z;   mMatrix[11] *= z;
}


// gl Rotate implementation
void 
VSMatrix::rotate(FLOATTYPE angle, FLOATTYPE x, FLOATTYPE y, FLOATTYPE z)
{
	FLOATTYPE mat[16];
	FLOATTYPE v[3];

	v[0] = x;
	v[1] = y;
	v[2] = z;

	FLOATTYPE radAngle = DegToRad(angle);
	FLOATTYPE co = cos(radAngle);
	FLOATTYPE si = sin(radAngle);
	normalize(v);
	FLOATTYPE x2 = v[0]*v[0];
	FLOATTYPE y2 = v[1]*v[1];
	FLOATTYPE z2 = v[2]*v[2];

//	mat[0] = x2 + (y2 + z2) * co; 
	mat[0] = co + x2 * (1 - co);// + (y2 + z2) * co; 
	mat[4] = v[0] * v[1] * (1 - co) - v[2] * si;
	mat[8] = v[0] * v[2] * (1 - co) + v[1] * si;
	mat[12]= 0.0f;
	   
	mat[1] = v[0] * v[1] * (1 - co) + v[2] * si;
//	mat[5] = y2 + (x2 + z2) * co;
	mat[5] = co + y2 * (1 - co);
	mat[9] = v[1] * v[2] * (1 - co) - v[0] * si;
	mat[13]= 0.0f;
	   
	mat[2] = v[0] * v[2] * (1 - co) - v[1] * si;
	mat[6] = v[1] * v[2] * (1 - co) + v[0] * si;
//	mat[10]= z2 + (x2 + y2) * co;
	mat[10]= co + z2 * (1 - co);
	mat[14]= 0.0f;
	   
	mat[3] = 0.0f;
	mat[7] = 0.0f;
	mat[11]= 0.0f;
	mat[15]= 1.0f;

	multMatrix(mat);
}


// gluLookAt implementation
void 
VSMatrix::lookAt(FLOATTYPE xPos, FLOATTYPE yPos, FLOATTYPE zPos,
					FLOATTYPE xLook, FLOATTYPE yLook, FLOATTYPE zLook,
					FLOATTYPE xUp, FLOATTYPE yUp, FLOATTYPE zUp)
{
	FLOATTYPE dir[3], right[3], up[3];

	up[0] = xUp;	up[1] = yUp;	up[2] = zUp;

	dir[0] =  (xLook - xPos);
	dir[1] =  (yLook - yPos);
	dir[2] =  (zLook - zPos);
	normalize(dir);

	crossProduct(dir,up,right);
	normalize(right);

	crossProduct(right,dir,up);
	normalize(up);

	FLOATTYPE m1[16],m2[16];

	m1[0]  = right[0];
	m1[4]  = right[1];
	m1[8]  = right[2];
	m1[12] = 0.0f;

	m1[1]  = up[0];
	m1[5]  = up[1];
	m1[9]  = up[2];
	m1[13] = 0.0f;

	m1[2]  = -dir[0];
	m1[6]  = -dir[1];
	m1[10] = -dir[2];
	m1[14] =  0.0f;

	m1[3]  = 0.0f;
	m1[7]  = 0.0f;
	m1[11] = 0.0f;
	m1[15] = 1.0f;

	setIdentityMatrix(m2,4);
	m2[12] = -xPos;
	m2[13] = -yPos;
	m2[14] = -zPos;

	multMatrix(m1);
	multMatrix(m2);
}


// gluPerspective implementation
void 
VSMatrix::perspective(FLOATTYPE fov, FLOATTYPE ratio, FLOATTYPE nearp, FLOATTYPE farp)
{
	FLOATTYPE f = 1.0f / tan (fov * (M_PI / 360.0f));

	loadIdentity();
	mMatrix[0] = f / ratio;
	mMatrix[1 * 4 + 1] = f;
	mMatrix[2 * 4 + 2] = (farp + nearp) / (nearp - farp);
	mMatrix[3 * 4 + 2] = (2.0f * farp * nearp) / (nearp - farp);
	mMatrix[2 * 4 + 3] = -1.0f;
	mMatrix[3 * 4 + 3] = 0.0f;
}


// gl Ortho implementation
void 
VSMatrix::ortho(FLOATTYPE left, FLOATTYPE right, 
			FLOATTYPE bottom, FLOATTYPE top, 
			FLOATTYPE nearp, FLOATTYPE farp)
{
	loadIdentity();

	mMatrix[0 * 4 + 0] = 2 / (right - left);
	mMatrix[1 * 4 + 1] = 2 / (top - bottom);	
	mMatrix[2 * 4 + 2] = -2 / (farp - nearp);
	mMatrix[3 * 4 + 0] = -(right + left) / (right - left);
	mMatrix[3 * 4 + 1] = -(top + bottom) / (top - bottom);
	mMatrix[3 * 4 + 2] = -(farp + nearp) / (farp - nearp);
}


// gl Frustum implementation
void 
VSMatrix::frustum(FLOATTYPE left, FLOATTYPE right, 
			FLOATTYPE bottom, FLOATTYPE top, 
			FLOATTYPE nearp, FLOATTYPE farp)
{
	FLOATTYPE m[16];

	setIdentityMatrix(m,4);

	m[0 * 4 + 0] = 2 * nearp / (right-left);
	m[1 * 4 + 1] = 2 * nearp / (top - bottom);
	m[2 * 4 + 0] = (right + left) / (right - left);
	m[2 * 4 + 1] = (top + bottom) / (top - bottom);
	m[2 * 4 + 2] = - (farp + nearp) / (farp - nearp);
	m[2 * 4 + 3] = -1.0f;
	m[3 * 4 + 2] = - 2 * farp * nearp / (farp-nearp);
	m[3 * 4 + 3] = 0.0f;

	multMatrix(m);
}


/*
// returns a pointer to the requested matrix
FLOATTYPE *
VSMatrix::get(MatrixTypes aType)
{
	return mMatrix[aType];
}
*/


/* -----------------------------------------------------
             SEND MATRICES TO OPENGL
------------------------------------------------------*/




// universal
void
VSMatrix::matrixToGL(int loc)
{
#ifdef USE_DOUBLE
	float copyto[16];
	copy(copyto);
	glUniformMatrix4fv(loc, 1, false, copyto);
#else
	glUniformMatrix4fv(loc, 1, false, mMatrix);
#endif
}

// -----------------------------------------------------
//                      AUX functions
// -----------------------------------------------------


// Compute res = M * point
void
VSMatrix::multMatrixPoint(const FLOATTYPE *point, FLOATTYPE *res) 
{

	for (int i = 0; i < 4; ++i) 
	{

		res[i] = 0.0f;
	
		for (int j = 0; j < 4; j++) {
		
			res[i] += point[j] * mMatrix[j*4 + i];
		} 
	}
}

// res = a cross b;
void 
VSMatrix::crossProduct(const FLOATTYPE *a, const FLOATTYPE *b, FLOATTYPE *res) {

	res[0] = a[1] * b[2]  -  b[1] * a[2];
	res[1] = a[2] * b[0]  -  b[2] * a[0];
	res[2] = a[0] * b[1]  -  b[0] * a[1];
}


// returns a . b
FLOATTYPE
VSMatrix::dotProduct(const FLOATTYPE *a, const FLOATTYPE *b) {

	FLOATTYPE res = a[0] * b[0]  +  a[1] * b[1]  +  a[2] * b[2];

	return res;
}


// Normalize a vec3
void 
VSMatrix::normalize(FLOATTYPE *a) {

	FLOATTYPE mag = sqrt(a[0] * a[0]  +  a[1] * a[1]  +  a[2] * a[2]);

	a[0] /= mag;
	a[1] /= mag;
	a[2] /= mag;
}


// res = b - a
void
VSMatrix::subtract(const FLOATTYPE *a, const FLOATTYPE *b, FLOATTYPE *res) {

	res[0] = b[0] - a[0];
	res[1] = b[1] - a[1];
	res[2] = b[2] - a[2];
}


// res = a + b
void
VSMatrix::add(const FLOATTYPE *a, const FLOATTYPE *b, FLOATTYPE *res) {

	res[0] = b[0] + a[0];
	res[1] = b[1] + a[1];
	res[2] = b[2] + a[2];
}


// returns |a|
FLOATTYPE
VSMatrix::length(const FLOATTYPE *a) {

	return(sqrt(a[0] * a[0]  +  a[1] * a[1]  +  a[2] * a[2]));

}


static inline int 
M3(int i, int j)
{ 
   return (i*3+j); 
};



// computes the derived normal matrix for the view matrix
void
VSMatrix::computeNormalMatrix(const FLOATTYPE *aMatrix) 
{

	FLOATTYPE mMat3x3[9];

	mMat3x3[0] = aMatrix[0];
	mMat3x3[1] = aMatrix[1];
	mMat3x3[2] = aMatrix[2];

	mMat3x3[3] = aMatrix[4];
	mMat3x3[4] = aMatrix[5];
	mMat3x3[5] = aMatrix[6];

	mMat3x3[6] = aMatrix[8];
	mMat3x3[7] = aMatrix[9];
	mMat3x3[8] = aMatrix[10];

	FLOATTYPE det, invDet;

	det = mMat3x3[0] * (mMat3x3[4] * mMat3x3[8] - mMat3x3[5] * mMat3x3[7]) +
		  mMat3x3[1] * (mMat3x3[5] * mMat3x3[6] - mMat3x3[8] * mMat3x3[3]) +
		  mMat3x3[2] * (mMat3x3[3] * mMat3x3[7] - mMat3x3[4] * mMat3x3[6]);

	invDet = 1.0f/det;

	mMatrix[0] = (mMat3x3[4] * mMat3x3[8] - mMat3x3[5] * mMat3x3[7]) * invDet;
	mMatrix[1] = (mMat3x3[5] * mMat3x3[6] - mMat3x3[8] * mMat3x3[3]) * invDet;
	mMatrix[2] = (mMat3x3[3] * mMat3x3[7] - mMat3x3[4] * mMat3x3[6]) * invDet;
	mMatrix[3] = 0.0f;
	mMatrix[4] = (mMat3x3[2] * mMat3x3[7] - mMat3x3[1] * mMat3x3[8]) * invDet;
	mMatrix[5] = (mMat3x3[0] * mMat3x3[8] - mMat3x3[2] * mMat3x3[6]) * invDet;
	mMatrix[6] = (mMat3x3[1] * mMat3x3[6] - mMat3x3[7] * mMat3x3[0]) * invDet;
	mMatrix[7] = 0.0f;
	mMatrix[8] = (mMat3x3[1] * mMat3x3[5] - mMat3x3[4] * mMat3x3[2]) * invDet;
	mMatrix[9] = (mMat3x3[2] * mMat3x3[3] - mMat3x3[0] * mMat3x3[5]) * invDet;
	mMatrix[10] =(mMat3x3[0] * mMat3x3[4] - mMat3x3[3] * mMat3x3[1]) * invDet;
	mMatrix[11] = 0.0;
	mMatrix[12] = 0.0;
	mMatrix[13] = 0.0;
	mMatrix[14] = 0.0;
	mMatrix[15] = 1.0;

}


// aux function resMat = resMat * aMatrix
void 
VSMatrix::multMatrix(FLOATTYPE *resMat, const FLOATTYPE *aMatrix)
{
	
	FLOATTYPE res[16];

	for (int i = 0; i < 4; ++i) 
	{
		for (int j = 0; j < 4; ++j) 
		{
			res[j*4 + i] = 0.0f;
			for (int k = 0; k < 4; ++k) 
			{
				res[j*4 + i] += resMat[k*4 + i] * aMatrix[j*4 + k]; 
			}
		}
	}
	memcpy(resMat, res, 16 * sizeof(FLOATTYPE));
}
