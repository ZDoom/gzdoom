
#pragma once

#include "drawercodegen.h"

enum class DrawWallVariant
{
	Opaque1, // vlinec1
	Opaque4, // vlinec4
	Masked1, // mvlinec1
	Masked4, // mvlinec4
	Add1, // tmvline1_add
	Add4, // tmvline4_add
	AddClamp1, // tmvline1_addclamp
	AddClamp4, // tmvline4_addclamp
	SubClamp1, // tmvline1_subclamp
	SubClamp4, // tmvline4_subclamp
	RevSubClamp1, // tmvline1_revsubclamp
	RevSubClamp4, // tmvline4_revsubclamp
};

class DrawWallCodegen : public DrawerCodegen
{
public:
	void Generate(DrawWallVariant variant, SSAValue args);
};
