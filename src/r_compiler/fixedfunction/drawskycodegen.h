
#pragma once

#include "drawercodegen.h"

enum class DrawSkyVariant
{
	Single,
	Double
};

class DrawSkyCodegen : public DrawerCodegen
{
public:
	void Generate(DrawSkyVariant variant, bool fourColumns, SSAValue args, SSAValue thread_data);

private:
	void Loop(DrawSkyVariant variant, bool fourColumns);
	SSAVec4i Sample(SSAInt frac, int index, DrawSkyVariant variant);
	SSAVec4i FadeOut(SSAInt frac, SSAVec4i color);

	SSAStack<SSAInt> stack_index, stack_frac[4];

	SSAUBytePtr dest;
	SSAUBytePtr source0[4];
	SSAUBytePtr source1[4];
	SSAInt pitch;
	SSAInt count;
	SSAInt dest_y;
	SSAInt texturefrac[4];
	SSAInt iscale[4];
	SSAInt textureheight0;
	SSAInt textureheight1;
	SSAVec4i top_color;
	SSAVec4i bottom_color;
	SSAWorkerThread thread;

	SSAInt fracstep[4];
};
