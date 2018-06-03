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

#include "models.h"
#include "sc_man.h"

class FOBJModel : public FModel
{
private:
  int mLumpNum;
  int numMtls;

  enum class FaceElement
  {
    VertexIndex,
    UVIndex,
    VNormalIndex
  };

  struct OBJFaceSide
  {
    int vertref;
    int normref;
    int uvref;
  };
  struct OBJFace
  {
    int sideCount;
    OBJFaceSide sides[4];
  };
  struct OBJTriangle
  {
    int vertref[3];
  };
  struct OBJSurface // 1 surface per 'usemtl'
  {
    int numTris; // Number of triangulated faces
    int numFaces; // Number of faces
    int faceStart; // Index of first face in faces array
    OBJFace* tris; // Triangles
    FTextureID skin;
    OBJSurface(FTextureID skin): numTris(0), numFaces(0), faceStart(0), tris(nullptr), skin(skin) {}
  };

  TArray<FVector3> verts;
  TArray<FVector3> norms;
  TArray<FVector2> uvs;
  TArray<OBJFace> faces;
  TArray<OBJSurface> surfaces;
  FTextureID curMtl;
  OBJSurface* curSurface;
  int aggSurfFaceCount;
  int curSurfFaceCount;
  FScanner sc;

  void ParseVector2(TArray<FVector2> &array);
  void ParseVector3(TArray<FVector3> &array);
  void ParseFaceSide(const FString &side, OBJFace &face, int sidx);
  void ConstructSurfaceTris(OBJSurface *surf);
  int ResolveIndex(int origIndex, FaceElement el);
  void TriangulateQuad(const OBJFace &quad, OBJFace *tris);
public:
  FOBJModel(): curSurface(nullptr), aggSurfFaceCount(0), curSurfFaceCount(0) {}
  ~FOBJModel();
  bool Load(const char* fn, int lumpnum, const char* buffer, int length) override;
  int FindFrame(const char* name) override;
  void RenderFrame(FModelRenderer* renderer, FTexture* skin, int frame, int frame2, double inter, int translation=0) override;
  void BuildVertexBuffer(FModelRenderer* renderer) override;
  void AddSkins(uint8_t* hitlist) override;
};

#endif
