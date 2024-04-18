
#include "filesystem.h"
#include "cmdlib.h"
#include "model_iqm.h"
#include "texturemanager.h"
#include "modelrenderer.h"
#include "engineerrors.h"
#include "dobject.h"
#include "bonecomponents.h"

IMPLEMENT_CLASS(DBoneComponents, false, false);


IQMModel::IQMModel()
{
}

IQMModel::~IQMModel()
{
}

bool IQMModel::Load(const char* path, int lumpnum, const char* buffer, int length)
{
	mLumpNum = lumpnum;

	try
	{
		IQMFileReader reader(buffer, length);

		char magic[16];
		reader.Read(magic, 16);
		if (memcmp(magic, "INTERQUAKEMODEL\0", 16) != 0)
			return false;

		uint32_t version = reader.ReadUInt32();
		if (version != 2)
			return false;

		uint32_t filesize = reader.ReadUInt32();
		uint32_t flags = reader.ReadUInt32();
		uint32_t num_text = reader.ReadUInt32();
		uint32_t ofs_text = reader.ReadUInt32();
		uint32_t num_meshes = reader.ReadUInt32();
		uint32_t ofs_meshes = reader.ReadUInt32();
		uint32_t num_vertexarrays = reader.ReadUInt32();
		uint32_t num_vertices = reader.ReadUInt32();
		uint32_t ofs_vertexarrays = reader.ReadUInt32();
		uint32_t num_triangles = reader.ReadUInt32();
		uint32_t ofs_triangles = reader.ReadUInt32();
		uint32_t ofs_adjacency = reader.ReadUInt32();
		uint32_t num_joints = reader.ReadUInt32();
		uint32_t ofs_joints = reader.ReadUInt32();
		uint32_t num_poses = reader.ReadUInt32();
		uint32_t ofs_poses = reader.ReadUInt32();
		uint32_t num_anims = reader.ReadUInt32();
		uint32_t ofs_anims = reader.ReadUInt32();
		uint32_t num_frames = reader.ReadUInt32();
		uint32_t num_framechannels = reader.ReadUInt32();
		uint32_t ofs_frames = reader.ReadUInt32();
		uint32_t ofs_bounds = reader.ReadUInt32();
		uint32_t num_comment = reader.ReadUInt32();
		uint32_t ofs_comment = reader.ReadUInt32();
		uint32_t num_extensions = reader.ReadUInt32();
		uint32_t ofs_extensions = reader.ReadUInt32();

		if (num_text == 0)
			return false;

		TArray<char> text(num_text, true);
		reader.SeekTo(ofs_text);
		reader.Read(text.Data(), text.Size());
		text[text.Size() - 1] = 0;

		Meshes.Resize(num_meshes);
		Triangles.Resize(num_triangles);
		Adjacency.Resize(num_triangles);
		Joints.Resize(num_joints);
		Poses.Resize(num_poses);
		Anims.Resize(num_anims);
		Bounds.Resize(num_frames);
		VertexArrays.Resize(num_vertexarrays);
		NumVertices = num_vertices;

		reader.SeekTo(ofs_meshes);
		for (IQMMesh& mesh : Meshes)
		{
			mesh.Name = reader.ReadName(text);
			mesh.Material = reader.ReadName(text);
			mesh.FirstVertex = reader.ReadUInt32();
			mesh.NumVertices = reader.ReadUInt32();
			mesh.FirstTriangle = reader.ReadUInt32();
			mesh.NumTriangles = reader.ReadUInt32();
			mesh.Skin = LoadSkin(path, mesh.Material.GetChars());
		}

		reader.SeekTo(ofs_triangles);
		for (IQMTriangle& triangle : Triangles)
		{
			triangle.Vertex[0] = reader.ReadUInt32();
			triangle.Vertex[1] = reader.ReadUInt32();
			triangle.Vertex[2] = reader.ReadUInt32();
		}

		reader.SeekTo(ofs_adjacency);
		for (IQMAdjacency& adj : Adjacency)
		{
			adj.Triangle[0] = reader.ReadUInt32();
			adj.Triangle[1] = reader.ReadUInt32();
			adj.Triangle[2] = reader.ReadUInt32();
		}

		reader.SeekTo(ofs_joints);
		for (IQMJoint& joint : Joints)
		{
			joint.Name = reader.ReadName(text);
			joint.Parent = reader.ReadInt32();
			joint.Translate.X = reader.ReadFloat();
			joint.Translate.Y = reader.ReadFloat();
			joint.Translate.Z = reader.ReadFloat();
			joint.Quaternion.X = reader.ReadFloat();
			joint.Quaternion.Y = reader.ReadFloat();
			joint.Quaternion.Z = reader.ReadFloat();
			joint.Quaternion.W = reader.ReadFloat();
			joint.Quaternion.MakeUnit();
			joint.Scale.X = reader.ReadFloat();
			joint.Scale.Y = reader.ReadFloat();
			joint.Scale.Z = reader.ReadFloat();
		}

		reader.SeekTo(ofs_poses);
		for (IQMPose& pose : Poses)
		{
			pose.Parent = reader.ReadInt32();
			pose.ChannelMask = reader.ReadUInt32();
			for (int i = 0; i < 10; i++) pose.ChannelOffset[i] = reader.ReadFloat();
			for (int i = 0; i < 10; i++) pose.ChannelScale[i] = reader.ReadFloat();
		}

		reader.SeekTo(ofs_anims);
		for(unsigned i = 0; i < Anims.Size(); i++)
		{
			IQMAnim& anim = Anims[i];
			anim.Name = reader.ReadName(text);
			NamedAnimations.Insert(anim.Name, i);

			anim.FirstFrame = reader.ReadUInt32();
			anim.NumFrames = reader.ReadUInt32();
			anim.Framerate = reader.ReadFloat();
			anim.Loop = !!(reader.ReadUInt32() & 1);
		}

		baseframe.Resize(num_joints);
		inversebaseframe.Resize(num_joints);

		for (uint32_t i = 0; i < num_joints; i++)
		{
			const IQMJoint& j = Joints[i];

			VSMatrix m, invm;
			m.loadIdentity();
			m.translate(j.Translate.X, j.Translate.Y, j.Translate.Z);
			m.multQuaternion(j.Quaternion);
			m.scale(j.Scale.X, j.Scale.Y, j.Scale.Z);
			m.inverseMatrix(invm);
			if (j.Parent >= 0)
			{
				baseframe[i] = baseframe[j.Parent];
				baseframe[i].multMatrix(m);
				inversebaseframe[i] = invm;
				inversebaseframe[i].multMatrix(inversebaseframe[j.Parent]);
			}
			else
			{
				baseframe[i] = m;
				inversebaseframe[i] = invm;
			}			
		}

		TRSData.Resize(num_frames * num_poses);
		reader.SeekTo(ofs_frames);
		for (uint32_t i = 0; i < num_frames; i++)
		{
			for (uint32_t j = 0; j < num_poses; j++)
			{
				const IQMPose& p = Poses[j];

				FVector3 translate;
				translate.X = p.ChannelOffset[0]; if (p.ChannelMask & 0x01) translate.X += reader.ReadUInt16() * p.ChannelScale[0];
				translate.Y = p.ChannelOffset[1]; if (p.ChannelMask & 0x02) translate.Y += reader.ReadUInt16() * p.ChannelScale[1];
				translate.Z = p.ChannelOffset[2]; if (p.ChannelMask & 0x04) translate.Z += reader.ReadUInt16() * p.ChannelScale[2];

				FVector4 quaternion;
				quaternion.X = p.ChannelOffset[3]; if (p.ChannelMask & 0x08) quaternion.X += reader.ReadUInt16() * p.ChannelScale[3];
				quaternion.Y = p.ChannelOffset[4]; if (p.ChannelMask & 0x10) quaternion.Y += reader.ReadUInt16() * p.ChannelScale[4];
				quaternion.Z = p.ChannelOffset[5]; if (p.ChannelMask & 0x20) quaternion.Z += reader.ReadUInt16() * p.ChannelScale[5];
				quaternion.W = p.ChannelOffset[6]; if (p.ChannelMask & 0x40) quaternion.W += reader.ReadUInt16() * p.ChannelScale[6];
				quaternion.MakeUnit();

				FVector3 scale;
				scale.X = p.ChannelOffset[7]; if (p.ChannelMask & 0x80) scale.X += reader.ReadUInt16() * p.ChannelScale[7];
				scale.Y = p.ChannelOffset[8]; if (p.ChannelMask & 0x100) scale.Y += reader.ReadUInt16() * p.ChannelScale[8];
				scale.Z = p.ChannelOffset[9]; if (p.ChannelMask & 0x200) scale.Z += reader.ReadUInt16() * p.ChannelScale[9];

				TRSData[i * num_poses + j].translation = translate;
				TRSData[i * num_poses + j].rotation = quaternion;
				TRSData[i * num_poses + j].scaling = scale;
			}
		}

		//If a model doesn't have an animation loaded, it will crash. We don't want that!
		if (num_frames <= 0)
		{
			num_frames = 1;
			TRSData.Resize(num_joints);

			for (uint32_t j = 0; j < num_joints; j++)
			{
				FVector3 translate;
				translate.X = Joints[j].Translate.X;
				translate.Y = Joints[j].Translate.Y;
				translate.Z = Joints[j].Translate.Z;
				
				FVector4 quaternion;
				quaternion.X = Joints[j].Quaternion.X;
				quaternion.Y = Joints[j].Quaternion.Y;
				quaternion.Z = Joints[j].Quaternion.Z;
				quaternion.W = Joints[j].Quaternion.W;
				quaternion.MakeUnit();

				FVector3 scale;
				scale.X = Joints[j].Scale.X;
				scale.Y = Joints[j].Scale.Y;
				scale.Z = Joints[j].Scale.Z;

				TRSData[j].translation = translate;
				TRSData[j].rotation = quaternion;
				TRSData[j].scaling = scale;
			}
		}

		reader.SeekTo(ofs_bounds);
		for (IQMBounds& bound : Bounds)
		{
			bound.BBMins[0] = reader.ReadFloat();
			bound.BBMins[1] = reader.ReadFloat();
			bound.BBMins[2] = reader.ReadFloat();
			bound.BBMaxs[0] = reader.ReadFloat();
			bound.BBMaxs[1] = reader.ReadFloat();
			bound.BBMaxs[2] = reader.ReadFloat();
			bound.XYRadius = reader.ReadFloat();
			bound.Radius = reader.ReadFloat();
		}

		reader.SeekTo(ofs_vertexarrays);
		for (IQMVertexArray& vertexArray : VertexArrays)
		{
			vertexArray.Type = (IQMVertexArrayType)reader.ReadUInt32();
			vertexArray.Flags = reader.ReadUInt32();
			vertexArray.Format = (IQMVertexArrayFormat)reader.ReadUInt32();
			vertexArray.Size = reader.ReadUInt32();
			vertexArray.Offset = reader.ReadUInt32();
		}

		return true;
	}
	catch (IQMReadErrorException)
	{
		return false;
	}
}

void IQMModel::LoadGeometry()
{
	try
	{
		auto lumpdata = fileSystem.ReadFile(mLumpNum);
		IQMFileReader reader(lumpdata.data(), (int)lumpdata.size());

		Vertices.Resize(NumVertices);
		for (IQMVertexArray& vertexArray : VertexArrays)
		{
			reader.SeekTo(vertexArray.Offset);
			if (vertexArray.Type == IQM_POSITION)
			{
				LoadPosition(reader, vertexArray);
			}
			else if (vertexArray.Type == IQM_TEXCOORD)
			{
				LoadTexcoord(reader, vertexArray);
			}
			else if (vertexArray.Type == IQM_NORMAL)
			{
				LoadNormal(reader, vertexArray);
			}
			else if (vertexArray.Type == IQM_BLENDINDEXES)
			{
				LoadBlendIndexes(reader, vertexArray);
			}
			else if (vertexArray.Type == IQM_BLENDWEIGHTS)
			{
				LoadBlendWeights(reader, vertexArray);
			}
		}
	}
	catch (IQMReadErrorException)
	{
	}
}

void IQMModel::LoadPosition(IQMFileReader& reader, const IQMVertexArray& vertexArray)
{
	float lu = 0.0f, lv = 0.0f, lindex = -1.0f;
	if (vertexArray.Format == IQM_FLOAT && vertexArray.Size == 3)
	{
		for (FModelVertex& v : Vertices)
		{
			v.x = reader.ReadFloat();
			v.z = reader.ReadFloat();
			v.y = reader.ReadFloat();

			v.lu = lu;
			v.lv = lv;
			v.lindex = lindex;
		}
	}
	else
	{
		I_FatalError("Unsupported IQM_POSITION vertex format");
	}
}

void IQMModel::LoadTexcoord(IQMFileReader& reader, const IQMVertexArray& vertexArray)
{
	if (vertexArray.Format == IQM_FLOAT && vertexArray.Size == 2)
	{
		for (FModelVertex& v : Vertices)
		{
			v.u = reader.ReadFloat();
			v.v = reader.ReadFloat();
		}
	}
	else
	{
		I_FatalError("Unsupported IQM_TEXCOORD vertex format");
	}
}

void IQMModel::LoadNormal(IQMFileReader& reader, const IQMVertexArray& vertexArray)
{
	if (vertexArray.Format == IQM_FLOAT && vertexArray.Size == 3)
	{
		for (FModelVertex& v : Vertices)
		{
			float x = reader.ReadFloat();
			float y = reader.ReadFloat();
			float z = reader.ReadFloat();

			v.SetNormal(x, z, y);
		}
	}
	else
	{
		I_FatalError("Unsupported IQM_NORMAL vertex format");
	}
}

void IQMModel::LoadBlendIndexes(IQMFileReader& reader, const IQMVertexArray& vertexArray)
{
	if (vertexArray.Format == IQM_UBYTE && vertexArray.Size == 4)
	{
		for (FModelVertex& v : Vertices)
		{
			int x = reader.ReadUByte();
			int y = reader.ReadUByte();
			int z = reader.ReadUByte();
			int w = reader.ReadUByte();
			v.SetBoneSelector(x, y, z, w);
		}
	}
	else if (vertexArray.Format == IQM_INT && vertexArray.Size == 4)
	{
		for (FModelVertex& v : Vertices)
		{
			int x = reader.ReadInt32();
			int y = reader.ReadInt32();
			int z = reader.ReadInt32();
			int w = reader.ReadInt32();
			v.SetBoneSelector(x, y, z, w);
		}
	}
	else
	{
		I_FatalError("Unsupported IQM_BLENDINDEXES vertex format");
	}
}

void IQMModel::LoadBlendWeights(IQMFileReader& reader, const IQMVertexArray& vertexArray)
{
	if (vertexArray.Format == IQM_UBYTE && vertexArray.Size == 4)
	{
		for (FModelVertex& v : Vertices)
		{
			int x = reader.ReadUByte();
			int y = reader.ReadUByte();
			int z = reader.ReadUByte();
			int w = reader.ReadUByte();
			v.SetBoneWeight(x, y, z, w);
		}
	}
	else if (vertexArray.Format == IQM_FLOAT && vertexArray.Size == 4)
	{
		for (FModelVertex& v : Vertices)
		{
			uint8_t x = (int)clamp(reader.ReadFloat() * 255.0f, 0.0f, 255.0f);
			uint8_t y = (int)clamp(reader.ReadFloat() * 255.0f, 0.0f, 255.0f);
			uint8_t z = (int)clamp(reader.ReadFloat() * 255.0f, 0.0f, 255.0f);
			uint8_t w = (int)clamp(reader.ReadFloat() * 255.0f, 0.0f, 255.0f);
			v.SetBoneWeight(x, y, z, w);
		}
	}
	else
	{
		I_FatalError("Unsupported IQM_BLENDWEIGHTS vertex format");
	}
}

void IQMModel::UnloadGeometry()
{
	Vertices.Reset();
}

int IQMModel::FindFrame(const char* name, bool nodefault)
{
	// [MK] allow looking up frames by animation name plus offset (using a colon as separator)
	const char* colon = strrchr(name,':');
	size_t nlen = (colon==nullptr)?strlen(name):(colon-name);
	for (unsigned i = 0; i < Anims.Size(); i++)
	{
		if (!strnicmp(name, Anims[i].Name.GetChars(), nlen))
		{
			// if no offset is given, return the first frame
			if (colon == nullptr) return Anims[i].FirstFrame;
			unsigned offset = atoi(colon+1);
			if (offset >= Anims[i].NumFrames) return FErr_NotFound;
			return Anims[i].FirstFrame+offset;
		}
	}
	return FErr_NotFound;
}

int IQMModel::FindFirstFrame(FName name)
{
	int * nam = NamedAnimations.CheckKey(name);
	if(nam) return Anims[*nam].FirstFrame;
	return FErr_NotFound;
}

int IQMModel::FindLastFrame(FName name)
{
	int * nam = NamedAnimations.CheckKey(name);
	if(nam) return Anims[*nam].FirstFrame + Anims[*nam].NumFrames;
	return FErr_NotFound;
}

double IQMModel::FindFramerate(FName name)
{
	int * nam = NamedAnimations.CheckKey(name);
	if(nam) return Anims[*nam].Framerate;
	return FErr_NotFound;
}

void IQMModel::RenderFrame(FModelRenderer* renderer, FGameTexture* skin, int frame1, int frame2, double inter, FTranslationID translation, const FTextureID* surfaceskinids, const TArray<VSMatrix>& boneData, int boneStartPosition)
{
	renderer->SetupFrame(this, 0, 0, NumVertices, boneData, boneStartPosition);

	FGameTexture* lastSkin = nullptr;
	for (unsigned i = 0; i < Meshes.Size(); i++)
	{
		FGameTexture* meshSkin = skin;

		if (!meshSkin)
		{
			if (surfaceskinids && surfaceskinids[i].isValid())
			{
				meshSkin = TexMan.GetGameTexture(surfaceskinids[i], true);
			}
			else if (Meshes[i].Skin.isValid())
			{
				meshSkin = TexMan.GetGameTexture(Meshes[i].Skin, true);
			}	
			else
			{
				continue;
			}
		}

		if (meshSkin->isValid())
		{
			if (meshSkin != lastSkin)
			{
				renderer->SetMaterial(meshSkin, false, translation);
				lastSkin = meshSkin;
			}
			renderer->DrawElements(Meshes[i].NumTriangles * 3, Meshes[i].FirstTriangle * 3 * sizeof(unsigned int));
		}
	}
}

void IQMModel::BuildVertexBuffer(FModelRenderer* renderer)
{
	if (!GetVertexBuffer(renderer->GetType()))
	{
		LoadGeometry();

		auto vbuf = renderer->CreateVertexBuffer(true, true);
		SetVertexBuffer(renderer->GetType(), vbuf);

		FModelVertex* vertptr = vbuf->LockVertexBuffer(Vertices.Size());
		memcpy(vertptr, Vertices.Data(), Vertices.Size() * sizeof(FModelVertex));
		vbuf->UnlockVertexBuffer();

		unsigned int* indxptr = vbuf->LockIndexBuffer(Triangles.Size() * 3);
		memcpy(indxptr, Triangles.Data(), Triangles.Size() * sizeof(unsigned int) * 3);
		vbuf->UnlockIndexBuffer();

		UnloadGeometry();
	}
}

void IQMModel::AddSkins(uint8_t* hitlist, const FTextureID* surfaceskinids)
{
	for (unsigned i = 0; i < Meshes.Size(); i++)
	{
		if (surfaceskinids && surfaceskinids[i].isValid())
			hitlist[surfaceskinids[i].GetIndex()] |= FTextureManager::HIT_Flat;
	}
}

const TArray<TRS>* IQMModel::AttachAnimationData()
{
	return &TRSData;
}

static TRS InterpolateBone(const TRS &from, const TRS &to, float t, float invt)
{
	TRS bone;

	bone.translation = from.translation * invt + to.translation * t;
	bone.rotation = from.rotation * invt;

	if ((bone.rotation | to.rotation * t) < 0)
	{
		bone.rotation.X *= -1; bone.rotation.Y *= -1; bone.rotation.Z *= -1; bone.rotation.W *= -1;
	}

	bone.rotation += to.rotation * t;
	bone.rotation.MakeUnit();
	bone.scaling = from.scaling * invt + to.scaling * t;

	return bone;
}

const TArray<VSMatrix> IQMModel::CalculateBones(int frame1, int frame2, float inter, int frame1_prev, float inter1_prev, int frame2_prev, float inter2_prev, const TArray<TRS>* animationData, DBoneComponents* boneComponentData, int index)
{
	const TArray<TRS>& animationFrames = animationData ? *animationData : TRSData;
	if (Joints.Size() > 0)
	{
		int numbones = Joints.SSize();

		if (boneComponentData->trscomponents[index].SSize() != numbones)
			boneComponentData->trscomponents[index].Resize(numbones);
		if (boneComponentData->trsmatrix[index].SSize() != numbones)
			boneComponentData->trsmatrix[index].Resize(numbones);

		frame1 = clamp(frame1, 0, (animationFrames.SSize() - 1) / numbones);
		frame2 = clamp(frame2, 0, (animationFrames.SSize() - 1) / numbones);

		int offset1 = frame1 * numbones;
		int offset2 = frame2 * numbones;

		int offset1_1 = frame1_prev * numbones;
		int offset2_1 = frame2_prev * numbones;

		float invt = 1.0f - inter;
		float invt1 = 1.0f - inter1_prev;
		float invt2 = 1.0f - inter2_prev;

		float swapYZ[16] = { 0.0f };
		swapYZ[0 + 0 * 4] = 1.0f;
		swapYZ[1 + 2 * 4] = 1.0f;
		swapYZ[2 + 1 * 4] = 1.0f;
		swapYZ[3 + 3 * 4] = 1.0f;

		TArray<VSMatrix> bones(numbones, true);
		TArray<bool> modifiedBone(numbones, true);
		for (int i = 0; i < numbones; i++)
		{
			TRS prev;

			if(frame1 >= 0 && (frame1_prev >= 0 || inter1_prev < 0))
			{
				prev = inter1_prev <= 0 ? animationFrames[offset1 + i] : InterpolateBone(animationFrames[offset1_1 + i], animationFrames[offset1 + i], inter1_prev, invt1);
			}

			TRS next;

			if(frame2 >= 0 && (frame2_prev >= 0 || inter2_prev < 0))
			{
				next = inter2_prev <= 0 ? animationFrames[offset2 + i] : InterpolateBone(animationFrames[offset2_1 + i], animationFrames[offset2 + i], inter2_prev, invt2);
			}

			TRS bone;

			if(frame1 >= 0 || inter < 0)
			{
				bone = inter < 0 ? animationFrames[offset1 + i] : InterpolateBone(prev, next , inter, invt);
			}

			if (Joints[i].Parent >= 0 && modifiedBone[Joints[i].Parent])
			{
				boneComponentData->trscomponents[index][i] = bone;
				modifiedBone[i] = true;
			}
			else if (boneComponentData->trscomponents[index][i].Equals(bone))
			{
				bones[i] = boneComponentData->trsmatrix[index][i];
				modifiedBone[i] = false;
				continue;
			}
			else
			{
				boneComponentData->trscomponents[index][i] = bone;
				modifiedBone[i] = true;
			}

			VSMatrix m;
			m.loadIdentity();
			m.translate(bone.translation.X, bone.translation.Y, bone.translation.Z);
			m.multQuaternion(bone.rotation);
			m.scale(bone.scaling.X, bone.scaling.Y, bone.scaling.Z);

			VSMatrix& result = bones[i];
			if (Joints[i].Parent >= 0)
			{
				result = bones[Joints[i].Parent];
				result.multMatrix(swapYZ);
				result.multMatrix(baseframe[Joints[i].Parent]);
				result.multMatrix(m);
				result.multMatrix(inversebaseframe[i]);
			}
			else
			{
				result.loadMatrix(swapYZ);
				result.multMatrix(m);
				result.multMatrix(inversebaseframe[i]);
			}
			result.multMatrix(swapYZ);
		}

		boneComponentData->trsmatrix[index] = bones;

		return bones;
	}
	return {};
}
