#pragma once

#include "Utilities.h"

#include "actor.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "dobject.h"
#include "filesystem.h"
#include "resourcefile.h"
#include "types.h"
#include "vm.h"
#include "vmintern.h"
#include "zstring.h"

struct FState;
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

inline bool IsFunctionAction(VMFunction *func)
{
	return func && func->VarFlags & VARF_Action;
}

inline bool IsFunctionMethod(VMFunction *func)
{
	return func && (func->VarFlags & VARF_Method);
}

inline bool IsFunctionStatic(VMFunction *func)
{
	return func && (func->VarFlags & VARF_Static || !(func->VarFlags & VARF_Method));
}

inline bool IsFunctionNative(VMFunction *func)
{
	return func && func->VarFlags & VARF_Native;
}

inline bool IsFunctionAbstract(VMFunction *func)
{
	return func && func->VarFlags & VARF_Abstract;
}

inline bool IsFunctionHasSelf(VMFunction *func)
{
	return func && func->ImplicitArgs > 0;
}


inline bool IsNonAbstractScriptFunction(VMFunction *func)
{
	return func && !IsFunctionAbstract(func) && !IsFunctionNative(func) && dynamic_cast<VMScriptFunction *>(func) != nullptr;
}
static inline bool IsBasicType(const PType *type)
{
	return type->Flags & TT::TypeFlags::TYPE_Scalar;
}

static inline bool IsBasicNonPointerType(const PType *type)
{
	return IsBasicType(type) && !(type->Flags & TT::TypeFlags::TYPE_Pointer);
}

static inline std::string GetIPRef(const char *qualifiedName, uint32_t PCdiff, const VMOP *PC) { return StringFormat("%s+%04x:%p", qualifiedName, PCdiff, PC); }

static inline std::string GetScriptPathNoQual(const std::string &scriptPath)
{
	auto colonPos = scriptPath.find_last_of(':');
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
	auto colonPos = scriptPath.find_last_of(':');
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
	std::string scriptName = ToLowerCopy(FileSys::ExtractBaseName(GetScriptPathNoQual(path).c_str(), true));
	auto ext = scriptName.substr(scriptName.find_last_of('.') + 1);
	scriptName = scriptName.substr(0, scriptName.find_last_of('.'));
	if (!(ext == "zs" || ext == "zsc" || ext == "zc" || ext == "acs" || ext == "dec" || ext == "deh" || (scriptName == "decorate") || (scriptName == "acs")
				|| (scriptName == "sbarinfo") || (scriptName == "dehacked")))
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

static bool TypeIsStructOrStructPtr(PType *p_type)
{
	auto type = p_type;
	if (type && type->isPointer())
	{
		type = dynamic_cast<PPointer *>(type)->PointedType;
	}
	return type && type->isStruct();
}

static bool TypeIsNonStructContainer(PType *p_type)
{
	auto type = p_type;
	if (type && type->isPointer())
	{
		type = dynamic_cast<PPointer *>(type)->PointedType;
	}
	return type && type->isContainer() && !type->isStruct();
}

static bool TypeIsArrayOrArrayPtr(PType *p_type)
{
	auto type = p_type;
	if (type->isPointer())
	{
		type = static_cast<PPointer *>(type)->PointedType;
	}
	return type->isArray() || type->isDynArray() || type->isStaticArray();
}

static inline bool IsVMValValidDObject(const VMValue *val) { return IsVMValueValid(val) && isValidDobject(static_cast<DObject *>(val->a)); }

static VMValue GetVMValue(void *addr, const PType *type, int bitvalue = -1)
{
	VMValue value = VMValue();
	if (type == TypeString)
	{
		auto str = static_cast<FString *>(addr);
		if (!isFStringValid(*str)) value = VMValue();
		else
			value = VMValue(str);
	}
	else if (type == TypeFloat32)
	{
		value = VMValue(*(float *)addr);
	}
	else if (type == TypeFloat64)
	{
		value = VMValue(*(double *)addr);
	}
	else if (type->isPointer() && type->Size == static_cast<uint32_t>(-1))
	{
		value = VMValue((void *)addr);
	}
	else if (type->isScalar())
	{
		switch (type->Size)
		{
		case 1:
			value = VMValue(*(uint8_t *)addr);
			break;
		case 2:
			value = VMValue(*(uint16_t *)addr);
			break;
		case 4:
			value = VMValue(*(uint32_t *)addr);
			break;
		case 8:
			value = VMValue((void *)(*(uint64_t *)addr));
			break;
		default:
			LogError("GetVMValue: scalar field size %d not supported", type->Size);
			break;
		}
	}
	else
	{
		value = VMValue((void *)addr);
	}
	if (bitvalue != -1)
	{
		// deprecated bitfield
		if (bitvalue > 64) bitvalue -= 64;
		value.i = (value.i & (1 << bitvalue)) != 0;
	}
	return value;
}

static VMValue GetRegisterValue(const VMFrame *m_stackFrame, uint8_t regType, int idx, bool get_non_string_reg_addr = false)
{
	switch (regType)
	{
	case REGT_INT:
		if (get_non_string_reg_addr)
		{
			return VMValue((void *)&m_stackFrame->GetRegD()[idx]);
		}
		return VMValue(m_stackFrame->GetRegD()[idx]);
	case REGT_FLOAT:
		if (get_non_string_reg_addr)
		{
			return VMValue((void *)&m_stackFrame->GetRegF()[idx]);
		}
		return VMValue(m_stackFrame->GetRegF()[idx]);
	case REGT_STRING:
		return VMValue(&m_stackFrame->GetRegS()[idx]);
	case REGT_POINTER:
		if (get_non_string_reg_addr)
		{
			return VMValue((void *)&m_stackFrame->GetRegA()[idx]);
		}
		return VMValue(m_stackFrame->GetRegA()[idx]);
	}
	return VMValue();
}

static inline VMValue GetVMValueVar(DObject *obj, FName field, PType *type, int bitvalue = -1)
{
	if (!isValidDobject(obj)) return VMValue();
	auto var = obj->ScriptVar(field, type);
	auto val = GetVMValue(var, type, bitvalue);
	return val;
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


static int GetScriptFileID(const std::string &script_path)
{
	int containerLump = -1;
	std::string namespaceName = GetArchiveNameFromPath(script_path);
	if (!namespaceName.empty())
	{
		containerLump = fileSystem.CheckIfResourceFileLoaded(namespaceName.c_str());
		if (containerLump == -1 && std::count(namespaceName.begin(), namespaceName.end(), ':') > 1)
		{
			namespaceName = namespaceName.substr(namespaceName.find_last_of(':') + 1);
			containerLump = fileSystem.CheckIfResourceFileLoaded(namespaceName.c_str());
		}
	}
	std::string truncScriptPath = GetScriptPathNoQual(script_path);
	auto ret =  fileSystem.CheckNumForFullName(truncScriptPath.c_str(), true, containerLump != -1 ? containerLump : FileSys::ns_global);
	if (ret == -1 && truncScriptPath.length() <= 8 && truncScriptPath.find_last_of('.') == std::string::npos){
		int lastlump = 0;
		ret = 0;
		while (ret != -1) {
			ret = fileSystem.FindLump(truncScriptPath.c_str(), &lastlump, true);
			if (ret != -1 && (containerLump == -1 || fileSystem.GetFileContainer(ret) == containerLump)){
				break;
			}
		}
	}
	return ret;
}

static std::map<std::string, int> FindScripts(const std::string &filter, int baselump = -1)
{
	std::map<std::string, int> scriptNames;
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
		scriptNames.insert({path, filelump});
	}
	return scriptNames;
}

static std::map<std::string, int> FindAllScripts(int baselump = -1)
{
	std::map<std::string, int>  scriptNames;
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
				scriptNames.insert({fqn, i});
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
	int Line = -1;
	VMValue Value;
	bool invalid_value = false;
	std::vector<LocalState> StructFields;
	bool IsValid() { return Type && !invalid_value; }
};

struct FrameLocalsState
{
	std::vector<LocalState> m_locals;
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
	for (auto &local : locals)
	{
		names.push_back(local.Name.GetChars());
	}
}

inline bool ClassIsActor(PClass *actorClass)
{
	if (!actorClass)
	{
		return false;
	}
	return actorClass->IsDescendantOf(RUNTIME_CLASS(AActor)) ? true : false;
}

inline PClass *GetClassDescriptor(PType *localtype)
{
	if (!localtype) return nullptr;
	if (localtype->isPointer())
	{
		localtype = localtype->toPointer()->PointedType;
	}
	if (localtype->isClass())
	{
		return PType::toClass(localtype)->Descriptor;
	}
	return nullptr;
}

static VMScriptFunction *GetVMScriptFunction(VMFunction *func)
{
	if (!func || IsFunctionNative(func))
	{
		return nullptr;
	}
	return dynamic_cast<VMScriptFunction *>(func);
}

static bool FunctionIsAnonymousStateFunction(VMFunction *func)
{
	if (!func || func->Name != 0)
	{
		return false;
	}
	auto vmfunc = GetVMScriptFunction(func);
	if (!vmfunc)
	{
		return false;
	}
	// TODO: Better way to do this?
	// find ".VMScriptFunction." in the PrintableName
	std::string name = vmfunc->PrintableName;
	if (name.find(".VMScriptFunction.") != std::string::npos)
	{
		return true;
	}
	return false;
}


static size_t GetImplicitParmeterCount(const VMFrame *m_stackFrame)
{ return m_stackFrame->Func->ImplicitArgs; }

static std::string GetFunctionClassName(const VMFunction *func)
{
	std::string_view st = func->QualifiedName;
	auto dotpos = st.find(".");
	if (dotpos == std::string_view::npos) return "";
	return std::string(st.substr(0, dotpos));
}

static PFunction *GetFunctionSymbol(const VMFunction *func)
{
	if (func)
	{
		// get the FName of the function
		auto funcName = func->Name;
		std::string className = GetFunctionClassName(func);
		auto cls = PClass::FindClass(className.c_str());
		PFunction * pfunc = nullptr;
		if (!cls){
			std::string structName = "Struct<" + className + ">";
			// maybe it's a struct?
			// TODO: this is very slow, cache this somehow?
			for (size_t i = 0; i < countof(TypeTable.TypeHash); ++i)
			{
				for (PType *ty = TypeTable.TypeHash[i]; ty != nullptr; ty = ty->HashNext)
				{
					if (ty->isContainer() && std::string_view(ty->mDescriptiveName.GetChars()).find(structName.c_str()) != std::string_view::npos){
						PContainerType *struct_type = dynamic_cast<PContainerType *>(ty);
						if (struct_type){
							pfunc = dyn_cast<PFunction>(struct_type->Symbols.FindSymbol(funcName, true));
							if (pfunc){
								break;
							}
						}
					}
				}
			}
		} else {
		 	pfunc = dyn_cast<PFunction>(cls->FindSymbol(funcName, true));
		}
		if (pfunc)
		{
			// pfunc has the parameter names
			for (auto &variant : pfunc->Variants)
			{
				if (variant.Implementation == func)
				{
					return pfunc;
				}
			}
		}
	}
	return nullptr;
}
static PFunction::Variant *GetFunctionVariant(const VMFunction *func)
{
	auto pfunc = GetFunctionSymbol(func);
	if (pfunc)
	{
		for (auto &variant : pfunc->Variants)
		{
			if (variant.Implementation == func)
			{
				return &variant;
			}
		}
	}
	return nullptr;
}

static std::string GetParameterName(const VMFrame *m_stackFrame, int paramidx)
{
	static const char *const ARG = "ARG";
	static const char *const SELF = "self";
	static const char *const INVOKER = "invoker";
	static const char *const STATE_POINTER = "stateinfo";

	int implicitCount = GetImplicitParmeterCount(m_stackFrame);
	if (paramidx < implicitCount)
	{
		switch (paramidx)
		{
		case 0:
			return SELF;
		case 1:
			return INVOKER;
		case 2:
			return STATE_POINTER;
		default:
			return "<unknown>";
		}
	}
	// get the function
	auto func = GetVMScriptFunction(m_stackFrame->Func);
	auto variant = GetFunctionVariant(func);
	if (variant)
	{
		auto &params = variant->ArgNames;
		if (paramidx < params.SSize())
		{
			return params[paramidx].GetChars();
		}
	}
	return StringFormat("%s%zu", ARG, paramidx - GetImplicitParmeterCount(m_stackFrame));
}

static FrameLocalsState GetLocalsState(const VMFrame *p_stackFrame);

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

	char *struct_ptr = static_cast<char *>(m_value.a);
	char *curr_ptr = struct_ptr;
	size_t currsize = 0;
	auto fields = GetStructFieldInfo(m_type);
	size_t min_offset = INT_MAX;
	size_t max_offset = 0;

	for (auto &field_pair : fields)
	{
		std::string field_name = field_pair.first;
		auto field = field_pair.second;
		try
		{
			if (!field || !struct_ptr)
			{
				m_structInfo.StructFields.push_back(LocalState {field_name, field->Type, static_cast<int>(field->Flags), -1, -1, -1, VMValue(), true, {}});
				continue;
			}
			auto offset = field->Offset;
			uint32_t fieldSize = field->Type->Size;
			auto type = field->Type;
			VMValue val;
			void *pointed_field = struct_ptr + offset;
			bool invalid = false;
			val = GetVMValue(pointed_field, type, field->BitValue);
			m_structInfo.StructFields.push_back(LocalState {field_name, field->Type, static_cast<int>(field->Flags), -1, -1, -1, val, true, {}});
			// increment struct_ptr by fieldSize
			if (curr_ptr)
			{
				currsize += fieldSize;
				curr_ptr = curr_ptr + fieldSize;
			}
		}
		// generic exception
		catch (...)
		{
			// struct name + type name
			LogError("GetStructState: %s (%s) Error getting field %s", struct_name.c_str(), structType->mDescriptiveName.GetChars(), field_name.c_str());
		}
	}
	return m_structInfo;
}


static bool GetRegisterValueChecked(const VMFrame *m_stackFrame, uint8_t regType, VMValue &value, int idx, bool get_non_string_reg_addr = false)
{
	switch (regType)
	{
	case REGT_INT:
		if (idx >= m_stackFrame->NumRegD)
		{
			LogError("GetRegisterValue: Function %s, int reg idx %d > %hu", m_stackFrame->Func->PrintableName, idx, m_stackFrame->NumRegD);
			value = VMValue();
			return false;
		}
		break;

	case REGT_FLOAT:
		if (idx >= m_stackFrame->NumRegF)
		{
			LogError("GetRegisterValue: Function %s, float reg idx %d > %hu", m_stackFrame->Func->PrintableName, idx, m_stackFrame->NumRegF);
			value = VMValue();
			return false;
		}
		break;
	case REGT_STRING:
		if (idx >= m_stackFrame->NumRegS)
		{
			LogError("GetRegisterValue: Function %s, string reg idx %d > %hu", m_stackFrame->Func->PrintableName, idx, m_stackFrame->NumRegS);
			value = VMValue();
			return false;
		}
		break;

	case REGT_POINTER:
		if (idx >= m_stackFrame->NumRegA)
		{
			LogError("GetRegisterValue: Function %s, pointer reg idx %d > %hu", m_stackFrame->Func->PrintableName, idx, m_stackFrame->NumRegA);
			value = VMValue();
			return false;
		}
		break;
	default:
	{
		LogError("GetRegisterValue: Function %s, invalid reg type %hu", m_stackFrame->Func->PrintableName, regType);
		value = VMValue();
		return false;
	}
	}
	value = GetRegisterValue(m_stackFrame, regType, idx, get_non_string_reg_addr);
	return true;
}

static FrameLocalsState GetLocalsState(const VMFrame *p_stackFrame)
{
	FrameLocalsState localState;
	std::map<std::string, LocalState> m_children;
	int numImplicitParams = GetImplicitParmeterCount(p_stackFrame);
	uint8_t NumRegInt = 0;
	uint8_t NumRegFloat = 0;
	uint8_t NumRegString = 0;
	uint8_t NumRegAddress = 0;
	auto getAndIncRegCount = [&](PType *type, VMValue &value, int idx = -1, int refregCount = -1, uint8_t reg_type = 255) -> bool
	{
		auto regType = reg_type == 255 ? type->RegType : reg_type;
		bool get_non_string_reg_addr = false;
		int reg_idx = -1;
		int inc = 1;
		if (refregCount > 0) {
				inc = refregCount;
		}
		if (regType != REGT_POINTER && TypeIsStructOrStructPtr(type))
		{
			get_non_string_reg_addr = true;
			if (refregCount == -1) {
				inc = type->RegCount;
			}
		}
		switch (regType)
		{
		case REGT_INT:
			reg_idx = idx < 0 ? NumRegInt : idx;
			NumRegInt += inc;
			break;
		case REGT_FLOAT:
			reg_idx = idx < 0 ? NumRegFloat : idx;
			NumRegFloat += inc;
			break;
		case REGT_STRING:
			reg_idx = idx < 0 ? NumRegString : idx;
			NumRegString += inc;
			break;
		case REGT_POINTER:
			reg_idx = idx < 0 ? NumRegAddress : idx;
			NumRegAddress += inc;
			break;
		}
		return GetRegisterValueChecked(p_stackFrame, regType, value, reg_idx, get_non_string_reg_addr);
	};
	VMScriptFunction *func = GetVMScriptFunction(p_stackFrame->Func);
	auto locals = func->GetLocalVariableBlocksAt(p_stackFrame->PC);
	auto pfunc = GetFunctionSymbol(p_stackFrame->Func);
	PFunction::Variant *var = GetFunctionVariant(p_stackFrame->Func);

	for (int paramidx = 0; paramidx < (int64_t)p_stackFrame->Func->Proto->ArgumentTypes.size(); paramidx++)
	{
		std::string name = GetParameterName(p_stackFrame, paramidx);
		VMValue val;
		auto type = p_stackFrame->Func->Proto->ArgumentTypes[paramidx];
		uint8_t reg_type = p_stackFrame->Func->RegTypes[paramidx];
		if (type == nullptr) { // varargs, continue
			continue;
		}
		bool invalid = !getAndIncRegCount(type, val, -1, -1, reg_type);
		if (type->RegType != reg_type && reg_type == REGT_POINTER){ // 'out' parameter
			if (IsBasicNonPointerType(type)){
				// val = DerefValue(&val, GetBasicType(type));
				type = NewPointer(type, false);

			}
		}

		localState.m_locals.push_back(LocalState {name, type, 0, paramidx, type->RegCount, -1, val, invalid, {}});
	}
	int specials = 0;
	std::sort(
		locals.begin(),
		locals.end(),
		[](const auto &a, const auto &b)
		{
			if (a.RegNum == b.RegNum)
			{
				return a.LineNumber < b.LineNumber;
			}
			return a.RegNum < b.RegNum;
		});

	if (GetVMScriptFunction(p_stackFrame->Func))
	{
		for (auto &local : locals)
		{

			int flags = 0; // TODO

			auto local_name = local.Name.GetChars();
			LocalState state;
			{
				state.RegCount = local.RegCount;
				bool invalid_reg_num = local.RegNum < 0;
				state.Name = local_name;
				state.Type = local.type;
				state.VarFlags = flags;
				state.RegNum = local.RegNum;
				state.Line = local.LineNumber;
				state.invalid_value = true;
				// stack-allocated variable
				if (invalid_reg_num && local.type->RegType == REGT_NIL && local.StackOffset > -1 && local.StackOffset + local.type->Size <= func->StackSize)
				{
					void *base = p_stackFrame->GetExtra();
					void *var = (char *)base + local.StackOffset;
					state.Value = GetVMValue(var, local.type);
					state.invalid_value = false;
				}
				else if (!IsBasicNonPointerType(local.type))
				{
					VMValue value;
					state.RegNum = invalid_reg_num ? NumRegAddress : local.RegNum;

					// This is a struct optimized by the compiler to be stored in the registers; we need to get their register values and increment the register count
					if (TypeIsStructOrStructPtr(local.type) && local.RegCount > 1)
					{
						auto fields = GetStructFieldInfo(local.type);
						state.RegCount = local.RegCount;
						if (local.type->RegCount != local.RegCount)
						{
							LogError("GetReg: RegCount mismatch for %s: %hu != %d", local_name, local.type->RegCount, local.RegCount);
						}
						assert(fields.empty() || fields[0].second->Type->RegType == REGT_FLOAT);
						state.RegNum = NumRegFloat;
						state.invalid_value = !getAndIncRegCount(state.Type, state.Value, state.RegNum, local.RegCount);
					}
					else
					{
						state.invalid_value = !getAndIncRegCount(state.Type, state.Value, state.RegNum);
					}
				}
				else
				{
					state.RegCount = 1;
					if (local.RegCount != 1)
					{
						auto reg_type = local.type->RegType;
						LogError("GetReg: RegCount mismatch for %s: %d != 1", local_name, local.RegCount);
					}
					state.RegNum = local.RegNum;
					state.invalid_value = !getAndIncRegCount(local.type, state.Value, state.RegNum);
				}
			}

			localState.m_locals.push_back(state);
		}
	}
	return localState;
}

static bool PCIsAtNativeCall(VMFrame *frame)
{
	if (!frame->PC)
	{
		return false;
	}
	// get the current op from the PC; if it's a call, we need to check to see what it's calling
	auto op = frame->PC->op;
	if (op == OP_CALL || op == OP_CALL_K)
	{
		void *ptr = nullptr;
		if (op == OP_CALL_K)
		{
			VMScriptFunction *func = dynamic_cast<VMScriptFunction *>(frame->Func);
			if (func)
			{
				ptr = func->KonstA[frame->PC->a].o;
			}
		}
		else if (op == OP_CALL)
		{
			ptr = frame->GetRegA()[frame->PC->a];
		}
		if (IsFunctionNative((VMFunction *)ptr))
		{
			return true;
		}
		return false;
	}
	return false;
}

static VMFunction *GetCalledFunction(VMFrame *frame)
{
	if (!frame->PC)
	{
		return nullptr;
	}
	auto op = frame->PC->op;
	if (!(op == OP_CALL || op == OP_CALL_K))
	{
		return nullptr;
	}
	void *ptr = nullptr;
	if (op == OP_CALL_K)
	{
		VMScriptFunction *func = dynamic_cast<VMScriptFunction *>(frame->Func);
		if (func)
		{
			ptr = func->KonstA[frame->PC->a].o;
		}
	}
	else if (op == OP_CALL)
	{
		ptr = frame->GetRegA()[frame->PC->a];
	}
	VMFunction *call = (VMFunction *)ptr;
	return call;
}


static std::string AddrToString(VMFunction *func, void *addr)
{
	// TODO: VSCode doesn't currently support non-number addresses (although the spec clearly implies it's allowed)
	// VMScriptFunction *scriptFunc = func ? dynamic_cast<VMScriptFunction *>(func) : nullptr;
	// if (!scriptFunc)
	// {
	// 	return StringFormat("%p", addr);
	// }
	// auto diff = (uint32_t)((VMOP *)addr - scriptFunc->Code);
	// return GetIPRef(scriptFunc->QualifiedName, diff, (VMOP *)addr);
	return StringFormat("%p", addr);
}

static FState *GetStateFromIdx(int i, PClass *actorClass, PClassActor *&rActualOwner)
{
	FState *state;
	if (!actorClass)
	{
		rActualOwner = nullptr;
		if (!(i > 0 && i < 0x10000000)) return nullptr;
		state = StateLabels.GetState(i, nullptr);
		if (!state) return nullptr;
	}
	else
	{
		rActualOwner = actorClass->IsDescendantOf(RUNTIME_CLASS(AActor)) ? static_cast<PClassActor *>(actorClass) : nullptr;

		state = rActualOwner ? StateLabels.GetState(i, rActualOwner) : nullptr;
		if (!state)
		{
			rActualOwner = PClass::FindActor(actorClass->TypeName);
			if (rActualOwner)
			{
				state = StateLabels.GetState(i, rActualOwner);
			}
		}
	}
	if (state && !rActualOwner)
	{
		rActualOwner = FState::StaticFindStateOwner(state);
	}
	return state;
}

static FConsoleCommand *FindConsoleCommand(const std::string &name)
{
	return FConsoleCommand::FindByName(name.c_str());
}

static FBaseCVar *FindConsoleVariable(const std::string &name)
{
	return FindCVar(name.c_str(), nullptr);
}

}
