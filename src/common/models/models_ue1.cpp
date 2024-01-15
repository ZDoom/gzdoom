//
//---------------------------------------------------------------------------
//
// Copyright (c) 2018-2022 Marisa Kirisame, UnSX Team
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//
//--------------------------------------------------------------------------
//

#include "filesystem.h"
#include "cmdlib.h"
#include "model_ue1.h"
#include "texturemanager.h"
#include "modelrenderer.h"

float unpackuvert( uint32_t n, int c )
{
	switch( c )
	{
	case 0:
		return ((int16_t)((n&0x7ff)<<5))/128.f;
	case 1:
		return ((int16_t)(((n>>11)&0x7ff)<<5))/128.f;
	case 2:
		return ((int16_t)(((n>>22)&0x3ff)<<6))/128.f;
	default:
		return 0.f;
	}
}

bool FUE1Model::Load( const char *filename, int lumpnum, const char *buffer, int length )
{
	int lumpnum2;
	hasSurfaces = true;
	FString realfilename = fileSystem.GetFileFullName(lumpnum);
	if ( (size_t)realfilename.IndexOf("_d.3d") == realfilename.Len()-5 )
	{
		realfilename.Substitute("_d.3d","_a.3d");
		lumpnum2 = fileSystem.CheckNumForFullName(realfilename.GetChars());
		mDataLump = lumpnum;
		mAnivLump = lumpnum2;
	}
	else
	{
		realfilename.Substitute("_a.3d","_d.3d");
		lumpnum2 = fileSystem.CheckNumForFullName(realfilename.GetChars());
		mAnivLump = lumpnum;
		mDataLump = lumpnum2;
	}
	return true;
}

void FUE1Model::LoadGeometry()
{
	const char *buffer, *buffer2;
	auto lump =  fileSystem.ReadFile(mDataLump);
	buffer = lump.string();
	auto lump2 =  fileSystem.ReadFile(mAnivLump);
	buffer2 = lump2.string();
	// map structures
	dhead = (const d3dhead*)(buffer);
	dpolys = (const d3dpoly*)(buffer+sizeof(d3dhead));
	ahead = (const a3dhead*)(buffer2);
	// detect deus ex format
	if ( (ahead->framesize/dhead->numverts) == 8 )
	{
		averts = nullptr;
		dxverts = (const dxvert*)(buffer2+sizeof(a3dhead));
	}
	else
	{
		averts = (const uint32_t*)(buffer2+sizeof(a3dhead));
		dxverts = nullptr;
	}
	// set counters
	numVerts = dhead->numverts;
	numFrames = ahead->numframes;
	numPolys = dhead->numpolys;
	numGroups = 0;
	// populate vertex arrays
	for ( int i=0; i<numFrames; i++ )
	{
		for ( int j=0; j<numVerts; j++ )
		{
			UE1Vertex Vert;
			if ( dxverts != NULL )
			{
				// convert padded XYZ16
				Vert.Pos = FVector3(dxverts[j+i*numVerts].x,
					dxverts[j+i*numVerts].z,
					(float)-dxverts[j+i*numVerts].y);
			}
			else
			{
				// convert packed XY11Z10
				Vert.Pos = FVector3(unpackuvert(averts[j+i*numVerts],0),
					unpackuvert(averts[j+i*numVerts],2),
					-unpackuvert(averts[j+i*numVerts],1));
			}
			// refs will be set later
			Vert.P.Reset();
			Vert.nP = 0;
			// push vertex (without normals, will be calculated later)
			verts.Push(Vert);
		}
	}
	// populate poly arrays
	for ( int i=0; i<numPolys; i++ )
	{
		UE1Poly Poly;
		// set indices
		for ( int j=0; j<3; j++ )
			Poly.V[j] = dpolys[i].vertices[j];
		// unpack coords
		for ( int j=0; j<3; j++ )
			Poly.C[j] = FVector2(dpolys[i].uv[j][0]/255.f,dpolys[i].uv[j][1]/255.f);
		// compute facet normals
		for ( int j=0; j<numFrames; j++ )
		{
			FVector3 dir[2];
			dir[0] = verts[Poly.V[1]+numVerts*j].Pos-verts[Poly.V[0]+numVerts*j].Pos;
			dir[1] = verts[Poly.V[2]+numVerts*j].Pos-verts[Poly.V[0]+numVerts*j].Pos;
			Poly.Normals.Push((dir[0]^dir[1]).Unit());
			// since we're iterating frames, also set references for later
			for ( int k=0; k<3; k++ )
			{
				verts[Poly.V[k]+numVerts*j].P.Push(i);
				verts[Poly.V[k]+numVerts*j].nP++;
			}
		}
		// push
		polys.Push(Poly);
	}
	// compute normals for vertex arrays (average of all referenced poly normals)
	// since we have references listed from before, this saves a lot of time
	// without having to loop through the entire model each vertex (especially true for very complex models)
	for ( int i=0; i<numFrames; i++ )
	{
		for ( int j=0; j<numVerts; j++ )
		{
			FVector3 nsum = FVector3(0,0,0);
			for ( int k=0; k<verts[j+numVerts*i].nP; k++ )
				nsum += polys[verts[j+numVerts*i].P[k]].Normals[i];
			verts[j+numVerts*i].Normal = nsum.Unit();
		}
	}
	// populate poly groups (subdivided by texture number and type)
	// this method minimizes searches in the group list as much as possible
	// while still doing a single pass through the poly list
	groups.Reset();
	int curgroup = -1;
	UE1Group Group;
	hasWeaponTriangle = false;
	int weapontri = -1;
	for ( int i=0; i<numPolys; i++ )
	{
		// while we're at it, look for a weapon triangle
		// (technically only one should exist)
		if ( dpolys[i].type&PT_WeaponTriangle )
			weapontri = i;
		if ( curgroup == -1 )
		{
			// no group, create it
			Group.P.Reset();
			Group.numPolys = 0;
			Group.texNum = dpolys[i].texnum;
			Group.type = dpolys[i].type;
			groups.Push(Group);
			curgroup = numGroups++;
		}
		else if ( (dpolys[i].texnum != groups[curgroup].texNum) || (dpolys[i].type != groups[curgroup].type) )
		{
			// different attributes than last time
			// search for existing group with new attributes, create one if not found
			curgroup = -1;
			for ( int j=0; j<numGroups; j++ )
			{
				if ( (groups[j].texNum != dpolys[i].texnum) || (groups[j].type != dpolys[i].type) ) continue;
				curgroup = j;
				break;
			}
			// counter the increment that will happen after continuing this loop
			// otherwise it'll be skipped over
			i--;
			continue;
		}
		groups[curgroup].P.Push(i);
		groups[curgroup].numPolys++;
	}
	if ( weapontri != -1 )
	{
		// TODO: generate TRS array from special poly to interface as a pseudo-bone
		hasWeaponTriangle = true;
	}
}

void FUE1Model::UnloadGeometry()
{
	for ( unsigned i=0; i<verts.Size(); i++ )
		verts[i].P.Reset();
	verts.Reset();
	for ( unsigned i=0; i<polys.Size(); i++ )
		polys[i].Normals.Reset();
	polys.Reset();
	for ( unsigned i=0; i<groups.Size(); i++ )
		groups[i].P.Reset();
	// do not unload groups themselves, they're needed for rendering
}

int FUE1Model::FindFrame(const char* name, bool nodefault)
{
	// there are no named frames, but we need something here to properly interface with it. So just treat the string as an index number.
	auto index = strtol(name, nullptr, 0);
	if (index < 0 || index >= numFrames) return FErr_NotFound;
	return index;
}

void FUE1Model::RenderFrame( FModelRenderer *renderer, FGameTexture *skin, int frame, int frame2, double inter, FTranslationID translation, const FTextureID* surfaceskinids, const TArray<VSMatrix>& boneData, int boneStartPosition)
{
	// the moment of magic
	if ( (frame < 0) || (frame2 < 0) || (frame >= numFrames) || (frame2 >= numFrames) ) return;
	renderer->SetInterpolation(inter);
	int vsize, fsize = 0, vofs = 0;
	for ( int i=0; i<numGroups; i++ ) fsize += groups[i].numPolys*3;
	for ( int i=0; i<numGroups; i++ )
	{
		vsize = groups[i].numPolys*3;
		if ( groups[i].type&PT_WeaponTriangle )
		{
			// weapon triangle should never be drawn, it only exists to calculate attachment position and orientation
			vofs += vsize;
			continue;
		}
		FGameTexture *sskin = skin;
		if ( !sskin )
		{
			int ssIndex = groups[i].texNum;
			if (surfaceskinids && surfaceskinids[ssIndex].isValid())
				sskin = TexMan.GetGameTexture(surfaceskinids[ssIndex], true);
			if ( !sskin )
			{
				vofs += vsize;
				continue;
			}
		}
		// TODO: Handle per-group render styles and other flags once functions for it are implemented
		// Future note: poly renderstyles should always be enforced unless the actor itself has a style other than Normal
		renderer->SetMaterial(sskin,false,translation);
		renderer->SetupFrame(this, vofs + frame * fsize, vofs + frame2 * fsize, vsize, {}, -1);
		renderer->DrawArrays(0,vsize);
		vofs += vsize;
	}
	renderer->SetInterpolation(0.f);
}

void FUE1Model::BuildVertexBuffer( FModelRenderer *renderer )
{
	if (GetVertexBuffer(renderer->GetType()))
		return;
	LoadGeometry();
	int vsize = 0;
	for ( int i=0; i<numGroups; i++ )
		vsize += groups[i].numPolys*3;
	vsize *= numFrames;
	auto vbuf = renderer->CreateVertexBuffer(false,numFrames==1);
	SetVertexBuffer(renderer->GetType(), vbuf);
	FModelVertex *vptr = vbuf->LockVertexBuffer(vsize);
	int vidx = 0;
	for ( int i=0; i<numFrames; i++ )
	{
		for ( int j=0; j<numGroups; j++ )
		{
			for ( int k=0; k<groups[j].numPolys; k++ )
			{
				for ( int l=0; l<3; l++ )
				{
					UE1Vertex V = verts[polys[groups[j].P[k]].V[l]+i*numVerts];
					FVector2 C = polys[groups[j].P[k]].C[l];
					FModelVertex *vert = &vptr[vidx++];
					vert->Set(V.Pos.X,V.Pos.Y,V.Pos.Z,C.X,C.Y);
					if ( groups[j].type&PT_Curvy )	// use facet normal
					{
						vert->SetNormal(polys[groups[j].P[k]].Normals[i].X,
							polys[groups[j].P[k]].Normals[i].Y,
							polys[groups[j].P[k]].Normals[i].Z);
					}
					else vert->SetNormal(V.Normal.X,V.Normal.Y,V.Normal.Z);
				}
			}
		}
	}
	vbuf->UnlockVertexBuffer();
	UnloadGeometry(); // don't forget this, save precious RAM
}

void FUE1Model::AddSkins( uint8_t *hitlist, const FTextureID* surfaceskinids)
{
	for (int i = 0; i < numGroups; i++)
	{
		int ssIndex = groups[i].texNum;
		if (surfaceskinids && surfaceskinids[ssIndex].isValid())
			hitlist[surfaceskinids[ssIndex].GetIndex()] |= FTextureManager::HIT_Flat;
	}
}

FUE1Model::~FUE1Model()
{
	groups.Reset();
}
