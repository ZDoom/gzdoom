#pragma once



struct FModelVertex
{
	float x, y, z;	// world position
	float u, v;		// texture coordinates
	unsigned packedNormal;	// normal vector as GL_INT_2_10_10_10_REV.
	float lu, lv;	// lightmap texture coordinates
	float lindex;	// lightmap texture index
	uint8_t boneselector[4];
	uint8_t boneweight[4];

	void Set(float xx, float yy, float zz, float uu, float vv)
	{
		x = xx;
		y = yy;
		z = zz;
		u = uu;
		v = vv;
		lu = 0.0f;
		lv = 0.0f;
		lindex = -1.0f;
	}

	void SetNormal(float nx, float ny, float nz)
	{
		int inx = clamp(int(nx * 512), -512, 511);
		int iny = clamp(int(ny * 512), -512, 511);
		int inz = clamp(int(nz * 512), -512, 511);
		int inw = 0;
		packedNormal = (inw << 30) | ((inz & 1023) << 20) | ((iny & 1023) << 10) | (inx & 1023);
	}

	void SetBoneSelector(int x, int y, int z, int w)
	{
		boneselector[0] = x;
		boneselector[1] = y;
		boneselector[2] = z;
		boneselector[3] = w;
	}

	void SetBoneWeight(int x, int y, int z, int w)
	{
		boneweight[0] = x;
		boneweight[1] = y;
		boneweight[2] = z;
		boneweight[3] = w;
	}
};

#define VMO ((FModelVertex*)nullptr)

class IModelVertexBuffer
{
public:
	virtual ~IModelVertexBuffer() { }

	virtual FModelVertex *LockVertexBuffer(unsigned int size) = 0;
	virtual void UnlockVertexBuffer() = 0;

	virtual unsigned int *LockIndexBuffer(unsigned int size) = 0;
	virtual void UnlockIndexBuffer() = 0;
};

