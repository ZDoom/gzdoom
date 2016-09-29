
#pragma once

#include "drawercodegen.h"

enum class DrawColumnVariant
{
	Opaque,
	Fuzz,
	Add,
	Translated,
	TlatedAdd,
	Shaded,
	AddClamp,
	AddClampTranslated,
	SubClamp,
	SubClampTranslated,
	RevSubClamp,
	RevSubClampTranslated
};

class DrawColumnCodegen : public DrawerCodegen
{
public:
	void Generate(DrawColumnVariant variant, SSAValue args);
};
