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

static bool TypeIsStructOrStructPtr(PType *p_type) { return p_type->isStruct() || p_type->isContainer() || (p_type->isPointer() && static_cast<PPointer *>(p_type)->PointedType->isStruct()); }


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


// register to local mapping with names and values
struct LocalState
{
	std::string Name;
	PType *Type;
	int VarFlags = -1;
	int RegNum = -1;
	int RegCount = -1;
	VMValue Value;
	std::vector<LocalState> StructFields;
	bool invalid_value = false;
	bool IsStruct() { return TypeIsStructOrStructPtr(Type); }
	bool IsValid() { return Type && !invalid_value; }
};

struct FrameLocalsState
{
	std::map<std::string, LocalState> m_locals;
};

using StructInfo = LocalState;

static std::vector<std::pair<std::string, PField *>> GetStructFieldInfo(PType *m_type)
{
	std::vector<std::pair<std::string, PField *>> fields;
	auto structType = dynamic_cast<PContainerType *>(m_type->isPointer() ? static_cast<PPointer *>(m_type)->PointedType : m_type);
	if (!structType)
	{
		return fields;
	}
	auto it = structType->Symbols.GetIterator();
	PSymbolTable::MapType::Pair *pair;
	while (it.NextPair(pair))
	{
		auto field_type = dynamic_cast<PField *>(pair->Value);
		if (field_type)
		{
			fields.push_back({pair->Key.GetChars(), field_type});
		}
	}
	// sort by field offset
	std::sort(
		fields.begin(), fields.end(),
		[](const auto &a, const auto &b)
		{
			if (!a.second && !b.second) return false;
			if (!a.second) return false;
			if (!b.second) return true;
			return a.second->Offset < b.second->Offset;
		});
	return fields;
}

static void GetLocalsNames(VMFrame *m_stackFrame, std::vector<std::string> &names)
{
	VMScriptFunction *func = dynamic_cast<VMScriptFunction *>(m_stackFrame->Func);
	auto locals = func->GetLocalVariableBlocksAt(m_stackFrame->PC);
	Printf("Locals: \n");
	for (auto &local : locals)
	{
		names.push_back(local.Name.GetChars());
		Printf("Name: %s, Type: %s", local.Name.GetChars(), local.type->DescriptiveName());
	}
}

static void GetExplicitParameterNames(const VMFrame *m_stackFrame, std::vector<std::string> &names)
{
	// TODO: Get parameter names
}

static size_t GetImplicitParmeterCount(const VMFrame *m_stackFrame)
{
	if (m_stackFrame->Func->VarFlags & VARF_Action)
	{
		return 3;
	}
	else if (m_stackFrame->Func->VarFlags & VARF_Method)
	{
		return 1;
		// TODO: Figure out if there is a state_pointer in non-action methods?
	}
	return 0;
}

static void GetImplicitParameterNames(const VMFrame *m_stackFrame, std::vector<std::string> &names)
{
	static const char *const LOCAL = "Local";
	static const char *const SELF = "self";
	static const char *const INVOKER = "invoker";
	static const char *const STATE_POINTER = "state_pointer";


	if (m_stackFrame->Func->VarFlags & VARF_Action)
	{
		names.push_back(SELF);
		names.push_back(INVOKER);
		names.push_back(STATE_POINTER);
	}
	else if (m_stackFrame->Func->VarFlags & VARF_Method)
	{
		names.push_back(SELF);
		// TODO: Figure out if there is a state_pointer in non-action methods?
	}

	// TODO: verify that a method is static (and has no members) as indicated by absence of VARF_Method on VMFunction::VarFlags
}
static FrameLocalsState GetLocalsState(const VMFrame *m_stackFrame);

static StructInfo GetStructState(std::string struct_name, VMValue m_value, PType *m_type, const VMFrame *m_currentFrame)
{
	
	StructInfo m_structInfo;
	m_structInfo.Name = struct_name;
	m_structInfo.Value = m_value;
	m_structInfo.Type = m_type;
	if (!m_type)
	{
		m_structInfo.invalid_value = true;
		return m_structInfo;
	}

	auto structType = static_cast<PContainerType *>(m_type->isPointer() ? static_cast<PPointer *>(m_type)->PointedType : m_type);
	auto it = structType->Symbols.GetIterator();
	PSymbolTable::MapType::Pair *pair;
	char *struct_ptr = static_cast<char *>(m_value.a);
	char *curr_ptr = struct_ptr;
	size_t struct_size = structType->Size;
	size_t currsize = 0;
	auto fields = GetStructFieldInfo(m_type);
	// some structs have their fields allocated in the registers; if so, we need to get the locals state	
	if (m_currentFrame)
	{
		auto localsState = GetLocalsState(m_currentFrame);
		for (auto &local : localsState.m_locals)
		{
			// If all the fields are allocated in the registers, we can just return the locals state
			if (local.first == struct_name && local.second.StructFields.size() == fields.size())
			{
				return local.second;
			}
		}
	}

	for (auto &field_pair : fields)
	{
		std::string field_name = field_pair.first;
		auto field = field_pair.second;
		try
		{
			if (!field || !struct_ptr)
			{
				m_structInfo.StructFields.push_back(LocalState {field_name, field->Type, static_cast<int>(field->Flags), -1, -1, VMValue(), {}, true});
				continue;
			}
			auto offset = field->Offset;
			uint32_t flags = field->Flags;
			uint32_t fieldSize = field->Type->Size;
			auto type = field->Type;

			VMValue val;
			void *pointed_field;
			pointed_field = struct_ptr + offset;
			bool invalid = false;
			if (type == TypeString){
				auto str = static_cast<FString *>(pointed_field);
				if (!isFStringValid(*str))
					val = VMValue();
				else
					val = VMValue(str);
			}
			else if (type == TypeFloat32)
			{
				val = VMValue(*(float *)pointed_field);
			}
			else if (type == TypeFloat64)
			{
				val = VMValue(*(double *)pointed_field);
			}
			else if (type->isScalar())
			{
				switch (fieldSize)
				{
				case 1:
					val = VMValue(*(uint8_t *)pointed_field);
					break;
				case 2:
					val = VMValue(*(uint16_t *)pointed_field);
					break;
				case 4:
					val = VMValue(*(uint32_t *)pointed_field);
					break;
				case 8:
					val = VMValue((void *)(*(uint64_t *)pointed_field));
					break;
				default:
					invalid = true;
					LogError("StructStateNode::GetChildNames: field %s scalar field size %d not supported", field_name.c_str(), fieldSize);
					break;
				}
			}
			else
			{
				val = VMValue((void *)pointed_field);
			}
			m_structInfo.StructFields.push_back(LocalState {field_name, field->Type, static_cast<int>(field->Flags), -1, -1, val, {}, invalid});
			// increment struct_ptr by fieldSize
			// don't increment if it's a transient field
			if (curr_ptr && !(flags & VARF_Transient))
			{
				currsize += fieldSize;
				curr_ptr = curr_ptr + fieldSize;
			}
		}
		catch (...)
		{
		}
		if (currsize > struct_size)
		{
			LogError("StructStateNode::GetChildNames: struct size %d exceeded on field %s, breaking", struct_size, field_name.c_str());
			break;
		}
	}
	return m_structInfo;
}

static FrameLocalsState GetLocalsState(const VMFrame *m_stackFrame)
{
	FrameLocalsState localState;
	std::map<std::string, LocalState> m_children;
	int numImplicitParams = GetImplicitParmeterCount(m_stackFrame);
	static const char *const LOCAL = "Local";
	static const char *const SELF = "self";
	static const char *const INVOKER = "invoker";
	static const char *const STATE_POINTER = "state_pointer";

	if (numImplicitParams > 0)
	{
		for (int paramidx = 0; paramidx < numImplicitParams; paramidx++)
		{
			std::string name;
			switch (paramidx)
			{
			case 0:
				name = SELF;
				break;
			case 1:
				name = INVOKER;
				break;
			case 2:
				name = STATE_POINTER;
				break;
			}
			if (m_stackFrame->NumRegA == 0)
			{
				LogError("Function %s has '%s' but no parameters", m_stackFrame->Func->QualifiedName, name.c_str());
				continue;
			}
			else if (m_stackFrame->NumRegA <= paramidx)
			{
				LogError("Function %s has '%s' but <= %d parameters", m_stackFrame->Func->QualifiedName, name.c_str(), paramidx);
				continue;
			}
			if (m_stackFrame->Func->Proto->ArgumentTypes.size() == 0)
			{
				LogError("Function %s has '%s' but no argument types", m_stackFrame->Func->QualifiedName, name.c_str());
				continue;
			}
			else if (m_stackFrame->Func->Proto->ArgumentTypes.size() <= paramidx)
			{
				LogError("Function %s has '%s' but <= %d argument types", m_stackFrame->Func->QualifiedName, name.c_str(), paramidx);
				continue;
			}
			auto type = m_stackFrame->Func->Proto->ArgumentTypes[paramidx];
			auto val = m_stackFrame->GetRegA()[paramidx];

			localState.m_locals[name] = LocalState {name, type, 0, paramidx, 1, val, {}, false};
		}
	}
	if (!IsFunctionNative(m_stackFrame->Func))
	{
		uint8_t NumRegInt = 0;
		uint8_t NumRegFloat = 0;
		uint8_t NumRegString = 0;
		uint8_t NumRegAddress = numImplicitParams;

		VMScriptFunction *func = dynamic_cast<VMScriptFunction *>(m_stackFrame->Func);
		auto _locals = func->GetLocalVariableBlocksAt(m_stackFrame->PC);
		std::vector<VMLocalVariable> locals;
		std::vector<std::string> local_names;
		for (auto &local : _locals)
		{
			locals.push_back(local);
			local_names.push_back(local.Name.GetChars());
		}
		std::vector<std::string> localNames;
		for (auto &local : locals)
		{

			int flags = 0; // TODO


			// auto -- need to declare the actual type becuase it's recursive
			std::function<LocalState(const std::string &, PType *, const std::string &)> IncCounts = [&](const std::string &name, PType *type, const std::string &struct_name = "")
			{
				LocalState state;
				state.Name = name;
				state.Type = type;
				state.VarFlags = flags;
				state.RegCount = local.RegCount;
				bool invalid_reg_num = local.RegNum < 0;
				if (!IsBasicNonPointerType(type))
				{
					VMValue value;
					state.RegNum = invalid_reg_num ? NumRegAddress : local.RegNum;
					// TODO: verify that structs embedded in local structs are on the heap vs. the registers
					if (TypeIsStructOrStructPtr(local.type) && struct_name.empty() && !invalid_reg_num)
					{
						auto fields = GetStructFieldInfo(local.type);
						state.RegCount = fields.size();
						std::string new_struct_name = struct_name;
						if (!struct_name.empty())
						{
							new_struct_name += ".";
						}
						new_struct_name += name;
					
						for (auto &field : fields)
						{
							uint32_t regNum = 0;
							PType *field_type = field.second->Type;
							LocalState field_state;
							state.StructFields.push_back(IncCounts(field.first, field_type, new_struct_name));
						}
					}
					else
					{
						if (state.RegNum >= m_stackFrame->NumRegA)
						{
							LogError("Function %s, local '%s' address reg idx %d > %d", m_stackFrame->Func->QualifiedName, name.c_str(), NumRegAddress, m_stackFrame->NumRegA);
							state.invalid_value = true;
							return state;
						}
						state.Value = VMValue(m_stackFrame->GetRegA()[state.RegNum]);
						NumRegAddress++;
					}
					
				}
				else
				{
					state.RegCount = 1;
					bool isStructMember = !struct_name.empty();
					auto basic_type = GetBasicType(type);
					VMValue value;
					switch (basic_type)
					{
					case BASIC_double:
					case BASIC_float:
						state.RegNum = isStructMember ? NumRegFloat : local.RegNum;
						if (state.RegNum >= m_stackFrame->NumRegF)
						{
							LogError("Function %s, local '%s' float reg idx %d > %d", m_stackFrame->Func->QualifiedName, name.c_str(), NumRegFloat, m_stackFrame->NumRegF);
							state.invalid_value = true;
							break;
						}
						value = VMValue(m_stackFrame->GetRegF()[state.RegNum]);
						NumRegFloat++;

						break;
					case BASIC_string:
						state.RegNum = isStructMember ? NumRegString : local.RegNum;
						if (state.RegNum >= m_stackFrame->NumRegS)
						{
							LogError("Function %s, local '%s' string reg idx %d > %d", m_stackFrame->Func->QualifiedName, name.c_str(), NumRegString, m_stackFrame->NumRegS);
							state.invalid_value = true;
							break;
						}
						value = VMValue(&m_stackFrame->GetRegS()[state.RegNum]);
						NumRegString++;

						break;
					default:
						state.RegNum = isStructMember ? NumRegInt : local.RegNum;
						if (state.RegNum >= m_stackFrame->NumRegD)
						{
							LogError("Function %s, local '%s' int reg idx %d > %d", m_stackFrame->Func->QualifiedName, name.c_str(), NumRegInt, m_stackFrame->NumRegD);
							state.invalid_value = true;
							break;
						}
						value = VMValue(m_stackFrame->GetRegD()[state.RegNum]);
						NumRegInt++;
						break;
					}
					state = LocalState {name, type, flags, NumRegAddress, 1, value, {}, false};
				}
				return state;
			};
			auto local_name = local.Name.GetChars();

			localState.m_locals[local_name] = IncCounts(local_name, local.type, "");
		}
	}
	// TODO: rest of the params
	return localState;
}


}