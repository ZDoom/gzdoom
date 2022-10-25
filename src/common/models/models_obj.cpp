//
//---------------------------------------------------------------------------
//
// Copyright(C) 2018 Kevin Caccamo
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
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

#include "filesystem.h"
#include "model_obj.h"
#include "texturemanager.h"
#include "modelrenderer.h"
#include "printf.h"
#include "textureid.h"

/**
 * Load an OBJ model
 *
 * @param fn The path to the model file
 * @param lumpnum The lump index in the wad collection
 * @param buffer The contents of the model file
 * @param length The size of the model file
 * @return Whether or not the model was parsed successfully
 */
bool FOBJModel::Load(const char* fn, int lumpnum, const char* buffer, int length)
{
	FString objName = fileSystem.GetFileFullPath(lumpnum);
	FString objBuf(buffer, length);

	// Do some replacements before we parse the OBJ string
	{
		// Ensure usemtl statements remain intact
		TArray<FString> mtlUsages;
		TArray<ptrdiff_t> mtlUsageIdxs;
		ptrdiff_t bpos = 0, nlpos = 0, slashpos = 0;
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
		char* wObjBuf = objBuf.LockBuffer();

		// Substitute broken usemtl statements with old ones
		for (size_t i = 0; i < mtlUsages.Size(); i++)
		{
			bpos = mtlUsageIdxs[i];
			nlpos = objBuf.IndexOf('\n', bpos);
			if (nlpos == -1)
			{
				nlpos = objBuf.Len();
			}
			memcpy(wObjBuf + bpos, mtlUsages[i].GetChars(), nlpos - bpos);
		}

		bpos = 0;
		// Find each OBJ line comment, and convert each to a C-style line comment
		while (1)
		{
			bpos = objBuf.IndexOf('#', bpos);
			if (bpos == -1) break;
			if (objBuf[(unsigned int)bpos + 1] == '\n')
			{
				wObjBuf[bpos] = ' ';
			}
			else
			{
				wObjBuf[bpos] = '/';
				wObjBuf[bpos+1] = '/';
			}
			bpos += 1;
		}
		wObjBuf = nullptr;
		objBuf.UnlockBuffer();
	}
	sc.OpenString(objName, objBuf);

	FTextureID curMtl = FNullTextureID();
	OBJSurface *curSurface = nullptr;
	unsigned int aggSurfFaceCount = 0;
	unsigned int curSurfFaceCount = 0;
	unsigned int curSmoothGroup = 0;

	while(sc.GetString())
	{
		if (sc.Compare("v")) // Vertex
		{
			ParseVector<FVector3, 3>(this->verts);
		}
		else if (sc.Compare("vn")) // Vertex normal
		{
			ParseVector<FVector3, 3>(this->norms);
		}
		else if (sc.Compare("vt")) // UV Coordinates
		{
			ParseVector<FVector2, 2>(this->uvs);
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
				sc.ScriptMessage("Material %s (#%u) not found.", sc.String, surfaces.Size());
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
				if (!ParseFaceSide(sides[i], face, i)) return false;
			}
			face.sideCount = 3;
			if (sc.GetString())
			{
				if (!sc.Compare("f") && FString(sc.String).IndexOfAny("-0123456789") == 0)
				{
					sides[3] = sc.String;
					face.sideCount += 1;
					if (!ParseFaceSide(sides[3], face, 3)) return false;
				}
				else
				{
					sc.UnGet(); // No 4th side, move back
				}
			}
			face.smoothGroup = curSmoothGroup;
			faces.Push(face);
			curSurfFaceCount += 1;
		}
		else if (sc.Compare("s"))
		{
			sc.MustGetString();
			if (sc.Compare("off"))
			{
				curSmoothGroup = 0;
			}
			else
			{
				sc.UnGet();
				sc.MustGetNumber();
				curSmoothGroup = sc.Number;
				hasSmoothGroups = hasSmoothGroups || curSmoothGroup > 0;
			}
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

	return true;
}

/**
 * Parse an x-Dimensional vector
 *
 * @tparam T A subclass of TVector2 to be used
 * @tparam L The length of the vector to parse
 * @param[out] array The array to append the parsed vector to
 */
template<typename T, size_t L> void FOBJModel::ParseVector(TArray<T> &array)
{
	T vec;
	for (unsigned axis = 0; axis < L; axis++)
	{
		sc.MustGetFloat();
		vec[axis] = (float)sc.Float;
	}
	array.Push(vec);
}

/**
 * Parse a side of a face
 *
 * @param[in] sideStr The side definition string
 * @param[out] face The face to assign the parsed side data to
 * @param sidx The 0-based index of the side
 * @return Whether or not the face side was parsed successfully
 */
bool FOBJModel::ParseFaceSide(const FString &sideStr, OBJFace &face, int sidx)
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
			return false;
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
				hasMissingNormals = true;
			}
		}
		else
		{
			side.normref = -1;
			hasMissingNormals = true;
		}
	}
	else
	{
		origIdx = atoi(sideStr.GetChars());
		side.vertref = ResolveIndex(origIdx, FaceElement::VertexIndex);
		side.normref = -1;
		hasMissingNormals = true;
		side.uvref = -1;
	}
	face.sides[sidx] = side;
	return true;
}

/**
 * Resolve an OBJ index to an absolute index
 *
 * OBJ indices are 1-based, and can also be negative
 *
 * @param origIndex The original OBJ index to resolve
 * @param el What type of element the index references
 * @return The absolute index of the element
 */
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

/**
 * Construct the vertex buffer for this model
 *
 * @param renderer A pointer to the model renderer. Used to allocate the vertex buffer.
 */
void FOBJModel::BuildVertexBuffer(FModelRenderer *renderer)
{
	if (GetVertexBuffer(renderer->GetType()))
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
	// Initialize/populate vertFaces
	if (hasMissingNormals && hasSmoothGroups)
	{
		AddVertFaces();
	}

	auto vbuf = renderer->CreateVertexBuffer(false,true);
	SetVertexBuffer(renderer->GetType(), vbuf);

	FModelVertex *vertptr = vbuf->LockVertexBuffer(vbufsize);

	for (unsigned int i = 0; i < surfaces.Size(); i++)
	{
		for (unsigned int j = 0; j < surfaces[i].numTris; j++)
		{
			for (size_t side = 0; side < 3; side++)
			{
				FModelVertex *mdv = vertptr +
					side + j * 3 + // Current surface and previous triangles
					surfaces[i].vbStart; // Previous surfaces

				OBJFaceSide &curSide = surfaces[i].tris[j].sides[2 - side];

				int vidx = curSide.vertref;
				int uvidx = (curSide.uvref >= 0 && (unsigned int)curSide.uvref < uvs.Size()) ? curSide.uvref : 0;
				int nidx = curSide.normref;

				FVector3 curVvec = RealignVector(verts[vidx]);
				FVector2 curUvec = FixUV(uvs[uvidx]);
				FVector3 nvec;

				mdv->Set(curVvec.X, curVvec.Y, curVvec.Z, curUvec.X, curUvec.Y);

				if (nidx >= 0 && (unsigned int)nidx < norms.Size())
				{
					nvec = RealignVector(norms[nidx]);
				}
				else
				{
					if (surfaces[i].tris[j].smoothGroup == 0)
					{
						nvec = CalculateNormalFlat(i, j);
					}
					else
					{
						nvec = CalculateNormalSmooth(vidx, surfaces[i].tris[j].smoothGroup);
					}
				}
				mdv->SetNormal(nvec.X, nvec.Y, nvec.Z);
			}
		}
		delete[] surfaces[i].tris;
	}

	// Destroy vertFaces
	if (hasMissingNormals && hasSmoothGroups)
	{
		for (size_t i = 0; i < verts.Size(); i++)
		{
			vertFaces[i].Clear();
		}
		delete[] vertFaces;
	}
	vbuf->UnlockVertexBuffer();
}

/**
 * Fill in the triangle data for a surface
 *
 * @param[in,out] surf The surface to fill in the triangle data for
 */
void FOBJModel::ConstructSurfaceTris(OBJSurface &surf)
{
	unsigned int triCount = 0;

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
			surf.tris[triIdx].smoothGroup = faces[i].smoothGroup;
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
		DPrintf(DMSG_SPAMMY, "Smooth group: %d\n", surf.tris[triIdx].smoothGroup);
	}
}

/**
 * Triangulate a 4-sided face
 *
 * @param[in] quad The 4-sided face to triangulate
 * @param[out] tris The resultant triangle data
 */
void FOBJModel::TriangulateQuad(const OBJFace &quad, OBJFace *tris)
{
	tris[0].sideCount = 3;
	tris[0].smoothGroup = quad.smoothGroup;
	tris[1].sideCount = 3;
	tris[1].smoothGroup = quad.smoothGroup;

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

/**
 * Add the vertices of all surfaces' triangles to the array of vertex->triangle references
 */
void FOBJModel::AddVertFaces() {
	// Initialize and populate vertFaces - this array stores references to triangles per vertex
	vertFaces = new TArray<OBJTriRef>[verts.Size()];
	for (unsigned int i = 0; i < surfaces.Size(); i++)
	{
		for (unsigned int j = 0; j < surfaces[i].numTris; j++)
		{
			OBJTriRef otr = OBJTriRef(i, j);
			for (size_t k = 0; k < surfaces[i].tris[j].sideCount; k++)
			{
				int vidx = surfaces[i].tris[j].sides[k].vertref;
				vertFaces[vidx].Push(otr);
			}
		}
	}
}

/**
 * Re-align a vector to match MD3 alignment
 *
 * @param vecToRealign The vector to re-align
 * @return The re-aligned vector
 */
inline FVector3 FOBJModel::RealignVector(FVector3 vecToRealign)
{
	vecToRealign.Z *= -1;
	return vecToRealign;
}

/**
 * Fix UV coordinates of a UV vector
 *
 * @param vecToRealign The vector to fix
 * @return The fixed UV coordinate vector
 */
inline FVector2 FOBJModel::FixUV(FVector2 vecToRealign)
{
	vecToRealign.Y *= -1;
	return vecToRealign;
}

/**
 * Calculate the surface normal for a triangle
 *
 * @param surfIdx The surface index
 * @param triIdx The triangle Index
 * @return The surface normal vector
 */
FVector3 FOBJModel::CalculateNormalFlat(unsigned int surfIdx, unsigned int triIdx)
{
	// https://www.khronos.org/opengl/wiki/Calculating_a_Surface_Normal
	int curVert = surfaces[surfIdx].tris[triIdx].sides[0].vertref;
	int nextVert = surfaces[surfIdx].tris[triIdx].sides[2].vertref;
	int lastVert = surfaces[surfIdx].tris[triIdx].sides[1].vertref;

	// Cross-multiply the U-vector and V-vector
	FVector3 curVvec = RealignVector(verts[curVert]);
	FVector3 uvec = RealignVector(verts[nextVert]) - curVvec;
	FVector3 vvec = RealignVector(verts[lastVert]) - curVvec;

	return uvec ^ vvec;
}

/**
 * Calculate the surface normal for a triangle
 *
 * @param otr A reference to the surface, and a triangle within that surface, as an OBJTriRef
 * @return The surface normal vector
 */
FVector3 FOBJModel::CalculateNormalFlat(OBJTriRef otr)
{
	return CalculateNormalFlat(otr.surf, otr.tri);
}

/**
 * Calculate the normal of a vertex in a specific smooth group
 *
 * @param vidx The index of the vertex in the array of vertices
 * @param smoothGroup The smooth group number
 */
FVector3 FOBJModel::CalculateNormalSmooth(unsigned int vidx, unsigned int smoothGroup)
{
	unsigned int connectedFaces = 0;
	TArray<OBJTriRef>& vTris = vertFaces[vidx];

	FVector3 vNormal(0,0,0);
	for (size_t face = 0; face < vTris.Size(); face++)
	{
		OBJFace& tri = surfaces[vTris[face].surf].tris[vTris[face].tri];
		if (tri.smoothGroup == smoothGroup)
		{
			FVector3 fNormal = CalculateNormalFlat(vTris[face]);
			connectedFaces += 1;
			vNormal += fNormal;
		}
	}
	vNormal /= (float)connectedFaces;
	return vNormal;
}

/**
 * Find the index of the frame with the given name
 *
 * OBJ models are not animated, so this always returns 0
 *
 * @param name The name of the frame
 * @return The index of the frame
 */
int FOBJModel::FindFrame(const char* name, bool nodefault)
{
	return nodefault ? FErr_Singleframe : 0; // OBJs are not animated.
}

/**
 * Render the model
 *
 * @param renderer The model renderer
 * @param skin The loaded skin for the surface
 * @param frameno The first frame to interpolate between. Only prevents the model from rendering if it is < 0, since OBJ models are static.
 * @param frameno2 The second frame to interpolate between.
 * @param inter The amount to interpolate the two frames.
 * @param translation The translation for the skin
 */
void FOBJModel::RenderFrame(FModelRenderer *renderer, FGameTexture * skin, int frameno, int frameno2, double inter, int translation, const FTextureID* surfaceskinids, const TArray<VSMatrix>& boneData, int boneStartPosition)
{
	// Prevent the model from rendering if the frame number is < 0
	if (frameno < 0 || frameno2 < 0) return;

	for (unsigned int i = 0; i < surfaces.Size(); i++)
	{
		OBJSurface *surf = &surfaces[i];

		FGameTexture *userSkin = skin;
		if (!userSkin)
		{
			if (surfaceskinids && surfaceskinids[i].isValid())
			{
				userSkin = TexMan.GetGameTexture(surfaceskinids[i], true);
			}
			else if (surf->skin.isValid())
			{
				userSkin = TexMan.GetGameTexture(surf->skin, true);
			}
		}

		// Still no skin after checking for one?
		if (!userSkin)
		{
			continue;
		}

		renderer->SetMaterial(userSkin, false, translation);
		renderer->SetupFrame(this, surf->vbStart, surf->vbStart, surf->numTris * 3, {}, -1);
		renderer->DrawArrays(0, surf->numTris * 3);
	}
}

/**
 * Pre-cache skins for the model
 *
 * @param hitlist The list of textures
 */
void FOBJModel::AddSkins(uint8_t* hitlist, const FTextureID* surfaceskinids)
{
	for (size_t i = 0; i < surfaces.Size(); i++)
	{
		if (surfaceskinids && i < MD3_MAX_SURFACES && surfaceskinids[i].isValid())
		{
			// Precache skins manually reassigned by the user.
			// On OBJs with lots of skins, such as Doom map OBJs exported from GZDB,
			// there may be too many skins for the user to manually change, unless
			// the limit is bumped or surfaceskinIDs is changed to a TArray<FTextureID>.
			hitlist[surfaceskinids[i].GetIndex()] |= FTextureManager::HIT_Flat;
			return; // No need to precache skin that was replaced
		}

		OBJSurface * surf = &surfaces[i];
		if (surf->skin.isValid())
		{
			hitlist[surf->skin.GetIndex()] |= FTextureManager::HIT_Flat;
		}
	}
}

/**
 * Remove the data that was loaded
 */
FOBJModel::~FOBJModel()
{
	verts.Clear();
	norms.Clear();
	uvs.Clear();
	faces.Clear();
	surfaces.Clear();
}
