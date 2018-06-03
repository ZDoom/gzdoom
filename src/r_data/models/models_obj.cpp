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

#include "w_wad.h"
#include "cmdlib.h"
#include "r_data/models/models_obj.h"

bool FOBJModel::Load(const char* fn, int lumpnum, const char* buffer, int length)
{
  FString objName = Wads.GetLumpFullPath(lumpnum);
  sc.OpenMem(objName, buffer, length);
  Printf("Parsing %s\n", objName.GetChars());
  while(sc.GetString())
  {
    if (sc.Compare("#")) // Line comment
    {
      sc.Line += 1; // I don't think this does anything, though...
    }
    else if (sc.Compare("v")) // Vertex
    {
      ParseVector3(this->verts);
    }
    else if (sc.Compare("vn")) // Vertex normal
    {
      ParseVector3(this->norms);
    }
    else if (sc.Compare("vt")) // UV Coordinates
    {
      ParseVector2(this->uvs);
    }
    else if (sc.Compare("usemtl"))
    {
      // Get material name and try to load it
      sc.MustGetString();

      curMtl = LoadSkin("", sc.String);
      if (!curMtl.isValid())
      {
        // Relative to model file path?
        curMtl = LoadSkin(fn, sc.String);
      }

      if (!curMtl.isValid())
      {
        // Don't use materials that don't exist
        continue;
      }
      // Most OBJs are sorted by material, but just in case the one we're loading isn't...
      // Build surface...
      if (curSurface == nullptr)
      {
        // First surface
        curSurface = new OBJSurface(curMtl);
      }
      else
      {
        if (curSurfFaceCount > 0)
        {
          // Search for existing surface with current material
          /*
          for(size_t i = 0; i < surfaces.Size(); i++)
          {
            if (surfaces[i].skin == curMtl)
            {
              curSurface = &surfaces[i];
              surfExists = true;
            }
          }
          */
          // Add previous surface
          curSurface->numFaces = curSurfFaceCount;
          curSurface->faceStart = aggSurfFaceCount;
          surfaces.Push(*curSurface);
          delete curSurface;
          // Go to next surface
          curSurface = new OBJSurface(curMtl);
          aggSurfFaceCount += curSurfFaceCount;
        }
        else
        {
          curSurface->skin = curMtl;
        }
      }
      curSurfFaceCount = 0;
    }
    else if (sc.Compare("f"))
    {
      FString sides[4];
      OBJFace face;
      for (int i = 0; i < 3; i++)
      {
        // A face must have at least 3 sides
        sc.MustGetString();
        sides[i] = sc.String;
        ParseFaceSide(sides[i], face, i);
      }
      face.sideCount = 3;
      if (sc.GetString())
      {
        if (!sc.Compare("f"))
        {
          sides[3] = sc.String;
          face.sideCount += 1;
          ParseFaceSide(sides[3], face, 3);
        }
        else
        {
          sc.UnGet(); // No 4th side, move back
        }
      }
      faces.Push(face);
      curSurfFaceCount += 1;
    }
  }
  sc.Close();

  // No valid materials detected
  if (curSurface == nullptr)
  {
    FTextureID dummyMtl = LoadSkin("", "-NOFLAT-"); // Built-in to GZDoom
    curSurface = new OBJSurface(dummyMtl);
  }
  curSurface->numFaces = curSurfFaceCount;
  curSurface->faceStart = aggSurfFaceCount;
  surfaces.Push(*curSurface);
  delete curSurface;

  /*
  Printf("%d vertices\n", verts.Size());
  Printf("%d normals\n", norms.Size());
  Printf("%d UVs\n", uvs.Size());
  Printf("%d faces\n", faces.Size());
  Printf("%d surfaces\n", surfaces.Size());
  */

  mLumpNum = lumpnum;
  for (size_t i = 0; i < surfaces.Size(); i++)
  {
    ConstructSurfaceTris(&surfaces[i]);
  }
  return true;
}

/**
  * Parse a 2D vector
  *
  * @param start The buffer to parse from
  * @param array The array to append the parsed vector to
  */
void FOBJModel::ParseVector2(TArray<FVector2> &array)
{
  float coord[2];
  for (int axis = 0; axis < 2; axis++)
  {
    sc.MustGetFloat();
    coord[axis] = (float)sc.Float;
  }
  FVector2 vec(coord);
  array.Push(vec);
}

/**
  * Parse a 3D vector
  *
  * @param start The buffer to parse from
  * @param array The array to append the parsed vector to
  */
void FOBJModel::ParseVector3(TArray<FVector3> &array)
{
  float coord[3];
  for (int axis = 0; axis < 3; axis++)
  {
    sc.MustGetFloat();
    coord[axis] = (float)sc.Float;
  }
  FVector3 vec(coord);
  array.Push(vec);
}

void FOBJModel::ParseFaceSide(const FString &sideStr, OBJFace &face, int sidx)
{
  OBJFaceSide side;
  int origIdx;
  if (sideStr.IndexOf("/") >= 0)
  {
    TArray<FString> sides = sideStr.Split("/");
    if (sides[0].Len() > 0)
    {
      origIdx = atoi(sides[0].GetChars());
      side.vertref = ResolveIndex(origIdx, FaceElement::VertexIndex);
    }
    if (sides[1].Len() > 0)
    {
      origIdx = atoi(sides[1].GetChars());
      side.uvref = ResolveIndex(origIdx, FaceElement::UVIndex);
    }
    if (sides.Size() > 2)
    {
      if (sides[2].Len() > 0)
      {
        origIdx = atoi(sides[2].GetChars());
        side.normref = ResolveIndex(origIdx, FaceElement::VNormalIndex);
      }
    }
  }
  else
  {
    origIdx = atoi(sideStr.GetChars());
    side.vertref = ResolveIndex(origIdx, FaceElement::VertexIndex);
    side.normref = 0;
    side.uvref = 0;
  }
  face.sides[sidx] = side;
}

int FOBJModel::ResolveIndex(int origIndex, FaceElement el)
{
  if (origIndex > 0)
  {
    return origIndex - 1; // OBJ indices start at 1
  }
  else if (origIndex < 0)
  {
    if (el == FaceElement::VertexIndex)
    {
      return this->verts.Size() + origIndex; // origIndex is negative
    }
    else if (el == FaceElement::UVIndex)
    {
      return this->uvs.Size() + origIndex;
    }
    else if (el == FaceElement::VNormalIndex)
    {
      return this->norms.Size() + origIndex;
    }
  }
  return 0;
}

void FOBJModel::TriangulateQuad(const OBJFace &quad, OBJFace *tris)
{
  // if (quad.sides < 3 || quad.sides > 4) exception
  // if (quad.sides == 3) return &quad;
  tris[0].sideCount = 3;
  tris[1].sideCount = 3;

  for (int i = 0; i < 3; i++)
  {
    tris[0].sides[i].vertref = quad.sides[i].vertref;
    tris[1].sides[i].vertref = quad.sides[i+1].vertref;
    tris[0].sides[i].uvref = quad.sides[i].uvref;
    tris[1].sides[i].uvref = quad.sides[i+1].uvref;
    tris[0].sides[i].normref = quad.sides[i].normref;
    tris[1].sides[i].normref = quad.sides[i+1].normref;
  }
}

void FOBJModel::ConstructSurfaceTris(OBJSurface *surf)
{
  int triCount = 0;

  size_t start = surf->faceStart;
  size_t end = start + surf->numFaces;
  for (size_t i = start; i < end; i++)
  {
    triCount += (faces[i].sideCount > 3) ? 2 : 1;
  }

  surf->numTris = triCount;
  surf->tris = new OBJFace[triCount];

  int quadCount = 0;
  for (size_t i = start; i < end; i++)
  {
    surf->tris[i+quadCount].sideCount = 3;
    if (faces[i].sideCount == 3)
    {
      memcpy(surf->tris[i+quadCount].sides, faces[i].sides, sizeof(OBJFaceSide) * 3);
    }
    else if (faces[i].sideCount == 4)
    {
      OBJFace *triangulated = new OBJFace[2];
      TriangulateQuad(faces[i], triangulated);
      memcpy(surf->tris[i+quadCount].sides, triangulated->sides, sizeof(OBJFaceSide) * 3);
      memcpy(surf->tris[i+quadCount+1].sides, (triangulated+1)->sides, sizeof(OBJFaceSide) * 3);
      delete[] triangulated;
      quadCount += 1;
    }
  }
}

int FOBJModel::FindFrame(const char* name)
{
  return 0; // OBJs are not animated.
}

// Stubs - I don't know what these do exactly
void FOBJModel::RenderFrame(FModelRenderer *renderer, FTexture * skin, int frameno, int frameno2, double inter, int translation)
{
  /*
  for (int i = 0; i < numMtls; i++)
  {
    renderer->SetMaterial(skin, false, translation);
    GetVertexBuffer(renderer)->SetupFrame(renderer, surf->vindex + frameno * surf->numVertices, surf->vindex + frameno2 * surf->numVertices, surf->numVertices);
    renderer->DrawElements(surf->numTriangles * 3, surf->iindex * sizeof(unsigned int));
  }
  */
}

void FOBJModel::BuildVertexBuffer(FModelRenderer *renderer)
{

}

void FOBJModel::AddSkins(uint8_t* hitlist)
{

}

FOBJModel::~FOBJModel()
{
  for (size_t i = 0; i < surfaces.Size(); i++)
  {
    delete[] surfaces[i].tris;
  }
}
