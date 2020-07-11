//
//---------------------------------------------------------------------------
//
// Copyright(C) 2018 Marisa Kirisame
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
	FString realfilename = fileSystem.GetFileFullName(lumpnum);
	if ( (size_t)realfilename.IndexOf("_d.3d") == realfilename.Len()-5 )
	{
		realfilename.Substitute("_d.3d","_a.3d");
		lumpnum2 = fileSystem.CheckNumForFullName(realfilename);
		mDataLump = lumpnum;
		mAnivLump = lumpnum2;
	}
	else
	{
		realfilename.Substitute("_a.3d","_d.3d");
		lumpnum2 = fileSystem.CheckNumForFullName(realfilename);
		mAnivLump = lumpnum;
		mDataLump = lumpnum2;
	}
	return true;
}

void FUE1Model::LoadGeometry()
{
	FileData lump, lump2;
	const char *buffer, *buffer2;
	lump = fileSystem.ReadFile(mDataLump);
	buffer = (char*)lump.GetMem();
	lump2 = fileSystem.ReadFile(mAnivLump);
	buffer2 = (char*)lump2.GetMem();
	// map structures
	dhead = (d3dhead*)(buffer);
	dpolys = (d3dpoly*)(buffer+sizeof(d3dhead));
	ahead = (a3dhead*)(buffer2);
	// detect deus ex format
	if ( (ahead->framesize/dhead->numverts) == 8 )
	{
		averts = NULL;
		dxverts = (dxvert*)(buffer2+sizeof(a3dhead));
	}
	else
	{
		averts = (uint32_t*)(buffer2+sizeof(a3dhead));
		dxverts = NULL;
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
	int curgroup = -1;
	UE1Group Group;
	for ( int i=0; i<numPolys; i++ )
	{
		// while we're at it, look for attachment triangles
		// technically only one should exist, but we ain't following the specs 100% here
		if ( dpolys[i].type&PT_WeaponTriangle ) specialPolys.Push(i);
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
	// ... and it's finally done
	mDataLoaded = true;
}

void FUE1Model::UnloadGeometry()
{
	mDataLoaded = false;
	specialPolys.Reset();
	numVerts = 0;
	numFrames = 0;
	numPolys = 0;
	numGroups = 0;
	verts.Reset();
	for ( int i=0; i<numPolys; i++ )
		polys[i].Normals.Reset();
	polys.Reset();
	for ( int i=0; i<numGroups; i++ )
		groups[i].P.Reset();
	groups.Reset();
}

int FUE1Model::FindFrame( const char *name )
{
	// unsupported, there are no named frames
	return -1;
}

void FUE1Model::RenderFrame( FModelRenderer *renderer, FGameTexture *skin, int frame, int frame2, double inter, int translation )
{
	// the moment of magic
	if ( (frame >= numFrames) || (frame2 >= numFrames) ) return;
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
			if ( curSpriteMDLFrame->surfaceskinIDs[curMDLIndex][groups[i].texNum].isValid() )
				sskin = TexMan.GetGameTexture(curSpriteMDLFrame->surfaceskinIDs[curMDLIndex][groups[i].texNum], true);
			if ( !sskin )
			{
				vofs += vsize;
				continue;
			}
		}
		// TODO: Handle per-group render styles and other flags once functions for it are implemented
		// Future note: poly renderstyles should always be enforced unless the actor itself has a style other than Normal
		renderer->SetMaterial(sskin,false,translation);
		renderer->SetupFrame(this, vofs+frame*fsize,vofs+frame2*fsize,vsize);
		renderer->DrawArrays(0,vsize);
		vofs += vsize;
	}
	renderer->SetInterpolation(0.f);
}

void FUE1Model::BuildVertexBuffer( FModelRenderer *renderer )
{
	if (GetVertexBuffer(renderer->GetType()))
		return;
	if ( !mDataLoaded )
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
}

void FUE1Model::AddSkins( uint8_t *hitlist )
{
	for ( int i=0; i<numGroups; i++ )
		if ( curSpriteMDLFrame->surfaceskinIDs[curMDLIndex][groups[i].texNum].isValid() )
			hitlist[curSpriteMDLFrame->surfaceskinIDs[curMDLIndex][groups[i].texNum].GetIndex()] |= FTextureManager::HIT_Flat;
}

FUE1Model::~FUE1Model()
{
	UnloadGeometry();
}
