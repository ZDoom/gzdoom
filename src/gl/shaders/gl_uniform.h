#ifndef __GL_UNIFORM_H
#define __GL_UNIFORM_H

#include "doomtype.h"

//==========================================================================
//
//
//==========================================================================

class FUniform1i
{
	int mIndex;

public:
	void Init(GLuint hShader, const GLchar *name)
	{
		mIndex = glGetUniformLocation(hShader, name);
	}

	void Set(int newvalue)
	{
		glUniform1i(mIndex, newvalue);
	}
};

class FBufferedUniform1i
{
	int mBuffer;
	int mIndex;

public:
	void Init(GLuint hShader, const GLchar *name)
	{
		mIndex = glGetUniformLocation(hShader, name);
		mBuffer = 0;
	}

	void Set(int newvalue)
	{
		if (newvalue != mBuffer)
		{
			mBuffer = newvalue;
			glUniform1i(mIndex, newvalue);
		}
	}
};

class FBufferedUniform4i
{
	int mBuffer[4];
	int mIndex;

public:
	void Init(GLuint hShader, const GLchar *name)
	{
		mIndex = glGetUniformLocation(hShader, name);
		memset(mBuffer, 0, sizeof(mBuffer));
	}

	void Set(const int *newvalue)
	{
		if (memcmp(newvalue, mBuffer, sizeof(mBuffer)))
		{
			memcpy(mBuffer, newvalue, sizeof(mBuffer));
			glUniform4iv(mIndex, 1, newvalue);
		}
	}
};

class FBufferedUniform1f
{
	float mBuffer;
	int mIndex;

public:
	void Init(GLuint hShader, const GLchar *name)
	{
		mIndex = glGetUniformLocation(hShader, name);
		mBuffer = 0;
	}

	void Set(float newvalue)
	{
		if (newvalue != mBuffer)
		{
			mBuffer = newvalue;
			glUniform1f(mIndex, newvalue);
		}
	}
};

class FBufferedUniform2f
{
	float mBuffer[2];
	int mIndex;

public:
	void Init(GLuint hShader, const GLchar *name)
	{
		mIndex = glGetUniformLocation(hShader, name);
		memset(mBuffer, 0, sizeof(mBuffer));
	}

	void Set(const float *newvalue)
	{
		if (memcmp(newvalue, mBuffer, sizeof(mBuffer)))
		{
			memcpy(mBuffer, newvalue, sizeof(mBuffer));
			glUniform2fv(mIndex, 1, newvalue);
		}
	}
};

class FBufferedUniform4f
{
	float mBuffer[4];
	int mIndex;

public:
	void Init(GLuint hShader, const GLchar *name)
	{
		mIndex = glGetUniformLocation(hShader, name);
		memset(mBuffer, 0, sizeof(mBuffer));
	}

	void Set(const float *newvalue)
	{
		if (memcmp(newvalue, mBuffer, sizeof(mBuffer)))
		{
			memcpy(mBuffer, newvalue, sizeof(mBuffer));
			glUniform4fv(mIndex, 1, newvalue);
		}
	}
};

class FUniform4f
{
	int mIndex;

public:
	void Init(GLuint hShader, const GLchar *name)
	{
		mIndex = glGetUniformLocation(hShader, name);
	}

	void Set(const float *newvalue)
	{
		glUniform4fv(mIndex, 1, newvalue);
	}

	void Set(float a, float b, float c, float d)
	{
		glUniform4f(mIndex, a, b, c, d);
	}
};

class FBufferedUniformPE
{
	PalEntry mBuffer;
	int mIndex;

public:
	void Init(GLuint hShader, const GLchar *name)
	{
		mIndex = glGetUniformLocation(hShader, name);
		mBuffer = 0;
	}

	void Set(PalEntry newvalue)
	{
		if (newvalue != mBuffer)
		{
			mBuffer = newvalue;
			glUniform4f(mIndex, newvalue.r/255.f, newvalue.g/255.f, newvalue.b/255.f, newvalue.a/255.f);
		}
	}
};


struct FMaterialUniform
{
	FGLUniform *mUni;
	int mLocation[2];

	void apply(bool nat);
};

class FShader;

class FGLUniform
{
protected:
	FString mName;
	int mType;

public:
	FGLUniform(const FString &name)
	{
		mName = name;
	}

	virtual ~FGLUniform() {}
	virtual void apply(int location) = 0;
	int getLocation(FShader *shader);
	FMaterialUniform getMaterialUniform(FShader *shader1, FShader *shader2);	// two shaders because we have different ones for alpha test on and off.
};

class FGLUniformConst1f : public FGLUniform
{
	float mValue;
public:
	FGLUniformConst1f(const FString & name, float val)
		: FGLUniform(name)
	{
		mValue = val;
	}
	virtual void apply(int location);
};

class FGLUniformConst2f : public FGLUniform
{
	float mValue[2];
public:
	FGLUniformConst2f(const FString & name, float *val)
		: FGLUniform(name)
	{
		mValue[0] = val[0];
		mValue[1] = val[1];
	}
	virtual void apply(int location);
};

class FGLUniformConst3f : public FGLUniform
{
	float mValue[3];
public:
	FGLUniformConst3f(const FString & name, float *val)
		: FGLUniform(name)
	{
		mValue[0] = val[0];
		mValue[1] = val[1];
		mValue[2] = val[2];
	}
	virtual void apply(int location);
};

class FGLUniformConst4f : public FGLUniform
{
	float mValue[4];
public:
	FGLUniformConst4f(const FString & name, float *val)
		: FGLUniform(name)
	{
		mValue[0] = val[0];
		mValue[1] = val[1];
		mValue[2] = val[2];
		mValue[3] = val[3];
	}
	FGLUniformConst4f(const FString & name, float v1, float v2, float v3, float v4)
		: FGLUniform(name)
	{
		mValue[0] = v1;
		mValue[1] = v2;
		mValue[2] = v3;
		mValue[3] = v4;
	}
	virtual void apply(int location);
};

class FGLUniformConst1i : public FGLUniform
{
	int mValue;
public:
	FGLUniformConst1i(const FString & name, int val)
		: FGLUniform(name)
	{
		mValue = val;
	}
	virtual void apply(int location);
};

class FGLUniformConst2i : public FGLUniform
{
	int mValue[2];
public:
	FGLUniformConst2i(const FString & name, int *val)
		: FGLUniform(name)
	{
		mValue[0] = val[0];
		mValue[1] = val[1];
	}
	virtual void apply(int location);
};

class FGLUniformConst3i : public FGLUniform
{
	int mValue[3];
public:
	FGLUniformConst3i(const FString & name, int *val)
		: FGLUniform(name)
	{
		mValue[0] = val[0];
		mValue[1] = val[1];
		mValue[2] = val[2];
	}
	virtual void apply(int location);
};

class FGLUniformConst4i : public FGLUniform
{
	int mValue[4];
public:
	FGLUniformConst4i(const FString & name, int *val)
		: FGLUniform(name)
	{
		mValue[0] = val[0];
		mValue[1] = val[1];
		mValue[2] = val[2];
		mValue[3] = val[3];
	}
	virtual void apply(int location);
};

inline void FMaterialUniform::apply(bool nat)
{
	if (mLocation[nat] >= 0) mUni->apply(mLocation[nat]);
}




#endif