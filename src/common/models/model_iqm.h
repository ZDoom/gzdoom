#pragma once

#include <stdint.h>
#include "model.h"
#include "vectors.h"
#include "matrix.h"
#include "common/rendering/i_modelvertexbuffer.h"
#include "m_swap.h"

class DBoneComponents;

struct IQMMesh
{
	FString Name;
	FString Material;
	uint32_t FirstVertex;
	uint32_t NumVertices;
	uint32_t FirstTriangle;
	uint32_t NumTriangles;
	FTextureID Skin;
};

enum IQMVertexArrayType
{
	IQM_POSITION = 0,     // float, 3
	IQM_TEXCOORD = 1,     // float, 2
	IQM_NORMAL = 2,       // float, 3
	IQM_TANGENT = 3,      // float, 4
	IQM_BLENDINDEXES = 4, // ubyte, 4
	IQM_BLENDWEIGHTS = 5, // ubyte, 4
	IQM_COLOR = 6,        // ubyte, 4
	IQM_CUSTOM = 0x10
};

enum IQMVertexArrayFormat
{
	IQM_BYTE = 0,
	IQM_UBYTE = 1,
	IQM_SHORT = 2,
	IQM_USHORT = 3,
	IQM_INT = 4,
	IQM_UINT = 5,
	IQM_HALF = 6,
	IQM_FLOAT = 7,
	IQM_DOUBLE = 8,
};

struct IQMVertexArray
{
	IQMVertexArrayType Type;
	uint32_t Flags;
	IQMVertexArrayFormat Format;
	uint32_t Size;
	uint32_t Offset;
};

struct IQMTriangle
{
	uint32_t Vertex[3];
};

struct IQMAdjacency
{
	uint32_t Triangle[3];
};

struct IQMJoint
{
	FString Name;
	int32_t Parent; // parent < 0 means this is a root bone
	FVector3 Translate;
	FVector4 Quaternion;
	FVector3 Scale;
};

struct IQMPose
{
	int32_t Parent; // parent < 0 means this is a root bone
	uint32_t ChannelMask; // mask of which 10 channels are present for this joint pose
	float ChannelOffset[10];
	float ChannelScale[10];
	// channels 0..2 are translation <Tx, Ty, Tz> and channels 3..6 are quaternion rotation <Qx, Qy, Qz, Qw>
	// rotation is in relative/parent local space
	// channels 7..9 are scale <Sx, Sy, Sz>
	// output = (input*scale)*rotation + translation
};

struct IQMAnim
{
	FString Name;
	uint32_t FirstFrame;
	uint32_t NumFrames;
	float Framerate;
	bool Loop;
};

struct IQMBounds
{
	float BBMins[3];
	float BBMaxs[3];
	float XYRadius;
	float Radius;
};

class IQMFileReader;

class IQMModel : public FModel
{
public:
	IQMModel();
	~IQMModel();

	bool Load(const char* fn, int lumpnum, const char* buffer, int length) override;
	int FindFrame(const char* name, bool nodefault) override;
	void RenderFrame(FModelRenderer* renderer, FGameTexture* skin, int frame, int frame2, double inter, int translation, const FTextureID* surfaceskinids, const TArray<VSMatrix>& boneData, int boneStartPosition) override;
	void BuildVertexBuffer(FModelRenderer* renderer) override;
	void AddSkins(uint8_t* hitlist, const FTextureID* surfaceskinids) override;
	const TArray<TRS>* AttachAnimationData() override;
	const TArray<VSMatrix> CalculateBones(int frame1, int frame2, double inter, const TArray<TRS>& animationData, DBoneComponents* bones, int index) override;

private:
	void LoadGeometry();
	void UnloadGeometry();

	void LoadPosition(IQMFileReader& reader, const IQMVertexArray& vertexArray);
	void LoadTexcoord(IQMFileReader& reader, const IQMVertexArray& vertexArray);
	void LoadNormal(IQMFileReader& reader, const IQMVertexArray& vertexArray);
	void LoadBlendIndexes(IQMFileReader& reader, const IQMVertexArray& vertexArray);
	void LoadBlendWeights(IQMFileReader& reader, const IQMVertexArray& vertexArray);

	int mLumpNum = -1;
	TArray<IQMMesh> Meshes;
	TArray<IQMTriangle> Triangles;
	TArray<IQMAdjacency> Adjacency;
	TArray<IQMJoint> Joints;
	TArray<IQMPose> Poses;
	TArray<IQMAnim> Anims;
	TArray<IQMBounds> Bounds;
	TArray<IQMVertexArray> VertexArrays;
	uint32_t NumVertices = 0;

	TArray<FModelVertex> Vertices;

	TArray<VSMatrix> baseframe;
	TArray<VSMatrix> inversebaseframe;
	TArray<TRS> TRSData;
};

struct IQMReadErrorException { };

class IQMFileReader
{
public:
	IQMFileReader(const void* buffer, int length) : buffer((const char*)buffer), length(length) { }

	uint8_t ReadUByte()
	{
		uint8_t value;
		Read(&value, sizeof(uint8_t));
		return value;
	}

	int32_t ReadInt32()
	{
		int32_t value;
		Read(&value, sizeof(int32_t));
		value = LittleLong(value);
		return value;
	}

	int16_t ReadInt16()
	{
		int16_t value;
		Read(&value, sizeof(int16_t));
		value = LittleShort(value);
		return value;
	}

	uint32_t ReadUInt32()
	{
		return ReadInt32();
	}

	uint16_t ReadUInt16()
	{
		return ReadInt16();
	}

	float ReadFloat()
	{
		float value;
		Read(&value, sizeof(float));
		return value;
	}

	FString ReadName(const TArray<char>& textBuffer)
	{
		uint32_t nameOffset = ReadUInt32();
		if (nameOffset >= textBuffer.Size())
			throw IQMReadErrorException();
		return textBuffer.Data() + nameOffset;
	}

	void Read(void* data, int size)
	{
		if (pos + size > length || size < 0 || size > 0x0fffffff)
			throw IQMReadErrorException();
		memcpy(data, buffer + pos, size);
		pos += size;
	}

	void SeekTo(int newPos)
	{
		if (newPos < 0 || newPos > length)
			throw IQMReadErrorException();
		pos = newPos;
	}

private:
	const char* buffer = nullptr;
	int length = 0;
	int pos = 0;
};
