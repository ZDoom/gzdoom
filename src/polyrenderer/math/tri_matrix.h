/*
**  Polygon Doom software renderer
**  Copyright (c) 2016 Magnus Norddahl
**
**  This software is provided 'as-is', without any express or implied
**  warranty.  In no event will the authors be held liable for any damages
**  arising from the use of this software.
**
**  Permission is granted to anyone to use this software for any purpose,
**  including commercial applications, and to alter it and redistribute it
**  freely, subject to the following restrictions:
**
**  1. The origin of this software must not be misrepresented; you must not
**     claim that you wrote the original software. If you use this software
**     in a product, an acknowledgment in the product documentation would be
**     appreciated but is not required.
**  2. Altered source versions must be plainly marked as such, and must not be
**     misrepresented as being the original software.
**  3. This notice may not be removed or altered from any source distribution.
**
*/

#pragma once

struct TriVertex;
struct FRenderViewpoint;

struct TriMatrix
{
	static TriMatrix null();
	static TriMatrix identity();
	static TriMatrix translate(float x, float y, float z);
	static TriMatrix scale(float x, float y, float z);
	static TriMatrix rotate(float angle, float x, float y, float z);
	static TriMatrix swapYZ();
	static TriMatrix perspective(float fovy, float aspect, float near, float far);
	static TriMatrix frustum(float left, float right, float bottom, float top, float near, float far);

	static TriMatrix worldToView(const FRenderViewpoint &viewpoint); // Software renderer world to view space transform
	static TriMatrix viewToClip(double focalTangent, double centerY, double YaspectMul); // Software renderer shearing projection

	FVector4 operator*(const FVector4 &v) const;
	TriMatrix operator*(const TriMatrix &m) const;

	float matrix[16];
};
