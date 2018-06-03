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

/**
 * Load an OBJ model
 *
 * @param fn The path to the model file
 * @param lumpnum The lump index in the wad collection
 * @param buffer The contents of the model file
 * @param length The size of the model file
 */
bool FOBJModel::Load(const char* fn, int lumpnum, const char* buffer, int length)
{
	FString objName = Wads.GetLumpFullPath(lumpnum);
	FString objBuf(buffer, length);

	// Do some replacements before we parse the OBJ string
	{
		// Ensure usemtl statements remain intact
		TArray<FString> mtlUsages;
		TArray<long> mtlUsageIdxs;
		long bpos = 0, nlpos = 0, slashpos = 0;
		while (1)
		{
			bpos = objBuf.IndexOf("\nusemtl", bpos);
			if (bpos == -1) break;
			slashpos = objBuf.IndexOf('/', bpos);
			nlpos = objBuf.IndexOf('\n', ++bpos);
			if (slashpos > nlpos || slashpos == -1)
			{
				continue;
			}
			if (nlpos == -1)
			{
				nlpos = objBuf.Len();
			}
			FString lineStr(objBuf.GetChars() + bpos, nlpos - bpos);
			mtlUsages.Push(lineStr);
			mtlUsageIdxs.Push(bpos);
		}

		// Replace forward slashes with percent signs so they aren't parsed as line comments
		objBuf.ReplaceChars('/', *newSideSep);

		// Substitute broken usemtl statements with old ones
		bpos = 0, nlpos = 0;
		for (size_t i = 0; i < mtlUsages.Size(); i++)
		{
			bpos = mtlUsageIdxs[i];
			nlpos = objBuf.IndexOf('\n', bpos);
			if (nlpos == -1)
			{
				nlpos = objBuf.Len();
			}
			FString lineStr(objBuf.GetChars() + bpos, nlpos - bpos);
			objBuf.Substitute(lineStr, mtlUsages[i]);
		}

		// Find each OBJ line comment, and convert each to a C-style line comment
		while (1)
		{
			bpos = objBuf.IndexOf('#');
			if (bpos == -1) break;
			objBuf.Remove(bpos, 1);
			objBuf.Insert(bpos, "//", 2);
		}
	}
	sc.OpenString(objName, objBuf);
	//Printf("Parsing %s\n", objName.GetChars());

	FTextureID curMtl = FNullTextureID();
	OBJSurface *curSurface = nullptr;
	int aggSurfFaceCount = 0;
	int curSurfFaceCount = 0;

	while(sc.GetString())
	{
		if /*(sc.Compare("#")) // Line comment
		{
			sc.Line += 1; // I don't think this does anything, though...
		}
		else if*/ (sc.Compare("v")) // Vertex
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
				if (!sc.Compare("f") && FString(sc.String).IndexOfAny("-0123456789") == 0)
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

	if (curSurface == nullptr)
	{ // No valid materials detected
		FTextureID dummyMtl = LoadSkin("", "-NOFLAT-"); // Built-in to GZDoom
		curSurface = new OBJSurface(dummyMtl);
	}
	curSurface->numFaces = curSurfFaceCount;
	curSurface->faceStart = aggSurfFaceCount;
	surfaces.Push(*curSurface);
	delete curSurface;

	if (uvs.Size() == 0)
	{ // Needed so that OBJs without UVs can work
		uvs.Push(FVector2(0.0, 0.0));
	}

	/*
	Printf("%d vertices\n", verts.Size());
	Printf("%d normals\n", norms.Size());
	Printf("%d UVs\n", uvs.Size());
	Printf("%d faces\n", faces.Size());
	Printf("%d surfaces\n", surfaces.Size());
	*/

	mLumpNum = lumpnum;
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
	if (sideStr.IndexOf(newSideSep) >= 0)
	{
		TArray<FString> sides = sideStr.Split(newSideSep, FString::TOK_KEEPEMPTY);

		if (sides[0].Len() > 0)
		{
			origIdx = atoi(sides[0].GetChars());
			side.vertref = ResolveIndex(origIdx, FaceElement::VertexIndex);
		}
		else
		{
			sc.ScriptError("Vertex reference is not optional!");
		}

		if (sides[1].Len() > 0)
		{
			origIdx = atoi(sides[1].GetChars());
			side.uvref = ResolveIndex(origIdx, FaceElement::UVIndex);
		}
		else
		{
			side.uvref = -1;
		}

		if (sides.Size() > 2)
		{
			if (sides[2].Len() > 0)
			{
				origIdx = atoi(sides[2].GetChars());
				side.normref = ResolveIndex(origIdx, FaceElement::VNormalIndex);
			}
			else
			{
				side.normref = -1;
			}
		}
		else
		{
			side.normref = -1;
		}
	}
	else
	{
		origIdx = atoi(sideStr.GetChars());
		side.vertref = ResolveIndex(origIdx, FaceElement::VertexIndex);
		side.normref = -1;
		side.uvref = -1;
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
			return verts.Size() + origIndex; // origIndex is negative
		}
		else if (el == FaceElement::UVIndex)
		{
			return uvs.Size() + origIndex;
		}
		else if (el == FaceElement::VNormalIndex)
		{
			return norms.Size() + origIndex;
		}
	}
	return -1;
}

void FOBJModel::BuildVertexBuffer(FModelRenderer *renderer)
{
	if (GetVertexBuffer(renderer))
	{
		return;
	}

	unsigned int vbufsize = 0;

	for (size_t i = 0; i < surfaces.Size(); i++)
	{
		ConstructSurfaceTris(surfaces[i]);
		surfaces[i].vbStart = vbufsize;
		vbufsize += surfaces[i].numTris * 3;
	}

	auto vbuf = renderer->CreateVertexBuffer(false,true);
	SetVertexBuffer(renderer, vbuf);

	FModelVertex *vertptr = vbuf->LockVertexBuffer(vbufsize);

	for (size_t i = 0; i < surfaces.Size(); i++)
	{
		for (size_t j = 0; j < surfaces[i].numTris; j++)
		{
			for (size_t side = 0; side < 3; side++)
			{
				FModelVertex *mdv = vertptr +
					side + j * 3 + // Current surface and previous triangles
					surfaces[i].vbStart; // Previous surfaces

				OBJFaceSide &curSide = surfaces[i].tris[j].sides[side];

				int vidx = curSide.vertref;
				int uvidx = (curSide.uvref >= 0 && curSide.uvref < uvs.Size()) ? curSide.uvref : 0;
				int nidx = curSide.normref;

				mdv->Set(verts[vidx].X, verts[vidx].Y, verts[vidx].Z, uvs[uvidx].X, uvs[uvidx].Y * -1);

				if (nidx >= 0 && nidx < norms.Size())
				{
					mdv->SetNormal(norms[nidx].X, norms[nidx].Y, norms[nidx].Z);
				}
				else
				{
					// https://www.khronos.org/opengl/wiki/Calculating_a_Surface_Normal
					// Find other sides of triangle
					int nextSidx = side + 1;
					if (nextSidx >= 3) nextSidx -= 3;

					int lastSidx = side + 2;
					if (lastSidx >= 3) lastSidx -= 3;

					OBJFaceSide &nextSide = surfaces[i].tris[j].sides[nextSidx];
					OBJFaceSide &lastSide = surfaces[i].tris[j].sides[lastSidx];

					// Cross-multiply the U-vector and V-vector
					FVector3 uvec = verts[nextSide.vertref] - verts[curSide.vertref];
					FVector3 vvec = verts[lastSide.vertref] - verts[curSide.vertref];

					FVector3 nvec = uvec ^ vvec;
					mdv->SetNormal(nvec.X, nvec.Y, nvec.Z);
				}
			}
		}
	}

	vbuf->UnlockVertexBuffer();
}

void FOBJModel::ConstructSurfaceTris(OBJSurface &surf)
{
	int triCount = 0;

	size_t start = surf.faceStart;
	size_t end = start + surf.numFaces;
	for (size_t i = start; i < end; i++)
	{
		triCount += faces[i].sideCount - 2;
	}

	surf.numTris = triCount;
	surf.tris = new OBJFace[triCount];

	for (size_t i = start, triIdx = 0; i < end; i++, triIdx++)
	{
		surf.tris[triIdx].sideCount = 3;
		if (faces[i].sideCount == 3)
		{
			memcpy(surf.tris[triIdx].sides, faces[i].sides, sizeof(OBJFaceSide) * 3);
		}
		else if (faces[i].sideCount == 4) // Triangulate face
		{
			OBJFace *triangulated = new OBJFace[2];
			TriangulateQuad(faces[i], triangulated);
			memcpy(surf.tris[triIdx].sides, triangulated[0].sides, sizeof(OBJFaceSide) * 3);
			memcpy(surf.tris[triIdx+1].sides, triangulated[1].sides, sizeof(OBJFaceSide) * 3);
			delete[] triangulated;
			triIdx += 1; // Filling out two faces
		}
	}
}

void FOBJModel::TriangulateQuad(const OBJFace &quad, OBJFace *tris)
{
	tris[0].sideCount = 3;
	tris[1].sideCount = 3;

	int tsidx[2][3] = {{0, 1, 3}, {1, 2, 3}};

	for (int i = 0; i < 3; i++)
	{
		for (int j = 0; j < 2; j++)
		{
			tris[j].sides[i].vertref = quad.sides[tsidx[j][i]].vertref;
			tris[j].sides[i].uvref = quad.sides[tsidx[j][i]].uvref;
			tris[j].sides[i].normref = quad.sides[tsidx[j][i]].normref;
		}
	}
}

int FOBJModel::FindFrame(const char* name)
{
	return 0; // OBJs are not animated.
}

void FOBJModel::RenderFrame(FModelRenderer *renderer, FTexture * skin, int frameno, int frameno2, double inter, int translation)
{
	for (unsigned int i = 0; i < surfaces.Size(); i++)
	{
		OBJSurface *surf = &surfaces[i];

		FTexture *userSkin = skin;
		if (!userSkin)
		{
			if (curSpriteMDLFrame->surfaceskinIDs[curMDLIndex][i].isValid())
			{
				userSkin = TexMan(curSpriteMDLFrame->surfaceskinIDs[curMDLIndex][i]);
			}
			else if (surf->skin.isValid())
			{
				userSkin = TexMan(surf->skin);
			}
		}
		if (!userSkin) return;

		renderer->SetMaterial(userSkin, false, translation);
		GetVertexBuffer(renderer)->SetupFrame(renderer, 0, 0, surf->numTris * 3);
		renderer->DrawArrays(surf->vbStart, surf->numTris * 3);
	}
}

void FOBJModel::AddSkins(uint8_t* hitlist)
{
	for (size_t i = 0; i < surfaces.Size(); i++)
	{
		if (curSpriteMDLFrame->surfaceskinIDs[curMDLIndex][i].isValid())
		{
			hitlist[curSpriteMDLFrame->surfaceskinIDs[curMDLIndex][i].GetIndex()] |= FTextureManager::HIT_Flat;
		}

		OBJSurface * surf = &surfaces[i];
		if (surf->skin.isValid())
		{
			hitlist[surf->skin.GetIndex()] |= FTextureManager::HIT_Flat;
		}
	}
}

FOBJModel::~FOBJModel()
{
	verts.Clear();
	norms.Clear();
	uvs.Clear();
	faces.Clear();
	for (size_t i = 0; i < surfaces.Size(); i++)
	{
		delete[] surfaces[i].tris;
	}
	surfaces.Clear();
}
