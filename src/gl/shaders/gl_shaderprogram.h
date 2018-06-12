
#pragma once

#include "hwrenderer/data/uniformbuffer.h"
#include "v_video.h"
#include "gl/data/gl_uniformbuffer.h"
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
		if (mBuffer != nullptr)
			delete mBuffer;
	}

	FString CreateDeclaration(const char *name, const std::vector<UniformFieldDesc> &fields)
	{
		mFields = fields;

		FString decl;
		decl.Format("layout(%s) uniform %s\n{\n", screen->GetUniformLayoutString(mBindingPoint).GetChars(), name);
		for (const auto &field : fields)
		{
			decl.AppendFormat("\t%s %s;\n", GetTypeStr(field.Type), field.Name);
		}
		decl += "};\n";

		return decl;
	}

	void Init()
	{
		if (mBuffer == nullptr)
			mBuffer = screen->CreateUniformBuffer(sizeof(T));
	}

	void Set(int index)
	{
		if (mBuffer != nullptr)
			mBuffer->SetData(&Values);

		// Needs to be done in an API independent way!
		glBindBufferBase(GL_UNIFORM_BUFFER, index, ((GLUniformBuffer*)mBuffer)->ID());
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

    IUniformBuffer *mBuffer = nullptr;
	std::vector<UniformFieldDesc> mFields;
	int mBindingPoint; // Fixme: This needs to be known on init because Vulkan wants to put it into the block declaration.
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
	void SetUniformBufferLocation(int index, const char *name);
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
