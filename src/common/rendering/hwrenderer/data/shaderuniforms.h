#pragma once

#include <vector>
#include "hwrenderer/data/buffers.h"
#include "v_video.h"

enum
{
	LIGHTBUF_BINDINGPOINT = 1,
	VIEWPOINT_BINDINGPOINT = 3,
	LIGHTNODES_BINDINGPOINT = 4,
	LIGHTLINES_BINDINGPOINT = 5,
	LIGHTLIST_BINDINGPOINT = 6,
	BONEBUF_BINDINGPOINT = 7
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
	UniformFieldDesc() = default;
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
