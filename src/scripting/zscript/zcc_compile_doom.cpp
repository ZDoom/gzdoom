/*
** zcc_compile_doom.cpp
**
** contains the Doom specific parts of the script parser, i.e.
** actor property definitions and associated content.
**
**---------------------------------------------------------------------------
** Copyright 2016-2020 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include "a_pickups.h"
#include "thingdef.h"
#include "c_console.h"
#include "filesystem.h"
#include "zcc_parser.h"
#include "zcc-parse.h"
#include "zcc_compile_doom.h"
#include "v_text.h"
#include "p_lnspec.h"
#include "v_video.h"


bool isActor(PContainerType *type);
void AddActorInfo(PClass *cls);
int GetIntConst(FxExpression* ex, FCompileContext& ctx);
double GetFloatConst(FxExpression* ex, FCompileContext& ctx);
VMFunction* GetFuncConst(FxExpression* ex, FCompileContext& ctx);

//==========================================================================
//
// ZCCCompiler :: Compile
//
// Compile everything defined at this level.
//
//==========================================================================

int ZCCDoomCompiler::Compile()
{
	CreateClassTypes();
	CreateStructTypes();
	CompileAllConstants();
	CompileAllFields();
	CompileAllProperties();
	InitDefaults();
	InitFunctions();
	CompileStates();
	InitDefaultFunctionPointers();
	return FScriptPosition::ErrorCounter;
}


//==========================================================================
//
// ZCCCompiler :: CompileAllProperties
//
// builds the property lists of all actor classes
//
//==========================================================================

void ZCCDoomCompiler::CompileAllProperties()
{
	for (auto c : Classes)
	{
		if (c->Properties.Size() > 0)
			CompileProperties(c->ClassType(), c->Properties, c->Type()->TypeName);

		if (c->FlagDefs.Size() > 0)
			CompileFlagDefs(c->ClassType(), c->FlagDefs, c->Type()->TypeName);

	}
}

//==========================================================================
//
// ZCCCompiler :: CompileProperties
//
// builds the internal structure of a single class or struct
//
//==========================================================================

bool ZCCDoomCompiler::CompileProperties(PClass *type, TArray<ZCC_Property *> &Properties, FName prefix)
{
	if (!type->IsDescendantOf(RUNTIME_CLASS(AActor)))
	{
		Error(Properties[0], "Properties can only be defined for actors");
		return false;
	}
	for(auto p : Properties)
	{
		TArray<PField *> fields;
		ZCC_Identifier *id = (ZCC_Identifier *)p->Body;

		if (FName(p->NodeName) == FName("prefix") && fileSystem.GetFileContainer(Lump) == 0)
		{
			// only for internal definitions: Allow setting a prefix. This is only for compatiblity with the old DECORATE property parser, but not for general use.
			prefix = id->Id;
		}
		else
		{
			do
			{
				auto f = dyn_cast<PField>(type->FindSymbol(id->Id, true));
				if (f == nullptr)
				{
					Error(id, "Variable %s not found in %s", FName(id->Id).GetChars(), type->TypeName.GetChars());
				}
				fields.Push(f);
				id = (ZCC_Identifier*)id->SiblingNext;
			} while (id != p->Body);

			FString qualifiedname;
			// Store the full qualified name and prepend some 'garbage' to the name so that no conflicts with other symbol types can happen.
			// All these will be removed from the symbol table after the compiler finishes to free up the allocated space.
			FName name = FName(p->NodeName);
			if (prefix == NAME_None) qualifiedname.Format("@property@%s", name.GetChars());
			else qualifiedname.Format("@property@%s.%s", prefix.GetChars(), name.GetChars());

			fields.ShrinkToFit();
			if (!type->VMType->Symbols.AddSymbol(Create<PProperty>(qualifiedname, fields)))
			{
				Error(id, "Unable to add property %s to class %s", FName(p->NodeName).GetChars(), type->TypeName.GetChars());
			}
		}
	}
	return true;
}

//==========================================================================
//
// ZCCCompiler :: CompileProperties
//
// builds the internal structure of a single class or struct
//
//==========================================================================

bool ZCCDoomCompiler::CompileFlagDefs(PClass *type, TArray<ZCC_FlagDef *> &Properties, FName prefix)
{
	//[RL0] allow property-less flagdefs for non-actors
	bool isActor = type->IsDescendantOf(RUNTIME_CLASS(AActor));
	for (auto p : Properties)
	{
		PField *field;
		FName referenced = FName(p->RefName);

		if (isActor && FName(p->NodeName) == FName("prefix") && fileSystem.GetFileContainer(Lump) == 0)
		{
			// only for internal definitions: Allow setting a prefix. This is only for compatiblity with the old DECORATE property parser, but not for general use.
			prefix = referenced;
		}
		else
		{
			if (referenced != NAME_None)
			{
				field = dyn_cast<PField>(type->FindSymbol(referenced, true));
				if (field == nullptr)
				{
					Error(p, "Variable %s not found in %s", referenced.GetChars(), type->TypeName.GetChars());
				}
				else if (!field->Type->isInt() || field->Type->Size != 4)
				{
					Error(p, "Variable %s in %s must have a size of 4 bytes for use as flag storage", referenced.GetChars(), type->TypeName.GetChars());
				}
			}
			else field = nullptr;
			
			FName name = FName(p->NodeName);

			if(isActor)
			{
				FString qualifiedname;
				// Store the full qualified name and prepend some 'garbage' to the name so that no conflicts with other symbol types can happen.
				// All these will be removed from the symbol table after the compiler finishes to free up the allocated space.
				
				for (int i = 0; i < 2; i++)
				{
					if (i == 0) qualifiedname.Format("@flagdef@%s", name.GetChars());
					else
					{
						if (prefix == NAME_None) continue;
						qualifiedname.Format("@flagdef@%s.%s", prefix.GetChars(), name.GetChars());
					}

					if (!type->VMType->Symbols.AddSymbol(Create<PPropFlag>(qualifiedname, field, p->BitValue, i == 0 && prefix != NAME_None)))
					{
						Error(p, "Unable to add flag definition %s to class %s", FName(p->NodeName).GetChars(), type->TypeName.GetChars());
					}
				}
			}

			if (field != nullptr)
				type->VMType->AddNativeField(FStringf("b%s", name.GetChars()), TypeSInt32, field->Offset, 0, 1 << p->BitValue);
		} 
	}
	return true;
}

//==========================================================================
//
// Parses an actor property's parameters and calls the handler
//
//==========================================================================

void ZCCDoomCompiler::DispatchProperty(FPropertyInfo *prop, ZCC_PropertyStmt *property, AActor *defaults, Baggage &bag)
{
	static TArray<FPropParam> params;
	static TArray<FString> strings;

	params.Clear();
	strings.Clear();
	params.Reserve(1);
	params[0].i = 0;
	if (prop->params[0] != '0')
	{
		if (property->Values == nullptr)
		{
			Error(property, "%s: arguments missing", prop->name);
			return;
		}
		const char * p = prop->params;
		auto exp = property->Values;

		FCompileContext ctx(OutNamespace, bag.Info->VMType, false, mVersion);
		while (true)
		{
			FPropParam conv;
			FPropParam pref;

			FxExpression *ex = ConvertNode(exp);
			ex = ex->Resolve(ctx);
			if (ex == nullptr)
			{
				return;
			}
			else if (!ex->isConstant())
			{
				// If we get TypeError, there has already been a message from deeper down so do not print another one.
				if (exp->Type != TypeError) Error(exp, "%s: non-constant parameter", prop->name);
				return;
			}
			conv.s = nullptr;
			pref.s = nullptr;
			pref.i = -1;
			switch ((*p) & 223)
			{

			case 'X':	// Expression in parentheses or number. We only support the constant here. The function will have to be handled by a separate property to get past the parser.
				conv.i = GetIntConst(ex, ctx);
				params.Push(conv);
				conv.exp = nullptr;
				break;

			case 'I':
			case 'M':	// special case for morph styles in DECORATE . This expression-aware parser will not need this.
			case 'N':	// special case for thing activations in DECORATE. This expression-aware parser will not need this.
				conv.i = GetIntConst(ex, ctx);
				break;

			case 'F':
				conv.d = GetFloatConst(ex, ctx);
				break;

			case 'G':
				conv.fu = GetFuncConst(ex, ctx);
				break;

			case 'Z':	// an optional string. Does not allow any numeric value.
				if (ex->ValueType != TypeString)
				{
					// apply this expression to the next argument on the list.
					params.Push(conv);
					params[0].i++;
					p++;
					continue;
				}
				conv.s = GetStringConst(ex, ctx);
				break;

			case 'C':	// this parser accepts colors only in string form.
				pref.i = 1;
				[[fallthrough]];
			case 'S':
			case 'T': // a filtered string (ZScript only parses filtered strings so there's nothing to do here.)
				conv.s = GetStringConst(ex, ctx);
				break;

			case 'L':	// Either a number or a list of strings
				if (ex->ValueType != TypeString)
				{
					pref.i = 0;
					conv.i = GetIntConst(ex, ctx);
				}
				else
				{
					pref.i = 1;
					params.Push(pref);
					params[0].i++;

					do
					{
						conv.s = GetStringConst(ex, ctx);
						if (conv.s != nullptr)
						{
							params.Push(conv);
							params[0].i++;
						}
						exp = static_cast<ZCC_Expression *>(exp->SiblingNext);
						if (exp != property->Values)
						{
							ex = ConvertNode(exp);
							ex = ex->Resolve(ctx);
							if (ex == nullptr) return;
						}
					} while (exp != property->Values);
					goto endofparm;
				}
				break;

			default:
				assert(false);
				break;

			}
			if (pref.i != -1)
			{
				params.Push(pref);
				params[0].i++;
			}
			params.Push(conv);
			params[0].i++;
			exp = static_cast<ZCC_Expression *>(exp->SiblingNext);
		endofparm:
			p++;
			// Skip the DECORATE 'no comma' marker
			if (*p == '_') p++;

			if (*p == 0)
			{
				if (exp != property->Values)
				{
					Error(property, "Too many values for '%s'", prop->name);
					return;
				}
				break;
			}
			else if (exp == property->Values)
			{
				if (*p < 'a')
				{
					Error(property, "Insufficient parameters for %s", prop->name);
					return;
				}
				break;
			}
		}
	}
	// call the handler
	try
	{
		prop->Handler(defaults, bag.Info, bag, &params[0]);
	}
	catch (CRecoverableError &error)
	{
		Error(property, "%s", error.GetMessage());
	}
}


//==========================================================================
//
// Parses an actor property's parameters and calls the handler
//
//==========================================================================

PFunction * FindFunctionPointer(PClass * cls, int fn_name);
PFunction *NativeFunctionPointerCast(PFunction *from, const PFunctionPointer *to);

struct FunctionPointerProperties
{
	ZCC_PropertyStmt *prop;
	PClass * cls;
	FName name;
	const PFunctionPointer * type;
	PFunction ** addr;
};

TArray<FunctionPointerProperties> DefaultFunctionPointers;

void ZCCDoomCompiler::InitDefaultFunctionPointers()
{
	for(auto &d : DefaultFunctionPointers)
	{
		PFunction * fn = FindFunctionPointer(d.cls, d.name.GetIndex());
		if(!fn)
		{
			Error(d.prop, "Could not find function '%s' in class '%s'",d.name.GetChars(), d.cls->TypeName.GetChars());
		}
		else
		{
			PFunction * casted = NativeFunctionPointerCast(fn,d.type);
			if(!casted)
			{
				FString fn_proto_name = PFunctionPointer::GenerateNameForError(fn);
				Error(d.prop, "Function has incompatible types, cannot convert from '%s' to '%s'",fn_proto_name.GetChars(), d.type->DescriptiveName());
			}
			else
			{
				(*d.addr) = casted;
			}
		}
	}
	DefaultFunctionPointers.Clear();
}

void ZCCDoomCompiler::DispatchScriptProperty(PProperty *prop, ZCC_PropertyStmt *property, AActor *defaults, Baggage &bag)
{
	ZCC_ExprConstant one;
	unsigned parmcount = 1;
	ZCC_TreeNode *x = property->Values;
	if (x == nullptr)
	{
		parmcount = 0;
	}
	else
	{
		while (x->SiblingNext != property->Values)
		{
			x = x->SiblingNext;
			parmcount++;
		}
	}
	if (parmcount == 0 && prop->Variables.Size() == 1 && prop->Variables[0]->Type == TypeBool)
	{
		// allow boolean properties to have the parameter omitted
		memset(&one, 0, sizeof(one));
		one.SourceName = property->SourceName;	// This may not be null!
		one.Operation = PEX_ConstValue;
		one.NodeType = AST_ExprConstant;
		one.Type = TypeBool;
		one.IntVal = 1;
		property->Values = &one;
	}
	else if (parmcount != prop->Variables.Size())
	{
		Error(x == nullptr? property : x, "Argument count mismatch: Got %u, expected %u", parmcount, prop->Variables.Size());
		return;
	}

	auto exp = property->Values;
	FCompileContext ctx(OutNamespace, bag.Info->VMType, false, mVersion);
	for (auto f : prop->Variables)
	{
		void *addr;
		if (f == nullptr)
		{
			// This variable was missing. The error had been reported for the property itself already.
			return;
		}

		if (f->Flags & VARF_Meta)
		{
			addr = ((char*)bag.Info->Meta) + f->Offset;
		}
		else
		{
			addr = ((char*)defaults) + f->Offset;
		}

		FxExpression *ex = ConvertNode(exp);
		ex = ex->Resolve(ctx);
		if (ex == nullptr)
		{
			return;
		}
		else if (!ex->isConstant())
		{
			if (ex->ExprType == EFX_VectorValue && ex->ValueType == f->Type)
			{
				auto v = static_cast<FxVectorValue *>(ex);
				if (f->Type == TypeVector2)
				{
					if(!v->isConstVector(2))
					{
						Error(exp, "%s: non-constant Vector2 parameter", prop->SymbolName.GetChars());
						return;
					}
					(*(DVector2*)addr) = DVector2(
						static_cast<FxConstant *>(v->xyzw[0])->GetValue().GetFloat(),
						static_cast<FxConstant *>(v->xyzw[1])->GetValue().GetFloat()
					);
					goto vector_ok;
				}
				else if (f->Type == TypeFVector2)
				{
					if(!v->isConstVector(2))
					{
						Error(exp, "%s: non-constant FVector2 parameter", prop->SymbolName.GetChars());
						return;
					}
					(*(FVector2*)addr) = FVector2(
						float(static_cast<FxConstant *>(v->xyzw[0])->GetValue().GetFloat()),
						float(static_cast<FxConstant *>(v->xyzw[1])->GetValue().GetFloat())
					);
					goto vector_ok;
				}
				else if (f->Type == TypeVector3)
				{
					if(!v->isConstVector(3))
					{
						Error(exp, "%s: non-constant Vector3 parameter", prop->SymbolName.GetChars());
						return;
					}
					(*(DVector3*)addr) = DVector3(
						static_cast<FxConstant *>(v->xyzw[0])->GetValue().GetFloat(),
						static_cast<FxConstant *>(v->xyzw[1])->GetValue().GetFloat(),
						static_cast<FxConstant *>(v->xyzw[2])->GetValue().GetFloat()
					);
					goto vector_ok;
				}
				else if (f->Type == TypeFVector3)
				{
					if(!v->isConstVector(3))
					{
						Error(exp, "%s: non-constant FVector3 parameter", prop->SymbolName.GetChars());
						return;
					}
					(*(FVector3*)addr) = FVector3(
						float(static_cast<FxConstant*>(v->xyzw[0])->GetValue().GetFloat()),
						float(static_cast<FxConstant*>(v->xyzw[1])->GetValue().GetFloat()),
						float(static_cast<FxConstant*>(v->xyzw[2])->GetValue().GetFloat())
					);
					goto vector_ok;
				}
				else if (f->Type == TypeVector4)
				{
					if(!v->isConstVector(4))
					{
						Error(exp, "%s: non-constant Vector4 parameter", prop->SymbolName.GetChars());
						return;
					}
					(*(DVector4*)addr) = DVector4(
						static_cast<FxConstant *>(v->xyzw[0])->GetValue().GetFloat(),
						static_cast<FxConstant *>(v->xyzw[1])->GetValue().GetFloat(),
						static_cast<FxConstant *>(v->xyzw[2])->GetValue().GetFloat(),
						static_cast<FxConstant *>(v->xyzw[3])->GetValue().GetFloat()
					);
					goto vector_ok;
				}
				else if (f->Type == TypeFVector4)
				{
					if(!v->isConstVector(4))
					{
						Error(exp, "%s: non-constant FVector4 parameter", prop->SymbolName.GetChars());
						return;
					}
					(*(FVector4*)addr) = FVector4(
						float(static_cast<FxConstant*>(v->xyzw[0])->GetValue().GetFloat()),
						float(static_cast<FxConstant*>(v->xyzw[1])->GetValue().GetFloat()),
						float(static_cast<FxConstant*>(v->xyzw[2])->GetValue().GetFloat()),
						float(static_cast<FxConstant *>(v->xyzw[3])->GetValue().GetFloat())
					);
					goto vector_ok;
				}
				else
				{
					Error(exp, "%s: invalid vector parameter", prop->SymbolName.GetChars());
					return;
				}
			}
			else
			{
				if (exp->Type != TypeError) Error(exp, "%s: non-constant parameter", prop->SymbolName.GetChars());
				return;
			}
			// If we get TypeError, there has already been a message from deeper down so do not print another one.
		}

		if (f->Type == TypeBool)
		{
			static_cast<PBool*>(f->Type)->SetValue(addr, !!GetIntConst(ex, ctx));
		}
		else if (f->Type == TypeName)
		{
			*(FName*)addr = GetStringConst(ex, ctx);
		}
		else if (f->Type == TypeSound)
		{
			*(FSoundID*)addr = S_FindSound(GetStringConst(ex, ctx));
		}
		else if (f->Type == TypeColor && ex->ValueType == TypeString)	// colors can also be specified as ints.
		{
			*(PalEntry*)addr = V_GetColor(GetStringConst(ex, ctx), &ex->ScriptPosition);
		}
		else if (f->Type->isIntCompatible())
		{
			static_cast<PInt*>(f->Type)->SetValue(addr, GetIntConst(ex, ctx));
		}
		else if (f->Type->isFloat())
		{
			static_cast<PFloat*>(f->Type)->SetValue(addr, GetFloatConst(ex, ctx));
		}
		else if (f->Type == TypeString)
		{
			*(FString*)addr = GetStringConst(ex, ctx);
		}
		else if (f->Type->isClassPointer())
		{
			auto clsname = GetStringConst(ex, ctx);
			if (*clsname == 0 || !stricmp(clsname, "none"))
			{
				*(PClass**)addr = nullptr;
			}
			else
			{
				auto cls = PClass::FindClass(clsname);
				auto cp = static_cast<PClassPointer*>(f->Type);
				if (cls == nullptr)
				{
					cls = cp->ClassRestriction->FindClassTentative(clsname);
				}
				else if (!cls->IsDescendantOf(cp->ClassRestriction))
				{
					Error(property, "class %s is not compatible with property type %s", clsname, cp->ClassRestriction->TypeName.GetChars());
				}
				*(PClass**)addr = cls;
			}
		}
		else if (f->Type->isFunctionPointer())
		{
			const char * fn_str = GetStringConst(ex, ctx);
			if (*fn_str == 0 || !stricmp(fn_str, "none"))
			{
				*(PFunction**)addr = nullptr;
			}
			else
			{
				TArray<FString> fn_info(FString(fn_str).Split("::", FString::TOK_SKIPEMPTY));
				if(fn_info.Size() != 2)
				{
					Error(property, "Malformed function pointer property \"%s\", must be \"Class::Function\"",fn_str);
				}
				PClass * cls = PClass::FindClass(fn_info[0]);
				if(!cls)
				{
					Error(property, "Could not find class '%s'",fn_info[0].GetChars());
					*(PFunction**)addr = nullptr;
				}
				else
				{
					FName fn_name(fn_info[1], true);
					if(fn_name.GetIndex() == 0)
					{
						Error(property, "Could not find function '%s' in class '%s'",fn_info[1].GetChars(),fn_info[0].GetChars());
						*(PFunction**)addr = nullptr;
					}
					else
					{
						DefaultFunctionPointers.Push({property, cls, fn_name, static_cast<const PFunctionPointer *>(f->Type), (PFunction**)addr});
						*(PFunction**)addr = nullptr;
					}

				}
			}

		}
		else
		{
			Error(property, "unhandled property type %s", f->Type->DescriptiveName());
		}
vector_ok:
		exp->ToErrorNode();	// invalidate after processing.
		exp = static_cast<ZCC_Expression *>(exp->SiblingNext);
	}
}

//==========================================================================
//
// Parses an actor property
//
//==========================================================================

void ZCCDoomCompiler::ProcessDefaultProperty(PClassActor *cls, ZCC_PropertyStmt *prop, Baggage &bag)
{
	auto namenode = prop->Prop;
	FString propname;

	if (namenode->SiblingNext == namenode)
	{
		if (namenode->Id == NAME_DamageFunction)
		{
			auto x = ConvertNode(prop->Values);
			CreateDamageFunction(OutNamespace, mVersion, cls, (AActor *)bag.Info->Defaults, x, false, Lump);
			((AActor *)bag.Info->Defaults)->DamageVal = -1;
			return;
		}

		// a one-name property
		propname = FName(namenode->Id).GetChars();

	}
	else if (namenode->SiblingNext->SiblingNext == namenode)
	{
		FName name(namenode->Id);

		if(name == NAME_self)
		{
			name = cls->TypeName;
		}

		// a two-name property
		propname << name.GetChars() << "." << FName(static_cast<ZCC_Identifier *>(namenode->SiblingNext)->Id).GetChars();
	}
	else
	{
		Error(prop, "Property name may at most contain two parts");
		return;
	}


	FPropertyInfo *property = FindProperty(propname.GetChars());

	if (property != nullptr && property->category != CAT_INFO)
	{
		auto pcls = PClass::FindActor(property->clsname);
		if (cls->IsDescendantOf(pcls))
		{
			DispatchProperty(property, prop, (AActor *)bag.Info->Defaults, bag);
		}
		else
		{
			Error(prop, "'%s' requires an actor of type '%s'\n", propname.GetChars(), pcls->TypeName.GetChars());
		}
	}
	else
	{
		propname.Insert(0, "@property@");
		FName name(propname, true);
		if (name != NAME_None)
		{
			auto propp = dyn_cast<PProperty>(cls->FindSymbol(name, true));
			if (propp != nullptr)
			{
				DispatchScriptProperty(propp, prop, (AActor *)bag.Info->Defaults, bag);
				return;
			}
		}
		Error(prop, "'%s' is an unknown actor property\n", propname.GetChars());
	}
}

//==========================================================================
//
// Finds a flag and sets or clears it
//
//==========================================================================

void ZCCDoomCompiler::ProcessDefaultFlag(PClassActor *cls, ZCC_FlagStmt *flg)
{
	auto namenode = flg->name;
	const char *n1 = FName(namenode->Id).GetChars(), *n2;

	if (namenode->SiblingNext == namenode)
	{
		// a one-name flag
		n2 = nullptr;
	}
	else if (namenode->SiblingNext->SiblingNext == namenode)
	{
		// a two-name flag

		if(namenode->Id == NAME_self)
		{
			n1 = cls->TypeName.GetChars();
		}


		n2 = FName(static_cast<ZCC_Identifier *>(namenode->SiblingNext)->Id).GetChars();
	}
	else
	{
		Error(flg, "Flag name may at most contain two parts");
		return;
	}

	auto fd = FindFlag(cls, n1, n2, true);
	if (fd != nullptr)
	{
		if ((fd->varflags & VARF_Deprecated) && fd->deprecationVersion <= this->mVersion)
		{
			Warn(flg, "Deprecated flag '%s%s%s' used, deprecated since %d.%d.%d", n1, n2 ? "." : "", n2 ? n2 : "", 
				fd->deprecationVersion.major, fd->deprecationVersion.minor, fd->deprecationVersion.revision);
		}
		if (fd->structoffset == -1)
		{
			HandleDeprecatedFlags((AActor*)cls->Defaults, flg->set, fd->flagbit);
		}
		else
		{
			ModActorFlag((AActor*)cls->Defaults, fd, flg->set);
		}
	}
	else
	{
		Error(flg, "Unknown flag '%s%s%s'", n1, n2 ? "." : "", n2 ? n2 : "");
	}
}

//==========================================================================
//
// Parses the default list
//
//==========================================================================

void ZCCDoomCompiler::InitDefaults()
{
	for (auto c : Classes)
	{
		// This may be removed if the conditions change, but right now only subclasses of Actor can define a Default block.
		if (!c->ClassType()->IsDescendantOf(RUNTIME_CLASS(AActor)))
		{
			if (c->Defaults.Size()) Error(c->cls, "%s: Non-actor classes may not have defaults", c->ClassType()->TypeName.GetChars());
			if (c->ClassType()->ParentClass)
			{
				auto ti = c->ClassType();
				ti->InitializeDefaults();
			}
		}
		else
		{
			auto cls = c->ClassType();
			// This should never happen.
			if (cls->Defaults != nullptr)
			{
				Error(c->cls, "%s already has defaults", cls->TypeName.GetChars());
			}
			// This can only occur if a native parent is not initialized. In all other cases the sorting of the class list should prevent this from ever happening.
			else if (cls->ParentClass->Defaults == nullptr && cls != RUNTIME_CLASS(AActor))
			{
				Error(c->cls, "Parent class %s of %s is not initialized", cls->ParentClass->TypeName.GetChars(), cls->TypeName.GetChars());
			}
			else
			{
				// Copy the parent's defaults and meta data.
				auto ti = static_cast<PClassActor *>(cls);

				ti->InitializeDefaults();

				// Replacements require that the meta data has been allocated by InitializeDefaults.
				if (c->cls->Replaces != nullptr && !ti->SetReplacement(c->cls->Replaces->Id))
				{
					Warn(c->cls, "Replaced type '%s' not found for %s", FName(c->cls->Replaces->Id).GetChars(), ti->TypeName.GetChars());
				}

				// We need special treatment for this one field in AActor's defaults which cannot be made visible to DECORATE as a property.
				// It's better to do this here under controlled conditions than deeper down in the class type classes.
				if (ti == RUNTIME_CLASS(AActor))
				{
					((AActor*)ti->Defaults)->ConversationRoot = 1;
				}

				Baggage bag;
			#ifdef _DEBUG
				bag.ClassName = cls->TypeName.GetChars();
			#endif
				bag.Version = mVersion;
				bag.Namespace = OutNamespace;
				bag.Info = ti;
				bag.DropItemSet = false;
				bag.StateSet = false;
				bag.fromDecorate = false;
				bag.CurrentState = 0;
				bag.Lumpnum = c->cls->SourceLump;
				bag.DropItemList = nullptr;
				// The actual script position needs to be set per property.

				for (auto d : c->Defaults)
				{
					auto content = d->Content;
					if (content != nullptr) do
					{
						switch (content->NodeType)
						{
						case AST_PropertyStmt:
							bag.ScriptPosition.FileName = *content->SourceName;
							bag.ScriptPosition.ScriptLine = content->SourceLoc;
							ProcessDefaultProperty(ti, static_cast<ZCC_PropertyStmt *>(content), bag);
							break;

						case AST_FlagStmt:
							ProcessDefaultFlag(ti, static_cast<ZCC_FlagStmt *>(content));
							break;

						default:
							break;
						}
						content = static_cast<decltype(content)>(content->SiblingNext);
					} while (content != d->Content);
				}
				if (bag.DropItemSet)
				{
					bag.Info->SetDropItems(bag.DropItemList);
				}
			}
		}
	}
}

//==========================================================================
//
// very complicated check for random duration.
//
//==========================================================================

static bool CheckRandom(ZCC_Expression *duration)
{
	if (duration->NodeType != AST_ExprFuncCall) return false;
	auto func = static_cast<ZCC_ExprFuncCall *>(duration);
	if (func->Function == nullptr) return false;
	if (func->Function->NodeType != AST_ExprID) return false;
	auto f2 = static_cast<ZCC_ExprID *>(func->Function);
	return f2->Identifier == NAME_Random;
}

//==========================================================================
//
// Sets up the action function call
//
//==========================================================================
FxExpression *ZCCDoomCompiler::SetupActionFunction(PClass *cls, ZCC_TreeNode *af, int StateFlags)
{
	// We have 3 cases to consider here:
	// 1. A function without parameters. This can be called directly
	// 2. A functon with parameters. This needs to be wrapped into a helper function to set everything up.
	// 3. An anonymous function.

	// 1. and 2. are exposed through AST_ExprFunctionCall
	if (af->NodeType == AST_ExprFuncCall)
	{
		auto fc = static_cast<ZCC_ExprFuncCall *>(af);
		assert(fc->Function->NodeType == AST_ExprID);
		auto id = static_cast<ZCC_ExprID *>(fc->Function);

		// We must skip ACS_NamedExecuteWithResult here, because this name both exists as an action function and as a builtin.
		// The code which gets called from here can easily make use of the builtin, so let's just pass this name without any checks.
		// The actual action function is still needed by DECORATE:
		if (id->Identifier != NAME_ACS_NamedExecuteWithResult)
		{
			PFunction *afd = dyn_cast<PFunction>(cls->VMType->Symbols.FindSymbol(id->Identifier, true));
			if (afd != nullptr)
			{
				if (fc->Parameters == nullptr && !(afd->Variants[0].Flags & VARF_Virtual))
				{
					FArgumentList argumentlist;
					// We can use this function directly without wrapping it in a caller.
					assert(PType::toClass(afd->Variants[0].SelfClass) != nullptr);	// non classes are not supposed to get here.

					int comboflags = afd->Variants[0].UseFlags & StateFlags;
					if (comboflags == StateFlags)	// the function must satisfy all the flags the state requires
					{
						if ((afd->Variants[0].Flags & VARF_Private) && afd->OwningClass != cls->VMType)
						{
							Error(af, "%s is declared private and not accessible", FName(id->Identifier).GetChars());
						}
						
						return new FxVMFunctionCall(new FxSelf(*af), afd, argumentlist, *af, false);
					}
					else
					{
						Error(af, "Cannot use non-action function %s here.", FName(id->Identifier).GetChars());
					}
				}
			}
			else
			{
				// it may also be an action special so check that first before printing an error.
				if (!P_FindLineSpecial(FName(id->Identifier).GetChars()))
				{
					Error(af, "%s: action function not found in %s", FName(id->Identifier).GetChars(), cls->TypeName.GetChars());
					return nullptr;
				}
				// Action specials fall through to the code generator.
			}
		}
	}
	return ConvertAST(cls->VMType, af);
}

//==========================================================================
//
// Compile the states
//
//==========================================================================

void ZCCDoomCompiler::CompileStates()
{
	for (auto c : Classes)
	{
		
		if (!c->ClassType()->IsDescendantOf(RUNTIME_CLASS(AActor)))
		{
			if (c->States.Size()) Error(c->cls, "%s: States can only be defined for actors.", c->Type()->TypeName.GetChars());
			continue;
		}

		FString statename;	// The state builder wants the label as one complete string, not separated into tokens.
		FStateDefinitions statedef;

		if (static_cast<PClassActor*>(c->ClassType())->ActorInfo()->SkipSuperSet)
		{
			// SKIP_SUPER'ed actors only get the base states from AActor.
			statedef.MakeStateDefines(RUNTIME_CLASS(AActor));
		}
		else
		{
			statedef.MakeStateDefines(ValidateActor(c->ClassType()->ParentClass));
		}


		int numframes = 0;

		for (auto s : c->States)
		{
			int flags;
			if (s->Flags != nullptr)
			{
				flags = 0;
				auto p = s->Flags;
				do
				{
					switch (p->Id)
					{
					case NAME_Actor:
						flags |= SUF_ACTOR;
						break;
					case NAME_Overlay:
						flags |= SUF_OVERLAY;
						break;
					case NAME_Weapon:
						flags |= SUF_WEAPON;
						break;
					case NAME_Item:
						flags |= SUF_ITEM;
						break;
					default:
						Error(p, "Unknown States qualifier %s", FName(p->Id).GetChars());
						break;
					}

					p = static_cast<decltype(p)>(p->SiblingNext);
				} while (p != s->Flags);
			}
			else
			{
				flags = static_cast<PClassActor *>(c->ClassType())->ActorInfo()->DefaultStateUsage;
			}
			auto st = s->Body;
			if (st != nullptr) do
			{
				switch (st->NodeType)
				{
				case AST_StateLabel:
				{
					auto sl = static_cast<ZCC_StateLabel *>(st);
					statename = FName(sl->Label).GetChars();
					statedef.AddStateLabel(statename.GetChars());
					break;
				}
				case AST_StateLine:
				{
					auto sl = static_cast<ZCC_StateLine *>(st);
					FState state;
					memset(&state, 0, sizeof(state));
					state.UseFlags = flags;
					if (sl->Sprite->Len() != 4)
					{
						Error(sl, "Sprite name must be exactly 4 characters. Found '%s'", sl->Sprite->GetChars());
					}
					else
					{
						state.sprite = GetSpriteIndex(sl->Sprite->GetChars());
					}
					FCompileContext ctx(OutNamespace, c->Type(), false, mVersion);
					if (CheckRandom(sl->Duration))
					{
						auto func = static_cast<ZCC_ExprFuncCall *>(sl->Duration);
						if (func->Parameters == func->Parameters->SiblingNext || func->Parameters != func->Parameters->SiblingNext->SiblingNext)
						{
							Error(sl, "Random duration requires exactly 2 parameters");
						}
						int v1 = IntConstFromNode(func->Parameters->Value, c->Type());
						int v2 = IntConstFromNode(static_cast<ZCC_FuncParm *>(func->Parameters->SiblingNext)->Value, c->Type());
						if (v1 > v2) std::swap(v1, v2);
						state.Tics = (int16_t)clamp<int>(v1, 0, INT16_MAX);
						state.TicRange = (uint16_t)clamp<int>(v2 - v1, 0, UINT16_MAX);
					}
					else
					{
						state.Tics = (int16_t)IntConstFromNode(sl->Duration, c->Type());
						state.TicRange = 0;
					}
					if (sl->bBright) state.StateFlags |= STF_FULLBRIGHT;
					if (sl->bFast) state.StateFlags |= STF_FAST;
					if (sl->bSlow) state.StateFlags |= STF_SLOW;
					if (sl->bCanRaise) state.StateFlags |= STF_CANRAISE;
					if (sl->bNoDelay) state.StateFlags |= STF_NODELAY;
					if (sl->bNoDelay)
					{
						if (statedef.GetStateLabelIndex(NAME_Spawn) != statedef.GetStateCount())
						{
							Warn(sl, "NODELAY only has an effect on the first state after 'Spawn:'");
						}
					}
					if (sl->Offset != nullptr)
					{
						state.Misc1 = IntConstFromNode(sl->Offset, c->Type());
						state.Misc2 = IntConstFromNode(static_cast<ZCC_Expression *>(sl->Offset->SiblingNext), c->Type());
					}

					if (sl->Lights != nullptr)
					{
						auto l = sl->Lights;
						do
						{
							AddStateLight(&state, StringConstFromNode(l, c->Type()).GetChars());
							l = static_cast<decltype(l)>(l->SiblingNext);
						} while (l != sl->Lights);
					}

					if (sl->Action != nullptr)
					{
						auto code = SetupActionFunction(static_cast<PClassActor *>(c->ClassType()), sl->Action, state.UseFlags);
						if (code != nullptr)
						{
							auto funcsym = CreateAnonymousFunction(c->Type(), nullptr, state.UseFlags);
							state.ActionFunc = FunctionBuildList.AddFunction(OutNamespace, mVersion, funcsym, code, FStringf("%s.StateFunction.%d", c->Type()->TypeName.GetChars(), statedef.GetStateCount()), false, statedef.GetStateCount(), (int)sl->Frames->Len(), Lump);
						}
					}

					int count = statedef.AddStates(&state, sl->Frames->GetChars(), *sl);
					if (count < 0)
					{
						Error(sl, "Invalid frame character string '%s'", sl->Frames->GetChars());
						count = -count;
					}
					break;
				}
				case AST_StateGoto:
				{
					auto sg = static_cast<ZCC_StateGoto *>(st);
					statename = "";
					if (sg->Qualifier != nullptr)
					{
						statename << FName(sg->Qualifier->Id).GetChars() << "::";
					}
					auto part = sg->Label;
					do
					{
						statename << FName(part->Id).GetChars() << '.';
						part = static_cast<decltype(part)>(part->SiblingNext);
					} while (part != sg->Label);
					statename.Truncate(statename.Len() - 1);	// remove the last '.' in the label name
					if (sg->Offset != nullptr)
					{
						int offset = IntConstFromNode(sg->Offset, c->Type());
						if (offset < 0)
						{
							Error(sg, "GOTO offset must be positive");
							offset = 0;
						}
						if (offset > 0)
						{
							statename.AppendFormat("+%d", offset);
						}
					}
					if (!statedef.SetGotoLabel(statename.GetChars()))
					{
						Error(sg, "GOTO before first state");
					}
					break;
				}
				case AST_StateFail:
				case AST_StateWait:
					if (!statedef.SetWait())
					{
						Error(st, "%s before first state", st->NodeType == AST_StateFail ? "Fail" : "Wait");
					}
					break;

				case AST_StateLoop:
					if (!statedef.SetLoop())
					{
						Error(st, "LOOP before first state");
					}
					break;

				case AST_StateStop:
					if (!statedef.SetStop())
					{
						Error(st, "STOP before first state");
					}
					break;

				default:
					assert(0 && "Bad AST node in state");
				}
				st = static_cast<decltype(st)>(st->SiblingNext);
			} while (st != s->Body);
		}
		try
		{
			FinalizeClass(c->ClassType(), statedef);
		}
		catch (CRecoverableError &err)
		{
			Error(c->cls, "%s", err.GetMessage());
		}
	}
}

//==========================================================================
//
// 'action' is a Doom specific keyword. The default implementation just
// errors out on it
//
//==========================================================================

int ZCCDoomCompiler::CheckActionKeyword(ZCC_FuncDeclarator* f, uint32_t &varflags, int useflags, ZCC_StructWork *c)
{
	if (varflags & VARF_ReadOnly)
	{
		Error(f, "Action functions cannot be declared const");
		varflags &= ~VARF_ReadOnly;
	}
	if (varflags & VARF_UI)
	{
		Error(f, "Action functions cannot be declared UI");
	}
	// Non-Actors cannot have action functions.
	if (!isActor(c->Type()))
	{
		Error(f, "'Action' can only be used in child classes of Actor");
	}

	varflags |= VARF_Final;	// Action implies Final.
	if (useflags & (SUF_OVERLAY | SUF_WEAPON | SUF_ITEM))
	{
		varflags |= VARF_Action;
		return 3;
	}
	else
	{
		return 1;
	}	
}

//==========================================================================
//
// AActor needs the actor info manually added to its meta data 
// before adding any scripted fields.
//
//==========================================================================

bool ZCCDoomCompiler::PrepareMetaData(PClass *type)
{
	if (type->TypeName == NAME_Actor)
	{
		assert(type->MetaSize == 0);
		AddActorInfo(type);	
		return true;
	}
	return false;
}

