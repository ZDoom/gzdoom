/*
** refdocs.cpp
**
**---------------------------------------------------------------------------
** Copyright -2016 Randy Heit
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

#include "dobject.h"
#include "sc_man.h"
#include "memarena.h"
#include "zcc_parser.h"
#include "zcc-parse.h"
#include <map>

extern const char *BuiltInTypeNames[];

static const char *HtmlBegin = R"htmlblock(<!DOCTYPE html>
<html>
<head>
	<meta charset="UTF-8" />
	<title>ZScript Reference</title>
	<style>
		html, body, form, h1 { margin: 0; border: none; padding: 0; }
		body { margin: 15px 30px; background: white; color: black; font: 13px/1.5 'Segoe UI', 'Tahoma', sans-serif; }
		h1 { font-weight: normal; font-size: 35px; margin: 15px 0 0 0; }
		h2 { font-size: 20px; margin: 15px 0 0 0; }
		h3 { font-size: 13px; font-style: italic; font-weight: normal; margin: 0; }
		.body { padding-left: 30px; }
		.keyword { }
		.typename { font-weight: bold; color: #3D578C; }
		.variable { font-weight: bold; color: #3D578C; }
		.function-name { font-weight: bold; color: #3D578C; }
		.property-name { font-weight: bold; color: #3D578C; }
		.enum-name { font-weight: bold; color: #3D578C; }
		.enum-member { font-weight: bold; color: #3D578C; }
		.const-name { font-weight: bold; color: #3D578C; }
		.class { }
		.struct { }
		.enum { }
		.enum-body { padding-left: 30px; }
		.constant { }
		.var-decl { }
		.function { }
		.property { }
		.states { }
		.default { }
		.unknown { color: red; }
		.index { }
		.index-column { float: left; width: calc(20% - 20px); margin-right: 10px; }
		.index-name a, .index-name a:visited { font-weight: bold; text-decoration: none; color: #3D578C; }
		.details { margin-top: 30px; }
		.members { margin-bottom: 15px; }
	</style>
</head>
<body>
<h1>ZScript Reference</h1>
)htmlblock";

static const char *HtmlEnd = R"htmlblock(
</body>
</html>
)htmlblock";

static const char *HtmlIndexBegin = R"htmlblock(
<div class='index'>
)htmlblock";

static const char *HtmlIndexEnd = R"htmlblock(
</div>
)htmlblock";

struct RefDocsBody
{
	std::map<FString, FString> ClassNames;
	std::map<FString, FString> StructNames;
	std::map<FString, FString> Enums;
	std::map<FString, FString> Constants;
	std::map<FString, FString> Variables;
	std::map<FString, FString> Functions;
	std::map<FString, FString> Properties;
	FString States;
	FString Defaults;
	FString Unknown;

	FString CreateIndex()
	{
		FString output;
		output += HtmlIndexBegin;
		output += CreateIndex("Classes", ClassNames);
		output += CreateIndex("Structs", StructNames);
		output += CreateIndex("Enums", Enums);
		output += CreateIndex("Constants", Constants);
		output += HtmlIndexEnd;
		return output;
	}

	FString CreateIndex(FString name, const std::map<FString, FString> &index)
	{
		FString output;
		output.AppendFormat("<h2>%s</h2>\n", name.GetChars());
		output += "<div class='index-column'>";
		size_t columnHeight = index.size() / 5 + 1;
		size_t i = 0;
		for (const auto &it : index)
		{
			if (i % columnHeight == 0 && i != 0)
			{
				output += "</div><div class='index-column'>";
			}

			output += "<div class='index-name'><a href='#" + it.first + "'>" + it.first + "</a></div>";
			i++;
		}
		output += "</div><div style='clear:both'></div>\n";
		return output;
	}

	FString ToString()
	{
		FString output;
		output += ToString("Functions", Functions);
		output += ToString("Properties", Properties);
		output += ToString("Classes", ClassNames);
		output += ToString("Structs", StructNames);
		output += ToString("Enums", Enums);
		output += ToString("Constants", Constants);
		output += ToString("Variables", Variables);
		return output;
	}

	FString ToString(FString name, const std::map<FString, FString> &index)
	{
		if (index.empty())
			return FString();

		FString output;
		output.AppendFormat("<h3>%s</h3>\n", name.GetChars());
		output += "<div class='members'>\n";
		for (const auto &it : index)
		{
			output.AppendFormat("<div id='%s'>%s</div>\n", it.first.GetChars(), it.second.GetChars());
		}
		output += "</div>\n";
		return output;
	}
};

void ZCC_PrintBodyRef(ZCC_TreeNode *body, RefDocsBody &docsBody)
{
	if (!body)
		return;

	TArray<FString> declarations;

	FString currentEnum, currentEnumName;

	ZCC_TreeNode *p = body;
	do
	{

		if (p->NodeType == AST_Struct)
		{
			ZCC_Struct *snode = (ZCC_Struct *)p;
			FString name = ((FName)snode->NodeName);
			FString out;
			out.AppendFormat("<div class='struct'><span class='keyword'>struct</span> <span class='typename'>%s</span>\n", name.GetChars());
			out += "<div class='body'>\n";
			RefDocsBody childBody;
			ZCC_PrintBodyRef(snode->Body, childBody);
			out += childBody.ToString();
			out += "</div></div>\n";
			docsBody.StructNames[name] += out;
		}
		else if (p->NodeType == AST_Class)
		{
			ZCC_Class *cnode = (ZCC_Class *)p;
			FString name = ((FName)cnode->NodeName);
			FString out;
			out.AppendFormat("<div class='class'><span class='keyword'>class</span> <span class='typename'>%s</span>\n", name.GetChars());
			out += "<div class='body'>\n";
			RefDocsBody childBody;
			ZCC_PrintBodyRef(cnode->Body, childBody);
			out += childBody.ToString();
			out += "</div></div>\n";
			docsBody.ClassNames[name] += out;
		}
		else if (p->NodeType == AST_VarDeclarator)
		{
			ZCC_VarDeclarator *vdnode = (ZCC_VarDeclarator *)p;
			ZCC_TreeNode *pname = vdnode->Names;

			FString typeName;
			if (vdnode->Type->NodeType == AST_BasicType)
			{
				ZCC_BasicType *btype = (ZCC_BasicType *)vdnode->Type;
				typeName += "<span class='keyword'>";
				typeName = BuiltInTypeNames[btype->Type];
				typeName += "</span>";
			}
			else
			{
				typeName = "<span class='typename'>unknown</span>";
			}

			do
			{
				if (pname->NodeType == AST_VarName)
				{
					ZCC_VarName *vname = (ZCC_VarName *)pname;
					FString name = ((FName)vname->Name);
					FString out;
					out.AppendFormat("<div class='var-decl'>%s <span class='variable'>%s</span></div>\n", typeName.GetChars(), name.GetChars());
					docsBody.Variables[name] += out;
				}

				pname = pname->SiblingNext;
			} while (pname != vdnode->Names);
		}
		else if (p->NodeType == AST_Enum)
		{
			ZCC_Enum *enode = (ZCC_Enum *)p;
			currentEnumName = ((FName)enode->NodeName);
			currentEnum.AppendFormat("<div class='enum'><span class='keyword'>enum</span> <span class='enum-name'>%s</span><div class='enum-body'>\n", currentEnumName.GetChars());
		}
		else if (p->NodeType == AST_ConstantDef)
		{
			ZCC_ConstantDef *cnode = (ZCC_ConstantDef *)p;
			if (currentEnum.Len() != 0)
			{
				currentEnum.AppendFormat("<div class='enum-member'>%s</div>\n", ((FName)cnode->NodeName).GetChars());
			}
			else
			{
				FString out;
				FString name = ((FName)cnode->NodeName);
				out.AppendFormat("<div class='constant'><span class='keyword'>const</span> <span class='const-name'>%s</span></div>\n", name.GetChars());
				docsBody.Constants[name] += out;
			}
		}
		else if (p->NodeType == AST_EnumTerminator)
		{
			ZCC_EnumTerminator *cnode = (ZCC_EnumTerminator *)p;
			FString out;
			out += currentEnum;
			out += "</div></div>\n";
			docsBody.Enums[currentEnumName] += out;
			currentEnum = "";
			currentEnumName = "";
		}
		else if (p->NodeType == AST_FuncDeclarator)
		{
			ZCC_FuncDeclarator *fdnode = (ZCC_FuncDeclarator *)p;
			FString name = ((FName)fdnode->Name);

			FString retVal;
			FString args;

			if (!fdnode->Type)
			{
				retVal += "<span class='keyword'>void</span>";
			}
			else if (fdnode->Type->NodeType == AST_BasicType)
			{
				ZCC_BasicType *btype = (ZCC_BasicType *)fdnode->Type;
				retVal += "<span class='keyword'>";
				retVal = BuiltInTypeNames[btype->Type];
				retVal += "</span>";
			}
			else
			{
				retVal = "<span class='typename'>unknown</span>";
			}

			if (fdnode->Params)
			{
				TArray<FString> argsHtml;
				ZCC_TreeNode *arg = fdnode->Params;
				do
				{
					ZCC_FuncParamDecl *param = (ZCC_FuncParamDecl *)arg;

					FString typeName;
					if (!param->Type)
					{
						typeName += "<span class='keyword'>void</span>";
					}
					else if (param->Type->NodeType == AST_BasicType)
					{
						ZCC_BasicType *btype = (ZCC_BasicType *)param->Type;
						typeName += "<span class='keyword'>";
						typeName = BuiltInTypeNames[btype->Type];
						typeName += "</span>";
					}
					else
					{
						typeName = "<span class='typename'>unknown</span>";
					}

					FString argName = ((FName)param->Name);
					FString out;
					out.AppendFormat("%s %s", typeName.GetChars(), argName.GetChars());
					argsHtml.Push(out);

					arg = arg->SiblingNext;
				} while (arg != fdnode->Params);

				for (unsigned int i = 0; i < argsHtml.Size(); i++)
				{
					if (i > 0) args += ", ";
					args += argsHtml[i];
				}
			}

			FString out;
			out.AppendFormat("<div class='function'>%s <span class='function-name'>%s</span>(%s)</div>\n", retVal.GetChars(), name.GetChars(), args.GetChars());

			docsBody.Functions[name] += out;
		}
		else if (p->NodeType == AST_Property)
		{
			ZCC_Property *pnode = (ZCC_Property *)p;
			FString name = ((FName)pnode->NodeName);
			FString out;
			out.AppendFormat("<div class='property'><span class='keyword'>property</span> <span class='property-name'>%s</span></div>\n", name.GetChars());
			docsBody.Properties[name] += out;
		}
		else if (p->NodeType == AST_States)
		{
			FString out;
			out.AppendFormat("<div class='states'><span class='keyword'>states</span></div>\n");
			docsBody.States += out;
		}
		else if (p->NodeType == AST_Default)
		{
			FString out;
			out.AppendFormat("<div class='default'><span class='keyword'>default</span></div>\n");
			docsBody.Defaults += out;
		}
		else
		{
			FString out;
			out.AppendFormat("<div class='unknown'>Node type %d</div>\n", (int)p->NodeType);
			docsBody.Unknown += out;
		}

		p = p->SiblingNext;
	} while (p != body);
}

FString ZCC_PrintRefDocs(ZCC_TreeNode *root)
{
	RefDocsBody docsBody;
	ZCC_PrintBodyRef(root, docsBody);
	return HtmlBegin + docsBody.CreateIndex() + "<div class='details'>" + docsBody.ToString() + "</div>" + HtmlEnd;
}
