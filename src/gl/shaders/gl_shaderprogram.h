
#pragma once

#include "gl_shader.h"

enum class UniformType
{
	Int,
	UInt,
	Float,
	Vec2,
	Vec3,
	Vec4,
	IVec2,
	IVec3,
	IVec4,
	UVec2,
	UVec3,
	UVec4,
};

class UniformFieldDesc
{
public:
	UniformFieldDesc() { }
	UniformFieldDesc(const char *name, UniformType type, std::size_t offset) : Name(name), Type(type), Offset(offset) { }

	const char *Name;
	UniformType Type;
	std::size_t Offset;
};

template<typename T>
class ShaderUniforms
{
public:
	ShaderUniforms()
	{
		memset(&Values, 0, sizeof(Values));
	}

	~ShaderUniforms()
	{
		if (mBufferHandle)
			glDeleteBuffers(1, &mBufferHandle);
	}

	FString CreateDeclaration(const char *name, const std::vector<UniformFieldDesc> &fields)
	{
		mName = name;
		mFields = fields;

		FString decl;
		decl.Format("layout(std140) uniform %s\n{\n", name);
		for (const auto &field : fields)
		{
			decl.AppendFormat("\t%s %s;\n", GetTypeStr(field.Type), field.Name);
		}
		decl += "};\n";

		return decl;
	}

	void Init(GLuint hShader, int index = POSTPROCESS_BINDINGPOINT)
	{
		GLuint uniformBlockIndex = glGetUniformBlockIndex(hShader, mName);
		if (uniformBlockIndex != GL_INVALID_INDEX)
		{
			glUniformBlockBinding(hShader, uniformBlockIndex, index);
			glGenBuffers(1, (GLuint*)&mBufferHandle);
			mIndex = index;
		}
	}

	void Set()
	{
		glBindBuffer(GL_UNIFORM_BUFFER, mBufferHandle);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(T), &Values, GL_STREAM_DRAW);

		glBindBufferBase(GL_UNIFORM_BUFFER, mIndex, mBufferHandle);
	}

	T *operator->() { return &Values; }
	const T *operator->() const { return &Values; }

	T Values;

private:
	ShaderUniforms(const ShaderUniforms &) = delete;
	ShaderUniforms &operator=(const ShaderUniforms &) = delete;

	const char *GetTypeStr(UniformType type)
	{
		switch (type)
		{
		default:
		case UniformType::Int: return "int";
		case UniformType::UInt: return "uint";
		case UniformType::Float: return "float";
		case UniformType::Vec2: return "vec2";
		case UniformType::Vec3: return "vec3";
		case UniformType::Vec4: return "vec4";
		case UniformType::IVec2: return "ivec2";
		case UniformType::IVec3: return "ivec3";
		case UniformType::IVec4: return "ivec4";
		case UniformType::UVec2: return "uvec2";
		case UniformType::UVec3: return "uvec3";
		case UniformType::UVec4: return "uvec4";
		}
	}

	const char *mName = nullptr;
	int mIndex = -1;
	GLuint mBufferHandle = 0;
	std::vector<UniformFieldDesc> mFields;
	std::vector<int> mUniformLocations;
};

class FShaderProgram
{
public:
	FShaderProgram();
	~FShaderProgram();

	enum ShaderType
	{
		Vertex,
		Fragment,
		NumShaderTypes
	};

	void Compile(ShaderType type, const char *lumpName, const char *defines, int maxGlslVersion);
	void Compile(ShaderType type, const char *name, const FString &code, const char *defines, int maxGlslVersion);
	void SetFragDataLocation(int index, const char *name);
	void Link(const char *name);
	void SetAttribLocation(int index, const char *name);
	void Bind();

	operator GLuint() const { return mProgram; }
	explicit operator bool() const { return mProgram != 0; }

private:
	FShaderProgram(const FShaderProgram &) = delete;
	FShaderProgram &operator=(const FShaderProgram &) = delete;

	static FString PatchShader(ShaderType type, const FString &code, const char *defines, int maxGlslVersion);

	void CreateShader(ShaderType type);
	FString GetShaderInfoLog(GLuint handle);
	FString GetProgramInfoLog(GLuint handle);

	GLuint mProgram = 0;
	GLuint mShaders[NumShaderTypes];
};
