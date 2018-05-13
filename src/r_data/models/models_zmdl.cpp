// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2018 Magnus Norddahl
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

#include "w_wad.h"
#include "cmdlib.h"
#include "r_data/models/models_zmdl.h"

struct ZModelStream
{
	ZModelStream(const char *buffer, int length) : filebuffer(buffer), filelength(length) { }

	void BeginChunk()
	{
		if (!ReadError() && filepos + 8 <= filelength)
		{
			name = *(uint32_t*)(filebuffer + filepos);
			length = *(uint32_t*)(filebuffer + filepos + 4);
			buffer = filebuffer + filepos + 8;
			pos = 0;
			if (length > 0x0f000000 || filepos + 8 + length > (uint32_t)filelength)
			{
				name = 0;
				pos = 1;
				length = 0;
				buffer = nullptr;
			}
		}
		else
		{
			name = 0;
			pos = 1;
			length = 0;
			buffer = nullptr;
		}
	}

	bool IsChunk(const char *n)
	{
		return memcmp(&name, n, 4) == 0;
	}

	void EndChunk()
	{
		if (!ReadError())
		{
			filepos += length + 8;
			name = 0;
			length = 0;
			pos = 0;
		}
	}

	uint32_t Uint32() { return Read<uint32_t>(); }
	float Float() { return Read<float>(); }
	ZModelVec2f Vec2f() { return Read<ZModelVec2f>(); }
	ZModelVec3f Vec3f() { return Read<ZModelVec3f>(); }
	ZModelVec4ub Vec4ub() { return Read<ZModelVec4ub>(); }
	ZModelQuaternionf Quaternionf() { return Read<ZModelQuaternionf>(); }

	TArray<uint32_t> Uint32Array() { return ReadArray<uint32_t>(); }
	TArray<float> FloatArray() { return ReadArray<float>(); }
	TArray<ZModelVec2f> Vec2fArray() { return ReadArray<ZModelVec2f>(); }
	TArray<ZModelVec3f> Vec3fArray() { return ReadArray<ZModelVec3f>(); }
	TArray<ZModelVec4ub> Vec4ubArray() { return ReadArray<ZModelVec4ub>(); }
	TArray<ZModelQuaternionf> QuaternionfArray() { return ReadArray<ZModelQuaternionf>(); }
	TArray<ZModelVertex> VertexArray() { return ReadArray<ZModelVertex>(); }

	FString String()
	{
		uint32_t strend = pos;
		while (strend < length && buffer[strend] != 0)
			strend++;

		const char *str = buffer + pos;
		int length = strend - pos;
		pos = strend + 1;
		return FString(str, length);
	}

	TArray<FString> StringArray()
	{
		TArray<FString> list;
		for (int i = Uint32(); i != 0 && !ReadError(); i--)
			list.Push(String());
		return list;
	}

	bool ReadError() const { return pos > length; }

private:
	const char *filebuffer = nullptr;
	int filelength = 0;
	int filepos = 0;

	uint32_t name = 0;
	uint32_t length = 0;
	const char *buffer = nullptr;
	uint32_t pos = 0;

	template<typename T>
	T Read()
	{
		if (pos + sizeof(T) <= (size_t)length)
		{
			T value;
			memcpy(&value, buffer + pos, sizeof(T));
			pos += sizeof(T);
			return value;
		}
		else
		{
			pos += sizeof(T);
			return T();
		}
	}

	template<typename T>
	TArray<T> ReadArray()
	{
		uint32_t count = Uint32();
		if (count != 0 && pos + count * sizeof(T) <= (size_t)length)
		{
			TArray<T> values(count, true);
			memcpy(&values[0], buffer + pos, count * sizeof(T));
			pos += count * sizeof(T);
			return values;
		}
		else
		{
			pos += count * sizeof(T);
			return TArray<T>();
		}
	}
};

bool FZMDLModel::Load(const char *filename, int lumpnum, const char *buffer, int length)
{
	mLumpNum = lumpnum;

	ZModelStream s(buffer, length);

	s.BeginChunk();

	// First chunk must always be ZMDL
	if (!s.IsChunk("ZMDL"))
		return false;

	mVersion = s.Uint32();
	if (mVersion != 1)
		return false;

	for (auto i = s.Uint32(); i > 0; i--)
	{
		ZModelMaterial mat;
		mat.Name = s.String();
		mat.Flags = s.Uint32();
		mat.Renderstyle.AsDWORD = s.Uint32();
		mat.StartElement = s.Uint32();
		mat.VertexCount = s.Uint32();
		mMaterials.Push(mat);
		if (s.ReadError()) return false;
	}

	for (auto i = s.Uint32(); i > 0; i--)
	{
		ZModelBone bone;
		bone.Name = s.String();
		bone.Type = (ZModelBoneType)s.Uint32();
		bone.ParentBone = s.Uint32();
		bone.Pivot = s.Vec3f();
		mBones.Push(bone);
		if (s.ReadError()) return false;
	}

	for (auto i = s.Uint32(); i > 0; i--)
	{
		ZModelAnimation anim;
		anim.Name = s.String();
		anim.VariationNext = s.Uint32();
		anim.Frequency = s.Float();
		anim.MinReplayCount = s.Uint32();
		anim.MaxReplayCount = s.Uint32();
		anim.Duration = s.Uint32();
		anim.BlendTimeIn = s.Uint32();
		anim.BlendTimeOut = s.Uint32();
		anim.AabbMin = s.Vec3f();
		anim.AabbMax = s.Vec3f();
		for (auto j = s.Uint32(); j > 0; j--)
		{
			ZModelBoneAnim bone;
			bone.Translation.Timestamps = s.Uint32Array();
			bone.Translation.Values = s.Vec3fArray();
			bone.Rotation.Timestamps = s.Uint32Array();
			bone.Rotation.Values = s.QuaternionfArray();
			bone.Scale.Timestamps = s.Uint32Array();
			bone.Scale.Values = s.Vec3fArray();
			anim.Bones.Push(bone);
			if (s.ReadError()) return false;
		}
		for (auto j = s.Uint32(); j > 0; j--)
		{
			ZModelMaterialAnim mat;
			mat.Translation.Timestamps = s.Uint32Array();
			mat.Translation.Values = s.Vec3fArray();
			mat.Rotation.Timestamps = s.Uint32Array();
			mat.Rotation.Values = s.QuaternionfArray();
			mat.Scale.Timestamps = s.Uint32Array();
			mat.Scale.Values = s.Vec3fArray();
			anim.Materials.Push(mat);
			if (s.ReadError()) return false;
		}
		anim.Events.Timestamps = s.Uint32Array();
		anim.Events.Values = s.Uint32Array();
		mAnimations.Push(anim);
		if (s.ReadError()) return false;
	}

	for (auto i = s.Uint32(); i > 0; i--)
	{
		ZModelAttachment attach;
		attach.Name = s.String();
		attach.Bone = s.Uint32();
		attach.Position = s.Vec3f();
		mAttachments.Push(attach);
		if (s.ReadError()) return false;
	}

	mEvents = s.StringArray();

	s.EndChunk();

	return !s.ReadError();
}

int FZMDLModel::FindFrame(const char *name)
{
	return 0;
}

void FZMDLModel::RenderFrame(FModelRenderer *renderer, FTexture *skin, int frame, int frame2, double inter, int translation)
{
	currentTime = (currentTime + 16) % mAnimations[0].Duration;

	TArray<VSMatrix> bones(mBones.Size(), true);
	for (unsigned int i = 0; i < mBones.Size(); i++)
	{
		// Find keys
		unsigned int translationIndex, translationIndex2;
		unsigned int rotationIndex, rotationIndex2;
		unsigned int scaleIndex, scaleIndex2;
		float translationInterpolation = mAnimations[0].Bones[i].Translation.FindAnimationKeys(currentTime, translationIndex, translationIndex2);
		float rotationInterpolation = mAnimations[0].Bones[i].Rotation.FindAnimationKeys(currentTime, rotationIndex, rotationIndex2);
		float scaleInterpolation = mAnimations[0].Bones[i].Scale.FindAnimationKeys(currentTime, scaleIndex, scaleIndex2);

		// Get values
		ZModelVec3f translation = mAnimations[0].Bones[i].Translation.Values[translationIndex];
		ZModelQuaternionf rotation = mAnimations[0].Bones[i].Rotation.Values[rotationIndex];
		ZModelVec3f scale = mAnimations[0].Bones[i].Scale.Values[scaleIndex];

		// Interpolate
		//ZModelVec3f translation2 = mAnimations[0].Bones[i].Translation.Values[translationIndex2];
		//ZModelQuaternionf rotation2 = mAnimations[0].Bones[i].Rotation.Values[rotationIndex2];
		//ZModelVec3f scale2 = mAnimations[0].Bones[i].Scale.Values[scaleIndex2];
		//position = mix(translation, position2, translationInterpolation);
		//rotation = Quaternionf::slerp(rotation, rotation2, rotationInterpolation);
		//scale = mix(scale, scale2, scaleInterpolation);

		// Quaternion to matrix
		float m[16] = { 0.0f };
		m[0 * 4 + 0] = 1.0f - 2.0f * rotation.Y*rotation.Y - 2.0f * rotation.Z*rotation.Z;
		m[1 * 4 + 0] = 2.0f * rotation.X*rotation.Y - 2.0f * rotation.W*rotation.Z;
		m[2 * 4 + 0] = 2.0f * rotation.X*rotation.Z + 2.0f * rotation.W*rotation.Y;
		m[0 * 4 + 1] = 2.0f * rotation.X*rotation.Y + 2.0f * rotation.W*rotation.Z;
		m[1 * 4 + 1] = 1.0f - 2.0f * rotation.X*rotation.X - 2.0f * rotation.Z*rotation.Z;
		m[2 * 4 + 1] = 2.0f * rotation.Y*rotation.Z - 2.0f * rotation.W*rotation.X;
		m[0 * 4 + 2] = 2.0f * rotation.X*rotation.Z - 2.0f * rotation.W*rotation.Y;
		m[1 * 4 + 2] = 2.0f * rotation.Y*rotation.Z + 2.0f * rotation.W*rotation.X;
		m[2 * 4 + 2] = 1.0f - 2.0f * rotation.X*rotation.X - 2.0f * rotation.Y*rotation.Y;
		m[3 * 4 + 3] = 1.0f;

		// Create 4x4 bones transform
		bones[i].loadIdentity();
		bones[i].translate(translation.X, translation.Y, translation.Z);
		bones[i].scale(scale.X, scale.Y, scale.Z);
		bones[i].multMatrix(m);
		bones[i].translate(-mBones[i].Pivot.X, -mBones[i].Pivot.Y, -mBones[i].Pivot.Z);
	}

	mVBuf->SetupFrame(renderer, 0, 0, 0, bones);

	for (unsigned int i = 0; i < mMaterials.Size(); i++)
	{
		FTexture *surfaceSkin = TexMan(mMaterials[i].Name.GetChars()); // To do: use texture id!!!!
		renderer->SetMaterial(surfaceSkin, false, translation);
		renderer->DrawElements(mMaterials[i].VertexCount, mMaterials[i].StartElement * sizeof(unsigned int));
	}
}

void FZMDLModel::BuildVertexBuffer(FModelRenderer *renderer)
{
	if (mVBuf == nullptr)
	{
		int length = Wads.LumpLength(mLumpNum);
		FMemLump lumpdata = Wads.ReadLump(mLumpNum);
		const char *buffer = (const char *)lumpdata.GetMem();
		ZModelStream s(buffer, length);

		while (!s.ReadError())
		{
			s.BeginChunk();

			if (s.IsChunk("ZDAT"))
			{
				mVertices = s.VertexArray();
				mElements = s.Uint32Array();
				break;
			}
			else if (s.IsChunk("ZEND"))
			{
				break;
			}

			s.EndChunk();
		}

		mVBuf = renderer->CreateVertexBuffer(true, true);

		FModelVertex *vertptr = mVBuf->LockVertexBuffer(mVertices.Size());
		for (unsigned int i = 0; i < mVertices.Size(); i++)
		{
			const auto &v = mVertices[i];
			vertptr[i].Set(v.Pos.X, v.Pos.Y, v.Pos.Z, v.TexCoords.X, v.TexCoords.Y);
			vertptr[i].SetNormal(v.Normal.X, v.Normal.Y, v.Normal.Z);
			vertptr[i].SetBoneSelector(v.BoneIndices.X, v.BoneIndices.Y, v.BoneIndices.Z, v.BoneIndices.W);
			vertptr[i].SetBoneWeight(v.BoneWeights.X, v.BoneWeights.Y, v.BoneWeights.Z, v.BoneWeights.W);
		}
		mVBuf->UnlockVertexBuffer();

		unsigned int *indxptr = mVBuf->LockIndexBuffer(mElements.Size());
		memcpy(indxptr, &mElements[0], sizeof(uint32_t) * mElements.Size());
		mVBuf->UnlockIndexBuffer();

		mVertices.Clear();
		mElements.Clear();
	}
}

void FZMDLModel::AddSkins(uint8_t *hitlist)
{
}
