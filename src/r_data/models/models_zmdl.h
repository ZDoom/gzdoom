
#pragma once

#include "models.h"

struct ZModelVec2f
{
	float X, Y;
};

struct ZModelVec3f
{
	float X, Y, Z;
};

struct ZModelVec4ub
{
	uint8_t X, Y, Z, W;
};

struct ZModelQuaternionf
{
	float X, Y, Z, W;
};

struct ZModelVertex
{
	ZModelVec3f Pos;
	ZModelVec4ub BoneWeights;
	ZModelVec4ub BoneIndices;
	ZModelVec3f Normal;
	ZModelVec2f TexCoords;
};

struct ZModelMaterial
{
	FString Name;
	uint32_t Flags = 0; // Two-sided, depth test/write, what else?
	FRenderStyle Renderstyle;
	uint32_t StartElement = 0;
	uint32_t VertexCount = 0;
};

template<typename Value>
struct ZModelTrack
{
	TArray<float> Timestamps;
	TArray<Value> Values;

	float FindAnimationKeys(float timestamp, unsigned int &index, unsigned int &index2)
	{
		if (Timestamps.Size() == 0)
		{
			index = 0;
			index2 = 0;
			return 0.0f;
		}

		index = BinarySearch(timestamp);
		index2 = MIN(index + 1, Timestamps.Size() - 1);

		float start = Timestamps[index];
		float end = Timestamps[index2];
		if (start != end)
			return clamp((timestamp - start) / (end - start), 0.0f, 1.0f);
		else
			return 0.0f;
	}

private:
	unsigned int BinarySearch(float timestamp) const
	{
		int search_max = static_cast<int>(Timestamps.Size()) - 1;
		int imin = 0;
		int imax = search_max;
		while (imin < imax)
		{
			int imid = imin + (imax - imin) / 2;
			if (Timestamps[imid] > timestamp)
				imax = imid;
			else
				imin = imid + 1;
		}
		bool found = (imax == imin && Timestamps[imin] > timestamp);
		return found ? imin - 1 : search_max;
	}
};

struct ZModelBoneAnim
{
	ZModelTrack<ZModelVec3f> Translation;
	ZModelTrack<ZModelQuaternionf> Rotation;
	ZModelTrack<ZModelVec3f> Scale;
};

struct ZModelMaterialAnim
{
	ZModelTrack<ZModelVec3f> Translation;
	ZModelTrack<ZModelQuaternionf> Rotation; // Rotation center is texture center (0.5, 0.5)
	ZModelTrack<ZModelVec3f> Scale;
};

struct ZModelAnimation
{
	FString Name;
	float Duration; // Length of this animation sequence in seconds

	ZModelVec3f AabbMin; // Animation bounds (for culling purposes)
	ZModelVec3f AabbMax;

	TArray<ZModelBoneAnim> Bones; // Animation tracks for each bone
	TArray<ZModelMaterialAnim> Materials; // Animation tracks for each material
};

enum class ZModelBoneType : uint32_t
{
	Normal,
	BillboardSpherical,
	BillboardCylindricalX,
	BillboardCylindricalY,
	BillboardCylindricalZ
};

struct ZModelBone
{
	FString Name;
	ZModelBoneType Type = ZModelBoneType::Normal;
	int32_t ParentBone = -1;
	ZModelVec3f Pivot;
};

struct ZModelAttachment
{
	FString Name;
	int32_t Bone = -1;
	ZModelVec3f Position;
};

class FZMDLModel : public FModel
{
public:
	bool Load(const char * fn, int lumpnum, const char * buffer, int length) override;
	int FindFrame(const char * name) override;
	void RenderFrame(FModelRenderer *renderer, FTexture * skin, int frame, int frame2, double inter, int translation = 0) override;
	void BuildVertexBuffer(FModelRenderer *renderer) override;
	void AddSkins(uint8_t *hitlist) override;

private:
	int mLumpNum;

	// ZMDL chunk
	uint32_t mVersion;
	TArray<ZModelMaterial> mMaterials;
	TArray<ZModelBone> mBones;
	TArray<ZModelAnimation> mAnimations;
	TArray<ZModelAttachment> mAttachments;
	bool mValid = false;

	TArray<VSMatrix> mBoneTransforms;
	TArray<FTextureID> mTextureIDs;

	int mCurrentAnim = 0;

	float currentTime = 0.0f;
};
