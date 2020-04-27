//
//---------------------------------------------------------------------------
//
// Copyright(C) 2018 Kevin Caccamo
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

#ifndef __GL_MODELS_OBJ_H__
#define __GL_MODELS_OBJ_H__

#include "model.h"
#include "sc_man.h"
#include "tarray.h"
#include "vectors.h"

class FOBJModel : public FModel
{
private:
	const char *newSideSep = "$"; // OBJ side separator is /, which is parsed as a line comment by FScanner if two of them are next to each other.
	bool hasMissingNormals;
	bool hasSmoothGroups;

	enum class FaceElement
	{
		VertexIndex,
		UVIndex,
		VNormalIndex
	};

	struct OBJTriRef
	{
		unsigned int surf;
		unsigned int tri;
		OBJTriRef(): surf(0), tri(0) {}
		OBJTriRef(unsigned int surf, unsigned int tri): surf(surf), tri(tri) {}
		bool operator== (OBJTriRef other) { return surf == other.surf && tri == other.tri; }
	};
	struct OBJFaceSide
	{
		int vertref;
		int normref;
		int uvref;
	};
	struct OBJFace
	{
		unsigned int sideCount;
		unsigned int smoothGroup;
		OBJFaceSide sides[4];
		OBJFace(): sideCount(0), smoothGroup(0) {}
	};
	struct OBJSurface // 1 surface per 'usemtl'
	{
		unsigned int numTris; // Number of triangulated faces
		unsigned int numFaces; // Number of faces
		unsigned int vbStart; // First index in vertex buffer
		unsigned int faceStart; // Index of first face in faces array
		OBJFace* tris; // Triangles
		FTextureID skin;
		OBJSurface(FTextureID skin): numTris(0), numFaces(0), vbStart(0), faceStart(0), tris(nullptr), skin(skin) {}
	};

	TArray<FVector3> verts;
	TArray<FVector3> norms;
	TArray<FVector2> uvs;
	TArray<OBJFace> faces;
	TArray<OBJSurface> surfaces;
	FScanner sc;
	TArray<OBJTriRef>* vertFaces;

	int ResolveIndex(int origIndex, FaceElement el);
	template<typename T, size_t L> void ParseVector(TArray<T> &array);
	bool ParseFaceSide(const FString &side, OBJFace &face, int sidx);
	void ConstructSurfaceTris(OBJSurface &surf);
	void AddVertFaces();
	void TriangulateQuad(const OBJFace &quad, OBJFace *tris);
	FVector3 RealignVector(FVector3 vecToRealign);
	FVector2 FixUV(FVector2 vecToRealign);
	FVector3 CalculateNormalFlat(unsigned int surfIdx, unsigned int triIdx);
	FVector3 CalculateNormalFlat(OBJTriRef otr);
	FVector3 CalculateNormalSmooth(unsigned int vidx, unsigned int smoothGroup);
public:
	FOBJModel(): hasMissingNormals(false), hasSmoothGroups(false), vertFaces(nullptr) {}
	~FOBJModel();
	bool Load(const char* fn, int lumpnum, const char* buffer, int length) override;
	int FindFrame(const char* name) override;
	void RenderFrame(FModelRenderer* renderer, FGameTexture* skin, int frame, int frame2, double inter, int translation=0) override;
	void BuildVertexBuffer(FModelRenderer* renderer) override;
	void AddSkins(uint8_t* hitlist) override;
};

#endif
