/*
**  Polygon Doom software renderer
**  Copyright (c) 2016 Magnus Norddahl
**
**  This software is provided 'as-is', without any express or implied
**  warranty.  In no event will the authors be held liable for any damages
**  arising from the use of this software.
**
**  Permission is granted to anyone to use this software for any purpose,
**  including commercial applications, and to alter it and redistribute it
**  freely, subject to the following restrictions:
**
**  1. The origin of this software must not be misrepresented; you must not
**     claim that you wrote the original software. If you use this software
**     in a product, an acknowledgment in the product documentation would be
**     appreciated but is not required.
**  2. Altered source versions must be plainly marked as such, and must not be
**     misrepresented as being the original software.
**  3. This notice may not be removed or altered from any source distribution.
**
*/

#pragma once

class PolyTriangleThreadData;

enum SWBlendColor
{
	SWBLEND_Sub = 1,
	SWBLEND_RevSub = 2
};

struct BlendColorOpt_Add { static const int Flags = 0; };
struct BlendColorOpt_Sub { static const int Flags = 1; };
struct BlendColorOpt_RevSub { static const int Flags = 2; };

template<typename OptT>
void BlendColor(int y, int x0, int x1, PolyTriangleThreadData* thread);
void BlendColorOpaque(int y, int x0, int x1, PolyTriangleThreadData* thread);
void BlendColorOpaque(int y, int x0, int x1, PolyTriangleThreadData* thread);
void BlendColorAdd_Src_InvSrc(int y, int x0, int x1, PolyTriangleThreadData* thread);
void BlendColorAdd_SrcCol_InvSrcCol(int y, int x0, int x1, PolyTriangleThreadData* thread);
void BlendColorAdd_Src_One(int y, int x0, int x1, PolyTriangleThreadData* thread);
void BlendColorAdd_SrcCol_One(int y, int x0, int x1, PolyTriangleThreadData* thread);
void BlendColorAdd_DstCol_Zero(int y, int x0, int x1, PolyTriangleThreadData* thread);
void BlendColorAdd_InvDstCol_Zero(int y, int x0, int x1, PolyTriangleThreadData* thread);
void BlendColorRevSub_Src_One(int y, int x0, int x1, PolyTriangleThreadData* thread);

void SelectWriteColorFunc(PolyTriangleThreadData* thread);
