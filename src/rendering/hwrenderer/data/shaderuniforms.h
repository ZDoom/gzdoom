#pragma once

#include <vector>
#include "hwrenderer/data/buffers.h"
#include "v_video.h"

enum
{
	LIGHTBUF_BINDINGPOINT = 1,
	POSTPROCESS_BINDINGPOINT = 2,
	VIEWPOINT_BINDINGPOINT = 3,
	LIGHTNODES_BINDINGPOINT = 4,
	LIGHTLINES_BINDINGPOINT = 5,
	LIGHTLIST_BINDINGPOINT = 6
};

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
	Mat4
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

class UniformBlockDecl
{
public:
	static FString Create(const char *name, const std::vector<UniformFieldDesc> &fields, int bindingpoint)
	{
		FString decl;
		FString layout;
		if (bindingpoint == -1)
		{
			layout = "push_constant";
		}
		else if (screen->glslversion < 4.20)
		{
			layout = "std140";
		}
		else
		{
			layout.Format("std140, binding = %d", bindingpoint);
		}
		decl.Format("layout(%s) uniform %s\n{\n", layout.GetChars(), name);
		for (size_t i = 0; i < fields.size(); i++)
		{
			decl.AppendFormat("\t%s %s;\n", GetTypeStr(fields[i].Type), fields[i].Name);
		}
		decl += "};\n";

		return decl;
	}

private:
	static const char *GetTypeStr(UniformType type)
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
		case UniformType::Mat4: return "mat4";
		}
	}
};

template<typename T, int bindingpoint>
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

	int BindingPoint() const
	{
		return bindingpoint;
	}

	FString CreateDeclaration(const char *name, const std::vector<UniformFieldDesc> &fields)
	{
		mFields = fields;
		return UniformBlockDecl::Create(name, fields, bindingpoint);
	}

	void Init()
	{
		if (mBuffer == nullptr)
			mBuffer = screen->CreateDataBuffer(bindingpoint, false, false);
	}

	void Set(bool bind = true)
	{
		if (mBuffer != nullptr)
			mBuffer->SetData(sizeof(T), &Values);

		// Let's hope this can be done better when things have moved further ahead.
		// This really is not the best place to add something that depends on API behavior.
		if (bind) mBuffer->BindBase();
	}

	T *operator->() { return &Values; }
	const T *operator->() const { return &Values; }

	T Values;

private:
	ShaderUniforms(const ShaderUniforms &) = delete;
	ShaderUniforms &operator=(const ShaderUniforms &) = delete;

    IDataBuffer *mBuffer = nullptr;
	std::vector<UniformFieldDesc> mFields;
};


