/*
** gl_models.cpp
**
** MD2/DMD model format code
**
**---------------------------------------------------------------------------
** Copyright 2005 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
** 4. When not used as part of GZDoom or a GZDoom derivative, this code will be
**    covered by the terms of the GNU Lesser General Public License as published
**    by the Free Software Foundation; either version 2.1 of the License, or (at
**    your option) any later version.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include "gl/system/gl_system.h"
#include "w_wad.h"
#include "cmdlib.h"
#include "sc_man.h"
#include "m_crc32.h"

#include "gl/renderer/gl_renderstate.h"
#include "gl/scene/gl_drawinfo.h"
#include "gl/models/gl_models.h"
#include "gl/textures/gl_material.h"
#include "gl/shaders/gl_shader.h"

static float   avertexnormals[NUMVERTEXNORMALS][3] = {
#include "tab_anorms.h"
};

//===========================================================================
//
// UnpackVector
//  Packed: pppppppy yyyyyyyy. Yaw is on the XY plane.
//
//===========================================================================

static void UnpackVector(unsigned short packed, float vec[3])
{
	float   yaw = (packed & 511) / 512.0f * 2 * PI;
	float   pitch = ((packed >> 9) / 127.0f - 0.5f) * PI;
	float   cosp = (float) cos(pitch);

	vec[VX] = (float) cos(yaw) * cosp;
	vec[VY] = (float) sin(yaw) * cosp;
	vec[VZ] = (float) sin(pitch);
}


//===========================================================================
//
// FDMDModel::Load
//
//===========================================================================

bool FDMDModel::Load(const char * path, int, const char * buffer, int length)
{
	struct dmd_chunk_t
	{
		int             type;
		int             length;		   // Next chunk follows...
	};

#pragma pack(1)
	struct dmd_packedVertex_t
	{
		byte            vertex[3];
		unsigned short  normal;		   // Yaw and pitch.
	};

	struct dmd_packedFrame_t
	{
		float           scale[3];
		float           translate[3];
		char            name[16];
		dmd_packedVertex_t vertices[1];
	} ;
#pragma pack()

	// Chunk types.
	enum 
	{
		DMC_END,					   // Must be the last chunk.
		DMC_INFO					   // Required; will be expected to exist.
	};

	dmd_chunk_t * chunk = (dmd_chunk_t*)(buffer+12);
	char   *temp;
	ModelFrame *frame;
	int     i, k, c;
	FTriangle *triangles[MAX_LODS];
	int     axis[3] = { VX, VY, VZ };

	int fileoffset=12+sizeof(dmd_chunk_t);

	chunk->type = LittleLong(chunk->type);
	while(chunk->type != DMC_END)
	{
		switch (chunk->type)
		{
		case DMC_INFO:			// Standard DMD information chunk.
			memcpy(&info, buffer + fileoffset, LittleLong(chunk->length));
			info.skinWidth = LittleLong(info.skinWidth);
			info.skinHeight = LittleLong(info.skinHeight);
			info.frameSize = LittleLong(info.frameSize);
			info.numSkins = LittleLong(info.numSkins);
			info.numVertices = LittleLong(info.numVertices);
			info.numTexCoords = LittleLong(info.numTexCoords);
			info.numFrames = LittleLong(info.numFrames);
			info.numLODs = LittleLong(info.numLODs);
			info.offsetSkins = LittleLong(info.offsetSkins);
			info.offsetTexCoords = LittleLong(info.offsetTexCoords);
			info.offsetFrames = LittleLong(info.offsetFrames);
			info.offsetLODs = LittleLong(info.offsetLODs);
			info.offsetEnd = LittleLong(info.offsetEnd);
			fileoffset += chunk->length;
			break;

		default:
			// Just skip all unknown chunks.
			fileoffset += chunk->length;
			break;
		}
		// Read the next chunk header.
		chunk = (dmd_chunk_t*)(buffer+fileoffset);
		chunk->type = LittleLong(chunk->type);
		fileoffset += sizeof(dmd_chunk_t);
	}

	// Allocate and load in the data.
	skins = new FTexture *[info.numSkins];

	for(i = 0; i < info.numSkins; i++)
	{
		skins[i] = LoadSkin(path, buffer + info.offsetSkins + i*64);
	}


	temp = (char*)buffer + info.offsetFrames;
	frames = new ModelFrame[info.numFrames];

	for(i = 0, frame = frames; i < info.numFrames; i++, frame++)
	{
		dmd_packedFrame_t *pfr = (dmd_packedFrame_t *) (temp + info.frameSize * i);
		dmd_packedVertex_t *pVtx;

		memcpy(frame->name, pfr->name, sizeof(pfr->name));
		frame->vertices = new FModelVertex[info.numVertices];
		frame->normals = new FModelVertex[info.numVertices];

		// Translate each vertex.
		for(k = 0, pVtx = pfr->vertices; k < info.numVertices; k++, pVtx++)
		{
			UnpackVector((unsigned short)(pVtx->normal), frame->normals[k].xyz);
			for(c = 0; c < 3; c++)
			{
				frame->vertices[k].xyz[axis[c]] =
					(pVtx->vertex[c] * FLOAT(pfr->scale[c]) + FLOAT(pfr->translate[c]));
			}
			// Aspect undo.
			frame->vertices[k].xyz[VZ] *= rModelAspectMod;
		}
	}

	memcpy(lodInfo, buffer+info.offsetLODs, info.numLODs * sizeof(DMDLoDInfo));
	for(i = 0; i < info.numLODs; i++)
	{
		lodInfo[i].numTriangles = LittleLong(lodInfo[i].numTriangles);
		lodInfo[i].numGlCommands = LittleLong(lodInfo[i].numGlCommands);
		lodInfo[i].offsetTriangles = LittleLong(lodInfo[i].offsetTriangles);
		lodInfo[i].offsetGlCommands = LittleLong(lodInfo[i].offsetGlCommands);

		triangles[i] = (FTriangle*)(buffer + lodInfo[i].offsetTriangles);

		lods[i].glCommands = new int[lodInfo[i].numGlCommands];
		memcpy(lods[i].glCommands, buffer + lodInfo[i].offsetGlCommands, sizeof(int) * lodInfo[i].numGlCommands);
	}

	// Determine vertex usage at each LOD level.
	vertexUsage = new char[info.numVertices];
	memset(vertexUsage, 0, info.numVertices);

	for(i = 0; i < info.numLODs; i++)
		for(k = 0; k < lodInfo[i].numTriangles; k++)
			for(c = 0; c < 3; c++)
				vertexUsage[short(triangles[i][k].vertexIndices[c])] |= 1 << i;

	loaded=true;
	return true;
}


FDMDModel::~FDMDModel()
{
	int i;

	// clean up
	if (skins != NULL)
	{
		// skins are managed by the texture manager so they must not be deleted here.
		delete [] skins;
	}

	if (frames != NULL)
	{
		for (i=0;i<info.numFrames;i++)
		{
			delete [] frames[i].vertices;
			delete [] frames[i].normals;
		}
		delete [] frames;
	}

	for(i = 0; i < info.numLODs; i++)
	{
		delete [] lods[i].glCommands;
	}

	if (vertexUsage != NULL) delete [] vertexUsage;
}

//===========================================================================
//
// FDMDModel::FindFrame
//
//===========================================================================
int FDMDModel::FindFrame(const char * name)
{
	for (int i=0;i<info.numFrames;i++)
	{
		if (!stricmp(name, frames[i].name)) return i;
	}
	return -1;
}

//===========================================================================
//
// Render a set of GL commands using the given data.
//
//===========================================================================
void FDMDModel::RenderGLCommands(void *glCommands, unsigned int numVertices,FModelVertex * vertices)
{
	char   *pos;
	FGLCommandVertex * v;
	int     count;

	gl_RenderState.Apply();
	for(pos = (char*)glCommands; *pos;)
	{
		count = *(int *) pos;
		pos += 4;

		// The type of primitive depends on the sign.
		glBegin(count > 0 ? GL_TRIANGLE_STRIP : GL_TRIANGLE_FAN);
		count = abs(count);

		while(count--)
		{
			v = (FGLCommandVertex *) pos;
			pos += sizeof(FGLCommandVertex);

			glTexCoord2fv(&v->s);
			glVertex3fv((float*)&vertices[v->index]);
		}

		glEnd();
	}
}


void FDMDModel::RenderFrame(FTexture * skin, int frameno, int cm, int translation)
{
	int activeLod;

	if (frameno>=info.numFrames) return;

	ModelFrame * frame = &frames[frameno];
	//int mainFlags = mf->flags;

	if (!skin)
	{
		if (info.numSkins==0) return;
		skin = skins[0];
		if (!skin) return;
	}

	FMaterial * tex = FMaterial::ValidateTexture(skin);

	tex->Bind(cm, 0, translation);

	int numVerts = info.numVertices;

	// Determine the suitable LOD.
	/*
	if(info.numLODs > 1 && rend_model_lod != 0)
	{
	float   lodFactor = rend_model_lod * screen->Width() / 640.0f / (GLRenderer->mCurrentFoV / 90.0f);
	if(lodFactor) lodFactor = 1 / lodFactor;

	// Determine the LOD we will be using.
	activeLod = (int) (lodFactor * spr->distance);
	if(activeLod < 0) activeLod = 0;
	if(activeLod >= mdl->info.numLODs) activeLod = mdl->info.numLODs - 1;
	vertexUsage = mdl->vertexUsage;
	}
	else
	*/
	{
		activeLod = 0;
	}

	RenderGLCommands(lods[activeLod].glCommands, numVerts, frame->vertices/*, modelColors, NULL*/);
}

void FDMDModel::RenderFrameInterpolated(FTexture * skin, int frameno, int frameno2, double inter, int cm, int translation)
{
	int activeLod = 0;

	if (frameno>=info.numFrames || frameno2>=info.numFrames) return;

	FModelVertex *vertices1 = frames[frameno].vertices;
	FModelVertex *vertices2 = frames[frameno2].vertices;

	if (!skin)
	{
		if (info.numSkins==0) return;
		skin = skins[0];
		if (!skin) return;
	}

	FMaterial * tex = FMaterial::ValidateTexture(skin);

	tex->Bind(cm, 0, translation);

	int numVerts = info.numVertices;

	// [BB] Calculate the interpolated vertices by linear interpolation.
	FModelVertex *verticesInterpolated = new FModelVertex[numVerts];
	for( int k = 0; k < numVerts; k++ )
	{
		for ( int i = 0; i < 3; i++ )
			verticesInterpolated[k].xyz[i] = (1-inter)*vertices1[k].xyz[i]+ (inter)*vertices2[k].xyz[i];
	}

	RenderGLCommands(lods[activeLod].glCommands, numVerts, verticesInterpolated/*, modelColors, NULL*/);
	delete[] verticesInterpolated;
}


//===========================================================================
//
// FMD2Model::Load
//
//===========================================================================

bool FMD2Model::Load(const char * path, int, const char * buffer, int length)
{
	// Internal data structures of MD2 files - only used during loading!
	struct md2_header_t
	{
		int             magic;
		int             version;
		int             skinWidth;
		int             skinHeight;
		int             frameSize;
		int             numSkins;
		int             numVertices;
		int             numTexCoords;
		int             numTriangles;
		int             numGlCommands;
		int             numFrames;
		int             offsetSkins;
		int             offsetTexCoords;
		int             offsetTriangles;
		int             offsetFrames;
		int             offsetGlCommands;
		int             offsetEnd;
	} ;

	struct md2_triangleVertex_t
	{
		byte            vertex[3];
		byte            lightNormalIndex;
	};

	struct md2_packedFrame_t
	{
		float           scale[3];
		float           translate[3];
		char            name[16];
		md2_triangleVertex_t vertices[1];
	};

	md2_header_t * md2header = (md2_header_t *)buffer;
	ModelFrame *frame;
	byte   *md2_frames;
	int     i, k, c;
	int     axis[3] = { VX, VY, VZ };

	// Convert it to DMD.
	header.magic = MD2_MAGIC;
	header.version = 8;
	header.flags = 0;
	vertexUsage = NULL;
	info.skinWidth = LittleLong(md2header->skinWidth);
	info.skinHeight = LittleLong(md2header->skinHeight);
	info.frameSize = LittleLong(md2header->frameSize);
	info.numLODs = 1;
	info.numSkins = LittleLong(md2header->numSkins);
	info.numTexCoords = LittleLong(md2header->numTexCoords);
	info.numVertices = LittleLong(md2header->numVertices);
	info.numFrames = LittleLong(md2header->numFrames);
	info.offsetSkins = LittleLong(md2header->offsetSkins);
	info.offsetTexCoords = LittleLong(md2header->offsetTexCoords);
	info.offsetFrames = LittleLong(md2header->offsetFrames);
	info.offsetLODs = LittleLong(md2header->offsetEnd);	// Doesn't exist.
	lodInfo[0].numTriangles = LittleLong(md2header->numTriangles);
	lodInfo[0].numGlCommands = LittleLong(md2header->numGlCommands);
	lodInfo[0].offsetTriangles = LittleLong(md2header->offsetTriangles);
	lodInfo[0].offsetGlCommands = LittleLong(md2header->offsetGlCommands);
	info.offsetEnd = LittleLong(md2header->offsetEnd);

	if (info.offsetFrames + info.frameSize * info.numFrames > length)
	{
		Printf("LoadModel: Model '%s' file too short\n", path);
		return false;
	}
	if (lodInfo[0].numGlCommands <= 0)
	{
		Printf("LoadModel: Model '%s' invalid NumGLCommands\n", path);
		return false;
	}

	// The frames need to be unpacked.
	md2_frames = (byte*)buffer + info.offsetFrames;

	frames = new ModelFrame[info.numFrames];

	for(i = 0, frame = frames; i < info.numFrames; i++, frame++)
	{
		md2_packedFrame_t *pfr = (md2_packedFrame_t *) (md2_frames + info.frameSize * i);
		md2_triangleVertex_t *pVtx;

		memcpy(frame->name, pfr->name, sizeof(pfr->name));
		frame->vertices = new FModelVertex[info.numVertices];
		frame->normals = new FModelVertex[info.numVertices];

		// Translate each vertex.
		for(k = 0, pVtx = pfr->vertices; k < info.numVertices; k++, pVtx++)
		{
			memcpy(frame->normals[k].xyz,
				avertexnormals[pVtx->lightNormalIndex], sizeof(float) * 3);

			for(c = 0; c < 3; c++)
			{
				frame->vertices[k].xyz[axis[c]] =
					(pVtx->vertex[c] * pfr->scale[c] + pfr->translate[c]);
			}
			// Aspect ratio adjustment (1.33 -> 1.6.)
			frame->vertices[k].xyz[VZ] *= rModelAspectMod;
		}
	}


	lods[0].glCommands = new int[lodInfo[0].numGlCommands];
	memcpy(lods[0].glCommands, buffer + lodInfo[0].offsetGlCommands, sizeof(int) * lodInfo[0].numGlCommands);

	skins = new FTexture *[info.numSkins];

	for(i = 0; i < info.numSkins; i++)
	{
		skins[i] = LoadSkin(path, buffer + info.offsetSkins + i*64);
	}
	loaded=true;
	return true;
}

FMD2Model::~FMD2Model()
{
}

