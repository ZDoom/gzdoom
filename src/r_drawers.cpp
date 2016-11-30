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
	void TriDrawNormal8_0_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawNormal8_1_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawNormal8_2_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawNormal8_3_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawNormal8_4_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawNormal8_5_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawNormal8_6_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawNormal8_7_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawNormal8_8_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawNormal8_9_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawNormal8_10_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawNormal8_11_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawNormal8_12_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawNormal32_0_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawNormal32_1_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawNormal32_2_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawNormal32_3_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawNormal32_4_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawNormal32_5_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawNormal32_6_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawNormal32_7_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawNormal32_8_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawNormal32_9_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawNormal32_10_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawNormal32_11_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawNormal32_12_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawNormal32_13_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillNormal8_0_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillNormal8_1_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillNormal8_2_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillNormal8_3_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillNormal8_4_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillNormal8_5_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillNormal8_6_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillNormal8_7_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillNormal8_8_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillNormal8_9_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillNormal8_10_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillNormal8_11_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillNormal8_12_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillNormal32_0_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillNormal32_1_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillNormal32_2_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillNormal32_3_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillNormal32_4_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillNormal32_5_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillNormal32_6_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillNormal32_7_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillNormal32_8_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillNormal32_9_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillNormal32_10_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillNormal32_11_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillNormal32_12_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillNormal32_13_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawSubsector8_0_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawSubsector8_1_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawSubsector8_2_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawSubsector8_3_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawSubsector8_4_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawSubsector8_5_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawSubsector8_6_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawSubsector8_7_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawSubsector8_8_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawSubsector8_9_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawSubsector8_10_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawSubsector8_11_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawSubsector8_12_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawSubsector8_13_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawSubsector32_0_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawSubsector32_1_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawSubsector32_2_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawSubsector32_3_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawSubsector32_4_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawSubsector32_5_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawSubsector32_6_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawSubsector32_7_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawSubsector32_8_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawSubsector32_9_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawSubsector32_10_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawSubsector32_11_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawSubsector32_12_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriDrawSubsector32_13_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillSubsector8_0_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillSubsector8_1_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillSubsector8_2_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillSubsector8_3_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillSubsector8_4_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillSubsector8_5_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillSubsector8_6_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillSubsector8_7_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillSubsector8_8_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillSubsector8_9_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillSubsector8_10_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillSubsector8_11_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillSubsector8_12_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillSubsector8_13_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillSubsector32_0_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillSubsector32_1_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillSubsector32_2_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillSubsector32_3_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillSubsector32_4_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillSubsector32_5_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillSubsector32_6_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillSubsector32_7_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillSubsector32_8_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillSubsector32_9_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillSubsector32_10_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillSubsector32_11_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillSubsector32_12_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriFillSubsector32_13_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriStencil_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
	void TriStencilClose_SSE2(const TriDrawTriangleArgs *, WorkerThreadData *);
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
	DrawColumnRt1 = DrawColumnRt1_SSE2;
	DrawColumnRt1Copy = DrawColumnRt1Copy_SSE2;
	DrawColumnRt1Add = DrawColumnRt1Add_SSE2;
	DrawColumnRt1Shaded = DrawColumnRt1Shaded_SSE2;
	DrawColumnRt1AddClamp = DrawColumnRt1AddClamp_SSE2;
	DrawColumnRt1SubClamp = DrawColumnRt1SubClamp_SSE2;
	DrawColumnRt1RevSubClamp = DrawColumnRt1RevSubClamp_SSE2;
	DrawColumnRt1Translated = DrawColumnRt1Translated_SSE2;
	DrawColumnRt1TlatedAdd = DrawColumnRt1TlatedAdd_SSE2;
	DrawColumnRt1AddClampTranslated = DrawColumnRt1AddClampTranslated_SSE2;
	DrawColumnRt1SubClampTranslated = DrawColumnRt1SubClampTranslated_SSE2;
	DrawColumnRt1RevSubClampTranslated = DrawColumnRt1RevSubClampTranslated_SSE2;
	DrawColumnRt4 = DrawColumnRt4_SSE2;
	DrawColumnRt4Copy = DrawColumnRt4Copy_SSE2;
	DrawColumnRt4Add = DrawColumnRt4Add_SSE2;
	DrawColumnRt4Shaded = DrawColumnRt4Shaded_SSE2;
	DrawColumnRt4AddClamp = DrawColumnRt4AddClamp_SSE2;
	DrawColumnRt4SubClamp = DrawColumnRt4SubClamp_SSE2;
	DrawColumnRt4RevSubClamp = DrawColumnRt4RevSubClamp_SSE2;
	DrawColumnRt4Translated = DrawColumnRt4Translated_SSE2;
	DrawColumnRt4TlatedAdd = DrawColumnRt4TlatedAdd_SSE2;
	DrawColumnRt4AddClampTranslated = DrawColumnRt4AddClampTranslated_SSE2;
	DrawColumnRt4SubClampTranslated = DrawColumnRt4SubClampTranslated_SSE2;
	DrawColumnRt4RevSubClampTranslated = DrawColumnRt4RevSubClampTranslated_SSE2;
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
	TriDrawNormal8.push_back(TriDrawNormal8_0_SSE2);
	TriDrawNormal8.push_back(TriDrawNormal8_1_SSE2);
	TriDrawNormal8.push_back(TriDrawNormal8_2_SSE2);
	TriDrawNormal8.push_back(TriDrawNormal8_3_SSE2);
	TriDrawNormal8.push_back(TriDrawNormal8_4_SSE2);
	TriDrawNormal8.push_back(TriDrawNormal8_5_SSE2);
	TriDrawNormal8.push_back(TriDrawNormal8_6_SSE2);
	TriDrawNormal8.push_back(TriDrawNormal8_7_SSE2);
	TriDrawNormal8.push_back(TriDrawNormal8_8_SSE2);
	TriDrawNormal8.push_back(TriDrawNormal8_9_SSE2);
	TriDrawNormal8.push_back(TriDrawNormal8_10_SSE2);
	TriDrawNormal8.push_back(TriDrawNormal8_11_SSE2);
	TriDrawNormal8.push_back(TriDrawNormal8_12_SSE2);
	TriDrawNormal32.push_back(TriDrawNormal32_0_SSE2);
	TriDrawNormal32.push_back(TriDrawNormal32_1_SSE2);
	TriDrawNormal32.push_back(TriDrawNormal32_2_SSE2);
	TriDrawNormal32.push_back(TriDrawNormal32_3_SSE2);
	TriDrawNormal32.push_back(TriDrawNormal32_4_SSE2);
	TriDrawNormal32.push_back(TriDrawNormal32_5_SSE2);
	TriDrawNormal32.push_back(TriDrawNormal32_6_SSE2);
	TriDrawNormal32.push_back(TriDrawNormal32_7_SSE2);
	TriDrawNormal32.push_back(TriDrawNormal32_8_SSE2);
	TriDrawNormal32.push_back(TriDrawNormal32_9_SSE2);
	TriDrawNormal32.push_back(TriDrawNormal32_10_SSE2);
	TriDrawNormal32.push_back(TriDrawNormal32_11_SSE2);
	TriDrawNormal32.push_back(TriDrawNormal32_12_SSE2);
	TriFillNormal8.push_back(TriFillNormal8_0_SSE2);
	TriFillNormal8.push_back(TriFillNormal8_1_SSE2);
	TriFillNormal8.push_back(TriFillNormal8_2_SSE2);
	TriFillNormal8.push_back(TriFillNormal8_3_SSE2);
	TriFillNormal8.push_back(TriFillNormal8_4_SSE2);
	TriFillNormal8.push_back(TriFillNormal8_5_SSE2);
	TriFillNormal8.push_back(TriFillNormal8_6_SSE2);
	TriFillNormal8.push_back(TriFillNormal8_7_SSE2);
	TriFillNormal8.push_back(TriFillNormal8_8_SSE2);
	TriFillNormal8.push_back(TriFillNormal8_9_SSE2);
	TriFillNormal8.push_back(TriFillNormal8_10_SSE2);
	TriFillNormal8.push_back(TriFillNormal8_11_SSE2);
	TriFillNormal8.push_back(TriFillNormal8_12_SSE2);
	TriFillNormal32.push_back(TriFillNormal32_0_SSE2);
	TriFillNormal32.push_back(TriFillNormal32_1_SSE2);
	TriFillNormal32.push_back(TriFillNormal32_2_SSE2);
	TriFillNormal32.push_back(TriFillNormal32_3_SSE2);
	TriFillNormal32.push_back(TriFillNormal32_4_SSE2);
	TriFillNormal32.push_back(TriFillNormal32_5_SSE2);
	TriFillNormal32.push_back(TriFillNormal32_6_SSE2);
	TriFillNormal32.push_back(TriFillNormal32_7_SSE2);
	TriFillNormal32.push_back(TriFillNormal32_8_SSE2);
	TriFillNormal32.push_back(TriFillNormal32_9_SSE2);
	TriFillNormal32.push_back(TriFillNormal32_10_SSE2);
	TriFillNormal32.push_back(TriFillNormal32_11_SSE2);
	TriFillNormal32.push_back(TriFillNormal32_12_SSE2);
	TriDrawSubsector8.push_back(TriDrawSubsector8_0_SSE2);
	TriDrawSubsector8.push_back(TriDrawSubsector8_1_SSE2);
	TriDrawSubsector8.push_back(TriDrawSubsector8_2_SSE2);
	TriDrawSubsector8.push_back(TriDrawSubsector8_3_SSE2);
	TriDrawSubsector8.push_back(TriDrawSubsector8_4_SSE2);
	TriDrawSubsector8.push_back(TriDrawSubsector8_5_SSE2);
	TriDrawSubsector8.push_back(TriDrawSubsector8_6_SSE2);
	TriDrawSubsector8.push_back(TriDrawSubsector8_7_SSE2);
	TriDrawSubsector8.push_back(TriDrawSubsector8_8_SSE2);
	TriDrawSubsector8.push_back(TriDrawSubsector8_9_SSE2);
	TriDrawSubsector8.push_back(TriDrawSubsector8_10_SSE2);
	TriDrawSubsector8.push_back(TriDrawSubsector8_11_SSE2);
	TriDrawSubsector8.push_back(TriDrawSubsector8_12_SSE2);
	TriDrawSubsector32.push_back(TriDrawSubsector32_0_SSE2);
	TriDrawSubsector32.push_back(TriDrawSubsector32_1_SSE2);
	TriDrawSubsector32.push_back(TriDrawSubsector32_2_SSE2);
	TriDrawSubsector32.push_back(TriDrawSubsector32_3_SSE2);
	TriDrawSubsector32.push_back(TriDrawSubsector32_4_SSE2);
	TriDrawSubsector32.push_back(TriDrawSubsector32_5_SSE2);
	TriDrawSubsector32.push_back(TriDrawSubsector32_6_SSE2);
	TriDrawSubsector32.push_back(TriDrawSubsector32_7_SSE2);
	TriDrawSubsector32.push_back(TriDrawSubsector32_8_SSE2);
	TriDrawSubsector32.push_back(TriDrawSubsector32_9_SSE2);
	TriDrawSubsector32.push_back(TriDrawSubsector32_10_SSE2);
	TriDrawSubsector32.push_back(TriDrawSubsector32_11_SSE2);
	TriDrawSubsector32.push_back(TriDrawSubsector32_12_SSE2);
	TriFillSubsector8.push_back(TriFillSubsector8_0_SSE2);
	TriFillSubsector8.push_back(TriFillSubsector8_1_SSE2);
	TriFillSubsector8.push_back(TriFillSubsector8_2_SSE2);
	TriFillSubsector8.push_back(TriFillSubsector8_3_SSE2);
	TriFillSubsector8.push_back(TriFillSubsector8_4_SSE2);
	TriFillSubsector8.push_back(TriFillSubsector8_5_SSE2);
	TriFillSubsector8.push_back(TriFillSubsector8_6_SSE2);
	TriFillSubsector8.push_back(TriFillSubsector8_7_SSE2);
	TriFillSubsector8.push_back(TriFillSubsector8_8_SSE2);
	TriFillSubsector8.push_back(TriFillSubsector8_9_SSE2);
	TriFillSubsector8.push_back(TriFillSubsector8_10_SSE2);
	TriFillSubsector8.push_back(TriFillSubsector8_11_SSE2);
	TriFillSubsector8.push_back(TriFillSubsector8_12_SSE2);
	TriFillSubsector32.push_back(TriFillSubsector32_0_SSE2);
	TriFillSubsector32.push_back(TriFillSubsector32_1_SSE2);
	TriFillSubsector32.push_back(TriFillSubsector32_2_SSE2);
	TriFillSubsector32.push_back(TriFillSubsector32_3_SSE2);
	TriFillSubsector32.push_back(TriFillSubsector32_4_SSE2);
	TriFillSubsector32.push_back(TriFillSubsector32_5_SSE2);
	TriFillSubsector32.push_back(TriFillSubsector32_6_SSE2);
	TriFillSubsector32.push_back(TriFillSubsector32_7_SSE2);
	TriFillSubsector32.push_back(TriFillSubsector32_8_SSE2);
	TriFillSubsector32.push_back(TriFillSubsector32_9_SSE2);
	TriFillSubsector32.push_back(TriFillSubsector32_10_SSE2);
	TriFillSubsector32.push_back(TriFillSubsector32_11_SSE2);
	TriFillSubsector32.push_back(TriFillSubsector32_12_SSE2);
	TriStencil = TriStencil_SSE2;
	TriStencilClose = TriStencilClose_SSE2;
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
