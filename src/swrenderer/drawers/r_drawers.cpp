/*
**  LLVM code generated drawers
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

#include "i_system.h"
#include "r_drawers.h"
#include "x86.h"
#include "c_cvars.h"
#include "version.h"
#include "m_misc.h"

/////////////////////////////////////////////////////////////////////////////

extern "C"
{
	void DrawColumn_SSE2(const DrawColumnArgs *, const WorkerThreadData *);
	void DrawColumnAdd_SSE2(const DrawColumnArgs *, const WorkerThreadData *);
	void DrawColumnShaded_SSE2(const DrawColumnArgs *, const WorkerThreadData *);
	void DrawColumnAddClamp_SSE2(const DrawColumnArgs *, const WorkerThreadData *);
	void DrawColumnSubClamp_SSE2(const DrawColumnArgs *, const WorkerThreadData *);
	void DrawColumnRevSubClamp_SSE2(const DrawColumnArgs *, const WorkerThreadData *);
	void DrawColumnTranslated_SSE2(const DrawColumnArgs *, const WorkerThreadData *);
	void DrawColumnTlatedAdd_SSE2(const DrawColumnArgs *, const WorkerThreadData *);
	void DrawColumnAddClampTranslated_SSE2(const DrawColumnArgs *, const WorkerThreadData *);
	void DrawColumnSubClampTranslated_SSE2(const DrawColumnArgs *, const WorkerThreadData *);
	void DrawColumnRevSubClampTranslated_SSE2(const DrawColumnArgs *, const WorkerThreadData *);
	void FillColumn_SSE2(const DrawColumnArgs *, const WorkerThreadData *);
	void FillColumnAdd_SSE2(const DrawColumnArgs *, const WorkerThreadData *);
	void FillColumnAddClamp_SSE2(const DrawColumnArgs *, const WorkerThreadData *);
	void FillColumnSubClamp_SSE2(const DrawColumnArgs *, const WorkerThreadData *);
	void FillColumnRevSubClamp_SSE2(const DrawColumnArgs *, const WorkerThreadData *);
	void DrawColumnRt1_SSE2(const DrawColumnArgs *, const WorkerThreadData *);
	void DrawColumnRt1Copy_SSE2(const DrawColumnArgs *, const WorkerThreadData *);
	void DrawColumnRt1Add_SSE2(const DrawColumnArgs *, const WorkerThreadData *);
	void DrawColumnRt1Shaded_SSE2(const DrawColumnArgs *, const WorkerThreadData *);
	void DrawColumnRt1AddClamp_SSE2(const DrawColumnArgs *, const WorkerThreadData *);
	void DrawColumnRt1SubClamp_SSE2(const DrawColumnArgs *, const WorkerThreadData *);
	void DrawColumnRt1RevSubClamp_SSE2(const DrawColumnArgs *, const WorkerThreadData *);
	void DrawColumnRt1Translated_SSE2(const DrawColumnArgs *, const WorkerThreadData *);
	void DrawColumnRt1TlatedAdd_SSE2(const DrawColumnArgs *, const WorkerThreadData *);
	void DrawColumnRt1AddClampTranslated_SSE2(const DrawColumnArgs *, const WorkerThreadData *);
	void DrawColumnRt1SubClampTranslated_SSE2(const DrawColumnArgs *, const WorkerThreadData *);
	void DrawColumnRt1RevSubClampTranslated_SSE2(const DrawColumnArgs *, const WorkerThreadData *);
	void DrawColumnRt4_SSE2(const DrawColumnArgs *, const WorkerThreadData *);
	void DrawColumnRt4Copy_SSE2(const DrawColumnArgs *, const WorkerThreadData *);
	void DrawColumnRt4Add_SSE2(const DrawColumnArgs *, const WorkerThreadData *);
	void DrawColumnRt4Shaded_SSE2(const DrawColumnArgs *, const WorkerThreadData *);
	void DrawColumnRt4AddClamp_SSE2(const DrawColumnArgs *, const WorkerThreadData *);
	void DrawColumnRt4SubClamp_SSE2(const DrawColumnArgs *, const WorkerThreadData *);
	void DrawColumnRt4RevSubClamp_SSE2(const DrawColumnArgs *, const WorkerThreadData *);
	void DrawColumnRt4Translated_SSE2(const DrawColumnArgs *, const WorkerThreadData *);
	void DrawColumnRt4TlatedAdd_SSE2(const DrawColumnArgs *, const WorkerThreadData *);
	void DrawColumnRt4AddClampTranslated_SSE2(const DrawColumnArgs *, const WorkerThreadData *);
	void DrawColumnRt4SubClampTranslated_SSE2(const DrawColumnArgs *, const WorkerThreadData *);
	void DrawColumnRt4RevSubClampTranslated_SSE2(const DrawColumnArgs *, const WorkerThreadData *);
	void DrawSpan_SSE2(const DrawSpanArgs *);
	void DrawSpanMasked_SSE2(const DrawSpanArgs *);
	void DrawSpanTranslucent_SSE2(const DrawSpanArgs *);
	void DrawSpanMaskedTranslucent_SSE2(const DrawSpanArgs *);
	void DrawSpanAddClamp_SSE2(const DrawSpanArgs *);
	void DrawSpanMaskedAddClamp_SSE2(const DrawSpanArgs *);
	void vlinec1_SSE2(const DrawWallArgs *, const WorkerThreadData *);
	void vlinec4_SSE2(const DrawWallArgs *, const WorkerThreadData *);
	void mvlinec1_SSE2(const DrawWallArgs *, const WorkerThreadData *);
	void mvlinec4_SSE2(const DrawWallArgs *, const WorkerThreadData *);
	void tmvline1_add_SSE2(const DrawWallArgs *, const WorkerThreadData *);
	void tmvline4_add_SSE2(const DrawWallArgs *, const WorkerThreadData *);
	void tmvline1_addclamp_SSE2(const DrawWallArgs *, const WorkerThreadData *);
	void tmvline4_addclamp_SSE2(const DrawWallArgs *, const WorkerThreadData *);
	void tmvline1_subclamp_SSE2(const DrawWallArgs *, const WorkerThreadData *);
	void tmvline4_subclamp_SSE2(const DrawWallArgs *, const WorkerThreadData *);
	void tmvline1_revsubclamp_SSE2(const DrawWallArgs *, const WorkerThreadData *);
	void tmvline4_revsubclamp_SSE2(const DrawWallArgs *, const WorkerThreadData *);
	void DrawSky1_SSE2(const DrawSkyArgs *, const WorkerThreadData *);
	void DrawSky4_SSE2(const DrawSkyArgs *, const WorkerThreadData *);
	void DrawDoubleSky1_SSE2(const DrawSkyArgs *, const WorkerThreadData *);
	void DrawDoubleSky4_SSE2(const DrawSkyArgs *, const WorkerThreadData *);
	void TriDraw8_0_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDraw8_1_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDraw8_2_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDraw8_3_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDraw8_4_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDraw8_5_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDraw8_6_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDraw8_7_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDraw8_8_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDraw8_9_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDraw8_10_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDraw8_11_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDraw8_12_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDraw8_13_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDraw8_14_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDraw32_0_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDraw32_1_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDraw32_2_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDraw32_3_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDraw32_4_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDraw32_5_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDraw32_6_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDraw32_7_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDraw32_8_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDraw32_9_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDraw32_10_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDraw32_11_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDraw32_12_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDraw32_13_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDraw32_14_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFill8_0_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFill8_1_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFill8_2_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFill8_3_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFill8_4_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFill8_5_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFill8_6_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFill8_7_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFill8_8_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFill8_9_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFill8_10_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFill8_11_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFill8_12_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFill8_13_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFill8_14_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFill32_0_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFill32_1_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFill32_2_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFill32_3_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFill32_4_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFill32_5_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFill32_6_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFill32_7_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFill32_8_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFill32_9_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFill32_10_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFill32_11_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFill32_12_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFill32_13_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFill32_14_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
}

/////////////////////////////////////////////////////////////////////////////

Drawers::Drawers()
{
	DrawColumn = DrawColumn_SSE2;
	DrawColumnAdd = DrawColumnAdd_SSE2;
	DrawColumnShaded = DrawColumnShaded_SSE2;
	DrawColumnAddClamp = DrawColumnAddClamp_SSE2;
	DrawColumnSubClamp = DrawColumnSubClamp_SSE2;
	DrawColumnRevSubClamp = DrawColumnRevSubClamp_SSE2;
	DrawColumnTranslated = DrawColumnTranslated_SSE2;
	DrawColumnTlatedAdd = DrawColumnTlatedAdd_SSE2;
	DrawColumnAddClampTranslated = DrawColumnAddClampTranslated_SSE2;
	DrawColumnSubClampTranslated = DrawColumnSubClampTranslated_SSE2;
	DrawColumnRevSubClampTranslated = DrawColumnRevSubClampTranslated_SSE2;
	FillColumn = FillColumn_SSE2;
	FillColumnAdd = FillColumnAdd_SSE2;
	FillColumnAddClamp = FillColumnAddClamp_SSE2;
	FillColumnSubClamp = FillColumnSubClamp_SSE2;
	FillColumnRevSubClamp = FillColumnRevSubClamp_SSE2;
	DrawSpan = DrawSpan_SSE2;
	DrawSpanMasked = DrawSpanMasked_SSE2;
	DrawSpanTranslucent = DrawSpanTranslucent_SSE2;
	DrawSpanMaskedTranslucent = DrawSpanMaskedTranslucent_SSE2;
	DrawSpanAddClamp = DrawSpanAddClamp_SSE2;
	DrawSpanMaskedAddClamp = DrawSpanMaskedAddClamp_SSE2;
	vlinec1 = vlinec1_SSE2;
	vlinec4 = vlinec4_SSE2;
	mvlinec1 = mvlinec1_SSE2;
	mvlinec4 = mvlinec4_SSE2;
	tmvline1_add = tmvline1_add_SSE2;
	tmvline4_add = tmvline4_add_SSE2;
	tmvline1_addclamp = tmvline1_addclamp_SSE2;
	tmvline4_addclamp = tmvline4_addclamp_SSE2;
	tmvline1_subclamp = tmvline1_subclamp_SSE2;
	tmvline4_subclamp = tmvline4_subclamp_SSE2;
	tmvline1_revsubclamp = tmvline1_revsubclamp_SSE2;
	tmvline4_revsubclamp = tmvline4_revsubclamp_SSE2;
	DrawSky1 = DrawSky1_SSE2;
	DrawSky4 = DrawSky4_SSE2;
	DrawDoubleSky1 = DrawDoubleSky1_SSE2;
	DrawDoubleSky4 = DrawDoubleSky4_SSE2;
	TriDraw8.push_back(TriDraw8_0_SSE2);
	TriDraw8.push_back(TriDraw8_1_SSE2);
	TriDraw8.push_back(TriDraw8_2_SSE2);
	TriDraw8.push_back(TriDraw8_3_SSE2);
	TriDraw8.push_back(TriDraw8_4_SSE2);
	TriDraw8.push_back(TriDraw8_5_SSE2);
	TriDraw8.push_back(TriDraw8_6_SSE2);
	TriDraw8.push_back(TriDraw8_7_SSE2);
	TriDraw8.push_back(TriDraw8_8_SSE2);
	TriDraw8.push_back(TriDraw8_9_SSE2);
	TriDraw8.push_back(TriDraw8_10_SSE2);
	TriDraw8.push_back(TriDraw8_11_SSE2);
	TriDraw8.push_back(TriDraw8_12_SSE2);
	TriDraw8.push_back(TriDraw8_13_SSE2);
	TriDraw8.push_back(TriDraw8_14_SSE2);
	TriDraw32.push_back(TriDraw32_0_SSE2);
	TriDraw32.push_back(TriDraw32_1_SSE2);
	TriDraw32.push_back(TriDraw32_2_SSE2);
	TriDraw32.push_back(TriDraw32_3_SSE2);
	TriDraw32.push_back(TriDraw32_4_SSE2);
	TriDraw32.push_back(TriDraw32_5_SSE2);
	TriDraw32.push_back(TriDraw32_6_SSE2);
	TriDraw32.push_back(TriDraw32_7_SSE2);
	TriDraw32.push_back(TriDraw32_8_SSE2);
	TriDraw32.push_back(TriDraw32_9_SSE2);
	TriDraw32.push_back(TriDraw32_10_SSE2);
	TriDraw32.push_back(TriDraw32_11_SSE2);
	TriDraw32.push_back(TriDraw32_12_SSE2);
	TriDraw32.push_back(TriDraw32_13_SSE2);
	TriDraw32.push_back(TriDraw32_14_SSE2);
	TriFill8.push_back(TriFill8_0_SSE2);
	TriFill8.push_back(TriFill8_1_SSE2);
	TriFill8.push_back(TriFill8_2_SSE2);
	TriFill8.push_back(TriFill8_3_SSE2);
	TriFill8.push_back(TriFill8_4_SSE2);
	TriFill8.push_back(TriFill8_5_SSE2);
	TriFill8.push_back(TriFill8_6_SSE2);
	TriFill8.push_back(TriFill8_7_SSE2);
	TriFill8.push_back(TriFill8_8_SSE2);
	TriFill8.push_back(TriFill8_9_SSE2);
	TriFill8.push_back(TriFill8_10_SSE2);
	TriFill8.push_back(TriFill8_11_SSE2);
	TriFill8.push_back(TriFill8_12_SSE2);
	TriFill8.push_back(TriFill8_13_SSE2);
	TriFill8.push_back(TriFill8_14_SSE2);
	TriFill32.push_back(TriFill32_0_SSE2);
	TriFill32.push_back(TriFill32_1_SSE2);
	TriFill32.push_back(TriFill32_2_SSE2);
	TriFill32.push_back(TriFill32_3_SSE2);
	TriFill32.push_back(TriFill32_4_SSE2);
	TriFill32.push_back(TriFill32_5_SSE2);
	TriFill32.push_back(TriFill32_6_SSE2);
	TriFill32.push_back(TriFill32_7_SSE2);
	TriFill32.push_back(TriFill32_8_SSE2);
	TriFill32.push_back(TriFill32_9_SSE2);
	TriFill32.push_back(TriFill32_10_SSE2);
	TriFill32.push_back(TriFill32_11_SSE2);
	TriFill32.push_back(TriFill32_12_SSE2);
	TriFill32.push_back(TriFill32_13_SSE2);
	TriFill32.push_back(TriFill32_14_SSE2);
}

Drawers *Drawers::Instance()
{
	static Drawers drawers;
	return &drawers;
}

FString DrawWallArgs::ToString()
{
	FString info;
	info.Format("dest_y = %i, count = %i, flags = %i, texturefrac[0] = %u, textureheight[0] = %u", dest_y, count, flags, texturefrac[0], textureheight[0]);
	return info;
}

FString DrawSpanArgs::ToString()
{
	FString info;
	info.Format("x1 = %i, x2 = %i, y = %i, flags = %i", x1, x2, y, flags);
	return info;
}

FString DrawColumnArgs::ToString()
{
	FString info;
	info.Format("dest_y = %i, count = %i, flags = %i, iscale = %i (%f), texturefrac = %i (%f)", dest_y, count, flags, iscale, ((fixed_t)iscale) / (float)FRACUNIT, texturefrac, ((fixed_t)texturefrac) / (float)FRACUNIT);
	return info;
}

FString DrawSkyArgs::ToString()
{
	FString info;
	info.Format("dest_y = %i, count = %i", dest_y, count);
	return info;
}
