#pragma once

#include <common/scripting/vm/vmintern.h>
#include <common/scripting/vm/vm.h>
#include <common/scripting/core/types.h>
#include <common/objects/dobject.h>
#include <common/utility/zstring.h>
#include "Utilities.h"
#include "filesystem.h"

namespace DebugServer
{
struct TT : PType
{
	using TypeFlags = PType::ETypeFlags;
};

enum BasicType
{
	BASIC_NONE,
	BASIC_uint32,
	BASIC_int32,
	BASIC_uint16,
	BASIC_int16,
	BASIC_uint8,
	BASIC_int8,
	BASIC_float,
	BASIC_double,
	BASIC_bool,
	BASIC_string,
	BASIC_name,
	BASIC_SpriteID,
	BASIC_TextureID,
	BASIC_TranslationID,
	BASIC_Sound,
	BASIC_Color,
	BASIC_Enum,
	BASIC_StateLabel,
	BASIC_pointer,
	BASIC_VoidPointer,
};

static inline bool IsFunctionInvoked(VMFunction *func) { return func && func->VarFlags & VARF_Action; }

static inline bool IsFunctionStatic(VMFunction *func) { return func && (func->VarFlags & VARF_Static || !(func->VarFlags & VARF_Method)); }

static inline bool IsFunctionNative(VMFunction *func) { return func && func->VarFlags & VARF_Native; }

static inline bool IsFunctionAbstract(VMFunction *func) { return func && func->VarFlags & VARF_Abstract; }

static inline std::string GetIPRef(const char *qualifiedName, uint32_t PCdiff, const VMOP *PC) { return StringFormat("%s+%04x:%p", qualifiedName, PCdiff, PC); }

static inline std::string GetScriptPathNoQual(const std::string &scriptPath)
{
	auto colonPos = scriptPath.find(':');
	if (colonPos == std::string::npos)
	{
		return scriptPath;
	}
	return scriptPath.substr(colonPos + 1);
}
static inline bool ScriptHasQual(const std::string &scriptPath) { return scriptPath.find(':') != std::string::npos; }

static inline std::string GetScriptWithQual(const std::string &scriptPath, const std::string &container) { return !container.empty() ? container + ":" + GetScriptPathNoQual(scriptPath) : scriptPath; }


static inline std::string GetArchiveNameFromPath(const std::string &scriptPath)
{
	auto colonPos = scriptPath.find(':');
	if (colonPos != std::string::npos)
	{
		return scriptPath.substr(0, colonPos);
	}
	return {};
}

inline std::string normalizePath(const std::string &path)
{
	auto normPath = path;
	std::replace(normPath.begin(), normPath.end(), '\\', '/');
	return normPath;
}

static inline bool isScriptPath(const std::string &path)
{
	if (path.empty())
	{
		return false;
	}
	std::string scriptName = ToLowerCopy(path.substr(normalizePath(path).find_last_of('/') + 1));
	auto ext = scriptName.substr(scriptName.find_last_of('.') + 1);
	if (!(ext == "zs" || ext == "zsc" || ext == "zc" || ext == "acs" || ext == "dec" || (scriptName == "DECORATE") || (scriptName == "ACS")))
	{
		return false;
	}
	return true;
}

static inline std::string GetIPRefFromFrame(VMFrame *m_stackFrame)
{
	if (!m_stackFrame || IsFunctionNative(m_stackFrame->Func)) return "";
	auto scriptFunction = static_cast<VMScriptFunction *>(m_stackFrame->Func);
	uint32_t PCIndex = (uint32_t)(m_stackFrame->PC - scriptFunction->Code);
	return GetIPRef(m_stackFrame->Func->QualifiedName, PCIndex * 4, m_stackFrame->PC);
}

static inline uint64_t GetAddrFromIPRef(const std::string &ref)
{
	auto pos = ref.find_last_of('x');
	if (pos == std::string::npos || pos + 1 >= ref.size()) return 0;
	auto addr = ref.substr(pos + 1);
	return std::stoull(addr, nullptr, 16);
}

static inline uint32_t GetOffsetFromIPRef(const std::string &ref)
{
	auto pos = ref.find_last_of('+');
	auto pos2 = ref.find_last_of(':');
	if (pos == std::string::npos || pos2 == std::string::npos) return 0;
	auto offset = ref.substr(pos + 1, pos2 - pos - 1);
	return std::stoul(offset, nullptr, 16);
}

static inline std::string GetFuncNameFromIPRef(const std::string &ref)
{
	auto pos = ref.find_last_of('+');
	if (pos == std::string::npos || pos + 1 >= ref.size()) return "";
	return ref.substr(0, pos);
}

static inline bool IsBasicType(PType *type) { return type->Flags & TT::TypeFlags::TYPE_Scalar; }

// TODO: for some reason, unitialized fields will have their lower 32-bit set to 0x00000000, but their upper 32-bit will be random.
// This may be a bug; for now, just check if the lower 32-bit is 0
static inline bool IsVMValueValid(const VMValue *val) { return !(!val || !val->a || !val->i); }

static inline bool isFStringValid(const FString &str)
{
	auto chars = str.GetChars();
	// check the lower 32-bits of the char pointer
	auto ptr = *(uint32_t *)&chars;
	return ptr != 0;
}

static inline bool isValidDobject(DObject *obj)
{
	return obj
#ifndef NDEBUG
		&& obj->MagicID == DObject::MAGIC_ID;
#else
		;
#endif
}

static inline bool IsVMValValidDObject(const VMValue *val) { return IsVMValueValid(val) && isValidDobject(static_cast<DObject *>(val->a)); }

static inline VMValue GetVMValueVar(DObject *obj, FName field, PType *type)
{
	if (!isValidDobject(obj)) return VMValue();
	auto var = obj->ScriptVar(field, type);
	if (type == TypeString)
	{
		auto str = static_cast<FString *>(var);
		if (!isFStringValid(*str)) return VMValue();
		return VMValue(str);
	}
	else if (!type->isScalar())
	{
		return VMValue(var);
	}
	auto vmvar = static_cast<VMValue *>(var);
	return *vmvar;
}

static inline VMValue TruncateVMValue(const VMValue *val, BasicType pointed_type)
{
	// to make sure that it's set to 0 for all bits
	if (!val)
	{
		return VMValue();
	}
	VMValue new_val = (void *)nullptr;

	switch (pointed_type)
	{
	case BASIC_SpriteID:
	case BASIC_TextureID:
	case BASIC_Sound:
	case BASIC_Color:
	case BASIC_TranslationID:
	case BASIC_uint32:
	case BASIC_int32:
	case BASIC_Enum:
	case BASIC_StateLabel:
	case BASIC_name:
		// no need to truncate, they're all 4 byte integers
		new_val.i = val->i;
		break;
	case BASIC_uint16:
		new_val.i = static_cast<uint16_t>(val->i);
		break;
	case BASIC_int16:
		new_val.i = static_cast<int16_t>(val->i);
		break;
	case BASIC_uint8:
		new_val.i = static_cast<uint8_t>(val->i);
		break;
	case BASIC_int8:
		new_val.i = static_cast<int8_t>(val->i);
		break;
	case BASIC_float:
		new_val.f = static_cast<float>(val->f);
		break;
	case BASIC_double:
		new_val.f = static_cast<double>(val->f);
		break;
	case BASIC_bool:
		new_val.i = static_cast<bool>(val->i);
		break;
	case BASIC_string:
	case BASIC_pointer:
	case BASIC_VoidPointer:
		new_val = *val;
		break;
	default:
		break;
	}
	return new_val;
}

static inline VMValue DerefVoidPointer(void *val, BasicType pointed_type)
{
	if (!val) return VMValue();
	switch (pointed_type)
	{
	case BASIC_SpriteID:
	case BASIC_TextureID:
	case BASIC_TranslationID:
	case BASIC_Sound:
	case BASIC_Color:
	case BASIC_name:
	case BASIC_StateLabel:
	case BASIC_Enum:
	case BASIC_uint32:
		return VMValue((int)*static_cast<uint32_t *>(val));
	case BASIC_int32:
		return VMValue((int)*static_cast<int32_t *>(val));
	case BASIC_uint16:
		return VMValue((int)*static_cast<uint16_t *>(val));
	case BASIC_int16:
		return VMValue((int)*static_cast<int16_t *>(val));
	case BASIC_uint8:
		return VMValue((int)*static_cast<uint8_t *>(val));
	case BASIC_int8:
		return VMValue((int)*static_cast<int8_t *>(val));
	case BASIC_float:
		return VMValue((float)*static_cast<float *>(val));
	case BASIC_double:
		return VMValue((double)*static_cast<double *>(val));
	case BASIC_bool:
		return VMValue((bool)*static_cast<bool *>(val));
	case BASIC_string:
	{
		FString **str = (FString **)val;
		return VMValue(*str);
	}
	case BASIC_VoidPointer:
	case BASIC_pointer:
		return VMValue((void *)*static_cast<void **>(val));
	default:
		break;
	}
	return VMValue();
}

static inline VMValue DerefValue(const VMValue *val, BasicType pointed_type)
{
	if (!IsVMValueValid(val)) return VMValue();
	return DerefVoidPointer(val->a, pointed_type);
}

static inline BasicType GetBasicType(PType *type)
{
	if (type == TypeUInt32) return BASIC_uint32;
	if (type == TypeSInt32) return BASIC_int32;
	if (type == TypeUInt16) return BASIC_uint16;
	if (type == TypeSInt16) return BASIC_int16;
	if (type == TypeUInt8) return BASIC_uint8;
	if (type == TypeSInt8) return BASIC_int8;
	if (type == TypeFloat32) return BASIC_float;
	if (type == TypeFloat64) return BASIC_double;
	if (type == TypeBool) return BASIC_bool;
	if (type == TypeString) return BASIC_string;
	if (type == TypeName) return BASIC_name;
	if (type == TypeSpriteID) return BASIC_SpriteID;
	if (type == TypeTextureID) return BASIC_TextureID;
	if (type == TypeTranslationID) return BASIC_TranslationID;
	if (type == TypeSound) return BASIC_Sound;
	if (type == TypeColor) return BASIC_Color;
	if (type == TypeVoidPtr) return BASIC_VoidPointer;
	if (type->isPointer()) return BASIC_pointer;
	if (type->Flags & TT::TypeFlags::TYPE_IntNotInt)
	{
		std::string name = type->mDescriptiveName.GetChars();
		// if name begins with "Enum"
		if (name.find("Enum") == 0) return BASIC_Enum;
		if (type->Size == 4) return BASIC_uint32;
		if (type->Size == 2) return BASIC_uint16;
		if (type->Size == 1) return BASIC_uint8;
	}
	return BASIC_NONE;
}

static inline bool IsBasicNonPointerType(PType *type) { return IsBasicType(type) && !(type->Flags & TT::TypeFlags::TYPE_Pointer); }

static int GetScriptFileID(const std::string &script_path)
{
	int containerLump = -1;
	std::string namespaceName = GetArchiveNameFromPath(script_path);
	if (!namespaceName.empty())
	{
		containerLump = fileSystem.CheckIfResourceFileLoaded(namespaceName.c_str());
	}
	std::string truncScriptPath = GetScriptPathNoQual(script_path);
	return fileSystem.CheckNumForFullName(truncScriptPath.c_str(), true, containerLump != -1 ? containerLump : FileSys::ns_global);
}

static std::vector<std::string> FindScripts(const std::string &filter, int baselump = -1)
{
	std::set<std::string> scriptNames;
	int filelump = -1;
	int containerLump = baselump;
	std::string path = filter;
	TArray<PNamespace *> namespaces;
	if (baselump == -1)
	{
		namespaces = Namespaces.AllNamespaces;
	}
	else
	{
		for (auto ns : Namespaces.AllNamespaces)
		{
			if (ns->FileNum == baselump)
			{
				namespaces.Push(ns);
				break;
			}
		}
	}

	for (auto ns : namespaces)
	{
		std::string namespaceName = GetArchiveNameFromPath(filter);
		std::string truncScriptPath = GetScriptPathNoQual(filter);
		filelump = fileSystem.CheckNumForFullName(truncScriptPath.c_str(), true, ns->FileNum);
		if (filelump == -1)
		{
			break;
		}
		containerLump = fileSystem.GetFileContainer(filelump);
		if (truncScriptPath == filter)
		{
			path = GetScriptWithQual(filter, fileSystem.GetResourceFileName(containerLump));
		}
		scriptNames.insert(path);
	}
	return {scriptNames.begin(), scriptNames.end()};
}

static std::vector<std::string> FindAllScripts(int baselump = -1)
{
	std::vector<std::string> scriptNames;
	for (int i = 0; i < fileSystem.GetNumEntries(); ++i)
	{
		auto fc = fileSystem.GetFileContainer(i);
		if (baselump == -1 || fc == baselump)
		{
			// get the container name
			std::string containerName = fileSystem.GetResourceFileName(fc);
			std::string scriptPath = fileSystem.GetFileFullName(i);
			std::string fqn = GetScriptWithQual(scriptPath, containerName);
			if (isScriptPath(fqn))
			{
				scriptNames.push_back(fqn);
			}
		}
	}
	return scriptNames;
}

}