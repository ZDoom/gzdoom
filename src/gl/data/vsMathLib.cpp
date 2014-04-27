/* --------------------------------------------------

Lighthouse3D

VSMathLib - Very Simple Matrix Library

http://www.lighthouse3d.com/very-simple-libs

This is a simplified version of VSMathLib that has been adjusted for GZDoom's needs.

----------------------------------------------------*/

#include "gl/system/gl_system.h"
#include "vsMathLib.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "doomtype.h"

// This var keeps track of the single instance of VSMathLib
VSMathLib VSML;

static inline double 
DegToRad(double degrees) 
{ 
	return (double)(degrees * (M_PI / 180.0f));
};


// gl PushMatrix implementation
void 
VSMathLib::pushMatrix(MatrixTypes aType) 
{
	unsigned int index = mMatrixStack[aType].Reserve(16);
	memcpy(&mMatrixStack[aType][index], mMatrix[aType], sizeof(double) * 16);
}


// gl PopMatrix implementation
void 
VSMathLib::popMatrix(MatrixTypes aType) 
{
	unsigned int index = mMatrixStack[aType].Size();
	if (index >= 16) 
	{
		memcpy(mMatrix[aType], &mMatrixStack[aType][index-16], sizeof(double) * 16);
		mMatrixStack[aType].Resize(index-16);
		mUpdateTick[aType]++;
	}
}


// glLoadIdentity implementation
void 
VSMathLib::loadIdentity(MatrixTypes aType)
{
	setIdentityMatrix(mMatrix[aType]);
	mUpdateTick[aType]++;
}


// glMultMatrix implementation
void 
VSMathLib::multMatrix(MatrixTypes aType, double *aMatrix)
{
	
	double *a, *b, res[16];
	a = mMatrix[aType];
	b = aMatrix;

	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			res[j*4 + i] = 0.0f;
			for (int k = 0; k < 4; ++k) {
				res[j*4 + i] += a[k*4 + i] * b[j*4 + k]; 
			}
		}
	}
	memcpy(mMatrix[aType], res, 16 * sizeof(double));
	mUpdateTick[aType]++;
}

// glMultMatrix implementation
void
VSMathLib::multMatrix(MatrixTypes aType, float *aMatrix)
{

	double *a, res[16];
	a = mMatrix[aType];
	float *b = aMatrix;

	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			res[j * 4 + i] = 0.0f;
			for (int k = 0; k < 4; ++k) {
				res[j * 4 + i] += a[k * 4 + i] * b[j * 4 + k];
			}
		}
	}
	memcpy(mMatrix[aType], res, 16 * sizeof(double));
	mUpdateTick[aType]++;
}




// glLoadMatrix implementation
void 
VSMathLib::loadMatrix(MatrixTypes aType, double *aMatrix)
{
	memcpy(mMatrix[aType], aMatrix, 16 * sizeof(double));
	mUpdateTick[aType]++;
}


// gl Translate implementation with matrix selection
void 
VSMathLib::translate(MatrixTypes aType, double x, double y, double z) 
{
	double mat[16];

	setIdentityMatrix(mat);
	mat[12] = x;
	mat[13] = y;
	mat[14] = z;

	multMatrix(aType,mat);
	mUpdateTick[aType]++;
}


// gl Translate on the MODEL matrix
void 
VSMathLib::translate(double x, double y, double z) 
{
	translate(MODEL, x,y,z);
}


// gl Scale implementation with matrix selection
void 
VSMathLib::scale(MatrixTypes aType, double x, double y, double z) 
{
	double mat[16];

	setIdentityMatrix(mat,4);
	mat[0] = x;
	mat[5] = y;
	mat[10] = z;

	multMatrix(aType,mat);
	mUpdateTick[aType]++;
}


// gl Scale on the MODELVIEW matrix
void 
VSMathLib::scale(double x, double y, double z) 
{
	scale(MODEL, x, y, z);
}


// gl Rotate implementation with matrix selection
void 
VSMathLib::rotate(MatrixTypes aType, double angle, double x, double y, double z)
{
	double mat[16];
	double v[3];

	v[0] = x;
	v[1] = y;
	v[2] = z;

	double radAngle = DegToRad(angle);
	double co = cos(radAngle);
	double si = sin(radAngle);
	normalize(v);
	double x2 = v[0]*v[0];
	double y2 = v[1]*v[1];
	double z2 = v[2]*v[2];

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

	multMatrix(aType,mat);
	mUpdateTick[aType]++;
}


// gl Rotate implementation in the MODELVIEW matrix
void 
VSMathLib::rotate(double angle, double x, double y, double z)
{
	rotate(MODEL,angle,x,y,z);
}


// gluLookAt implementation
void 
VSMathLib::lookAt(double xPos, double yPos, double zPos,
					double xLook, double yLook, double zLook,
					double xUp, double yUp, double zUp)
{
	double dir[3], right[3], up[3];

	up[0] = xUp;	up[1] = yUp;	up[2] = zUp;

	dir[0] =  (xLook - xPos);
	dir[1] =  (yLook - yPos);
	dir[2] =  (zLook - zPos);
	normalize(dir);

	crossProduct(dir,up,right);
	normalize(right);

	crossProduct(right,dir,up);
	normalize(up);

	double m1[16],m2[16];

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

	multMatrix(VIEW, m1);
	multMatrix(VIEW, m2);
}


// gluPerspective implementation
void 
VSMathLib::perspective(double fov, double ratio, double nearp, double farp)
{
	double projMatrix[16];

	double f = 1.0f / tan (fov * (M_PI / 360.0f));

	setIdentityMatrix(projMatrix,4);

	projMatrix[0] = f / ratio;
	projMatrix[1 * 4 + 1] = f;
	projMatrix[2 * 4 + 2] = (farp + nearp) / (nearp - farp);
	projMatrix[3 * 4 + 2] = (2.0f * farp * nearp) / (nearp - farp);
	projMatrix[2 * 4 + 3] = -1.0f;
	projMatrix[3 * 4 + 3] = 0.0f;

	multMatrix(PROJECTION, projMatrix);
}


// glOrtho implementation
void 
VSMathLib::ortho(double left, double right, 
			double bottom, double top, 
			double nearp, double farp)
{
	double m[16];

	setIdentityMatrix(m,4);

	m[0 * 4 + 0] = 2 / (right - left);
	m[1 * 4 + 1] = 2 / (top - bottom);	
	m[2 * 4 + 2] = -2 / (farp - nearp);
	m[3 * 4 + 0] = -(right + left) / (right - left);
	m[3 * 4 + 1] = -(top + bottom) / (top - bottom);
	m[3 * 4 + 2] = -(farp + nearp) / (farp - nearp);

	multMatrix(PROJECTION, m);
}


// glFrustum implementation
void 
VSMathLib::frustum(double left, double right, 
			double bottom, double top, 
			double nearp, double farp)
{
	double m[16];

	setIdentityMatrix(m,4);

	m[0 * 4 + 0] = 2 * nearp / (right-left);
	m[1 * 4 + 1] = 2 * nearp / (top - bottom);
	m[2 * 4 + 0] = (right + left) / (right - left);
	m[2 * 4 + 1] = (top + bottom) / (top - bottom);
	m[2 * 4 + 2] = - (farp + nearp) / (farp - nearp);
	m[2 * 4 + 3] = -1.0f;
	m[3 * 4 + 2] = - 2 * farp * nearp / (farp-nearp);
	m[3 * 4 + 3] = 0.0f;

	multMatrix(PROJECTION, m);
}


// returns a pointer to the requested matrix
double *
VSMathLib::get(MatrixTypes aType)
{
	return mMatrix[aType];
}


// returns a pointer to the requested matrix
double *
VSMathLib::get(ComputedMatrixTypes aType)
{
	switch (aType) {
	
		case NORMAL: 
			computeNormalMatrix3x3();
			return mNormal3x3; 
			break;
		case NORMAL_VIEW: 
			computeNormalViewMatrix3x3();
			return mNormalView3x3; 
			break;
		case NORMAL_MODEL: 
			computeNormalModelMatrix3x3();
			return mNormalModel3x3; 
			break;
		default:
			computeDerivedMatrix(aType);
			return mCompMatrix[aType]; 
			break;
	}
	// this should never happen!
	return NULL;
}

/* -----------------------------------------------------
             SEND MATRICES TO OPENGL
------------------------------------------------------*/




// universal
void
VSMathLib::matrixToGL(int loc, MatrixTypes aType)
{
	float copyto[16];
	copy(copyto, aType);
	glUniformMatrix4fv(loc, 1, false, copyto);
}

void
VSMathLib::matrixToGL(int loc, ComputedMatrixTypes aType)
{
	/*
	if (aType == NORMAL) 
	{
		computeNormalMatrix3x3();
		glUniformMatrix3fv(loc, 1, false, mNormal3x3);
	}
	else
	{
		computeDerivedMatrix(aType);
		glUniformMatrix4fv(loc, 1, false, mCompMatrix[aType]);	
	}
	*/
}


// -----------------------------------------------------
//                      AUX functions
// -----------------------------------------------------

// sets the square matrix mat to the identity matrix,
// size refers to the number of rows (or columns)
void 
VSMathLib::setIdentityMatrix( double *mat, int size) {

	// fill matrix with 0s
	for (int i = 0; i < size * size; ++i)
			mat[i] = 0.0f;

	// fill diagonal with 1s
	for (int i = 0; i < size; ++i)
		mat[i + i * size] = 1.0f;
}


// Compute res = M * point
void
VSMathLib::multMatrixPoint(MatrixTypes aType, double *point, double *res) {

	for (int i = 0; i < 4; ++i) {

		res[i] = 0.0f;
	
		for (int j = 0; j < 4; j++) {
		
			res[i] += point[j] * mMatrix[aType][j*4 + i];
		} 
	}
}

// Compute res = M * point
void
VSMathLib::multMatrixPoint(ComputedMatrixTypes aType, double *point, double *res) {


	if (aType == NORMAL) {
		computeNormalMatrix();
		for (int i = 0; i < 3; ++i) {

			res[i] = 0.0f;
	
			for (int j = 0; j < 3; j++) {
		
				res[i] += point[j] * mNormal[j*4 + i];
			} 
		}
	}
	else if (aType == NORMAL_VIEW) {
		computeNormalViewMatrix();
		for (int i = 0; i < 3; ++i) {

			res[i] = 0.0f;
	
			for (int j = 0; j < 3; j++) {
		
				res[i] += point[j] * mNormalView[j*4 + i];
			} 
		}
	}
	else if (aType == NORMAL_MODEL) {
		computeNormalModelMatrix();
		for (int i = 0; i < 3; ++i) {

			res[i] = 0.0f;
	
			for (int j = 0; j < 3; j++) {
		
				res[i] += point[j] * mNormalModel[j*4 + i];
			} 
		}
	}

	else {
		computeDerivedMatrix(aType);
		for (int i = 0; i < 4; ++i) {

			res[i] = 0.0f;
	
			for (int j = 0; j < 4; j++) {
		
				res[i] += point[j] * mCompMatrix[aType][j*4 + i];
			} 
		}
	}
}

// res = a cross b;
void 
VSMathLib::crossProduct( double *a, double *b, double *res) {

	res[0] = a[1] * b[2]  -  b[1] * a[2];
	res[1] = a[2] * b[0]  -  b[2] * a[0];
	res[2] = a[0] * b[1]  -  b[0] * a[1];
}


// returns a . b
double
VSMathLib::dotProduct(double *a, double *b) {

	double res = a[0] * b[0]  +  a[1] * b[1]  +  a[2] * b[2];

	return res;
}


// Normalize a vec3
void 
VSMathLib::normalize(double *a) {

	double mag = sqrt(a[0] * a[0]  +  a[1] * a[1]  +  a[2] * a[2]);

	a[0] /= mag;
	a[1] /= mag;
	a[2] /= mag;
}


// res = b - a
void
VSMathLib::subtract(double *a, double *b, double *res) {

	res[0] = b[0] - a[0];
	res[1] = b[1] - a[1];
	res[2] = b[2] - a[2];
}


// res = a + b
void
VSMathLib::add(double *a, double *b, double *res) {

	res[0] = b[0] + a[0];
	res[1] = b[1] + a[1];
	res[2] = b[2] + a[2];
}


// returns |a|
double
VSMathLib::length(double *a) {

	return(sqrt(a[0] * a[0]  +  a[1] * a[1]  +  a[2] * a[2]));

}


static inline int 
M3(int i, int j)
{ 
   return (i*3+j); 
};


// computes the derived normal matrix
void
VSMathLib::computeNormalMatrix() {

	computeDerivedMatrix(VIEW_MODEL);

	mMat3x3[0] = mCompMatrix[VIEW_MODEL][0];
	mMat3x3[1] = mCompMatrix[VIEW_MODEL][1];
	mMat3x3[2] = mCompMatrix[VIEW_MODEL][2];

	mMat3x3[3] = mCompMatrix[VIEW_MODEL][4];
	mMat3x3[4] = mCompMatrix[VIEW_MODEL][5];
	mMat3x3[5] = mCompMatrix[VIEW_MODEL][6];

	mMat3x3[6] = mCompMatrix[VIEW_MODEL][8];
	mMat3x3[7] = mCompMatrix[VIEW_MODEL][9];
	mMat3x3[8] = mCompMatrix[VIEW_MODEL][10];

	double det, invDet;

	det = mMat3x3[0] * (mMat3x3[4] * mMat3x3[8] - mMat3x3[5] * mMat3x3[7]) +
		  mMat3x3[1] * (mMat3x3[5] * mMat3x3[6] - mMat3x3[8] * mMat3x3[3]) +
		  mMat3x3[2] * (mMat3x3[3] * mMat3x3[7] - mMat3x3[4] * mMat3x3[6]);

	invDet = 1.0f/det;

	mNormal[0] = (mMat3x3[4] * mMat3x3[8] - mMat3x3[5] * mMat3x3[7]) * invDet;
	mNormal[1] = (mMat3x3[5] * mMat3x3[6] - mMat3x3[8] * mMat3x3[3]) * invDet;
	mNormal[2] = (mMat3x3[3] * mMat3x3[7] - mMat3x3[4] * mMat3x3[6]) * invDet;
	mNormal[3] = 0.0f;
	mNormal[4] = (mMat3x3[2] * mMat3x3[7] - mMat3x3[1] * mMat3x3[8]) * invDet;
	mNormal[5] = (mMat3x3[0] * mMat3x3[8] - mMat3x3[2] * mMat3x3[6]) * invDet;
	mNormal[6] = (mMat3x3[1] * mMat3x3[6] - mMat3x3[7] * mMat3x3[0]) * invDet;
	mNormal[7] = 0.0f;
	mNormal[8] = (mMat3x3[1] * mMat3x3[5] - mMat3x3[4] * mMat3x3[2]) * invDet;
	mNormal[9] = (mMat3x3[2] * mMat3x3[3] - mMat3x3[0] * mMat3x3[5]) * invDet;
	mNormal[10] =(mMat3x3[0] * mMat3x3[4] - mMat3x3[3] * mMat3x3[1]) * invDet;
	mNormal[11] = 0.0;

}


// computes the derived normal matrix for the view matrix
void
VSMathLib::computeNormalViewMatrix() {

	mMat3x3[0] = mMatrix[VIEW][0];
	mMat3x3[1] = mMatrix[VIEW][1];
	mMat3x3[2] = mMatrix[VIEW][2];

	mMat3x3[3] = mMatrix[VIEW][4];
	mMat3x3[4] = mMatrix[VIEW][5];
	mMat3x3[5] = mMatrix[VIEW][6];

	mMat3x3[6] = mMatrix[VIEW][8];
	mMat3x3[7] = mMatrix[VIEW][9];
	mMat3x3[8] = mMatrix[VIEW][10];

	double det, invDet;

	det = mMat3x3[0] * (mMat3x3[4] * mMat3x3[8] - mMat3x3[5] * mMat3x3[7]) +
		  mMat3x3[1] * (mMat3x3[5] * mMat3x3[6] - mMat3x3[8] * mMat3x3[3]) +
		  mMat3x3[2] * (mMat3x3[3] * mMat3x3[7] - mMat3x3[4] * mMat3x3[6]);

	invDet = 1.0f/det;

	mNormalView[0] = (mMat3x3[4] * mMat3x3[8] - mMat3x3[5] * mMat3x3[7]) * invDet;
	mNormalView[1] = (mMat3x3[5] * mMat3x3[6] - mMat3x3[8] * mMat3x3[3]) * invDet;
	mNormalView[2] = (mMat3x3[3] * mMat3x3[7] - mMat3x3[4] * mMat3x3[6]) * invDet;
	mNormalView[3] = 0.0f;
	mNormalView[4] = (mMat3x3[2] * mMat3x3[7] - mMat3x3[1] * mMat3x3[8]) * invDet;
	mNormalView[5] = (mMat3x3[0] * mMat3x3[8] - mMat3x3[2] * mMat3x3[6]) * invDet;
	mNormalView[6] = (mMat3x3[1] * mMat3x3[6] - mMat3x3[7] * mMat3x3[0]) * invDet;
	mNormalView[7] = 0.0f;
	mNormalView[8] = (mMat3x3[1] * mMat3x3[5] - mMat3x3[4] * mMat3x3[2]) * invDet;
	mNormalView[9] = (mMat3x3[2] * mMat3x3[3] - mMat3x3[0] * mMat3x3[5]) * invDet;
	mNormalView[10] =(mMat3x3[0] * mMat3x3[4] - mMat3x3[3] * mMat3x3[1]) * invDet;
	mNormalView[11] = 0.0;

}


// computes the derived normal matrix for the model matrix
void
VSMathLib::computeNormalModelMatrix() {

	mMat3x3[0] = mMatrix[MODEL][0];
	mMat3x3[1] = mMatrix[MODEL][1];
	mMat3x3[2] = mMatrix[MODEL][2];

	mMat3x3[3] = mMatrix[MODEL][4];
	mMat3x3[4] = mMatrix[MODEL][5];
	mMat3x3[5] = mMatrix[MODEL][6];

	mMat3x3[6] = mMatrix[MODEL][8];
	mMat3x3[7] = mMatrix[MODEL][9];
	mMat3x3[8] = mMatrix[MODEL][10];

	double det, invDet;

	det = mMat3x3[0] * (mMat3x3[4] * mMat3x3[8] - mMat3x3[5] * mMat3x3[7]) +
		  mMat3x3[1] * (mMat3x3[5] * mMat3x3[6] - mMat3x3[8] * mMat3x3[3]) +
		  mMat3x3[2] * (mMat3x3[3] * mMat3x3[7] - mMat3x3[4] * mMat3x3[6]);

	invDet = 1.0f/det;

	mNormalModel[0] = (mMat3x3[4] * mMat3x3[8] - mMat3x3[5] * mMat3x3[7]) * invDet;
	mNormalModel[1] = (mMat3x3[5] * mMat3x3[6] - mMat3x3[8] * mMat3x3[3]) * invDet;
	mNormalModel[2] = (mMat3x3[3] * mMat3x3[7] - mMat3x3[4] * mMat3x3[6]) * invDet;
	mNormalModel[3] = 0.0f;
	mNormalModel[4] = (mMat3x3[2] * mMat3x3[7] - mMat3x3[1] * mMat3x3[8]) * invDet;
	mNormalModel[5] = (mMat3x3[0] * mMat3x3[8] - mMat3x3[2] * mMat3x3[6]) * invDet;
	mNormalModel[6] = (mMat3x3[1] * mMat3x3[6] - mMat3x3[7] * mMat3x3[0]) * invDet;
	mNormalModel[7] = 0.0f;
	mNormalModel[8] = (mMat3x3[1] * mMat3x3[5] - mMat3x3[4] * mMat3x3[2]) * invDet;
	mNormalModel[9] = (mMat3x3[2] * mMat3x3[3] - mMat3x3[0] * mMat3x3[5]) * invDet;
	mNormalModel[10] =(mMat3x3[0] * mMat3x3[4] - mMat3x3[3] * mMat3x3[1]) * invDet;
	mNormalModel[11] = 0.0;

}


// computes the derived normal matrix
void
VSMathLib::computeNormalMatrix3x3() {

	computeDerivedMatrix(VIEW_MODEL);

	mMat3x3[0] = mCompMatrix[VIEW_MODEL][0];
	mMat3x3[1] = mCompMatrix[VIEW_MODEL][1];
	mMat3x3[2] = mCompMatrix[VIEW_MODEL][2];

	mMat3x3[3] = mCompMatrix[VIEW_MODEL][4];
	mMat3x3[4] = mCompMatrix[VIEW_MODEL][5];
	mMat3x3[5] = mCompMatrix[VIEW_MODEL][6];

	mMat3x3[6] = mCompMatrix[VIEW_MODEL][8];
	mMat3x3[7] = mCompMatrix[VIEW_MODEL][9];
	mMat3x3[8] = mCompMatrix[VIEW_MODEL][10];

	double det, invDet;

	det = mMat3x3[0] * (mMat3x3[4] * mMat3x3[8] - mMat3x3[5] * mMat3x3[7]) +
		  mMat3x3[1] * (mMat3x3[5] * mMat3x3[6] - mMat3x3[8] * mMat3x3[3]) +
		  mMat3x3[2] * (mMat3x3[3] * mMat3x3[7] - mMat3x3[4] * mMat3x3[6]);

	invDet = 1.0f/det;

	mNormal3x3[0] = (mMat3x3[4] * mMat3x3[8] - mMat3x3[5] * mMat3x3[7]) * invDet;
	mNormal3x3[1] = (mMat3x3[5] * mMat3x3[6] - mMat3x3[8] * mMat3x3[3]) * invDet;
	mNormal3x3[2] = (mMat3x3[3] * mMat3x3[7] - mMat3x3[4] * mMat3x3[6]) * invDet;
	mNormal3x3[3] = (mMat3x3[2] * mMat3x3[7] - mMat3x3[1] * mMat3x3[8]) * invDet;
	mNormal3x3[4] = (mMat3x3[0] * mMat3x3[8] - mMat3x3[2] * mMat3x3[6]) * invDet;
	mNormal3x3[5] = (mMat3x3[1] * mMat3x3[6] - mMat3x3[7] * mMat3x3[0]) * invDet;
	mNormal3x3[6] = (mMat3x3[1] * mMat3x3[5] - mMat3x3[4] * mMat3x3[2]) * invDet;
	mNormal3x3[7] = (mMat3x3[2] * mMat3x3[3] - mMat3x3[0] * mMat3x3[5]) * invDet;
	mNormal3x3[8] = (mMat3x3[0] * mMat3x3[4] - mMat3x3[3] * mMat3x3[1]) * invDet;

}

// computes the derived normal matrix for the view matrix only
void
VSMathLib::computeNormalViewMatrix3x3() {


	mMat3x3[0] = mMatrix[VIEW][0];
	mMat3x3[1] = mMatrix[VIEW][1];
	mMat3x3[2] = mMatrix[VIEW][2];

	mMat3x3[3] = mMatrix[VIEW][4];
	mMat3x3[4] = mMatrix[VIEW][5];
	mMat3x3[5] = mMatrix[VIEW][6];

	mMat3x3[6] = mMatrix[VIEW][8];
	mMat3x3[7] = mMatrix[VIEW][9];
	mMat3x3[8] = mMatrix[VIEW][10];

	double det, invDet;

	det = mMat3x3[0] * (mMat3x3[4] * mMat3x3[8] - mMat3x3[5] * mMat3x3[7]) +
		  mMat3x3[1] * (mMat3x3[5] * mMat3x3[6] - mMat3x3[8] * mMat3x3[3]) +
		  mMat3x3[2] * (mMat3x3[3] * mMat3x3[7] - mMat3x3[4] * mMat3x3[6]);

	invDet = 1.0f/det;

	mNormalView3x3[0] = (mMat3x3[4] * mMat3x3[8] - mMat3x3[5] * mMat3x3[7]) * invDet;
	mNormalView3x3[1] = (mMat3x3[5] * mMat3x3[6] - mMat3x3[8] * mMat3x3[3]) * invDet;
	mNormalView3x3[2] = (mMat3x3[3] * mMat3x3[7] - mMat3x3[4] * mMat3x3[6]) * invDet;
	mNormalView3x3[3] = (mMat3x3[2] * mMat3x3[7] - mMat3x3[1] * mMat3x3[8]) * invDet;
	mNormalView3x3[4] = (mMat3x3[0] * mMat3x3[8] - mMat3x3[2] * mMat3x3[6]) * invDet;
	mNormalView3x3[5] = (mMat3x3[1] * mMat3x3[6] - mMat3x3[7] * mMat3x3[0]) * invDet;
	mNormalView3x3[6] = (mMat3x3[1] * mMat3x3[5] - mMat3x3[4] * mMat3x3[2]) * invDet;
	mNormalView3x3[7] = (mMat3x3[2] * mMat3x3[3] - mMat3x3[0] * mMat3x3[5]) * invDet;
	mNormalView3x3[8] = (mMat3x3[0] * mMat3x3[4] - mMat3x3[3] * mMat3x3[1]) * invDet;

}


// computes the derived normal matrix for the model matrix only
void
VSMathLib::computeNormalModelMatrix3x3() {


	mMat3x3[0] = mMatrix[MODEL][0];
	mMat3x3[1] = mMatrix[MODEL][1];
	mMat3x3[2] = mMatrix[MODEL][2];

	mMat3x3[3] = mMatrix[MODEL][4];
	mMat3x3[4] = mMatrix[MODEL][5];
	mMat3x3[5] = mMatrix[MODEL][6];

	mMat3x3[6] = mMatrix[MODEL][8];
	mMat3x3[7] = mMatrix[MODEL][9];
	mMat3x3[8] = mMatrix[MODEL][10];

	double det, invDet;

	det = mMat3x3[0] * (mMat3x3[4] * mMat3x3[8] - mMat3x3[5] * mMat3x3[7]) +
		  mMat3x3[1] * (mMat3x3[5] * mMat3x3[6] - mMat3x3[8] * mMat3x3[3]) +
		  mMat3x3[2] * (mMat3x3[3] * mMat3x3[7] - mMat3x3[4] * mMat3x3[6]);

	invDet = 1.0f/det;

	mNormalModel3x3[0] = (mMat3x3[4] * mMat3x3[8] - mMat3x3[5] * mMat3x3[7]) * invDet;
	mNormalModel3x3[1] = (mMat3x3[5] * mMat3x3[6] - mMat3x3[8] * mMat3x3[3]) * invDet;
	mNormalModel3x3[2] = (mMat3x3[3] * mMat3x3[7] - mMat3x3[4] * mMat3x3[6]) * invDet;
	mNormalModel3x3[3] = (mMat3x3[2] * mMat3x3[7] - mMat3x3[1] * mMat3x3[8]) * invDet;
	mNormalModel3x3[4] = (mMat3x3[0] * mMat3x3[8] - mMat3x3[2] * mMat3x3[6]) * invDet;
	mNormalModel3x3[5] = (mMat3x3[1] * mMat3x3[6] - mMat3x3[7] * mMat3x3[0]) * invDet;
	mNormalModel3x3[6] = (mMat3x3[1] * mMat3x3[5] - mMat3x3[4] * mMat3x3[2]) * invDet;
	mNormalModel3x3[7] = (mMat3x3[2] * mMat3x3[3] - mMat3x3[0] * mMat3x3[5]) * invDet;
	mNormalModel3x3[8] = (mMat3x3[0] * mMat3x3[4] - mMat3x3[3] * mMat3x3[1]) * invDet;

}


// Computes derived matrices
void 
VSMathLib::computeDerivedMatrix(ComputedMatrixTypes aType) {
	
	if (aType == PROJ_VIEW)
	{
		memcpy(mCompMatrix[PROJ_VIEW], mMatrix[PROJECTION], 16 * sizeof(double));
		multMatrix(mCompMatrix[PROJ_VIEW], mMatrix[VIEW]);
	}
	else
	{
		memcpy(mCompMatrix[VIEW_MODEL], mMatrix[VIEW], 16 * sizeof(double));
		multMatrix(mCompMatrix[VIEW_MODEL], mMatrix[MODEL]);

		if (aType == PROJ_VIEW_MODEL) {
			memcpy(mCompMatrix[PROJ_VIEW_MODEL], mMatrix[PROJECTION], 16 * sizeof(double));
			multMatrix(mCompMatrix[PROJ_VIEW_MODEL], mCompMatrix[VIEW_MODEL]);
		}
	}
}


// aux function resMat = resMat * aMatrix
void 
VSMathLib::multMatrix(double *resMat, double *aMatrix)
{
	
	double *a, *b, res[16];
	a = resMat;
	b = aMatrix;

	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			res[j*4 + i] = 0.0f;
			for (int k = 0; k < 4; ++k) {
				res[j*4 + i] += a[k*4 + i] * b[j*4 + k]; 
			}
		}
	}
	memcpy(a, res, 16 * sizeof(double));
}