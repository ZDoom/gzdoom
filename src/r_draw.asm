	TITLE	r_draw.cpp
	.386P
include listing.inc
if @Version gt 510
.model FLAT
else
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
_DATA	SEGMENT DWORD USE32 PUBLIC 'DATA'
_DATA	ENDS
CONST	SEGMENT DWORD USE32 PUBLIC 'CONST'
CONST	ENDS
_BSS	SEGMENT PARA USE32 PUBLIC 'BSS'
_BSS	ENDS
_TLS	SEGMENT DWORD USE32 PUBLIC 'TLS'
_TLS	ENDS
;	COMDAT ??_C@_06NJMJ@detail?$AA@
_DATA	SEGMENT DWORD USE32 PUBLIC 'DATA'
_DATA	ENDS
;	COMDAT ??_C@_0BF@CFEP@doubling?5?$DN?5?$CF04?41f?5ms?$AA@
_DATA	SEGMENT DWORD USE32 PUBLIC 'DATA'
_DATA	ENDS
;	COMDAT ??_C@_0M@EMGL@r_drawtrans?$AA@
_DATA	SEGMENT DWORD USE32 PUBLIC 'DATA'
_DATA	ENDS
CRT$XCA	SEGMENT DWORD USE32 PUBLIC 'DATA'
CRT$XCA	ENDS
CRT$XCU	SEGMENT DWORD USE32 PUBLIC 'DATA'
CRT$XCU	ENDS
CRT$XCL	SEGMENT DWORD USE32 PUBLIC 'DATA'
CRT$XCL	ENDS
CRT$XCC	SEGMENT DWORD USE32 PUBLIC 'DATA'
CRT$XCC	ENDS
CRT$XCZ	SEGMENT DWORD USE32 PUBLIC 'DATA'
CRT$XCZ	ENDS
sreg$u	SEGMENT DWORD USE32 PUBLIC ''
sreg$u	ENDS
greg$u	SEGMENT DWORD USE32 PUBLIC ''
greg$u	ENDS
areg$u	SEGMENT DWORD USE32 PUBLIC ''
areg$u	ENDS
creg$u	SEGMENT DWORD USE32 PUBLIC ''
creg$u	ENDS
;	COMDAT ?IsAncestorOf@TypeInfo@@QBE_NPBU1@@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ??1FFile@@UAE@XZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ??_GFFile@@UAEPAXI@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ??6FArchive@@QAEAAV0@AAF@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ??6FArchive@@QAEAAV0@AAJ@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ??_GFArchive@@UAEPAXI@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?MulScale16@@YAJJJ@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?DMulScale16@@YAJJJJJ@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?TMulScale16@@YAJJJJJJJ@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?DivScale1@@YAJJJ@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?DivScale2@@YAJJJ@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?DivScale3@@YAJJJ@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?DivScale4@@YAJJJ@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?DivScale5@@YAJJJ@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?DivScale6@@YAJJJ@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?DivScale7@@YAJJJ@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?DivScale8@@YAJJJ@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?DivScale9@@YAJJJ@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?DivScale10@@YAJJJ@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?DivScale11@@YAJJJ@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?DivScale12@@YAJJJ@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?DivScale13@@YAJJJ@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?DivScale14@@YAJJJ@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?DivScale15@@YAJJJ@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?DivScale16@@YAJJJ@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?DivScale17@@YAJJJ@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?DivScale18@@YAJJJ@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?DivScale19@@YAJJJ@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?DivScale20@@YAJJJ@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?DivScale21@@YAJJJ@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?DivScale22@@YAJJJ@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?DivScale23@@YAJJJ@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?DivScale24@@YAJJJ@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?DivScale25@@YAJJJ@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?DivScale26@@YAJJJ@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?DivScale27@@YAJJJ@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?DivScale28@@YAJJJ@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?DivScale29@@YAJJJ@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?DivScale30@@YAJJJ@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?DivScale32@@YAJJJ@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ??_GFBaseCVar@@UAEPAXI@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ??BFBoolCVar@@QBE_NXZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ??BFIntCVar@@QBEHXZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ??BFFloatCVar@@QBEMXZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?StaticType@ASectorAction@@UBEPAUTypeInfo@@XZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ??_GASectorAction@@UAEPAXI@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ??1ASectorAction@@UAE@XZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?ZatPoint@secplane_t@@QBEJJJ@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?StaticType@AInventory@@UBEPAUTypeInfo@@XZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ??0AInventory@@IAE@XZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ??_GAInventory@@UAEPAXI@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ??1AInventory@@UAE@XZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?StaticType@AWeapon@@UBEPAUTypeInfo@@XZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ??_GAWeapon@@UAEPAXI@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ??1AWeapon@@UAE@XZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?StaticType@AHealth@@UBEPAUTypeInfo@@XZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ??_GAHealth@@UAEPAXI@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ??1AHealth@@UAE@XZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?StaticType@AArmor@@UBEPAUTypeInfo@@XZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ??_GAArmor@@UAEPAXI@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ??1AArmor@@UAE@XZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?StaticType@AAmmo@@UBEPAUTypeInfo@@XZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ??_GAAmmo@@UAEPAXI@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ??1AAmmo@@UAE@XZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?StaticType@AKey@@UBEPAUTypeInfo@@XZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ??_GAKey@@UAEPAXI@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ??1AKey@@UAE@XZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?StaticType@AArtifact@@UBEPAUTypeInfo@@XZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ??_GAArtifact@@UAEPAXI@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ??1AArtifact@@UAE@XZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?StaticType@APlayerPawn@@UBEPAUTypeInfo@@XZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ??0APlayerPawn@@IAE@XZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ??_GAPlayerPawn@@UAEPAXI@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ??1APlayerPawn@@UAE@XZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?StaticType@APlayerChunk@@UBEPAUTypeInfo@@XZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ??_GAPlayerChunk@@UAEPAXI@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ??1APlayerChunk@@UAE@XZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?GetWidth@DCanvas@@QBEHXZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?GetPitch@DCanvas@@QBEHXZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?DrawPatch@DCanvas@@QBEXPBUpatch_s@@HH@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?GetClockCycle@@YAKXZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?R_StretchColumnP_C@@YAXXZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?R_FillColumnP@@YAXXZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?R_InitFuzzTable@@YAXH@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?R_DrawAddColumnP_C@@YAXXZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?R_DrawTranslatedColumnP_C@@YAXXZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?R_DrawTlatedAddColumnP_C@@YAXXZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?R_DrawShadedColumnP_C@@YAXXZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?R_DrawAddClampColumnP_C@@YAXXZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?R_DrawAddClampTranslatedColumnP_C@@YAXXZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?R_FillSpan@@YAXXZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?setupvline@@YAXH@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?R_InitTranslationTables@@YAXXZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?R_BuildPlayerTranslation@@YAXHH@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?R_DrawBorder@@YAXHHHH@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?R_DrawViewBorder@@YAXXZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?R_DrawTopBorder@@YAXXZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?R_DetailDouble@@YAXXZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT _$E52
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ??0Stat_detail@@QAE@XZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT _$E53
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT _$E54
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT _$E55
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ??1Stat_detail@@UAE@XZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ??_GStat_detail@@UAEPAXI@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?GetStats@Stat_detail@@UAEXPAD@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?R_InitColumnDrawers@@YAXXZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT _$E57
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT _$E58
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT _$E59
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT _$E60
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ??1FBoolCVar@@UAE@XZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?R_SetBlendFunc@@YA_NJJ@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?R_SetPatchStyle@@YA?AW4ESPSResult@@HJPAEK@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?R_FinishSetPatchStyle@@YAXXZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ??0?$TArray@UNameMap@FArchive@@@@QAE@XZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ??1?$TArray@UNameMap@FArchive@@@@QAE@XZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ??0?$TArray@D@@QAE@XZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ??1?$TArray@D@@QAE@XZ
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?MIN@@YA?BEEE@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?clamp@@YAMMMM@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?clamp@@YAHHHH@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?MIN@@YA?BMMM@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ?clamp@@YAJJJJ@Z
_TEXT	SEGMENT PARA USE32 PUBLIC 'CODE'
_TEXT	ENDS
;	COMDAT ??_7FBaseCVar@@6B@
CONST	SEGMENT DWORD USE32 PUBLIC 'CONST'
CONST	ENDS
;	COMDAT ??_7APlayerPawn@@6B@
CONST	SEGMENT DWORD USE32 PUBLIC 'CONST'
CONST	ENDS
;	COMDAT ??_7FFile@@6B@
CONST	SEGMENT DWORD USE32 PUBLIC 'CONST'
CONST	ENDS
;	COMDAT ??_7AInventory@@6B@
CONST	SEGMENT DWORD USE32 PUBLIC 'CONST'
CONST	ENDS
;	COMDAT ??_7AWeapon@@6B@
CONST	SEGMENT DWORD USE32 PUBLIC 'CONST'
CONST	ENDS
;	COMDAT ??_7APlayerChunk@@6B@
CONST	SEGMENT DWORD USE32 PUBLIC 'CONST'
CONST	ENDS
;	COMDAT ??_7AKey@@6B@
CONST	SEGMENT DWORD USE32 PUBLIC 'CONST'
CONST	ENDS
;	COMDAT ??_7ASectorAction@@6B@
CONST	SEGMENT DWORD USE32 PUBLIC 'CONST'
CONST	ENDS
;	COMDAT ??_7AAmmo@@6B@
CONST	SEGMENT DWORD USE32 PUBLIC 'CONST'
CONST	ENDS
;	COMDAT ??_7AArtifact@@6B@
CONST	SEGMENT DWORD USE32 PUBLIC 'CONST'
CONST	ENDS
;	COMDAT ??_7Stat_detail@@6B@
CONST	SEGMENT DWORD USE32 PUBLIC 'CONST'
CONST	ENDS
;	COMDAT ??_7AArmor@@6B@
CONST	SEGMENT DWORD USE32 PUBLIC 'CONST'
CONST	ENDS
;	COMDAT ??_7FArchive@@6B@
CONST	SEGMENT DWORD USE32 PUBLIC 'CONST'
CONST	ENDS
;	COMDAT ??_7AHealth@@6B@
CONST	SEGMENT DWORD USE32 PUBLIC 'CONST'
CONST	ENDS
FLAT	GROUP _DATA, CONST, _BSS, CRT$XCA, CRT$XCU, CRT$XCL, CRT$XCC, CRT$XCZ, sreg$u, greg$u, areg$u, creg$u
	ASSUME	CS: FLAT, DS: FLAT, SS: FLAT
endif
PUBLIC	?viewwindowx@@3HA				; viewwindowx
PUBLIC	?viewwindowy@@3HA				; viewwindowy
PUBLIC	_viewheight
PUBLIC	_viewwidth
PUBLIC	_halfviewwidth
PUBLIC	_realviewwidth
PUBLIC	_realviewheight
PUBLIC	_detailxshift
PUBLIC	_detailyshift
PUBLIC	?BorderNeedRefresh@@3HA				; BorderNeedRefresh
PUBLIC	?BorderTopRefresh@@3HA				; BorderTopRefresh
PUBLIC	?r_drawtrans@@3VFBoolCVar@@A			; r_drawtrans
PUBLIC	?viewimage@@3PAEA				; viewimage
PUBLIC	?scaledviewwidth@@3HA				; scaledviewwidth
PUBLIC	_dccount
PUBLIC	?DetailDoubleCycles@@3KA			; DetailDoubleCycles
PUBLIC	_fuzzoffset
PUBLIC	_fuzzpos
PUBLIC	_ylookup
PUBLIC	_dc_pitch
PUBLIC	_dc_colormap
PUBLIC	_dc_x
PUBLIC	_dc_yl
PUBLIC	_dc_yh
PUBLIC	_dc_iscale
PUBLIC	_dc_texturemid
PUBLIC	_dc_texturefrac
PUBLIC	_dc_color
PUBLIC	_dc_srcblend
PUBLIC	_dc_destblend
PUBLIC	_dc_source
PUBLIC	_dc_dest
PUBLIC	_dc_count
PUBLIC	_vplce
PUBLIC	_vince
PUBLIC	_palookupoffse
PUBLIC	_bufplce
PUBLIC	?R_DrawColumn@@3P6AXXZA				; R_DrawColumn
PUBLIC	?dovline1@@3P6AKXZA				; dovline1
PUBLIC	?doprevline1@@3P6AKXZA				; doprevline1
PUBLIC	?dovline4@@3P6AXXZA				; dovline4
PUBLIC	?R_DrawFuzzColumn@@3P6AXXZA			; R_DrawFuzzColumn
PUBLIC	?R_DrawShadedColumn@@3P6AXXZA			; R_DrawShadedColumn
PUBLIC	?R_DrawTranslatedColumn@@3P6AXXZA		; R_DrawTranslatedColumn
PUBLIC	?R_DrawSpan@@3P6AXXZA				; R_DrawSpan
PUBLIC	?R_DrawColumnHoriz@@3P6AXXZA			; R_DrawColumnHoriz
PUBLIC	_dscount
PUBLIC	?rt_map4cols@@3P6AXHHH@ZA			; rt_map4cols
PUBLIC	_ds_y
PUBLIC	_ds_x1
PUBLIC	_ds_x2
PUBLIC	_ds_colormap
PUBLIC	_ds_xfrac
PUBLIC	_ds_yfrac
PUBLIC	_ds_xstep
PUBLIC	_ds_ystep
PUBLIC	_ds_source
PUBLIC	_ds_color
PUBLIC	?translationtables@@3PAEA			; translationtables
PUBLIC	?dc_translation@@3PAEA				; dc_translation
EXTRN	_vlinetallasm1:NEAR
EXTRN	_vlinetallasm4:NEAR
_BSS	SEGMENT
?viewwindowx@@3HA DD 01H DUP (?)			; viewwindowx
?viewwindowy@@3HA DD 01H DUP (?)			; viewwindowy
_viewheight DD	01H DUP (?)
_viewwidth DD	01H DUP (?)
_halfviewwidth DD 01H DUP (?)
_realviewwidth DD 01H DUP (?)
_realviewheight DD 01H DUP (?)
_detailxshift DD 01H DUP (?)
_detailyshift DD 01H DUP (?)
?BorderNeedRefresh@@3HA DD 01H DUP (?)			; BorderNeedRefresh
?BorderTopRefresh@@3HA DD 01H DUP (?)			; BorderTopRefresh
?r_drawtrans@@3VFBoolCVar@@A DB 018H DUP (?)		; r_drawtrans
?viewimage@@3PAEA DD 01H DUP (?)			; viewimage
?scaledviewwidth@@3HA DD 01H DUP (?)			; scaledviewwidth
_dccount DD	01H DUP (?)
?DetailDoubleCycles@@3KA DD 01H DUP (?)			; DetailDoubleCycles
_fuzzoffset DD	033H DUP (?)
_fuzzpos DD	01H DUP (?)
_ylookup DD	04b0H DUP (?)
_dc_colormap DD	01H DUP (?)
_dc_x	DD	01H DUP (?)
_dc_yl	DD	01H DUP (?)
_dc_yh	DD	01H DUP (?)
_dc_iscale DD	01H DUP (?)
_dc_texturemid DD 01H DUP (?)
_dc_texturefrac DD 01H DUP (?)
_dc_color DD	01H DUP (?)
_dc_srcblend DD	01H DUP (?)
_dc_destblend DD 01H DUP (?)
_dc_source DD	01H DUP (?)
_dc_dest DD	01H DUP (?)
_dc_count DD	01H DUP (?)
_vplce	DD	04H DUP (?)
_vince	DD	04H DUP (?)
_palookupoffse DD 04H DUP (?)
_bufplce DD	04H DUP (?)
?R_DrawColumn@@3P6AXXZA DD 01H DUP (?)			; R_DrawColumn
?R_DrawFuzzColumn@@3P6AXXZA DD 01H DUP (?)		; R_DrawFuzzColumn
?R_DrawShadedColumn@@3P6AXXZA DD 01H DUP (?)		; R_DrawShadedColumn
?R_DrawTranslatedColumn@@3P6AXXZA DD 01H DUP (?)	; R_DrawTranslatedColumn
?R_DrawSpan@@3P6AXXZA DD 01H DUP (?)			; R_DrawSpan
?R_DrawColumnHoriz@@3P6AXXZA DD 01H DUP (?)		; R_DrawColumnHoriz
_dscount DD	01H DUP (?)
?rt_map4cols@@3P6AXHHH@ZA DD 01H DUP (?)		; rt_map4cols
_ds_y	DD	01H DUP (?)
_ds_x1	DD	01H DUP (?)
_ds_x2	DD	01H DUP (?)
_ds_colormap DD	01H DUP (?)
_ds_xfrac DD	01H DUP (?)
_ds_yfrac DD	01H DUP (?)
_ds_xstep DD	01H DUP (?)
_ds_ystep DD	01H DUP (?)
_ds_source DD	01H DUP (?)
_ds_color DD	01H DUP (?)
?translationtables@@3PAEA DD 01H DUP (?)		; translationtables
?dc_translation@@3PAEA DD 01H DUP (?)			; dc_translation
_BSS	ENDS
_DATA	SEGMENT
_dc_pitch DD	012345678H
_DATA	ENDS
CONST	SEGMENT
_fuzzinit DB	01H
	DB	0ffH
	DB	01H
	DB	0ffH
	DB	01H
	DB	01H
	DB	0ffH
	DB	01H
	DB	01H
	DB	0ffH
	DB	01H
	DB	01H
	DB	01H
	DB	0ffH
	DB	01H
	DB	01H
	DB	01H
	DB	0ffH
	DB	0ffH
	DB	0ffH
	DB	0ffH
	DB	01H
	DB	0ffH
	DB	0ffH
	DB	01H
	DB	01H
	DB	01H
	DB	01H
	DB	0ffH
	DB	01H
	DB	0ffH
	DB	01H
	DB	01H
	DB	0ffH
	DB	0ffH
	DB	01H
	DB	01H
	DB	0ffH
	DB	0ffH
	DB	0ffH
	DB	0ffH
	DB	01H
	DB	01H
	DB	01H
	DB	01H
	DB	0ffH
	DB	01H
	DB	01H
	DB	0ffH
	DB	01H
CONST	ENDS
_DATA	SEGMENT
?dovline1@@3P6AKXZA DD FLAT:_vlinetallasm1		; dovline1
?doprevline1@@3P6AKXZA DD FLAT:_vlinetallasm1		; doprevline1
?dovline4@@3P6AXXZA DD FLAT:_vlinetallasm4		; dovline4
_DATA	ENDS
CRT$XCU	SEGMENT
_$S56	DD	FLAT:_$E55
_$S61	DD	FLAT:_$E60
CRT$XCU	ENDS
PUBLIC	?R_StretchColumnP_C@@YAXXZ			; R_StretchColumnP_C
;	COMDAT ?R_StretchColumnP_C@@YAXXZ
_TEXT	SEGMENT
?R_StretchColumnP_C@@YAXXZ PROC NEAR			; R_StretchColumnP_C, COMDAT

; 205  : 	int 				count;
; 206  : 	byte*				dest;
; 207  : 	fixed_t 			frac;
; 208  : 	fixed_t 			fracstep;
; 209  : 
; 210  : 	count = dc_yh - dc_yl;

	mov	eax, DWORD PTR _dc_yl
	push	esi
	mov	esi, DWORD PTR _dc_yh
	sub	esi, eax

; 211  : 
; 212  : 	if (count < 0)

	js	SHORT $L9975

; 213  : 		return;
; 214  : 
; 215  : 	count++;
; 216  : 
; 217  : #ifdef RANGECHECK 
; 218  : 	if (dc_x >= screen->width
; 219  : 		|| dc_yl < 0
; 220  : 		|| dc_yh >= screen->height)
; 221  : 	{
; 222  : 		Printf ("R_StretchColumnP_C: %i to %i at %i\n", dc_yl, dc_yh, dc_x);
; 223  : 		return;
; 224  : 	}
; 225  : #endif
; 226  : 
; 227  : 	dest = ylookup[dc_yl] + dc_x;

	mov	eax, DWORD PTR _ylookup[eax*4]
	mov	ecx, DWORD PTR _dc_x

; 228  : 	fracstep = dc_iscale; 
; 229  : 	frac = dc_texturefrac;
; 230  : 
; 231  : 	{
; 232  : 		byte *source = dc_source;
; 233  : 		int pitch = dc_pitch;

	mov	edx, DWORD PTR _dc_pitch
	push	ebx
	push	ebp
	mov	ebp, DWORD PTR _dc_source
	inc	esi
	push	edi
	mov	edi, DWORD PTR _dc_iscale
	add	eax, ecx
	mov	ecx, DWORD PTR _dc_texturefrac
$L9983:

; 234  : 
; 235  : 		do
; 236  : 		{
; 237  : 			*dest = source[frac>>FRACBITS];

	mov	ebx, ecx

; 238  : 			dest += pitch;
; 239  : 			frac += fracstep;

	add	ecx, edi
	sar	ebx, 16					; 00000010H
	mov	bl, BYTE PTR [ebx+ebp]
	mov	BYTE PTR [eax], bl
	add	eax, edx

; 240  : 		} while (--count);

	dec	esi
	jne	SHORT $L9983
	pop	edi
	pop	ebp
	pop	ebx
$L9975:
	pop	esi

; 241  : 	}
; 242  : } 

	ret	0
?R_StretchColumnP_C@@YAXXZ ENDP				; R_StretchColumnP_C
_TEXT	ENDS
PUBLIC	?R_FillColumnP@@YAXXZ				; R_FillColumnP
;	COMDAT ?R_FillColumnP@@YAXXZ
_TEXT	SEGMENT
?R_FillColumnP@@YAXXZ PROC NEAR				; R_FillColumnP, COMDAT

; 247  : 	int 				count;
; 248  : 	byte*				dest;
; 249  : 
; 250  : 	count = dc_yh - dc_yl;

	mov	ecx, DWORD PTR _dc_yh
	mov	eax, DWORD PTR _dc_yl
	sub	ecx, eax

; 251  : 
; 252  : 	if (count < 0)

	js	SHORT $L9988

; 253  : 		return;
; 254  : 
; 255  : 	count++;
; 256  : 
; 257  : #ifdef RANGECHECK 
; 258  : 	if (dc_x >= screen->width
; 259  : 		|| dc_yl < 0
; 260  : 		|| dc_yh >= screen->height)
; 261  : 	{
; 262  : 		Printf ("R_StretchColumnP_C: %i to %i at %i\n", dc_yl, dc_yh, dc_x);
; 263  : 		return;
; 264  : 	}
; 265  : #endif
; 266  : 
; 267  : 	dest = ylookup[dc_yl] + dc_x;

	mov	eax, DWORD PTR _ylookup[eax*4]
	mov	edx, DWORD PTR _dc_x
	inc	ecx
	push	esi

; 268  : 
; 269  : 	{
; 270  : 		int pitch = dc_pitch;

	mov	esi, DWORD PTR _dc_pitch
	add	eax, edx

; 271  : 		byte color = dc_color;

	mov	dl, BYTE PTR _dc_color
$L9994:

; 272  : 
; 273  : 		do
; 274  : 		{
; 275  : 			*dest = color;

	mov	BYTE PTR [eax], dl

; 276  : 			dest += pitch;

	add	eax, esi

; 277  : 		} while (--count);

	dec	ecx
	jne	SHORT $L9994
	pop	esi
$L9988:

; 278  : 	}
; 279  : } 

	ret	0
?R_FillColumnP@@YAXXZ ENDP				; R_FillColumnP
_TEXT	ENDS
PUBLIC	?R_InitFuzzTable@@YAXH@Z			; R_InitFuzzTable
;	COMDAT ?R_InitFuzzTable@@YAXH@Z
_TEXT	SEGMENT
_fuzzoff$ = 8
?R_InitFuzzTable@@YAXH@Z PROC NEAR			; R_InitFuzzTable, COMDAT

; 314  : 	int i;
; 315  : 
; 316  : 	for (i = 0; i < FUZZTABLE; i++)

	mov	edx, DWORD PTR _fuzzoff$[esp-4]
	push	esi
	xor	ecx, ecx
	mov	eax, OFFSET FLAT:_fuzzoffset
$L10004:

; 317  : 	{
; 318  : 		fuzzoffset[i] = fuzzinit[i] * fuzzoff;

	movsx	esi, BYTE PTR _fuzzinit[ecx]
	imul	esi, edx
	mov	DWORD PTR [eax], esi
	add	eax, 4
	inc	ecx
	cmp	eax, OFFSET FLAT:_fuzzoffset+200
	jl	SHORT $L10004
	pop	esi

; 319  : 	}
; 320  : }

	ret	0
?R_InitFuzzTable@@YAXH@Z ENDP				; R_InitFuzzTable
_TEXT	ENDS
PUBLIC	?R_DrawAddColumnP_C@@YAXXZ			; R_DrawAddColumnP_C
EXTRN	_RGB32k:BYTE
;	COMDAT ?R_DrawAddColumnP_C@@YAXXZ
_TEXT	SEGMENT
_fg2rgb$10015 = -12
_bg2rgb$10016 = -8
_colormap$10017 = -16
_source$10018 = -20
_pitch$10019 = -4
?R_DrawAddColumnP_C@@YAXXZ PROC NEAR			; R_DrawAddColumnP_C, COMDAT

; 458  : 	int count;
; 459  : 	byte *dest;
; 460  : 	fixed_t frac;
; 461  : 	fixed_t fracstep;
; 462  : 
; 463  : 	count = dc_yh - dc_yl;

	mov	eax, DWORD PTR _dc_yl
	sub	esp, 20					; 00000014H
	push	esi
	mov	esi, DWORD PTR _dc_yh
	sub	esi, eax

; 464  : 	if (count < 0)

	js	$L10009

; 465  : 		return;
; 466  : 	count++;
; 467  : 
; 468  : #ifdef RANGECHECK
; 469  : 	if (dc_x >= screen->width
; 470  : 		|| dc_yl < 0
; 471  : 		|| dc_yh >= screen->height)
; 472  : 	{
; 473  : 		I_Error ( "R_DrawTranslucentColumnP_C: %i to %i at %i",
; 474  : 				  dc_yl, dc_yh, dc_x);
; 475  : 	}
; 476  : #endif 
; 477  : 
; 478  : 	dest = ylookup[dc_yl] + dc_x;

	mov	ecx, DWORD PTR _ylookup[eax*4]
	mov	eax, DWORD PTR _dc_x
	inc	esi
	add	ecx, eax

; 479  : 
; 480  : 	fracstep = dc_iscale;
; 481  : 	frac = dc_texturefrac;
; 482  : 
; 483  : 	{
; 484  : 		DWORD *fg2rgb = dc_srcblend;

	mov	eax, DWORD PTR _dc_srcblend
	mov	edx, DWORD PTR _dc_texturefrac
	mov	DWORD PTR _fg2rgb$10015[esp+24], eax

; 485  : 		DWORD *bg2rgb = dc_destblend;

	mov	eax, DWORD PTR _dc_destblend
	mov	DWORD PTR _bg2rgb$10016[esp+24], eax

; 486  : 		byte *colormap = dc_colormap;

	mov	eax, DWORD PTR _dc_colormap
	mov	DWORD PTR _colormap$10017[esp+24], eax

; 487  : 		byte *source = dc_source;

	mov	eax, DWORD PTR _dc_source
	push	ebx
	mov	DWORD PTR _source$10018[esp+28], eax

; 488  : 		int pitch = dc_pitch;

	mov	eax, DWORD PTR _dc_pitch
	push	ebp
	push	edi
	mov	edi, DWORD PTR _dc_iscale
	mov	DWORD PTR _pitch$10019[esp+36], eax
$L10020:

; 489  : 
; 490  : 		do
; 491  : 		{
; 492  : 			DWORD fg = colormap[source[frac>>FRACBITS]];
; 493  : 			DWORD bg = *dest;
; 494  : 
; 495  : 			fg = fg2rgb[fg];
; 496  : 			bg = bg2rgb[bg];
; 497  : 			fg = (fg+bg) | 0x1f07c1f;

	mov	ebp, DWORD PTR _source$10018[esp+36]
	mov	eax, edx
	sar	eax, 16					; 00000010H
	xor	ebx, ebx

; 498  : 			*dest = RGB32k[0][0][fg & (fg>>15)];
; 499  : 			dest += pitch;
; 500  : 			frac += fracstep;

	add	edx, edi
	mov	bl, BYTE PTR [eax+ebp]
	mov	ebp, DWORD PTR _colormap$10017[esp+36]
	xor	eax, eax
	mov	al, BYTE PTR [ebx+ebp]
	mov	ebp, DWORD PTR _fg2rgb$10015[esp+36]
	xor	ebx, ebx
	mov	eax, DWORD PTR [ebp+eax*4]
	mov	bl, BYTE PTR [ecx]
	mov	ebp, DWORD PTR _bg2rgb$10016[esp+36]
	add	eax, DWORD PTR [ebp+ebx*4]
	mov	ebp, DWORD PTR _pitch$10019[esp+36]
	or	eax, 32537631				; 01f07c1fH
	mov	ebx, eax
	shr	ebx, 15					; 0000000fH
	and	ebx, eax
	mov	al, BYTE PTR _RGB32k[ebx]
	mov	BYTE PTR [ecx], al
	add	ecx, ebp

; 501  : 		} while (--count);

	dec	esi
	jne	SHORT $L10020
	pop	edi
	pop	ebp
	pop	ebx
$L10009:
	pop	esi

; 502  : 	}
; 503  : }

	add	esp, 20					; 00000014H
	ret	0
?R_DrawAddColumnP_C@@YAXXZ ENDP				; R_DrawAddColumnP_C
_TEXT	ENDS
PUBLIC	?R_DrawTranslatedColumnP_C@@YAXXZ		; R_DrawTranslatedColumnP_C
;	COMDAT ?R_DrawTranslatedColumnP_C@@YAXXZ
_TEXT	SEGMENT
_colormap$10033 = -8
_translation$10034 = -12
_source$10035 = -16
_pitch$10036 = -4
?R_DrawTranslatedColumnP_C@@YAXXZ PROC NEAR		; R_DrawTranslatedColumnP_C, COMDAT

; 519  : 	int 				count;
; 520  : 	byte*				dest;
; 521  : 	fixed_t 			frac;
; 522  : 	fixed_t 			fracstep;
; 523  : 
; 524  : 	count = dc_yh - dc_yl;

	mov	eax, DWORD PTR _dc_yl
	sub	esp, 16					; 00000010H
	push	esi
	mov	esi, DWORD PTR _dc_yh
	sub	esi, eax

; 525  : 	if (count < 0) 

	js	SHORT $L10027

; 526  : 		return;
; 527  : 	count++;
; 528  : 
; 529  : #ifdef RANGECHECK 
; 530  : 	if (dc_x >= screen->width
; 531  : 		|| dc_yl < 0
; 532  : 		|| dc_yh >= screen->height)
; 533  : 	{
; 534  : 		I_Error ( "R_DrawTranslatedColumnP_C: %i to %i at %i",
; 535  : 				  dc_yl, dc_yh, dc_x);
; 536  : 	}
; 537  : 	
; 538  : #endif 
; 539  : 
; 540  : 	dest = ylookup[dc_yl] + dc_x;
; 541  : 
; 542  : 	fracstep = dc_iscale;
; 543  : 	frac = dc_texturefrac;
; 544  : 
; 545  : 	{
; 546  : 		// [RH] Local copies of global vars to improve compiler optimizations
; 547  : 		byte *colormap = dc_colormap;

	mov	edx, DWORD PTR _dc_colormap
	mov	eax, DWORD PTR _ylookup[eax*4]
	mov	ecx, DWORD PTR _dc_x
	mov	DWORD PTR _colormap$10033[esp+20], edx

; 548  : 		byte *translation = dc_translation;

	mov	edx, DWORD PTR ?dc_translation@@3PAEA	; dc_translation
	push	ebx
	mov	DWORD PTR _translation$10034[esp+24], edx

; 549  : 		byte *source = dc_source;

	mov	edx, DWORD PTR _dc_source
	mov	DWORD PTR _source$10035[esp+24], edx

; 550  : 		int pitch = dc_pitch;

	mov	edx, DWORD PTR _dc_pitch
	push	ebp
	inc	esi
	push	edi
	mov	edi, DWORD PTR _dc_iscale
	add	eax, ecx
	mov	ecx, DWORD PTR _dc_texturefrac
	mov	DWORD PTR _pitch$10036[esp+32], edx
$L10037:

; 551  : 
; 552  : 		do
; 553  : 		{
; 554  : 			*dest = colormap[translation[source[frac>>FRACBITS]]];

	mov	ebp, DWORD PTR _source$10035[esp+32]
	mov	edx, ecx
	sar	edx, 16					; 00000010H
	xor	ebx, ebx

; 555  : 			dest += pitch;
; 556  : 
; 557  : 			frac += fracstep;

	add	ecx, edi
	mov	bl, BYTE PTR [edx+ebp]
	mov	ebp, DWORD PTR _translation$10034[esp+32]
	xor	edx, edx
	mov	dl, BYTE PTR [ebx+ebp]
	mov	ebx, DWORD PTR _colormap$10033[esp+32]
	mov	ebp, DWORD PTR _pitch$10036[esp+32]
	mov	dl, BYTE PTR [edx+ebx]
	mov	BYTE PTR [eax], dl
	add	eax, ebp

; 558  : 		} while (--count);

	dec	esi
	jne	SHORT $L10037
	pop	edi
	pop	ebp
	pop	ebx
$L10027:
	pop	esi

; 559  : 	}
; 560  : }

	add	esp, 16					; 00000010H
	ret	0
?R_DrawTranslatedColumnP_C@@YAXXZ ENDP			; R_DrawTranslatedColumnP_C
_TEXT	ENDS
PUBLIC	?R_DrawTlatedAddColumnP_C@@YAXXZ		; R_DrawTlatedAddColumnP_C
;	COMDAT ?R_DrawTlatedAddColumnP_C@@YAXXZ
_TEXT	SEGMENT
_fg2rgb$10048 = -12
_bg2rgb$10049 = -8
_translation$10050 = -20
_colormap$10051 = -16
_source$10052 = -24
_pitch$10053 = -4
?R_DrawTlatedAddColumnP_C@@YAXXZ PROC NEAR		; R_DrawTlatedAddColumnP_C, COMDAT

; 565  : 	int count;
; 566  : 	byte *dest;
; 567  : 	fixed_t frac;
; 568  : 	fixed_t fracstep;
; 569  : 
; 570  : 	count = dc_yh - dc_yl;

	mov	eax, DWORD PTR _dc_yl
	sub	esp, 24					; 00000018H
	push	esi
	mov	esi, DWORD PTR _dc_yh
	sub	esi, eax

; 571  : 	if (count < 0)

	js	$L10042

; 572  : 		return;
; 573  : 	count++;
; 574  : 
; 575  : #ifdef RANGECHECK
; 576  : 	if (dc_x >= screen->width
; 577  : 		|| dc_yl < 0
; 578  : 		|| dc_yh >= screen->height)
; 579  : 	{
; 580  : 		I_Error ( "R_DrawTlatedLucentColumnP_C: %i to %i at %i",
; 581  : 				  dc_yl, dc_yh, dc_x);
; 582  : 	}
; 583  : 	
; 584  : #endif 
; 585  : 
; 586  : 	dest = ylookup[dc_yl] + dc_x;

	mov	ecx, DWORD PTR _ylookup[eax*4]
	mov	eax, DWORD PTR _dc_x
	inc	esi
	add	ecx, eax

; 587  : 
; 588  : 	fracstep = dc_iscale;
; 589  : 	frac = dc_texturefrac;
; 590  : 
; 591  : 	{
; 592  : 		DWORD *fg2rgb = dc_srcblend;

	mov	eax, DWORD PTR _dc_srcblend
	mov	edx, DWORD PTR _dc_texturefrac
	mov	DWORD PTR _fg2rgb$10048[esp+28], eax

; 593  : 		DWORD *bg2rgb = dc_destblend;

	mov	eax, DWORD PTR _dc_destblend
	mov	DWORD PTR _bg2rgb$10049[esp+28], eax

; 594  : 		byte *translation = dc_translation;

	mov	eax, DWORD PTR ?dc_translation@@3PAEA	; dc_translation
	mov	DWORD PTR _translation$10050[esp+28], eax

; 595  : 		byte *colormap = dc_colormap;

	mov	eax, DWORD PTR _dc_colormap
	mov	DWORD PTR _colormap$10051[esp+28], eax

; 596  : 		byte *source = dc_source;

	mov	eax, DWORD PTR _dc_source
	push	ebx
	mov	DWORD PTR _source$10052[esp+32], eax

; 597  : 		int pitch = dc_pitch;

	mov	eax, DWORD PTR _dc_pitch
	push	ebp
	push	edi
	mov	edi, DWORD PTR _dc_iscale
	mov	DWORD PTR _pitch$10053[esp+40], eax
$L10054:

; 598  : 
; 599  : 		do
; 600  : 		{
; 601  : 			DWORD fg = colormap[translation[source[frac>>FRACBITS]]];
; 602  : 			DWORD bg = *dest;
; 603  : 
; 604  : 			fg = fg2rgb[fg];
; 605  : 			bg = bg2rgb[bg];
; 606  : 			fg = (fg+bg) | 0x1f07c1f;

	mov	ebp, DWORD PTR _source$10052[esp+40]
	mov	eax, edx
	sar	eax, 16					; 00000010H
	xor	ebx, ebx

; 607  : 			*dest = RGB32k[0][0][fg & (fg>>15)];
; 608  : 			dest += pitch;
; 609  : 			frac += fracstep;

	add	edx, edi
	mov	bl, BYTE PTR [eax+ebp]
	mov	ebp, DWORD PTR _translation$10050[esp+40]
	xor	eax, eax
	mov	al, BYTE PTR [ebx+ebp]
	mov	ebp, DWORD PTR _colormap$10051[esp+40]
	xor	ebx, ebx
	mov	bl, BYTE PTR [eax+ebp]
	xor	eax, eax
	mov	al, BYTE PTR [ecx]
	mov	ebp, eax
	mov	eax, DWORD PTR _fg2rgb$10048[esp+40]
	mov	eax, DWORD PTR [eax+ebx*4]
	mov	ebx, DWORD PTR _bg2rgb$10049[esp+40]
	add	eax, DWORD PTR [ebx+ebp*4]
	mov	ebp, DWORD PTR _pitch$10053[esp+40]
	or	eax, 32537631				; 01f07c1fH
	mov	ebx, eax
	shr	ebx, 15					; 0000000fH
	and	ebx, eax
	mov	al, BYTE PTR _RGB32k[ebx]
	mov	BYTE PTR [ecx], al
	add	ecx, ebp

; 610  : 		} while (--count);

	dec	esi
	jne	SHORT $L10054
	pop	edi
	pop	ebp
	pop	ebx
$L10042:
	pop	esi

; 611  : 	}
; 612  : }

	add	esp, 24					; 00000018H
	ret	0
?R_DrawTlatedAddColumnP_C@@YAXXZ ENDP			; R_DrawTlatedAddColumnP_C
_TEXT	ENDS
PUBLIC	?R_DrawShadedColumnP_C@@YAXXZ			; R_DrawShadedColumnP_C
EXTRN	_Col2RGB8:BYTE
;	COMDAT ?R_DrawShadedColumnP_C@@YAXXZ
_TEXT	SEGMENT
_source$10067 = -16
_colormap$10068 = -12
_pitch$10069 = -4
_fgstart$10070 = -8
?R_DrawShadedColumnP_C@@YAXXZ PROC NEAR			; R_DrawShadedColumnP_C, COMDAT

; 618  : 	int  count;
; 619  : 	byte *dest;
; 620  : 	fixed_t frac, fracstep;
; 621  : 
; 622  : 	count = dc_yh - dc_yl;

	mov	eax, DWORD PTR _dc_yl
	sub	esp, 16					; 00000010H
	push	edi
	mov	edi, DWORD PTR _dc_yh
	sub	edi, eax

; 623  : 
; 624  : 	if (count < 0)

	js	$L10061

; 625  : 		return;
; 626  : 
; 627  : 	count++;
; 628  : 
; 629  : #ifdef RANGECHECK 
; 630  : 	if (dc_x >= screen->width
; 631  : 		|| dc_yl < 0
; 632  : 		|| dc_yh >= screen->height) {
; 633  : 		Printf ("R_DrawShadedColumnP_C: %i to %i at %i\n", dc_yl, dc_yh, dc_x);
; 634  : 		return;
; 635  : 	}
; 636  : #endif
; 637  : 
; 638  : 	dest = ylookup[dc_yl] + dc_x;

	mov	edx, DWORD PTR _ylookup[eax*4]
	mov	eax, DWORD PTR _dc_x

; 639  : 
; 640  : 	fracstep = dc_iscale; 
; 641  : 	frac = dc_texturefrac;
; 642  : 
; 643  : 	{
; 644  : 		byte *source = dc_source;
; 645  : 		byte *colormap = dc_colormap;

	mov	ecx, DWORD PTR _dc_colormap
	inc	edi
	add	edx, eax
	mov	eax, DWORD PTR _dc_source
	mov	DWORD PTR _source$10067[esp+20], eax

; 646  : 		int pitch = dc_pitch;

	mov	eax, DWORD PTR _dc_pitch
	mov	DWORD PTR _colormap$10068[esp+20], ecx

; 647  : 		DWORD *fgstart = &Col2RGB8[0][dc_color];

	mov	ecx, DWORD PTR _dc_color
	push	ebx
	mov	DWORD PTR _pitch$10069[esp+24], eax
	push	ebp
	mov	ebp, DWORD PTR _dc_iscale
	lea	eax, DWORD PTR _Col2RGB8[ecx*4]
	push	esi
	mov	esi, DWORD PTR _dc_texturefrac
	mov	DWORD PTR _fgstart$10070[esp+32], eax
$L10071:

; 648  : 
; 649  : 		do
; 650  : 		{
; 651  : 			DWORD val = colormap[source[frac>>FRACBITS]];

	mov	ebx, DWORD PTR _source$10067[esp+32]
	mov	ecx, esi
	sar	ecx, 16					; 00000010H
	xor	eax, eax

; 652  : 			DWORD fg = fgstart[val<<8];
; 653  : 			val = (Col2RGB8[64-val][*dest] + fg) | 0x1f07c1f;
; 654  : 			*dest = RGB32k[0][0][val & (val>>15)];
; 655  : 
; 656  : 			dest += pitch;
; 657  : 			frac += fracstep;

	add	esi, ebp
	mov	al, BYTE PTR [ecx+ebx]
	mov	ebx, DWORD PTR _colormap$10068[esp+32]
	xor	ecx, ecx
	mov	cl, BYTE PTR [eax+ebx]
	xor	ebx, ebx
	mov	bl, BYTE PTR [edx]
	mov	eax, ecx
	shl	eax, 8
	sub	ebx, eax
	shl	ecx, 10					; 0000000aH
	mov	eax, DWORD PTR _Col2RGB8[ebx*4+65536]
	mov	ebx, DWORD PTR _fgstart$10070[esp+32]
	add	eax, DWORD PTR [ecx+ebx]
	mov	ebx, DWORD PTR _pitch$10069[esp+32]
	or	eax, 32537631				; 01f07c1fH
	mov	ecx, eax
	shr	ecx, 15					; 0000000fH
	and	ecx, eax
	mov	al, BYTE PTR _RGB32k[ecx]
	mov	BYTE PTR [edx], al
	add	edx, ebx

; 658  : 		} while (--count);

	dec	edi
	jne	SHORT $L10071
	pop	esi
	pop	ebp
	pop	ebx
$L10061:
	pop	edi

; 659  : 	}
; 660  : } 

	add	esp, 16					; 00000010H
	ret	0
?R_DrawShadedColumnP_C@@YAXXZ ENDP			; R_DrawShadedColumnP_C
_TEXT	ENDS
PUBLIC	?R_DrawAddClampColumnP_C@@YAXXZ			; R_DrawAddClampColumnP_C
;	COMDAT ?R_DrawAddClampColumnP_C@@YAXXZ
_TEXT	SEGMENT
_colormap$10083 = -16
_source$10084 = -20
_pitch$10085 = -4
_fg2rgb$10086 = -12
_bg2rgb$10087 = -8
?R_DrawAddClampColumnP_C@@YAXXZ PROC NEAR		; R_DrawAddClampColumnP_C, COMDAT

; 665  : 	int count;
; 666  : 	byte *dest;
; 667  : 	fixed_t frac;
; 668  : 	fixed_t fracstep;
; 669  : 
; 670  : 	count = dc_yh - dc_yl;

	mov	eax, DWORD PTR _dc_yl
	sub	esp, 20					; 00000014H
	push	edi
	mov	edi, DWORD PTR _dc_yh
	sub	edi, eax

; 671  : 	if (count < 0)

	js	$L10077

; 672  : 		return;
; 673  : 	count++;
; 674  : 
; 675  : #ifdef RANGECHECK
; 676  : 	if (dc_x >= screen->width
; 677  : 		|| dc_yl < 0
; 678  : 		|| dc_yh >= screen->height)
; 679  : 	{
; 680  : 		I_Error ( "R_DrawAddColumnP_C: %i to %i at %i",
; 681  : 				  dc_yl, dc_yh, dc_x);
; 682  : 	}
; 683  : #endif 
; 684  : 
; 685  : 	dest = ylookup[dc_yl] + dc_x;

	mov	edx, DWORD PTR _ylookup[eax*4]
	mov	eax, DWORD PTR _dc_x

; 686  : 
; 687  : 	fracstep = dc_iscale;
; 688  : 	frac = dc_texturefrac;
; 689  : 
; 690  : 	{
; 691  : 		byte *colormap = dc_colormap;
; 692  : 		byte *source = dc_source;

	mov	ecx, DWORD PTR _dc_source
	inc	edi
	add	edx, eax
	mov	eax, DWORD PTR _dc_colormap
	mov	DWORD PTR _colormap$10083[esp+24], eax

; 693  : 		int pitch = dc_pitch;

	mov	eax, DWORD PTR _dc_pitch
	push	ebx
	mov	ebx, DWORD PTR _dc_iscale
	mov	DWORD PTR _source$10084[esp+28], ecx

; 694  : 		DWORD *fg2rgb = dc_srcblend;

	mov	ecx, DWORD PTR _dc_srcblend
	mov	DWORD PTR _pitch$10085[esp+28], eax

; 695  : 		DWORD *bg2rgb = dc_destblend;

	mov	eax, DWORD PTR _dc_destblend
	push	ebp
	push	esi
	mov	esi, DWORD PTR _dc_texturefrac
	mov	DWORD PTR _fg2rgb$10086[esp+36], ecx
	mov	DWORD PTR _bg2rgb$10087[esp+36], eax
$L10088:

; 696  : 
; 697  : 		do
; 698  : 		{
; 699  : 			DWORD a = fg2rgb[colormap[source[frac>>FRACBITS]]]
; 700  : 				+ bg2rgb[*dest];

	mov	ebp, DWORD PTR _source$10084[esp+36]
	mov	ecx, esi
	sar	ecx, 16					; 00000010H
	xor	eax, eax

; 701  : 			DWORD b = a;
; 702  : 
; 703  : 			a |= 0x01f07c1f;
; 704  : 			b &= 0x40100400;
; 705  : 			a &= 0x3fffffff;
; 706  : 			b = b - (b >> 5);
; 707  : 			a |= b;
; 708  : 			*dest = RGB32k[0][0][a & (a>>15)];
; 709  : 			dest += pitch;
; 710  : 			frac += fracstep;

	add	esi, ebx
	mov	al, BYTE PTR [ecx+ebp]
	mov	ebp, DWORD PTR _colormap$10083[esp+36]
	xor	ecx, ecx
	mov	cl, BYTE PTR [eax+ebp]
	mov	ebp, DWORD PTR _fg2rgb$10086[esp+36]
	xor	eax, eax
	mov	ecx, DWORD PTR [ebp+ecx*4]
	mov	al, BYTE PTR [edx]
	mov	ebp, DWORD PTR _bg2rgb$10087[esp+36]
	add	ecx, DWORD PTR [ebp+eax*4]
	mov	eax, ecx

; 711  : 		} while (--count);

	and	ecx, 1041204192				; 3e0f83e0H
	and	eax, 1074791424				; 40100400H
	mov	ebp, eax
	shr	ebp, 5
	sub	eax, ebp
	mov	ebp, DWORD PTR _pitch$10085[esp+36]
	or	eax, ecx
	or	eax, 32537631				; 01f07c1fH
	mov	ecx, eax
	shr	ecx, 15					; 0000000fH
	and	ecx, eax
	mov	al, BYTE PTR _RGB32k[ecx]
	mov	BYTE PTR [edx], al
	add	edx, ebp
	dec	edi
	jne	SHORT $L10088
	pop	esi
	pop	ebp
	pop	ebx
$L10077:
	pop	edi

; 712  : 	}
; 713  : }

	add	esp, 20					; 00000014H
	ret	0
?R_DrawAddClampColumnP_C@@YAXXZ ENDP			; R_DrawAddClampColumnP_C
_TEXT	ENDS
PUBLIC	?R_DrawAddClampTranslatedColumnP_C@@YAXXZ	; R_DrawAddClampTranslatedColumnP_C
;	COMDAT ?R_DrawAddClampTranslatedColumnP_C@@YAXXZ
_TEXT	SEGMENT
_translation$10100 = -20
_colormap$10101 = -16
_source$10102 = -24
_pitch$10103 = -4
_fg2rgb$10104 = -12
_bg2rgb$10105 = -8
?R_DrawAddClampTranslatedColumnP_C@@YAXXZ PROC NEAR	; R_DrawAddClampTranslatedColumnP_C, COMDAT

; 718  : 	int count;
; 719  : 	byte *dest;
; 720  : 	fixed_t frac;
; 721  : 	fixed_t fracstep;
; 722  : 
; 723  : 	count = dc_yh - dc_yl;

	mov	eax, DWORD PTR _dc_yl
	sub	esp, 24					; 00000018H
	push	edi
	mov	edi, DWORD PTR _dc_yh
	sub	edi, eax

; 724  : 	if (count < 0)

	js	$L10094

; 725  : 		return;
; 726  : 	count++;
; 727  : 
; 728  : #ifdef RANGECHECK
; 729  : 	if (dc_x >= screen->width
; 730  : 		|| dc_yl < 0
; 731  : 		|| dc_yh >= screen->height)
; 732  : 	{
; 733  : 		I_Error ( "R_DrawAddColumnP_C: %i to %i at %i",
; 734  : 				  dc_yl, dc_yh, dc_x);
; 735  : 	}
; 736  : #endif 
; 737  : 
; 738  : 	dest = ylookup[dc_yl] + dc_x;

	mov	edx, DWORD PTR _ylookup[eax*4]
	mov	eax, DWORD PTR _dc_x

; 739  : 
; 740  : 	fracstep = dc_iscale;
; 741  : 	frac = dc_texturefrac;
; 742  : 
; 743  : 	{
; 744  : 		byte *translation = dc_translation;
; 745  : 		byte *colormap = dc_colormap;

	mov	ecx, DWORD PTR _dc_colormap
	inc	edi
	add	edx, eax
	mov	eax, DWORD PTR ?dc_translation@@3PAEA	; dc_translation
	mov	DWORD PTR _translation$10100[esp+28], eax

; 746  : 		byte *source = dc_source;

	mov	eax, DWORD PTR _dc_source
	mov	DWORD PTR _colormap$10101[esp+28], ecx

; 747  : 		int pitch = dc_pitch;

	mov	ecx, DWORD PTR _dc_pitch
	push	ebx
	mov	ebx, DWORD PTR _dc_iscale
	mov	DWORD PTR _source$10102[esp+32], eax

; 748  : 		DWORD *fg2rgb = dc_srcblend;

	mov	eax, DWORD PTR _dc_srcblend
	mov	DWORD PTR _pitch$10103[esp+32], ecx

; 749  : 		DWORD *bg2rgb = dc_destblend;

	mov	ecx, DWORD PTR _dc_destblend
	push	ebp
	push	esi
	mov	esi, DWORD PTR _dc_texturefrac
	mov	DWORD PTR _fg2rgb$10104[esp+40], eax
	mov	DWORD PTR _bg2rgb$10105[esp+40], ecx
$L10106:

; 750  : 
; 751  : 		do
; 752  : 		{
; 753  : 			DWORD a = fg2rgb[colormap[translation[source[frac>>FRACBITS]]]]
; 754  : 				+ bg2rgb[*dest];

	mov	ebp, DWORD PTR _source$10102[esp+40]
	mov	eax, esi
	sar	eax, 16					; 00000010H
	xor	ecx, ecx

; 755  : 			DWORD b = a;
; 756  : 
; 757  : 			a |= 0x01f07c1f;
; 758  : 			b &= 0x40100400;
; 759  : 			a &= 0x3fffffff;
; 760  : 			b = b - (b >> 5);
; 761  : 			a |= b;
; 762  : 			*dest = RGB32k[0][0][(a>>15) & a];
; 763  : 			dest += pitch;
; 764  : 			frac += fracstep;

	add	esi, ebx
	mov	cl, BYTE PTR [eax+ebp]
	mov	ebp, DWORD PTR _translation$10100[esp+40]
	xor	eax, eax
	mov	al, BYTE PTR [ecx+ebp]
	mov	ebp, DWORD PTR _colormap$10101[esp+40]
	xor	ecx, ecx
	mov	cl, BYTE PTR [eax+ebp]
	mov	ebp, DWORD PTR _fg2rgb$10104[esp+40]
	xor	eax, eax
	mov	ecx, DWORD PTR [ebp+ecx*4]
	mov	al, BYTE PTR [edx]
	mov	ebp, DWORD PTR _bg2rgb$10105[esp+40]
	add	ecx, DWORD PTR [ebp+eax*4]
	mov	eax, ecx

; 765  : 		} while (--count);

	and	ecx, 1041204192				; 3e0f83e0H
	and	eax, 1074791424				; 40100400H
	mov	ebp, eax
	shr	ebp, 5
	sub	eax, ebp
	mov	ebp, DWORD PTR _pitch$10103[esp+40]
	or	eax, ecx
	or	eax, 32537631				; 01f07c1fH
	mov	ecx, eax
	shr	ecx, 15					; 0000000fH
	and	ecx, eax
	mov	al, BYTE PTR _RGB32k[ecx]
	mov	BYTE PTR [edx], al
	add	edx, ebp
	dec	edi
	jne	SHORT $L10106
	pop	esi
	pop	ebp
	pop	ebx
$L10094:
	pop	edi

; 766  : 	}
; 767  : }

	add	esp, 24					; 00000018H
	ret	0
?R_DrawAddClampTranslatedColumnP_C@@YAXXZ ENDP		; R_DrawAddClampTranslatedColumnP_C
_TEXT	ENDS
PUBLIC	?R_FillSpan@@YAXXZ				; R_FillSpan
;	COMDAT ?R_FillSpan@@YAXXZ
_TEXT	SEGMENT
?R_FillSpan@@YAXXZ PROC NEAR				; R_FillSpan, COMDAT

; 858  : #ifdef RANGECHECK
; 859  : 	if (ds_x2 < ds_x1
; 860  : 		|| ds_x1<0
; 861  : 		|| ds_x2>=screen->width
; 862  : 		|| ds_y>screen->height)
; 863  : 	{
; 864  : 		I_Error( "R_FillSpan: %i to %i at %i",
; 865  : 				 ds_x1,ds_x2,ds_y);
; 866  : 	}
; 867  : //		dscount++;
; 868  : #endif
; 869  : 
; 870  : 	memset (ylookup[ds_y] + ds_x1, ds_color, ds_x2 - ds_x1 + 1);

	mov	al, BYTE PTR _ds_color
	mov	ecx, DWORD PTR _ds_x2
	mov	edx, DWORD PTR _ds_x1
	push	ebx
	push	esi
	mov	esi, DWORD PTR _ds_y
	mov	bl, al
	push	edi
	mov	edi, DWORD PTR _ylookup[esi*4]
	sub	ecx, edx
	mov	bh, bl
	inc	ecx
	mov	eax, ebx
	add	edi, edx
	mov	edx, ecx
	shl	eax, 16					; 00000010H
	mov	ax, bx
	shr	ecx, 2
	rep stosd
	mov	ecx, edx
	and	ecx, 3
	rep stosb
	pop	edi
	pop	esi
	pop	ebx

; 871  : }

	ret	0
?R_FillSpan@@YAXXZ ENDP					; R_FillSpan
_TEXT	ENDS
PUBLIC	?setupvline@@YAXH@Z				; setupvline
EXTRN	_CPUFamily:BYTE
EXTRN	_vlineasm1:NEAR
EXTRN	_prevlineasm1:NEAR
EXTRN	_vlineasm4:NEAR
EXTRN	_setupvlineasm:NEAR
EXTRN	_setupvlinetallasm:NEAR
;	COMDAT ?setupvline@@YAXH@Z
_TEXT	SEGMENT
_fracbits$ = 8
?setupvline@@YAXH@Z PROC NEAR				; setupvline, COMDAT

; 907  : #ifdef USEASM
; 908  : 	if (CPUFamily <= 5)

	mov	al, BYTE PTR _CPUFamily
	cmp	al, 5

; 909  : 	{
; 910  : 		if (fracbits >= 24)

	mov	eax, DWORD PTR _fracbits$[esp-4]
	ja	SHORT $L10127
	cmp	eax, 24					; 00000018H

; 911  : 		{
; 912  : 			setupvlineasm (fracbits);

	push	eax
	jl	SHORT $L10128
	call	_setupvlineasm
	add	esp, 4

; 913  : 			dovline4 = vlineasm4;

	mov	DWORD PTR ?dovline4@@3P6AXXZA, OFFSET FLAT:_vlineasm4 ; dovline4

; 914  : 			dovline1 = vlineasm1;

	mov	DWORD PTR ?dovline1@@3P6AKXZA, OFFSET FLAT:_vlineasm1 ; dovline1

; 915  : 			doprevline1 = prevlineasm1;

	mov	DWORD PTR ?doprevline1@@3P6AKXZA, OFFSET FLAT:_prevlineasm1 ; doprevline1

; 927  : 	}
; 928  : #else
; 929  : 	vlinebits = fracbits;
; 930  : #endif
; 931  : }

	ret	0
$L10128:

; 916  : 		}
; 917  : 		else
; 918  : 		{
; 919  : 			setupvlinetallasm (fracbits);

	call	_setupvlinetallasm

; 920  : 			dovline1 = doprevline1 = vlinetallasm1;

	mov	eax, OFFSET FLAT:_vlinetallasm1
	add	esp, 4
	mov	DWORD PTR ?doprevline1@@3P6AKXZA, eax	; doprevline1
	mov	DWORD PTR ?dovline1@@3P6AKXZA, eax	; dovline1

; 921  : 			dovline4 = vlinetallasm4;

	mov	DWORD PTR ?dovline4@@3P6AXXZA, OFFSET FLAT:_vlinetallasm4 ; dovline4

; 927  : 	}
; 928  : #else
; 929  : 	vlinebits = fracbits;
; 930  : #endif
; 931  : }

	ret	0
$L10127:

; 922  : 		}
; 923  : 	}
; 924  : 	else
; 925  : 	{
; 926  : 		setupvlinetallasm (fracbits);

	push	eax
	call	_setupvlinetallasm
	pop	ecx

; 927  : 	}
; 928  : #else
; 929  : 	vlinebits = fracbits;
; 930  : #endif
; 931  : }

	ret	0
?setupvline@@YAXH@Z ENDP				; setupvline
_TEXT	ENDS
PUBLIC	?R_InitTranslationTables@@YAXXZ			; R_InitTranslationTables
EXTRN	?Z_Malloc@@YAPAXIHPAX@Z:NEAR			; Z_Malloc
EXTRN	?gameinfo@@3Ugameinfo_t@@A:BYTE			; gameinfo
;	COMDAT ?R_InitTranslationTables@@YAXXZ
_TEXT	SEGMENT
?R_InitTranslationTables@@YAXXZ PROC NEAR		; R_InitTranslationTables, COMDAT

; 984  : {

	push	ecx

; 985  : 	int i, j;
; 986  : 		
; 987  : 	translationtables = (byte *)Z_Malloc (256*(2*MAXPLAYERS+3+
; 988  : 		NUMCOLORMAPS*16)+255, PU_STATIC, NULL);

	push	0
	push	1
	push	136191					; 000213ffH
	call	?Z_Malloc@@YAPAXIHPAX@Z			; Z_Malloc

; 989  : 	translationtables = (byte *)(((ptrdiff_t)translationtables + 255) & ~255);

	add	eax, 255				; 000000ffH
	add	esp, 12					; 0000000cH
	and	al, 0

; 990  : 	
; 991  : 	// [RH] Each player now gets their own translation table. These are set
; 992  : 	//		up during netgame arbitration and as-needed rather than in here.
; 993  : 
; 994  : 	for (i = 0; i < 256; i++)

	xor	ecx, ecx
	mov	DWORD PTR ?translationtables@@3PAEA, eax ; translationtables
	jmp	SHORT $L10138
$L10717:
	mov	eax, DWORD PTR ?translationtables@@3PAEA ; translationtables
$L10138:

; 995  : 		translationtables[i] = i;

	mov	BYTE PTR [eax+ecx], cl
	inc	ecx
	cmp	ecx, 256				; 00000100H
	jl	SHORT $L10717
	push	ebx
	push	ebp
	push	esi
	push	edi

; 996  : 
; 997  : 	for (i = 1; i < MAXPLAYERS*2+3; i++)

	mov	eax, 256				; 00000100H
$L10141:

; 998  : 		memcpy (translationtables + i*256, translationtables, 256);

	mov	esi, DWORD PTR ?translationtables@@3PAEA ; translationtables
	mov	ecx, 64					; 00000040H
	lea	edi, DWORD PTR [eax+esi]
	add	eax, 256				; 00000100H
	cmp	eax, 4864				; 00001300H
	rep movsd
	jl	SHORT $L10141

; 999  : 
; 1000 : 	// create translation tables for dehacked patches that expect them
; 1001 : 	if (gameinfo.gametype == GAME_Doom)

	mov	eax, DWORD PTR ?gameinfo@@3Ugameinfo_t@@A+144
	cmp	eax, 1
	jne	SHORT $L10144

; 1002 : 	{
; 1003 : 		for (i = 0x70; i < 0x80; i++)

	mov	ecx, 112				; 00000070H
$L10145:

; 1004 : 		{ // map green ramp to gray, brown, red
; 1005 : 			translationtables[i+(MAXPLAYERS*2+0)*256] = 0x60 + (i&0xf);

	mov	esi, DWORD PTR ?translationtables@@3PAEA ; translationtables
	mov	al, cl
	and	al, 15					; 0000000fH
	mov	dl, al
	add	dl, 96					; 00000060H
	mov	BYTE PTR [esi+ecx+4096], dl

; 1006 : 			translationtables[i+(MAXPLAYERS*2+1)*256] = 0x40 + (i&0xf);

	mov	esi, DWORD PTR ?translationtables@@3PAEA ; translationtables
	mov	dl, al

; 1007 : 			translationtables[i+(MAXPLAYERS*2+2)*256] = 0x20 + (i&0xf);

	add	al, 32					; 00000020H
	add	dl, 64					; 00000040H
	mov	BYTE PTR [esi+ecx+4352], dl
	mov	edx, DWORD PTR ?translationtables@@3PAEA ; translationtables
	mov	BYTE PTR [edx+ecx+4608], al
	inc	ecx
	cmp	ecx, 128				; 00000080H
	jl	SHORT $L10145

; 1008 : 		}
; 1009 : 	}
; 1010 : 	else if (gameinfo.gametype == GAME_Heretic)

	jmp	SHORT $L10152
$L10144:
	cmp	eax, 2
	jne	SHORT $L10152

; 1011 : 	{
; 1012 : 		for (i = 225; i <= 240; i++)

	mov	eax, 225				; 000000e1H
$L10150:

; 1013 : 		{
; 1014 : 			translationtables[i+(MAXPLAYERS*2+0)*256] = 114+(i-225); // yellow

	mov	edx, DWORD PTR ?translationtables@@3PAEA ; translationtables
	mov	cl, al
	sub	cl, 111					; 0000006fH
	mov	BYTE PTR [edx+eax+4096], cl

; 1015 : 			translationtables[i+(MAXPLAYERS*2+1)*256] = 145+(i-225); // red

	mov	edx, DWORD PTR ?translationtables@@3PAEA ; translationtables
	mov	cl, al
	sub	cl, 80					; 00000050H
	mov	BYTE PTR [edx+eax+4352], cl

; 1016 : 			translationtables[i+(MAXPLAYERS*2+2)*256] = 190+(i-225); // blue

	mov	edx, DWORD PTR ?translationtables@@3PAEA ; translationtables
	mov	cl, al
	sub	cl, 35					; 00000023H
	mov	BYTE PTR [edx+eax+4608], cl
	inc	eax
	cmp	eax, 240				; 000000f0H
	jle	SHORT $L10150
$L10152:

; 1017 : 		}
; 1018 : 	}
; 1019 : 
; 1020 : 	// set up shading tables for shaded columns
; 1021 : 	// 16 colormap sets, progressing from full alpha to minimum visible alpha
; 1022 : 
; 1023 : 	BYTE *table = translationtables + (MAXPLAYERS*2+3)*256;

	mov	eax, DWORD PTR ?translationtables@@3PAEA ; translationtables
	mov	ebx, -128				; ffffff80H
	mov	DWORD PTR -4+[esp+20], 4096		; 00001000H
	lea	edi, DWORD PTR [eax+4864]
$L10154:

; 1024 : 
; 1025 : 	// Full alpha
; 1026 : 	for (i = 0; i < 16; ++i)
; 1027 : 	{
; 1028 : 		for (j = 0; j < NUMCOLORMAPS; ++j)

	mov	esi, DWORD PTR -4+[esp+20]
	mov	ebp, 32					; 00000020H
$L10157:

; 1029 : 		{
; 1030 : 			int a = (NUMCOLORMAPS - j) * (256 / NUMCOLORMAPS) * (16-i);
; 1031 : 			for (int k = 0; k < 256; ++k)

	xor	eax, eax
	mov	edx, 256				; 00000100H
$L10162:

; 1032 : 			{
; 1033 : 				BYTE v = ((k * a) + 256) >> 14;

	mov	ecx, edx
	sar	ecx, 14					; 0000000eH

; 1034 : 				table[k] = MIN<BYTE> (v, 64);

	cmp	cl, 64					; 00000040H
	jb	SHORT $L10702
	mov	cl, 64					; 00000040H
$L10702:
	mov	BYTE PTR [eax+edi], cl
	inc	eax
	add	edx, esi
	cmp	eax, 256				; 00000100H
	jl	SHORT $L10162

; 1035 : 			}
; 1036 : 			table += 256;

	add	edi, 256				; 00000100H
	add	esi, ebx
	dec	ebp
	jne	SHORT $L10157
	mov	eax, DWORD PTR -4+[esp+20]
	add	ebx, 8
	sub	eax, 256				; 00000100H
	test	eax, eax
	mov	DWORD PTR -4+[esp+20], eax
	jg	SHORT $L10154
	pop	edi
	pop	esi
	pop	ebp
	pop	ebx

; 1037 : 		}
; 1038 : 	}
; 1039 : }

	pop	ecx
	ret	0
?R_InitTranslationTables@@YAXXZ ENDP			; R_InitTranslationTables
_TEXT	ENDS
PUBLIC	__real@4@3ff78080808080808000
PUBLIC	__real@4@3ffceb851f0000000000
PUBLIC	__real@4@3ffbcccccd0000000000
PUBLIC	__real@4@bffef0ed3e0000000000
PUBLIC	__real@4@00000000000000000000
PUBLIC	__real@4@3fff8000000000000000
PUBLIC	__real@4@4006ff00000000000000
PUBLIC	__real@4@3ffdd67c280000000000
PUBLIC	__real@4@3ffcf0f0d50000000000
PUBLIC	__real@4@3fffa666660000000000
PUBLIC	__real@4@3ffbc504810000000000
PUBLIC	__real@4@3ffee581060000000000
PUBLIC	__real@4@3ffac84b5e0000000000
PUBLIC	__real@4@3ffcd73eab0000000000
PUBLIC	?R_BuildPlayerTranslation@@YAXHH@Z		; R_BuildPlayerTranslation
EXTRN	?RGBtoHSV@@YAXMMMPAM00@Z:NEAR			; RGBtoHSV
EXTRN	?HSVtoRGB@@YAXPAM00MMM@Z:NEAR			; HSVtoRGB
EXTRN	?players@@3PAVplayer_s@@A:BYTE			; players
EXTRN	?skins@@3PAVFPlayerSkin@@A:DWORD		; skins
EXTRN	?Pick@FColorMatcher@@QAEEHHH@Z:NEAR		; FColorMatcher::Pick
EXTRN	?ColorMatcher@@3VFColorMatcher@@A:BYTE		; ColorMatcher
EXTRN	__ftol:NEAR
EXTRN	__fltused:NEAR
;	COMDAT __real@4@3ff78080808080808000
; File templates.h
CONST	SEGMENT
__real@4@3ff78080808080808000 DD 03b808081r	; 0.00392157
CONST	ENDS
;	COMDAT __real@4@3ffceb851f0000000000
CONST	SEGMENT
__real@4@3ffceb851f0000000000 DD 03e6b851fr	; 0.23
CONST	ENDS
;	COMDAT __real@4@3ffbcccccd0000000000
CONST	SEGMENT
__real@4@3ffbcccccd0000000000 DD 03dcccccdr	; 0.1
CONST	ENDS
;	COMDAT __real@4@bffef0ed3e0000000000
CONST	SEGMENT
__real@4@bffef0ed3e0000000000 DD 0bf70ed3er	; -0.94112
CONST	ENDS
;	COMDAT __real@4@00000000000000000000
CONST	SEGMENT
__real@4@00000000000000000000 DD 000000000r	; 0
CONST	ENDS
;	COMDAT __real@4@3fff8000000000000000
CONST	SEGMENT
__real@4@3fff8000000000000000 DD 03f800000r	; 1
CONST	ENDS
;	COMDAT __real@4@4006ff00000000000000
CONST	SEGMENT
__real@4@4006ff00000000000000 DD 0437f0000r	; 255
CONST	ENDS
;	COMDAT __real@4@3ffdd67c280000000000
CONST	SEGMENT
__real@4@3ffdd67c280000000000 DD 03ed67c28r	; 0.418916
CONST	ENDS
;	COMDAT __real@4@3ffcf0f0d50000000000
CONST	SEGMENT
__real@4@3ffcf0f0d50000000000 DD 03e70f0d5r	; 0.235294
CONST	ENDS
;	COMDAT __real@4@3fffa666660000000000
CONST	SEGMENT
__real@4@3fffa666660000000000 DD 03fa66666r	; 1.3
CONST	ENDS
;	COMDAT __real@4@3ffbc504810000000000
CONST	SEGMENT
__real@4@3ffbc504810000000000 DD 03dc50481r	; 0.0962
CONST	ENDS
;	COMDAT __real@4@3ffee581060000000000
CONST	SEGMENT
__real@4@3ffee581060000000000 DD 03f658106r	; 0.8965
CONST	ENDS
;	COMDAT __real@4@3ffac84b5e0000000000
CONST	SEGMENT
__real@4@3ffac84b5e0000000000 DD 03d484b5er	; 0.0489
CONST	ENDS
;	COMDAT __real@4@3ffcd73eab0000000000
CONST	SEGMENT
__real@4@3ffcd73eab0000000000 DD 03e573eabr	; 0.2102
CONST	ENDS
;	COMDAT ?R_BuildPlayerTranslation@@YAXHH@Z
_TEXT	SEGMENT
_player$ = 8
_color$ = 12
_start$ = -8
_end$ = -12
_r$ = -28
_g$ = -32
_b$ = -36
_h$ = -8
_s$ = 12
_v$ = 8
_bases$ = -24
_basev$ = -20
_sdelta$ = -4
_vdelta$ = -16
_range$ = -16
_uses$10199 = -24
_usev$10200 = -20
_vdelta$10233 = -4
?R_BuildPlayerTranslation@@YAXHH@Z PROC NEAR		; R_BuildPlayerTranslation, COMDAT

; 1044 : {

	sub	esp, 36					; 00000024H

; 1045 : 	byte *table = &translationtables[player*256*2];

	mov	ecx, DWORD PTR _player$[esp+32]
	mov	eax, DWORD PTR ?translationtables@@3PAEA ; translationtables
	push	ebx
	push	ebp
	push	esi
	push	edi
	mov	edi, ecx

; 1046 : 	FPlayerSkin *skin = &skins[players[player].userinfo.skin];

	mov	edx, DWORD PTR ?skins@@3PAVFPlayerSkin@@A ; skins
	shl	edi, 9
	add	edi, eax
	mov	eax, ecx
	shl	eax, 4
	add	eax, ecx
	lea	eax, DWORD PTR [eax+eax*4]
	mov	eax, DWORD PTR ?players@@3PAVplayer_s@@A[eax*8+52]
	shl	eax, 5
	add	eax, edx

; 1047 : 
; 1048 : 	byte i;
; 1049 : 	byte start = skin->range0start;

	mov	bl, BYTE PTR [eax+21]
	mov	BYTE PTR _start$[esp+52], bl

; 1050 : 	byte end = skin->range0end;

	mov	cl, BYTE PTR [eax+22]

; 1051 : 	float r = (float)RPART(color) / 255.f;

	mov	eax, DWORD PTR _color$[esp+48]
	mov	BYTE PTR _end$[esp+52], cl
	mov	edx, eax

; 1052 : 	float g = (float)GPART(color) / 255.f;

	mov	ecx, eax
	sar	edx, 16					; 00000010H
	and	edx, 255				; 000000ffH

; 1053 : 	float b = (float)BPART(color) / 255.f;
; 1054 : 	float h, s, v;
; 1055 : 	float bases, basev;
; 1056 : 	float sdelta, vdelta;
; 1057 : 	float range;
; 1058 : 
; 1059 : 	range = (float)(end-start+1);

	mov	esi, DWORD PTR _end$[esp+52]
	mov	DWORD PTR 8+[esp+48], edx
	mov	ebp, DWORD PTR _start$[esp+52]
	fild	DWORD PTR 8+[esp+48]
	sar	ecx, 8
	and	ecx, 255				; 000000ffH
	and	eax, 255				; 000000ffH
	fmul	DWORD PTR __real@4@3ff78080808080808000
	mov	DWORD PTR 8+[esp+48], ecx
	and	esi, 255				; 000000ffH
	and	ebp, 255				; 000000ffH
	mov	edx, esi
	sub	edx, ebp

; 1060 : 
; 1061 : 	RGBtoHSV (r, g, b, &h, &s, &v);

	lea	ecx, DWORD PTR _s$[esp+48]
	fstp	DWORD PTR _r$[esp+52]
	fild	DWORD PTR 8+[esp+48]
	mov	DWORD PTR 8+[esp+48], eax
	inc	edx
	lea	eax, DWORD PTR _v$[esp+48]
	fmul	DWORD PTR __real@4@3ff78080808080808000
	push	eax
	push	ecx
	fstp	DWORD PTR _g$[esp+60]
	fild	DWORD PTR 8+[esp+56]
	mov	ecx, DWORD PTR _g$[esp+60]
	mov	DWORD PTR 8+[esp+56], edx
	lea	edx, DWORD PTR _h$[esp+60]
	fmul	DWORD PTR __real@4@3ff78080808080808000
	push	edx
	mov	edx, DWORD PTR _r$[esp+64]
	fstp	DWORD PTR _b$[esp+64]
	mov	eax, DWORD PTR _b$[esp+64]
	fild	DWORD PTR 8+[esp+60]
	push	eax
	push	ecx
	push	edx
	fstp	DWORD PTR _range$[esp+76]
	call	?RGBtoHSV@@YAXMMMPAM00@Z		; RGBtoHSV

; 1062 : 
; 1063 : 	bases = s;

	mov	eax, DWORD PTR _s$[esp+72]

; 1064 : 	basev = v;

	mov	ecx, DWORD PTR _v$[esp+72]
	mov	DWORD PTR _bases$[esp+76], eax

; 1065 : 
; 1066 : 	if (gameinfo.gametype == GAME_Doom)

	mov	eax, DWORD PTR ?gameinfo@@3Ugameinfo_t@@A+144
	add	esp, 24					; 00000018H
	cmp	eax, 1
	mov	DWORD PTR _basev$[esp+52], ecx
	jne	$L10195

; 1067 : 	{
; 1068 : 		// Build player sprite translation
; 1069 : 		s -= 0.23f;

	fld	DWORD PTR _s$[esp+48]
	fsub	DWORD PTR __real@4@3ffceb851f0000000000

; 1070 : 		v += 0.1f;
; 1071 : 		sdelta = 0.23f / range;
; 1072 : 		vdelta = -0.94112f / range;
; 1073 : 
; 1074 : 		for (i = start; i <= end; i++)

	cmp	bl, BYTE PTR _end$[esp+52]
	fstp	DWORD PTR _s$[esp+48]
	fld	DWORD PTR _v$[esp+48]
	fadd	DWORD PTR __real@4@3ffbcccccd0000000000
	fstp	DWORD PTR _v$[esp+48]
	fld	DWORD PTR __real@4@3ffceb851f0000000000
	fdiv	DWORD PTR _range$[esp+52]
	fstp	DWORD PTR _sdelta$[esp+52]
	fld	DWORD PTR __real@4@bffef0ed3e0000000000
	fdiv	DWORD PTR _range$[esp+52]
	fstp	DWORD PTR _vdelta$[esp+52]
	ja	$L10272
	sub	esi, ebp
	lea	ebx, DWORD PTR [edi+ebp]
	inc	esi
$L10196:

; 1075 : 		{
; 1076 : 			float uses, usev;
; 1077 : 			uses = clamp (s, 0.f, 1.f);

	fld	DWORD PTR _s$[esp+48]
	fcomp	DWORD PTR __real@4@00000000000000000000
	fnstsw	ax
	test	ah, 65					; 00000041H
	je	SHORT $L10731
	mov	DWORD PTR _uses$10199[esp+52], 0
	jmp	SHORT $L10730
$L10731:
	fld	DWORD PTR _s$[esp+48]
	fcomp	DWORD PTR __real@4@3fff8000000000000000
	fnstsw	ax
	test	ah, 1
	jne	SHORT $L10729
	mov	DWORD PTR _uses$10199[esp+52], 1065353216 ; 3f800000H
	jmp	SHORT $L10730
$L10729:
	mov	edx, DWORD PTR _s$[esp+48]
	mov	DWORD PTR _uses$10199[esp+52], edx
$L10730:

; 1078 : 			usev = clamp (v, 0.f, 1.f);

	fld	DWORD PTR _v$[esp+48]
	fcomp	DWORD PTR __real@4@00000000000000000000
	fnstsw	ax
	test	ah, 65					; 00000041H
	je	SHORT $L10742
	mov	DWORD PTR _usev$10200[esp+52], 0
	jmp	SHORT $L10741
$L10742:
	fld	DWORD PTR _v$[esp+48]
	fcomp	DWORD PTR __real@4@3fff8000000000000000
	fnstsw	ax
	test	ah, 1
	jne	SHORT $L10740
	mov	DWORD PTR _usev$10200[esp+52], 1065353216 ; 3f800000H
	jmp	SHORT $L10741
$L10740:
	mov	eax, DWORD PTR _v$[esp+48]
	mov	DWORD PTR _usev$10200[esp+52], eax
$L10741:

; 1079 : 			HSVtoRGB (&r, &g, &b, h, uses, usev);

	mov	ecx, DWORD PTR _usev$10200[esp+52]
	mov	edx, DWORD PTR _uses$10199[esp+52]
	mov	eax, DWORD PTR _h$[esp+52]
	push	ecx
	push	edx
	lea	ecx, DWORD PTR _b$[esp+60]
	push	eax
	lea	edx, DWORD PTR _g$[esp+64]
	push	ecx
	lea	eax, DWORD PTR _r$[esp+68]
	push	edx
	push	eax
	call	?HSVtoRGB@@YAXPAM00MMM@Z		; HSVtoRGB

; 1080 : 			table[i] = ColorMatcher.Pick (
; 1081 : 				clamp ((int)(r * 255.f), 0, 255),
; 1082 : 				clamp ((int)(g * 255.f), 0, 255),
; 1083 : 				clamp ((int)(b * 255.f), 0, 255));

	fld	DWORD PTR _b$[esp+76]
	fmul	DWORD PTR __real@4@4006ff00000000000000
	add	esp, 24					; 00000018H
	call	__ftol
	test	eax, eax
	jg	SHORT $L10753
	xor	ebp, ebp
	jmp	SHORT $L10752
$L10753:
	cmp	eax, 255				; 000000ffH
	mov	ebp, 255				; 000000ffH
	jge	SHORT $L10752
	mov	ebp, eax
$L10752:
	fld	DWORD PTR _g$[esp+52]
	fmul	DWORD PTR __real@4@4006ff00000000000000
	call	__ftol
	test	eax, eax
	jg	SHORT $L10765
	xor	edi, edi
	jmp	SHORT $L10764
$L10765:
	cmp	eax, 255				; 000000ffH
	mov	edi, 255				; 000000ffH
	jge	SHORT $L10764
	mov	edi, eax
$L10764:
	fld	DWORD PTR _r$[esp+52]
	fmul	DWORD PTR __real@4@4006ff00000000000000
	call	__ftol
	test	eax, eax
	jg	SHORT $L10777
	xor	eax, eax
	jmp	SHORT $L10776
$L10777:
	cmp	eax, 255				; 000000ffH
	jl	SHORT $L10776
	mov	eax, 255				; 000000ffH
$L10776:
	push	ebp
	push	edi
	push	eax
	mov	ecx, OFFSET FLAT:?ColorMatcher@@3VFColorMatcher@@A
	call	?Pick@FColorMatcher@@QAEEHHH@Z		; FColorMatcher::Pick

; 1084 : 			s += sdelta;

	fld	DWORD PTR _sdelta$[esp+52]
	mov	BYTE PTR [ebx], al
	inc	ebx
	fadd	DWORD PTR _s$[esp+48]
	dec	esi
	fstp	DWORD PTR _s$[esp+48]

; 1085 : 			v += vdelta;

	fld	DWORD PTR _vdelta$[esp+52]
	fadd	DWORD PTR _v$[esp+48]
	fstp	DWORD PTR _v$[esp+48]
	jne	$L10196
	pop	edi
	pop	esi
	pop	ebp
	pop	ebx

; 1117 : 		}
; 1118 : 	}
; 1119 : }

	add	esp, 36					; 00000024H
	ret	0
$L10195:

; 1086 : 		}
; 1087 : 	}
; 1088 : 	else if (gameinfo.gametype == GAME_Heretic)

	cmp	eax, 2
	jne	$L10272

; 1089 : 	{
; 1090 : 		float vdelta = 0.418916f / range;

	fld	DWORD PTR __real@4@3ffdd67c280000000000
	fdiv	DWORD PTR _range$[esp+52]

; 1091 : 
; 1092 : 		// Build player sprite translation
; 1093 : 		for (i = start; i <= end; i++)

	cmp	bl, BYTE PTR _end$[esp+52]
	fstp	DWORD PTR _vdelta$10233[esp+52]
	ja	$L10236
	sub	esi, ebp
	mov	ebx, ebp
	inc	esi
	mov	DWORD PTR -16+[esp+52], esi
$L10234:

; 1094 : 		{
; 1095 : 			v = vdelta * (float)(i - start) + basev - 0.2352937f;

	mov	ecx, ebx
	sub	ecx, ebp
	mov	DWORD PTR 8+[esp+48], ecx
	fild	DWORD PTR 8+[esp+48]
	fmul	DWORD PTR _vdelta$10233[esp+52]
	fadd	DWORD PTR _basev$[esp+52]
	fsub	DWORD PTR __real@4@3ffcf0f0d50000000000
	fst	DWORD PTR _v$[esp+48]

; 1096 : 			v = clamp (v, 0.f, 1.f);

	fcomp	DWORD PTR __real@4@00000000000000000000
	fnstsw	ax
	test	ah, 65					; 00000041H
	je	SHORT $L10789
	mov	DWORD PTR _v$[esp+48], 0
	jmp	SHORT $L10787
$L10789:
	fld	DWORD PTR _v$[esp+48]
	fcomp	DWORD PTR __real@4@3fff8000000000000000
	fnstsw	ax
	test	ah, 1
	jne	SHORT $L10787
	mov	DWORD PTR _v$[esp+48], 1065353216	; 3f800000H
$L10787:

; 1097 : 			HSVtoRGB (&r, &g, &b, h, s, v);

	mov	edx, DWORD PTR _v$[esp+48]
	mov	eax, DWORD PTR _s$[esp+48]
	mov	ecx, DWORD PTR _h$[esp+52]
	push	edx
	push	eax
	lea	edx, DWORD PTR _b$[esp+60]
	push	ecx
	lea	eax, DWORD PTR _g$[esp+64]
	push	edx
	lea	ecx, DWORD PTR _r$[esp+68]
	push	eax
	push	ecx
	call	?HSVtoRGB@@YAXPAM00MMM@Z		; HSVtoRGB

; 1098 : 			table[i] = ColorMatcher.Pick (
; 1099 : 				clamp ((int)(r * 255.f), 0, 255),
; 1100 : 				clamp ((int)(g * 255.f), 0, 255),
; 1101 : 				clamp ((int)(b * 255.f), 0, 255));

	fld	DWORD PTR _b$[esp+76]
	fmul	DWORD PTR __real@4@4006ff00000000000000
	add	esp, 24					; 00000018H
	call	__ftol
	xor	esi, esi
	cmp	eax, esi
	jg	SHORT $L10800
	mov	DWORD PTR -12+[esp+52], esi
	jmp	SHORT $L10799
$L10800:
	cmp	eax, 255				; 000000ffH
	mov	DWORD PTR -12+[esp+52], 255		; 000000ffH
	jge	SHORT $L10799
	mov	DWORD PTR -12+[esp+52], eax
$L10799:
	fld	DWORD PTR _g$[esp+52]
	fmul	DWORD PTR __real@4@4006ff00000000000000
	call	__ftol
	cmp	eax, esi
	jle	SHORT $L10811
	cmp	eax, 255				; 000000ffH
	mov	esi, 255				; 000000ffH
	jge	SHORT $L10811
	mov	esi, eax
$L10811:
	fld	DWORD PTR _r$[esp+52]
	fmul	DWORD PTR __real@4@4006ff00000000000000
	call	__ftol
	test	eax, eax
	jg	SHORT $L10824
	xor	eax, eax
	jmp	SHORT $L10823
$L10824:
	cmp	eax, 255				; 000000ffH
	jl	SHORT $L10823
	mov	eax, 255				; 000000ffH
$L10823:
	mov	edx, DWORD PTR -12+[esp+52]
	mov	ecx, OFFSET FLAT:?ColorMatcher@@3VFColorMatcher@@A
	push	edx
	push	esi
	push	eax
	call	?Pick@FColorMatcher@@QAEEHHH@Z		; FColorMatcher::Pick
	mov	BYTE PTR [ebx+edi], al
	mov	eax, DWORD PTR -16+[esp+52]
	inc	ebx
	dec	eax
	mov	DWORD PTR -16+[esp+52], eax
	jne	$L10234
$L10236:

; 1102 : 		}
; 1103 : 
; 1104 : 		// Build rain/lifegem translation
; 1105 : 		table += 256;
; 1106 : 		bases = MIN (bases*1.3f, 1.f);

	fld	DWORD PTR _bases$[esp+52]
	fmul	DWORD PTR __real@4@3fffa666660000000000
	add	edi, 256				; 00000100H
	fcom	DWORD PTR __real@4@3fff8000000000000000
	fnstsw	ax
	test	ah, 1
	je	SHORT $L10832
	fstp	DWORD PTR _bases$[esp+52]
	jmp	SHORT $L10833
$L10832:
	fstp	ST(0)
	mov	DWORD PTR _bases$[esp+52], 1065353216	; 3f800000H
$L10833:

; 1107 : 		basev = MIN (basev*1.3f, 1.f);

	fld	DWORD PTR _basev$[esp+52]
	fmul	DWORD PTR __real@4@3fffa666660000000000
	fcom	DWORD PTR __real@4@3fff8000000000000000
	fnstsw	ax
	test	ah, 1
	je	SHORT $L10840
	fstp	DWORD PTR _basev$[esp+52]
	jmp	SHORT $L10841
$L10840:
	fstp	ST(0)
	mov	DWORD PTR _basev$[esp+52], 1065353216	; 3f800000H
$L10841:

; 1108 : 		for (i = 145; i <= 168; i++)

	mov	ebp, 1
	add	edi, 145				; 00000091H
	mov	DWORD PTR -12+[esp+52], ebp
	mov	DWORD PTR -16+[esp+52], 24		; 00000018H
$L10270:

; 1109 : 		{
; 1110 : 			s = MIN (bases, 0.8965f - 0.0962f*(float)(i - 161));

	lea	eax, DWORD PTR [ebp-17]
	mov	DWORD PTR -4+[esp+52], eax
	fild	DWORD PTR -4+[esp+52]
	fmul	DWORD PTR __real@4@3ffbc504810000000000
	fsubr	DWORD PTR __real@4@3ffee581060000000000
	fld	DWORD PTR _bases$[esp+52]
	fcomp	ST(1)
	fnstsw	ax
	test	ah, 1
	je	SHORT $L10848
	mov	ecx, DWORD PTR _bases$[esp+52]
	fstp	ST(0)
	mov	DWORD PTR _s$[esp+48], ecx
	jmp	SHORT $L10849
$L10848:
	fstp	DWORD PTR _s$[esp+48]
$L10849:

; 1111 : 			v = MIN (1.f, (0.2102f + 0.0489f*(float)(i - 144)) * basev);

	fild	DWORD PTR -12+[esp+52]
	fmul	DWORD PTR __real@4@3ffac84b5e0000000000
	fadd	DWORD PTR __real@4@3ffcd73eab0000000000
	fmul	DWORD PTR _basev$[esp+52]
	fld	DWORD PTR __real@4@3fff8000000000000000
	fcomp	ST(1)
	fnstsw	ax
	test	ah, 1
	je	SHORT $L10856
	fstp	ST(0)
	mov	DWORD PTR _v$[esp+48], 1065353216	; 3f800000H
	jmp	SHORT $L10857
$L10856:
	fstp	DWORD PTR _v$[esp+48]
$L10857:

; 1112 : 			HSVtoRGB (&r, &g, &b, h, s, v);

	mov	edx, DWORD PTR _v$[esp+48]
	mov	eax, DWORD PTR _s$[esp+48]
	mov	ecx, DWORD PTR _h$[esp+52]
	push	edx
	push	eax
	lea	edx, DWORD PTR _b$[esp+60]
	push	ecx
	lea	eax, DWORD PTR _g$[esp+64]
	push	edx
	lea	ecx, DWORD PTR _r$[esp+68]
	push	eax
	push	ecx
	call	?HSVtoRGB@@YAXPAM00MMM@Z		; HSVtoRGB

; 1113 : 			table[i] = ColorMatcher.Pick (
; 1114 : 				clamp ((int)(r * 255.f), 0, 255),
; 1115 : 				clamp ((int)(g * 255.f), 0, 255),
; 1116 : 				clamp ((int)(b * 255.f), 0, 255));

	fld	DWORD PTR _b$[esp+76]
	fmul	DWORD PTR __real@4@4006ff00000000000000
	add	esp, 24					; 00000018H
	call	__ftol
	test	eax, eax
	jg	SHORT $L10868
	xor	ebx, ebx
	jmp	SHORT $L10867
$L10868:
	cmp	eax, 255				; 000000ffH
	mov	ebx, 255				; 000000ffH
	jge	SHORT $L10867
	mov	ebx, eax
$L10867:
	fld	DWORD PTR _g$[esp+52]
	fmul	DWORD PTR __real@4@4006ff00000000000000
	call	__ftol
	test	eax, eax
	jg	SHORT $L10880
	xor	esi, esi
	jmp	SHORT $L10879
$L10880:
	cmp	eax, 255				; 000000ffH
	mov	esi, 255				; 000000ffH
	jge	SHORT $L10879
	mov	esi, eax
$L10879:
	fld	DWORD PTR _r$[esp+52]
	fmul	DWORD PTR __real@4@4006ff00000000000000
	call	__ftol
	test	eax, eax
	jg	SHORT $L10892
	xor	eax, eax
	jmp	SHORT $L10891
$L10892:
	cmp	eax, 255				; 000000ffH
	jl	SHORT $L10891
	mov	eax, 255				; 000000ffH
$L10891:
	push	ebx
	push	esi
	push	eax
	mov	ecx, OFFSET FLAT:?ColorMatcher@@3VFColorMatcher@@A
	call	?Pick@FColorMatcher@@QAEEHHH@Z		; FColorMatcher::Pick
	mov	BYTE PTR [edi], al
	mov	eax, DWORD PTR -16+[esp+52]
	inc	ebp
	inc	edi
	dec	eax
	mov	DWORD PTR -12+[esp+52], ebp
	mov	DWORD PTR -16+[esp+52], eax
	jne	$L10270
$L10272:
	pop	edi
	pop	esi
	pop	ebp
	pop	ebx

; 1117 : 		}
; 1118 : 	}
; 1119 : }

	add	esp, 36					; 00000024H
	ret	0
?R_BuildPlayerTranslation@@YAXHH@Z ENDP			; R_BuildPlayerTranslation
_TEXT	ENDS
PUBLIC	?R_DrawBorder@@YAXHHHH@Z			; R_DrawBorder
EXTRN	?W_CheckNumForName@@YAHPBDH@Z:NEAR		; W_CheckNumForName
EXTRN	?screen@@3PAVDFrameBuffer@@A:DWORD		; screen
EXTRN	?W_CacheLumpNum@@YAPAXHH@Z:NEAR			; W_CacheLumpNum
;	COMDAT ?R_DrawBorder@@YAXHHHH@Z
_TEXT	SEGMENT
_x1$ = 8
_y1$ = 12
_x2$ = 16
_y2$ = 20
?R_DrawBorder@@YAXHHHH@Z PROC NEAR			; R_DrawBorder, COMDAT

; 1123 : 	int lump;
; 1124 : 
; 1125 : 	lump = W_CheckNumForName (gameinfo.borderFlat, ns_flats);

	push	2
	push	OFFSET FLAT:?gameinfo@@3Ugameinfo_t@@A+128
	call	?W_CheckNumForName@@YAHPBDH@Z		; W_CheckNumForName
	add	esp, 8

; 1126 : 	if (lump >= 0)

	test	eax, eax
	jl	SHORT $L10308

; 1127 : 	{
; 1128 : 		screen->FlatFill (x1 & ~63, y1, x2, y2,
; 1129 : 			(byte *)W_CacheLumpNum (lump, PU_CACHE));

	mov	ecx, DWORD PTR ?screen@@3PAVDFrameBuffer@@A ; screen
	push	esi
	push	101					; 00000065H
	push	eax
	mov	esi, DWORD PTR [ecx]
	call	?W_CacheLumpNum@@YAPAXHH@Z		; W_CacheLumpNum
	mov	edx, DWORD PTR _y2$[esp+8]
	mov	ecx, DWORD PTR _y1$[esp+8]
	add	esp, 8
	push	eax
	mov	eax, DWORD PTR _x2$[esp+4]
	push	edx
	mov	edx, DWORD PTR _x1$[esp+8]
	push	eax
	and	edx, -64				; ffffffc0H
	push	ecx
	mov	ecx, DWORD PTR ?screen@@3PAVDFrameBuffer@@A ; screen
	push	edx
	call	DWORD PTR [esi+80]
	pop	esi

; 1134 : 	}
; 1135 : }

	ret	0
$L10308:

; 1130 : 	}
; 1131 : 	else
; 1132 : 	{
; 1133 : 		screen->Clear (x1, y1, x2, y2, 0);

	mov	edx, DWORD PTR _y2$[esp-4]
	mov	ecx, DWORD PTR ?screen@@3PAVDFrameBuffer@@A ; screen
	push	0
	push	edx
	mov	edx, DWORD PTR _x2$[esp+4]
	mov	eax, DWORD PTR [ecx]
	push	edx
	mov	edx, DWORD PTR _y1$[esp+8]
	push	edx
	mov	edx, DWORD PTR _x1$[esp+12]
	push	edx
	call	DWORD PTR [eax+84]

; 1134 : 	}
; 1135 : }

	ret	0
?R_DrawBorder@@YAXHHHH@Z ENDP				; R_DrawBorder
_TEXT	ENDS
PUBLIC	?R_DrawViewBorder@@YAXXZ			; R_DrawViewBorder
EXTRN	?M_DrawFrame@@YAXHHHH@Z:NEAR			; M_DrawFrame
EXTRN	?V_MarkRect@@YAXHHHH@Z:NEAR			; V_MarkRect
EXTRN	?ST_Y@@3HA:DWORD				; ST_Y
EXTRN	?SB_state@@3HA:DWORD				; SB_state
;	COMDAT ?R_DrawViewBorder@@YAXXZ
_TEXT	SEGMENT
?R_DrawViewBorder@@YAXXZ PROC NEAR			; R_DrawViewBorder, COMDAT

; 1154 : 	// [RH] Redraw the status bar if SCREENWIDTH > status bar width.
; 1155 : 	// Will draw borders around itself, too.
; 1156 : 	if (SCREENWIDTH > 320)

	mov	ecx, DWORD PTR ?screen@@3PAVDFrameBuffer@@A ; screen
	cmp	DWORD PTR [ecx+20], 320			; 00000140H
	jle	SHORT $L10324

; 1157 : 	{
; 1158 : 		SB_state = screen->GetPageCount ();

	mov	eax, DWORD PTR [ecx]
	call	DWORD PTR [eax+164]
	mov	ecx, DWORD PTR ?screen@@3PAVDFrameBuffer@@A ; screen
	mov	DWORD PTR ?SB_state@@3HA, eax		; SB_state
$L10324:

; 1159 : 	}
; 1160 : 
; 1161 : 	if (realviewwidth == SCREENWIDTH)

	mov	eax, DWORD PTR [ecx+20]
	mov	ecx, DWORD PTR _realviewwidth
	cmp	ecx, eax
	je	$L10323

; 1162 : 	{
; 1163 : 		return;
; 1164 : 	}
; 1165 : 
; 1166 : 	R_DrawBorder (0, 0, SCREENWIDTH, viewwindowy);

	mov	ecx, DWORD PTR ?viewwindowy@@3HA	; viewwindowy
	push	ecx
	push	eax
	push	0
	push	0
	call	?R_DrawBorder@@YAXHHHH@Z		; R_DrawBorder

; 1167 : 	R_DrawBorder (0, viewwindowy, viewwindowx, realviewheight + viewwindowy);

	mov	eax, DWORD PTR ?viewwindowy@@3HA	; viewwindowy
	mov	edx, DWORD PTR _realviewheight
	mov	ecx, DWORD PTR ?viewwindowx@@3HA	; viewwindowx
	add	edx, eax
	push	edx
	push	ecx
	push	eax
	push	0
	call	?R_DrawBorder@@YAXHHHH@Z		; R_DrawBorder

; 1168 : 	R_DrawBorder (viewwindowx + realviewwidth, viewwindowy, SCREENWIDTH, realviewheight + viewwindowy);

	mov	eax, DWORD PTR ?viewwindowy@@3HA	; viewwindowy
	mov	edx, DWORD PTR _realviewheight
	mov	ecx, DWORD PTR ?screen@@3PAVDFrameBuffer@@A ; screen
	add	edx, eax
	push	edx
	mov	edx, DWORD PTR [ecx+20]
	mov	ecx, DWORD PTR ?viewwindowx@@3HA	; viewwindowx
	push	edx
	push	eax
	mov	eax, DWORD PTR _realviewwidth
	add	ecx, eax
	push	ecx
	call	?R_DrawBorder@@YAXHHHH@Z		; R_DrawBorder

; 1169 : 	R_DrawBorder (0, viewwindowy + realviewheight, SCREENWIDTH, ST_Y);

	mov	eax, DWORD PTR ?screen@@3PAVDFrameBuffer@@A ; screen
	mov	edx, DWORD PTR ?ST_Y@@3HA		; ST_Y
	push	edx
	mov	edx, DWORD PTR ?viewwindowy@@3HA	; viewwindowy
	mov	ecx, DWORD PTR [eax+20]
	mov	eax, DWORD PTR _realviewheight
	add	eax, edx
	push	ecx
	push	eax
	push	0
	call	?R_DrawBorder@@YAXHHHH@Z		; R_DrawBorder

; 1170 : 
; 1171 : 	M_DrawFrame (viewwindowx, viewwindowy, realviewwidth, realviewheight);

	mov	ecx, DWORD PTR _realviewheight
	mov	edx, DWORD PTR _realviewwidth
	mov	eax, DWORD PTR ?viewwindowy@@3HA	; viewwindowy
	add	esp, 64					; 00000040H
	push	ecx
	mov	ecx, DWORD PTR ?viewwindowx@@3HA	; viewwindowx
	push	edx
	push	eax
	push	ecx
	call	?M_DrawFrame@@YAXHHHH@Z			; M_DrawFrame

; 1172 : 	V_MarkRect (0, 0, SCREENWIDTH, ST_Y);

	mov	eax, DWORD PTR ?screen@@3PAVDFrameBuffer@@A ; screen
	mov	edx, DWORD PTR ?ST_Y@@3HA		; ST_Y
	push	edx
	mov	ecx, DWORD PTR [eax+20]
	push	ecx
	push	0
	push	0
	call	?V_MarkRect@@YAXHHHH@Z			; V_MarkRect
	add	esp, 32					; 00000020H
$L10323:

; 1173 : }

	ret	0
?R_DrawViewBorder@@YAXXZ ENDP				; R_DrawViewBorder
_TEXT	ENDS
PUBLIC	?R_DrawTopBorder@@YAXXZ				; R_DrawTopBorder
EXTRN	?W_GetNumForName@@YAHPBD@Z:NEAR			; W_GetNumForName
;	COMDAT ?R_DrawTopBorder@@YAXXZ
_TEXT	SEGMENT
?R_DrawTopBorder@@YAXXZ PROC NEAR			; R_DrawTopBorder, COMDAT

; 1188 : 	int x, y;
; 1189 : 	int offset;
; 1190 : 	int size;
; 1191 : 
; 1192 : 	if (realviewwidth == SCREENWIDTH)

	mov	eax, DWORD PTR ?screen@@3PAVDFrameBuffer@@A ; screen
	mov	ecx, DWORD PTR _realviewwidth
	push	ebp
	push	esi
	mov	eax, DWORD PTR [eax+20]
	push	edi
	cmp	ecx, eax
	je	$L10998

; 1193 : 		return;
; 1194 : 
; 1195 : 	R_DrawBorder (0, 0, SCREENWIDTH, 34);

	push	34					; 00000022H
	push	eax
	push	0
	push	0
	call	?R_DrawBorder@@YAXHHHH@Z		; R_DrawBorder

; 1196 : 	offset = gameinfo.border->offset;

	mov	eax, DWORD PTR ?gameinfo@@3Ugameinfo_t@@A+136

; 1197 : 	size = gameinfo.border->size;
; 1198 : 
; 1199 : 	if (viewwindowy < 35)

	mov	esi, DWORD PTR ?viewwindowy@@3HA	; viewwindowy
	xor	ecx, ecx
	xor	edx, edx
	mov	cl, BYTE PTR [eax]
	mov	dl, BYTE PTR [eax+1]
	add	esp, 16					; 00000010H
	cmp	esi, 35					; 00000023H
	mov	edi, ecx
	mov	ebp, edx
	jge	$L10998

; 1200 : 	{
; 1201 : 		for (x = viewwindowx; x < viewwindowx + realviewwidth; x += size)

	mov	eax, DWORD PTR ?viewwindowx@@3HA	; viewwindowx
	mov	ecx, DWORD PTR _realviewwidth
	push	ebx
	mov	ebx, eax
	lea	edx, DWORD PTR [eax+ecx]
	cmp	ebx, edx
	jge	SHORT $L10336
$L10334:

; 1202 : 		{
; 1203 : 			screen->DrawPatch ((patch_t *)W_CacheLumpName(gameinfo.border->t, PU_CACHE),
; 1204 : 				x, viewwindowy - offset);

	mov	eax, DWORD PTR ?gameinfo@@3Ugameinfo_t@@A+136
	push	101					; 00000065H
	add	eax, 10					; 0000000aH
	sub	esi, edi
	push	eax
	call	?W_GetNumForName@@YAHPBD@Z		; W_GetNumForName
	add	esp, 4
	push	eax
	call	?W_CacheLumpNum@@YAPAXHH@Z		; W_CacheLumpNum
	mov	ecx, DWORD PTR ?screen@@3PAVDFrameBuffer@@A ; screen
	add	esp, 8
	mov	edx, DWORD PTR [ecx]
	push	esi
	push	ebx
	push	eax
	push	0
	call	DWORD PTR [edx+108]
	mov	eax, DWORD PTR ?viewwindowx@@3HA	; viewwindowx
	mov	ecx, DWORD PTR _realviewwidth
	mov	esi, DWORD PTR ?viewwindowy@@3HA	; viewwindowy
	add	ebx, ebp
	lea	edx, DWORD PTR [eax+ecx]
	cmp	ebx, edx
	jl	SHORT $L10334
$L10336:

; 1205 : 		}
; 1206 : 		for (y = viewwindowy; y < 35; y += size)

	cmp	esi, 35					; 00000023H
	jge	$L10340
	jmp	SHORT $L10338
$L11009:
	mov	eax, DWORD PTR ?viewwindowx@@3HA	; viewwindowx
$L10338:

; 1207 : 		{
; 1208 : 			screen->DrawPatch ((patch_t *)W_CacheLumpName (gameinfo.border->l, PU_CACHE),
; 1209 : 				viewwindowx - offset, y);

	sub	eax, edi
	push	101					; 00000065H
	mov	ebx, eax
	mov	eax, DWORD PTR ?gameinfo@@3Ugameinfo_t@@A+136
	add	eax, 26					; 0000001aH
	push	eax
	call	?W_GetNumForName@@YAHPBD@Z		; W_GetNumForName
	add	esp, 4
	push	eax
	call	?W_CacheLumpNum@@YAPAXHH@Z		; W_CacheLumpNum
	mov	ecx, DWORD PTR ?screen@@3PAVDFrameBuffer@@A ; screen
	add	esp, 8
	mov	edx, DWORD PTR [ecx]
	push	esi
	push	ebx
	push	eax
	push	0
	call	DWORD PTR [edx+108]

; 1210 : 			screen->DrawPatch ((patch_t *)W_CacheLumpName(gameinfo.border->r, PU_CACHE),
; 1211 : 				viewwindowx + realviewwidth, y);

	mov	edx, DWORD PTR ?gameinfo@@3Ugameinfo_t@@A+136
	mov	eax, DWORD PTR _realviewwidth
	mov	ecx, DWORD PTR ?viewwindowx@@3HA	; viewwindowx
	add	edx, 34					; 00000022H
	push	101					; 00000065H
	push	edx
	lea	ebx, DWORD PTR [ecx+eax]
	call	?W_GetNumForName@@YAHPBD@Z		; W_GetNumForName
	add	esp, 4
	push	eax
	call	?W_CacheLumpNum@@YAPAXHH@Z		; W_CacheLumpNum
	mov	ecx, DWORD PTR ?screen@@3PAVDFrameBuffer@@A ; screen
	add	esp, 8
	mov	edx, DWORD PTR [ecx]
	push	esi
	push	ebx
	push	eax
	push	0
	call	DWORD PTR [edx+108]
	add	esi, ebp
	cmp	esi, 35					; 00000023H
	jl	SHORT $L11009
	mov	esi, DWORD PTR ?viewwindowy@@3HA	; viewwindowy
	mov	eax, DWORD PTR ?viewwindowx@@3HA	; viewwindowx
$L10340:

; 1212 : 		}
; 1213 : 
; 1214 : 		screen->DrawPatch ((patch_t *)W_CacheLumpName(gameinfo.border->tl, PU_CACHE),
; 1215 : 			viewwindowx-offset, viewwindowy - offset);

	sub	eax, edi
	push	101					; 00000065H
	mov	ebx, eax
	mov	eax, DWORD PTR ?gameinfo@@3Ugameinfo_t@@A+136
	add	eax, 2
	sub	esi, edi
	push	eax
	call	?W_GetNumForName@@YAHPBD@Z		; W_GetNumForName
	add	esp, 4
	push	eax
	call	?W_CacheLumpNum@@YAPAXHH@Z		; W_CacheLumpNum
	mov	ecx, DWORD PTR ?screen@@3PAVDFrameBuffer@@A ; screen
	add	esp, 8
	mov	edx, DWORD PTR [ecx]
	push	esi
	push	ebx
	push	eax
	push	0
	call	DWORD PTR [edx+108]

; 1216 : 		screen->DrawPatch ((patch_t *)W_CacheLumpName(gameinfo.border->tr, PU_CACHE),
; 1217 : 			viewwindowx+realviewwidth, viewwindowy - offset);

	mov	edx, DWORD PTR ?gameinfo@@3Ugameinfo_t@@A+136
	mov	esi, DWORD PTR ?viewwindowy@@3HA	; viewwindowy
	mov	eax, DWORD PTR _realviewwidth
	mov	ecx, DWORD PTR ?viewwindowx@@3HA	; viewwindowx
	add	edx, 18					; 00000012H
	push	101					; 00000065H
	sub	esi, edi
	push	edx
	lea	edi, DWORD PTR [ecx+eax]
	call	?W_GetNumForName@@YAHPBD@Z		; W_GetNumForName
	add	esp, 4
	push	eax
	call	?W_CacheLumpNum@@YAPAXHH@Z		; W_CacheLumpNum
	mov	ecx, DWORD PTR ?screen@@3PAVDFrameBuffer@@A ; screen
	add	esp, 8
	mov	edx, DWORD PTR [ecx]
	push	esi
	push	edi
	push	eax
	push	0
	call	DWORD PTR [edx+108]
	pop	ebx
$L10998:
	pop	edi
	pop	esi
	pop	ebp

; 1218 : 	}
; 1219 : }

	ret	0
?R_DrawTopBorder@@YAXXZ ENDP				; R_DrawTopBorder
_TEXT	ENDS
PUBLIC	?R_DetailDouble@@YAXXZ				; R_DetailDouble
EXTRN	?UseMMX@@3HA:DWORD				; UseMMX
EXTRN	?RenderTarget@@3PAVDCanvas@@A:DWORD		; RenderTarget
EXTRN	_HaveRDTSC:DWORD
EXTRN	_DoubleHoriz_MMX:NEAR
EXTRN	_DoubleHorizVert_MMX:NEAR
EXTRN	_DoubleVert_ASM:NEAR
;	COMDAT ?R_DetailDouble@@YAXXZ
_TEXT	SEGMENT
_res$11015 = -4
_res$11040 = -4
?R_DetailDouble@@YAXXZ PROC NEAR			; R_DetailDouble, COMDAT

; 1225 : {

	push	ebp
	mov	ebp, esp
	push	ecx

; 1226 : 	DetailDoubleCycles = 0;
; 1227 : 	clock (DetailDoubleCycles);

	mov	eax, DWORD PTR _HaveRDTSC
	mov	DWORD PTR ?DetailDoubleCycles@@3KA, 0	; DetailDoubleCycles
	test	eax, eax
	je	SHORT $L11014

; 1228 : 
; 1229 : 	switch ((detailxshift << 1) | detailyshift)
; 1230 : 	{

	xor	eax, eax

; 1231 : 	case 1:		// y-double

	xor	edx, edx

; 1232 : #ifdef USEASM

	DB	15					; 0000000fH

; 1233 : 		DoubleVert_ASM (viewheight, viewwidth, ylookup[0], RenderTarget->GetPitch());

	DB	49					; 00000031H

; 1234 : #else

	mov	DWORD PTR _res$11015[ebp], eax

; 1226 : 	DetailDoubleCycles = 0;
; 1227 : 	clock (DetailDoubleCycles);

	mov	eax, DWORD PTR _res$11015[ebp]
	jmp	SHORT $L11016
$L11014:
	xor	eax, eax
$L11016:

; 1228 : 
; 1229 : 	switch ((detailxshift << 1) | detailyshift)
; 1230 : 	{

	mov	edx, DWORD PTR _detailyshift
	push	ebx
	push	esi
	mov	esi, DWORD PTR ?DetailDoubleCycles@@3KA	; DetailDoubleCycles
	sub	esi, eax
	mov	eax, DWORD PTR _detailxshift
	add	eax, eax
	push	edi
	or	eax, edx
	mov	DWORD PTR ?DetailDoubleCycles@@3KA, esi	; DetailDoubleCycles
	dec	eax
	je	$L10351
	dec	eax
	je	$L10352
	dec	eax
	jne	$L10348

; 1277 : 
; 1278 : 	case 3:		// x- and y-double
; 1279 : #ifdef USEASM
; 1280 : 		if (UseMMX)

	mov	eax, DWORD PTR ?UseMMX@@3HA		; UseMMX
	test	eax, eax
	je	SHORT $L10369

; 1281 : 		{
; 1282 : 			DoubleHorizVert_MMX (viewheight, viewwidth, ylookup[0]+viewwidth, RenderTarget->GetPitch());

	mov	ecx, DWORD PTR ?RenderTarget@@3PAVDCanvas@@A ; RenderTarget
	mov	eax, DWORD PTR _viewwidth
	mov	edx, DWORD PTR [ecx+28]
	mov	ecx, DWORD PTR _ylookup
	push	edx
	lea	edx, DWORD PTR [eax+ecx]
	push	edx
	push	eax
	mov	eax, DWORD PTR _viewheight
	push	eax
	call	_DoubleHorizVert_MMX

; 1283 : 		}
; 1284 : 		else

	jmp	$L11056
$L10369:

; 1288 : 			int realpitch = RenderTarget->GetPitch();

	mov	ecx, DWORD PTR ?RenderTarget@@3PAVDCanvas@@A ; RenderTarget

; 1289 : 			int pitch = realpitch << 1;
; 1290 : 			int y,x;
; 1291 : 			byte *linefrom, *lineto;
; 1292 : 
; 1293 : 			linefrom = ylookup[0];
; 1294 : 			for (y = viewheight; y != 0; --y, linefrom += pitch)

	mov	eax, DWORD PTR _viewheight
	mov	ebx, DWORD PTR _viewwidth
	mov	edi, DWORD PTR _ylookup
	mov	esi, DWORD PTR [ecx+28]
	test	eax, eax
	je	$L10348

; 1285 : #endif
; 1286 : 		{
; 1287 : 			int rowsize = viewwidth;

	mov	DWORD PTR -4+[ebp], eax
$L10378:

; 1295 : 			{
; 1296 : 				lineto = linefrom - viewwidth;

	mov	edx, DWORD PTR _viewwidth
	mov	eax, edi
	sub	eax, edx

; 1297 : 				for (x = 0; x < rowsize; ++x)

	xor	edx, edx
	test	ebx, ebx
	jle	SHORT $L10379

; 1295 : 			{
; 1296 : 				lineto = linefrom - viewwidth;

	inc	eax
$L10381:

; 1298 : 				{
; 1299 : 					byte c = linefrom[x];

	mov	cl, BYTE PTR [edx+edi]
	inc	edx

; 1300 : 					lineto[x*2] = c;

	mov	BYTE PTR [eax-1], cl

; 1301 : 					lineto[x*2+1] = c;

	mov	BYTE PTR [eax], cl

; 1302 : 					lineto[x*2+realpitch] = c;

	mov	BYTE PTR [esi+eax-1], cl

; 1303 : 					lineto[x*2+realpitch+1] = c;

	mov	BYTE PTR [eax+esi], cl
	add	eax, 2
	cmp	edx, ebx
	jl	SHORT $L10381
$L10379:

; 1289 : 			int pitch = realpitch << 1;
; 1290 : 			int y,x;
; 1291 : 			byte *linefrom, *lineto;
; 1292 : 
; 1293 : 			linefrom = ylookup[0];
; 1294 : 			for (y = viewheight; y != 0; --y, linefrom += pitch)

	lea	eax, DWORD PTR [esi+esi]
	add	edi, eax
	mov	eax, DWORD PTR -4+[ebp]
	dec	eax
	mov	DWORD PTR -4+[ebp], eax
	jne	SHORT $L10378

; 1304 : 				}
; 1305 : 			}
; 1306 : 		}
; 1307 : 		break;

	jmp	$L10348
$L10352:

; 1235 : 		{
; 1236 : 			int rowsize = realviewwidth;
; 1237 : 			int pitch = RenderTarget->GetPitch();
; 1238 : 			int y;
; 1239 : 			byte *line;
; 1240 : 
; 1241 : 			line = ylookup[0];
; 1242 : 			for (y = viewheight; y != 0; --y, line += pitch<<1)
; 1243 : 			{
; 1244 : 				memcpy (line+pitch, line, rowsize);
; 1245 : 			}
; 1246 : 		}
; 1247 : #endif
; 1248 : 		break;
; 1249 : 
; 1250 : 	case 2:		// x-double
; 1251 : #ifdef USEASM
; 1252 : 		if (UseMMX)

	mov	eax, DWORD PTR ?UseMMX@@3HA		; UseMMX
	test	eax, eax
	je	SHORT $L10353

; 1253 : 		{
; 1254 : 			DoubleHoriz_MMX (viewheight, viewwidth, ylookup[0]+viewwidth, RenderTarget->GetPitch());

	mov	edx, DWORD PTR ?RenderTarget@@3PAVDCanvas@@A ; RenderTarget
	mov	ecx, DWORD PTR _ylookup
	mov	eax, DWORD PTR [edx+28]
	push	eax
	mov	eax, DWORD PTR _viewwidth
	lea	edx, DWORD PTR [eax+ecx]
	push	edx
	push	eax
	mov	eax, DWORD PTR _viewheight
	push	eax
	call	_DoubleHoriz_MMX

; 1255 : 		}
; 1256 : 		else

	jmp	SHORT $L11056
$L10353:

; 1260 : 			int pitch = RenderTarget->GetPitch();

	mov	ecx, DWORD PTR ?RenderTarget@@3PAVDCanvas@@A ; RenderTarget

; 1261 : 			int y,x;
; 1262 : 			byte *linefrom, *lineto;
; 1263 : 
; 1264 : 			linefrom = ylookup[0];
; 1265 : 			for (y = viewheight; y != 0; --y, linefrom += pitch)

	mov	eax, DWORD PTR _viewheight
	mov	edi, DWORD PTR _viewwidth
	mov	esi, DWORD PTR _ylookup
	mov	ebx, DWORD PTR [ecx+28]
	test	eax, eax
	je	SHORT $L10348

; 1257 : #endif
; 1258 : 		{
; 1259 : 			int rowsize = viewwidth;

	mov	DWORD PTR -4+[ebp], eax
$L10361:

; 1266 : 			{
; 1267 : 				lineto = linefrom - viewwidth;

	mov	eax, DWORD PTR _viewwidth
	mov	ecx, esi
	sub	ecx, eax

; 1268 : 				for (x = 0; x < rowsize; ++x)

	xor	eax, eax
	test	edi, edi
	jle	SHORT $L10362
$L10364:

; 1269 : 				{
; 1270 : 					byte c = linefrom[x];

	mov	dl, BYTE PTR [eax+esi]
	inc	eax

; 1271 : 					lineto[x*2] = c;

	mov	BYTE PTR [ecx], dl

; 1272 : 					lineto[x*2+1] = c;

	mov	BYTE PTR [ecx+1], dl
	add	ecx, 2
	cmp	eax, edi
	jl	SHORT $L10364
$L10362:

; 1261 : 			int y,x;
; 1262 : 			byte *linefrom, *lineto;
; 1263 : 
; 1264 : 			linefrom = ylookup[0];
; 1265 : 			for (y = viewheight; y != 0; --y, linefrom += pitch)

	mov	eax, DWORD PTR -4+[ebp]
	add	esi, ebx
	dec	eax
	mov	DWORD PTR -4+[ebp], eax
	jne	SHORT $L10361

; 1273 : 				}
; 1274 : 			}
; 1275 : 		}
; 1276 : 		break;

	jmp	SHORT $L10348
$L10351:

; 1233 : 		DoubleVert_ASM (viewheight, viewwidth, ylookup[0], RenderTarget->GetPitch());

	mov	edx, DWORD PTR ?RenderTarget@@3PAVDCanvas@@A ; RenderTarget
	mov	ecx, DWORD PTR _ylookup
	mov	eax, DWORD PTR [edx+28]
	mov	edx, DWORD PTR _viewwidth
	push	eax
	mov	eax, DWORD PTR _viewheight
	push	ecx
	push	edx
	push	eax
	call	_DoubleVert_ASM
$L11056:
	add	esp, 16					; 00000010H
$L10348:

; 1308 : 	}
; 1309 : 
; 1310 : 	unclock (DetailDoubleCycles);

	mov	eax, DWORD PTR _HaveRDTSC
	pop	edi
	pop	esi
	pop	ebx
	test	eax, eax
	je	SHORT $L11039

; 1228 : 
; 1229 : 	switch ((detailxshift << 1) | detailyshift)
; 1230 : 	{

	xor	eax, eax

; 1231 : 	case 1:		// y-double

	xor	edx, edx

; 1232 : #ifdef USEASM

	DB	15					; 0000000fH

; 1233 : 		DoubleVert_ASM (viewheight, viewwidth, ylookup[0], RenderTarget->GetPitch());

	DB	49					; 00000031H

; 1234 : #else

	mov	DWORD PTR _res$11040[ebp], eax

; 1308 : 	}
; 1309 : 
; 1310 : 	unclock (DetailDoubleCycles);

	mov	eax, DWORD PTR _res$11040[ebp]
	mov	ecx, DWORD PTR ?DetailDoubleCycles@@3KA	; DetailDoubleCycles
	add	ecx, eax
	mov	DWORD PTR ?DetailDoubleCycles@@3KA, ecx	; DetailDoubleCycles

; 1311 : }

	mov	esp, ebp
	pop	ebp
	ret	0

; 1308 : 	}
; 1309 : 
; 1310 : 	unclock (DetailDoubleCycles);

$L11039:
	mov	ecx, DWORD PTR ?DetailDoubleCycles@@3KA	; DetailDoubleCycles
	xor	eax, eax
	add	ecx, eax
	mov	DWORD PTR ?DetailDoubleCycles@@3KA, ecx	; DetailDoubleCycles

; 1311 : }

	mov	esp, ebp
	pop	ebp
	ret	0
?R_DetailDouble@@YAXXZ ENDP				; R_DetailDouble
_TEXT	ENDS
;	COMDAT _$E55
_TEXT	SEGMENT
_$E55	PROC NEAR					; COMDAT
	call	_$E52
	jmp	_$E54
_$E55	ENDP
_TEXT	ENDS
PUBLIC	?GetStats@Stat_detail@@UAEXPAD@Z		; Stat_detail::GetStats
PUBLIC	??_7Stat_detail@@6B@				; Stat_detail::`vftable'
PUBLIC	??_GStat_detail@@UAEPAXI@Z			; Stat_detail::`scalar deleting destructor'
PUBLIC	??_EStat_detail@@UAEPAXI@Z			; Stat_detail::`vector deleting destructor'
PUBLIC	??_C@_06NJMJ@detail?$AA@			; `string'
EXTRN	??0FStat@@QAE@PBD@Z:NEAR			; FStat::FStat
_BSS	SEGMENT
_Istaticstatdetail DB 0cH DUP (?)
_BSS	ENDS
;	COMDAT ??_7Stat_detail@@6B@
; File r_draw.cpp
CONST	SEGMENT
??_7Stat_detail@@6B@ DD FLAT:??_EStat_detail@@UAEPAXI@Z	; Stat_detail::`vftable'
	DD	FLAT:?GetStats@Stat_detail@@UAEXPAD@Z
CONST	ENDS
;	COMDAT ??_C@_06NJMJ@detail?$AA@
_DATA	SEGMENT
??_C@_06NJMJ@detail?$AA@ DB 'detail', 00H		; `string'
_DATA	ENDS
;	COMDAT _$E52
_TEXT	SEGMENT
_$E52	PROC NEAR					; COMDAT

; 1313 : ADD_STAT(detail,out)

	push	OFFSET FLAT:??_C@_06NJMJ@detail?$AA@	; `string'
	mov	ecx, OFFSET FLAT:_Istaticstatdetail
	call	??0FStat@@QAE@PBD@Z			; FStat::FStat
	mov	DWORD PTR _Istaticstatdetail, OFFSET FLAT:??_7Stat_detail@@6B@ ; Stat_detail::`vftable'
	ret	0
_$E52	ENDP
_TEXT	ENDS
EXTRN	_atexit:NEAR
;	COMDAT _$E54
_TEXT	SEGMENT
_$E54	PROC NEAR					; COMDAT
	push	OFFSET FLAT:_$E53
	call	_atexit
	pop	ecx
	ret	0
_$E54	ENDP
_TEXT	ENDS
EXTRN	??1FStat@@UAE@XZ:NEAR				; FStat::~FStat
;	COMDAT _$E53
_TEXT	SEGMENT
_$E53	PROC NEAR					; COMDAT
	mov	ecx, OFFSET FLAT:_Istaticstatdetail
	jmp	??1FStat@@UAE@XZ			; FStat::~FStat
_$E53	ENDP
_TEXT	ENDS
EXTRN	??3@YAXPAX@Z:NEAR				; operator delete
;	COMDAT ??_GStat_detail@@UAEPAXI@Z
_TEXT	SEGMENT
___flags$ = 8
??_GStat_detail@@UAEPAXI@Z PROC NEAR			; Stat_detail::`scalar deleting destructor', COMDAT
	push	esi
	mov	esi, ecx
	call	??1FStat@@UAE@XZ			; FStat::~FStat
	test	BYTE PTR ___flags$[esp], 1
	je	SHORT $L11076
	push	esi
	call	??3@YAXPAX@Z				; operator delete
	add	esp, 4
$L11076:
	mov	eax, esi
	pop	esi
	ret	4
??_GStat_detail@@UAEPAXI@Z ENDP				; Stat_detail::`scalar deleting destructor'
_TEXT	ENDS
PUBLIC	??_C@_0BF@CFEP@doubling?5?$DN?5?$CF04?41f?5ms?$AA@ ; `string'
PUBLIC	__real@8@4008fa00000000000000
EXTRN	_SecondsPerCycle:QWORD
EXTRN	_sprintf:NEAR
;	COMDAT ??_C@_0BF@CFEP@doubling?5?$DN?5?$CF04?41f?5ms?$AA@
; File r_draw.cpp
_DATA	SEGMENT
??_C@_0BF@CFEP@doubling?5?$DN?5?$CF04?41f?5ms?$AA@ DB 'doubling = %04.1f '
	DB	'ms', 00H					; `string'
_DATA	ENDS
;	COMDAT __real@8@4008fa00000000000000
CONST	SEGMENT
__real@8@4008fa00000000000000 DQ 0408f400000000000r ; 1000
CONST	ENDS
;	COMDAT ?GetStats@Stat_detail@@UAEXPAD@Z
_TEXT	SEGMENT
_out$ = 8
?GetStats@Stat_detail@@UAEXPAD@Z PROC NEAR		; Stat_detail::GetStats, COMDAT

; 1314 : {

	sub	esp, 8

; 1315 : 	sprintf (out, "doubling = %04.1f ms", (double)DetailDoubleCycles * 1000 * SecondsPerCycle);

	mov	eax, DWORD PTR ?DetailDoubleCycles@@3KA	; DetailDoubleCycles
	mov	DWORD PTR -8+[esp+12], 0
	mov	DWORD PTR -8+[esp+8], eax
	mov	ecx, DWORD PTR _out$[esp+4]
	fild	QWORD PTR -8+[esp+8]
	sub	esp, 8
	fmul	QWORD PTR _SecondsPerCycle
	fmul	QWORD PTR __real@8@4008fa00000000000000
	fstp	QWORD PTR [esp]
	push	OFFSET FLAT:??_C@_0BF@CFEP@doubling?5?$DN?5?$CF04?41f?5ms?$AA@ ; `string'
	push	ecx
	call	_sprintf

; 1316 : }

	add	esp, 24					; 00000018H
	ret	4
?GetStats@Stat_detail@@UAEXPAD@Z ENDP			; Stat_detail::GetStats
_TEXT	ENDS
PUBLIC	?R_InitColumnDrawers@@YAXXZ			; R_InitColumnDrawers
EXTRN	_rt_map4cols_asm1:NEAR
EXTRN	_rt_map4cols_asm2:NEAR
EXTRN	_R_DrawColumnHorizP_ASM:NEAR
EXTRN	_R_DrawColumnP_ASM:NEAR
EXTRN	_R_DrawFuzzColumnP_ASM:NEAR
EXTRN	_R_DrawSpanP_ASM:NEAR
;	COMDAT ?R_InitColumnDrawers@@YAXXZ
_TEXT	SEGMENT
?R_InitColumnDrawers@@YAXXZ PROC NEAR			; R_InitColumnDrawers, COMDAT

; 1321 : #ifdef USEASM
; 1322 : 	R_DrawColumn			= R_DrawColumnP_ASM;
; 1323 : 	R_DrawColumnHoriz		= R_DrawColumnHorizP_ASM;
; 1324 : 	R_DrawFuzzColumn		= R_DrawFuzzColumnP_ASM;
; 1325 : 	R_DrawTranslatedColumn	= R_DrawTranslatedColumnP_C;
; 1326 : 	R_DrawShadedColumn		= R_DrawShadedColumnP_C;
; 1327 : 	R_DrawSpan				= R_DrawSpanP_ASM;
; 1328 : 	if (CPUFamily <= 5)

	mov	al, BYTE PTR _CPUFamily
	mov	DWORD PTR ?R_DrawColumn@@3P6AXXZA, OFFSET FLAT:_R_DrawColumnP_ASM ; R_DrawColumn
	cmp	al, 5
	mov	DWORD PTR ?R_DrawColumnHoriz@@3P6AXXZA, OFFSET FLAT:_R_DrawColumnHorizP_ASM ; R_DrawColumnHoriz
	mov	DWORD PTR ?R_DrawFuzzColumn@@3P6AXXZA, OFFSET FLAT:_R_DrawFuzzColumnP_ASM ; R_DrawFuzzColumn
	mov	DWORD PTR ?R_DrawTranslatedColumn@@3P6AXXZA, OFFSET FLAT:?R_DrawTranslatedColumnP_C@@YAXXZ ; R_DrawTranslatedColumn, R_DrawTranslatedColumnP_C
	mov	DWORD PTR ?R_DrawShadedColumn@@3P6AXXZA, OFFSET FLAT:?R_DrawShadedColumnP_C@@YAXXZ ; R_DrawShadedColumn, R_DrawShadedColumnP_C
	mov	DWORD PTR ?R_DrawSpan@@3P6AXXZA, OFFSET FLAT:_R_DrawSpanP_ASM ; R_DrawSpan

; 1329 : 	{
; 1330 : 		rt_map4cols			= rt_map4cols_asm2;

	mov	DWORD PTR ?rt_map4cols@@3P6AXHHH@ZA, OFFSET FLAT:_rt_map4cols_asm2 ; rt_map4cols
	jbe	SHORT $L10435

; 1331 : 	}
; 1332 : 	else
; 1333 : 	{
; 1334 : 		rt_map4cols			= rt_map4cols_asm1;

	mov	DWORD PTR ?rt_map4cols@@3P6AXHHH@ZA, OFFSET FLAT:_rt_map4cols_asm1 ; rt_map4cols
$L10435:

; 1335 : 	}
; 1336 : #else
; 1337 : 	R_DrawColumnHoriz		= R_DrawColumnHorizP_C;
; 1338 : 	R_DrawColumn			= R_DrawColumnP_C;
; 1339 : 	R_DrawFuzzColumn		= R_DrawFuzzColumnP_C;
; 1340 : 	R_DrawTranslatedColumn	= R_DrawTranslatedColumnP_C;
; 1341 : 	R_DrawShadedColumn		= R_DrawShadedColumnP_C;
; 1342 : 	R_DrawSpan				= R_DrawSpanP_C;
; 1343 : 	rt_map4cols				= rt_map4cols_c;
; 1344 : #endif
; 1345 : }

	ret	0
?R_InitColumnDrawers@@YAXXZ ENDP			; R_InitColumnDrawers
_TEXT	ENDS
;	COMDAT _$E60
_TEXT	SEGMENT
_$E60	PROC NEAR					; COMDAT
	call	_$E57
	jmp	_$E59
_$E60	ENDP
_TEXT	ENDS
PUBLIC	??_C@_0M@EMGL@r_drawtrans?$AA@			; `string'
EXTRN	??0FBoolCVar@@QAE@PBD_NKP6AXAAV0@@Z@Z:NEAR	; FBoolCVar::FBoolCVar
;	COMDAT ??_C@_0M@EMGL@r_drawtrans?$AA@
; File r_draw.cpp
_DATA	SEGMENT
??_C@_0M@EMGL@r_drawtrans?$AA@ DB 'r_drawtrans', 00H	; `string'
_DATA	ENDS
;	COMDAT _$E57
_TEXT	SEGMENT
_$E57	PROC NEAR					; COMDAT

; 1350 : CVAR (Bool, r_drawtrans, true, 0)

	push	0
	push	0
	push	1
	push	OFFSET FLAT:??_C@_0M@EMGL@r_drawtrans?$AA@ ; `string'
	mov	ecx, OFFSET FLAT:?r_drawtrans@@3VFBoolCVar@@A
	call	??0FBoolCVar@@QAE@PBD_NKP6AXAAV0@@Z@Z	; FBoolCVar::FBoolCVar
	ret	0
_$E57	ENDP
_TEXT	ENDS
;	COMDAT _$E59
_TEXT	SEGMENT
_$E59	PROC NEAR					; COMDAT
	push	OFFSET FLAT:_$E58
	call	_atexit
	pop	ecx
	ret	0
_$E59	ENDP
_TEXT	ENDS
EXTRN	??1FBaseCVar@@UAE@XZ:NEAR			; FBaseCVar::~FBaseCVar
;	COMDAT _$E58
_TEXT	SEGMENT
_$E58	PROC NEAR					; COMDAT
	mov	ecx, OFFSET FLAT:?r_drawtrans@@3VFBoolCVar@@A
	jmp	??1FBaseCVar@@UAE@XZ			; FBaseCVar::~FBaseCVar
_$E58	ENDP
_TEXT	ENDS
PUBLIC	?R_SetPatchStyle@@YA?AW4ESPSResult@@HJPAEK@Z	; R_SetPatchStyle
PUBLIC	__real@4@400f8000000000000000
EXTRN	?r_drawfuzz@@3VFBoolCVar@@A:BYTE		; r_drawfuzz
EXTRN	?transsouls@@3VFFloatCVar@@A:BYTE		; transsouls
EXTRN	?basecolormap@@3PAEA:DWORD			; basecolormap
EXTRN	?fixedlightlev@@3HA:DWORD			; fixedlightlev
EXTRN	?fixedcolormap@@3PAEA:DWORD			; fixedcolormap
EXTRN	?r_MarkTrans@@3_NA:BYTE				; r_MarkTrans
EXTRN	?colfunc@@3P6AXXZA:DWORD			; colfunc
EXTRN	?fuzzcolfunc@@3P6AXXZA:DWORD			; fuzzcolfunc
EXTRN	?hcolfunc_post1@@3P6AXHHHH@ZA:DWORD		; hcolfunc_post1
EXTRN	?hcolfunc_post4@@3P6AXHHH@ZA:DWORD		; hcolfunc_post4
EXTRN	?r_columnmethod@@3VFIntCVar@@A:BYTE		; r_columnmethod
EXTRN	?rt_shaded1col@@YAXHHHH@Z:NEAR			; rt_shaded1col
EXTRN	?rt_shaded4cols@@YAXHHH@Z:NEAR			; rt_shaded4cols
_BSS	SEGMENT
_basecolormapsave DD 01H DUP (?)
_BSS	ENDS
;	COMDAT __real@4@400f8000000000000000
; File c_cvars.h
CONST	SEGMENT
__real@4@400f8000000000000000 DD 047800000r	; 65536
CONST	ENDS
;	COMDAT ?R_SetPatchStyle@@YA?AW4ESPSResult@@HJPAEK@Z
_TEXT	SEGMENT
_style$ = 8
_alpha$ = 12
_translation$ = 16
_color$ = 20
?R_SetPatchStyle@@YA?AW4ESPSResult@@HJPAEK@Z PROC NEAR	; R_SetPatchStyle, COMDAT

; 1422 : {

	push	esi

; 1423 : 	fixed_t fglevel, bglevel;
; 1424 : 
; 1425 : 	if (style == STYLE_OptFuzzy)

	mov	esi, DWORD PTR _style$[esp]
	cmp	esi, 4
	push	edi
	jne	SHORT $L10475

; 1426 : 	{
; 1427 : 		style = (r_drawfuzz || !r_drawtrans) ? STYLE_Fuzzy : STYLE_Translucent;

	mov	al, BYTE PTR ?r_drawfuzz@@3VFBoolCVar@@A+20
	test	al, al
	jne	SHORT $L11096
	mov	al, BYTE PTR ?r_drawtrans@@3VFBoolCVar@@A+20
	test	al, al
	je	SHORT $L11096
	mov	esi, 64					; 00000040H
$L11135:

; 1430 : 	{
; 1431 : 		style = STYLE_Translucent;
; 1432 : 		alpha = (fixed_t)(FRACUNIT * transsouls);

	mov	eax, DWORD PTR _alpha$[esp+4]
$L10477:

; 1433 : 	}
; 1434 : 
; 1435 : 	alpha = clamp<fixed_t> (alpha, 0, FRACUNIT);

	test	eax, eax
	jg	SHORT $L11123
	xor	eax, eax
	jmp	SHORT $L11121
$L11096:

; 1426 : 	{
; 1427 : 		style = (r_drawfuzz || !r_drawtrans) ? STYLE_Fuzzy : STYLE_Translucent;

	mov	esi, 2

; 1428 : 	}
; 1429 : 	else if (style == STYLE_SoulTrans)

	jmp	SHORT $L11135
$L10475:
	cmp	esi, 3
	jne	SHORT $L11135

; 1430 : 	{
; 1431 : 		style = STYLE_Translucent;
; 1432 : 		alpha = (fixed_t)(FRACUNIT * transsouls);

	fld	DWORD PTR ?transsouls@@3VFFloatCVar@@A+20
	fmul	DWORD PTR __real@4@400f8000000000000000
	mov	esi, 64					; 00000040H
	call	__ftol
	jmp	SHORT $L10477

; 1433 : 	}
; 1434 : 
; 1435 : 	alpha = clamp<fixed_t> (alpha, 0, FRACUNIT);

$L11123:
	cmp	eax, 65536				; 00010000H
	jl	SHORT $L11121
	mov	eax, 65536				; 00010000H
$L11121:

; 1436 : 
; 1437 : 	dc_translation = translation;

	mov	ecx, DWORD PTR _translation$[esp+4]

; 1438 : 	basecolormapsave = basecolormap;

	mov	edi, DWORD PTR ?basecolormap@@3PAEA	; basecolormap
	mov	DWORD PTR ?dc_translation@@3PAEA, ecx	; dc_translation

; 1439 : 
; 1440 : 	switch (style)
; 1441 : 	{

	lea	ecx, DWORD PTR [esi-1]
	cmp	ecx, 65					; 00000041H
	mov	DWORD PTR _basecolormapsave, edi
	ja	$L11100
	xor	edx, edx
	mov	dl, BYTE PTR $L11136[ecx]
	jmp	DWORD PTR $L11137[edx*4]
$L10489:

; 1442 : 		// Special modes
; 1443 : 	case STYLE_Fuzzy:
; 1444 : 		colfunc = fuzzcolfunc;

	mov	eax, DWORD PTR ?fuzzcolfunc@@3P6AXXZA	; fuzzcolfunc
	pop	edi
	mov	DWORD PTR ?colfunc@@3P6AXXZA, eax	; colfunc

; 1445 : 		r_MarkTrans = true;

	mov	eax, 1
	mov	BYTE PTR ?r_MarkTrans@@3_NA, al		; r_MarkTrans
	pop	esi

; 1485 : }

	ret	0
$L10490:

; 1446 : 		return DoDraw0;
; 1447 : 
; 1448 : 	case STYLE_Shaded:
; 1449 : 		// Shaded drawer only gets 16 levels because it saves memory.
; 1450 : 		if ((alpha >>= 12) == 0)

	sar	eax, 12					; 0000000cH

; 1451 : 			return DontDraw;

	je	$L11100

; 1452 : 		colfunc = R_DrawShadedColumn;

	mov	ecx, DWORD PTR ?R_DrawShadedColumn@@3P6AXXZA ; R_DrawShadedColumn

; 1453 : 		hcolfunc_post1 = rt_shaded1col;

	mov	DWORD PTR ?hcolfunc_post1@@3P6AXHHHH@ZA, OFFSET FLAT:?rt_shaded1col@@YAXHHHH@Z ; hcolfunc_post1, rt_shaded1col
	mov	DWORD PTR ?colfunc@@3P6AXXZA, ecx	; colfunc

; 1454 : 		hcolfunc_post4 = rt_shaded4cols;
; 1455 : 		dc_color = fixedcolormap ? fixedcolormap[APART(color)] : basecolormap[APART(color)];

	mov	ecx, DWORD PTR ?fixedcolormap@@3PAEA	; fixedcolormap
	test	ecx, ecx
	mov	DWORD PTR ?hcolfunc_post4@@3P6AXHHH@ZA, OFFSET FLAT:?rt_shaded4cols@@YAXHHH@Z ; hcolfunc_post4, rt_shaded4cols
	je	SHORT $L11098
	mov	edx, DWORD PTR _color$[esp+4]
	shr	edx, 24					; 00000018H
	mov	cl, BYTE PTR [edx+ecx]
	jmp	SHORT $L11099
$L11098:
	mov	ecx, DWORD PTR _color$[esp+4]
	shr	ecx, 24					; 00000018H
	mov	cl, BYTE PTR [ecx+edi]
$L11099:

; 1456 : 		dc_colormap = basecolormap = &translationtables[(MAXPLAYERS*2+3+(16-alpha)*NUMCOLORMAPS)*256];

	shl	eax, 13					; 0000000dH
	mov	edx, eax
	mov	eax, DWORD PTR ?translationtables@@3PAEA ; translationtables
	and	ecx, 255				; 000000ffH
	sub	eax, edx
	mov	DWORD PTR _dc_color, ecx

; 1457 : 		if (fixedlightlev)

	mov	ecx, DWORD PTR ?fixedlightlev@@3HA	; fixedlightlev
	add	eax, 135936				; 00021300H
	test	ecx, ecx
	mov	DWORD PTR ?basecolormap@@3PAEA, eax	; basecolormap
	mov	DWORD PTR _dc_colormap, eax
	je	SHORT $L10492

; 1458 : 		{
; 1459 : 			dc_colormap += fixedlightlev;

	add	ecx, eax
	mov	DWORD PTR _dc_colormap, ecx
$L10492:

; 1460 : 		}
; 1461 : 		r_MarkTrans = true;
; 1462 : 		return r_columnmethod ? DoDraw1 : DoDraw0;

	mov	edx, DWORD PTR ?r_columnmethod@@3VFIntCVar@@A+20
	xor	eax, eax
	test	edx, edx
	setne	al
	pop	edi
	mov	BYTE PTR ?r_MarkTrans@@3_NA, 1		; r_MarkTrans
	inc	eax
	pop	esi

; 1485 : }

	ret	0
$L10493:

; 1463 : 
; 1464 : 		// Standard modes
; 1465 : 	case STYLE_Normal:
; 1466 : 		fglevel = BL_ONE;

	mov	eax, 65536				; 00010000H

; 1467 : 		bglevel = BL_ZERO;

	xor	ecx, ecx

; 1468 : 		break;

	jmp	SHORT $L10486
$L10494:

; 1469 : 
; 1470 : 	case STYLE_Translucent:
; 1471 : 		fglevel = BL_SRC_ALPHA;
; 1472 : 		bglevel = BL_INV_SRC_ALPHA;

	mov	ecx, 65536				; 00010000H
	sub	ecx, eax

; 1473 : 		break;

	jmp	SHORT $L10486
$L10495:

; 1474 : 
; 1475 : 	case STYLE_Add:
; 1476 : 		fglevel = BL_SRC_ALPHA;
; 1477 : 		bglevel = BL_ONE;

	mov	ecx, 65536				; 00010000H
$L10486:

; 1482 : 	}
; 1483 : 	return R_SetBlendFunc (fglevel, bglevel) ?
; 1484 : 		(r_columnmethod ? DoDraw1 : DoDraw0) : DontDraw;

	push	ecx
	push	eax
	call	?R_SetBlendFunc@@YA_NJJ@Z		; R_SetBlendFunc
	add	esp, 8
	test	al, al
	je	SHORT $L11100
	mov	edx, DWORD PTR ?r_columnmethod@@3VFIntCVar@@A+20
	xor	eax, eax
	test	edx, edx
	setne	al
	pop	edi
	inc	eax
	pop	esi

; 1485 : }

	ret	0
$L11100:
	pop	edi

; 1478 : 		break;
; 1479 : 
; 1480 : 	default:
; 1481 : 		return DontDraw;

	xor	eax, eax
	pop	esi

; 1485 : }

	ret	0
	npad	1
$L11137:
	DD	$L10493
	DD	$L10489
	DD	$L10494
	DD	$L10495
	DD	$L10490
	DD	$L11100
$L11136:
	DB	0
	DB	1
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	5
	DB	2
	DB	3
	DB	4
?R_SetPatchStyle@@YA?AW4ESPSResult@@HJPAEK@Z ENDP	; R_SetPatchStyle
_TEXT	ENDS
EXTRN	?basecolfunc@@3P6AXXZA:DWORD			; basecolfunc
EXTRN	?transcolfunc@@3P6AXXZA:DWORD			; transcolfunc
EXTRN	_Col2RGB8_LessPrecision:BYTE
EXTRN	?rt_add1col@@YAXHHHH@Z:NEAR			; rt_add1col
EXTRN	?rt_add4cols@@YAXHHH@Z:NEAR			; rt_add4cols
EXTRN	?rt_tlate1col@@YAXHHHH@Z:NEAR			; rt_tlate1col
EXTRN	?rt_tlate4cols@@YAXHHH@Z:NEAR			; rt_tlate4cols
EXTRN	?rt_tlateadd1col@@YAXHHHH@Z:NEAR		; rt_tlateadd1col
EXTRN	?rt_tlateadd4cols@@YAXHHH@Z:NEAR		; rt_tlateadd4cols
EXTRN	?rt_addclamp1col@@YAXHHHH@Z:NEAR		; rt_addclamp1col
EXTRN	?rt_addclamp4cols@@YAXHHH@Z:NEAR		; rt_addclamp4cols
EXTRN	?rt_tlateaddclamp1col@@YAXHHHH@Z:NEAR		; rt_tlateaddclamp1col
EXTRN	?rt_tlateaddclamp4cols@@YAXHHH@Z:NEAR		; rt_tlateaddclamp4cols
EXTRN	_rt_map1col_asm:NEAR
;	COMDAT ?R_SetBlendFunc@@YA_NJJ@Z
_TEXT	SEGMENT
_fglevel$ = 8
_bglevel$ = 12
?R_SetBlendFunc@@YA_NJJ@Z PROC NEAR			; R_SetBlendFunc, COMDAT

; 1362 : 	if (!r_drawtrans || (fglevel == BL_ONE && bglevel == BL_ZERO))

	mov	al, BYTE PTR ?r_drawtrans@@3VFBoolCVar@@A+20
	test	al, al
	je	$L10457
	mov	eax, DWORD PTR _fglevel$[esp-4]
	mov	ecx, DWORD PTR _bglevel$[esp-4]
	cmp	eax, 65536				; 00010000H
	jne	SHORT $L10456
	test	ecx, ecx
	je	$L10457
$L10460:

; 1382 : 	}
; 1383 : 	if (fglevel + bglevel <= BL_ONE)

	lea	edx, DWORD PTR [eax+ecx]
	cmp	edx, 65536				; 00010000H
	jg	$L10461

; 1384 : 	{ // Colors won't overflow when added
; 1385 : 		dc_srcblend = Col2RGB8[fglevel>>10];

	sar	eax, 10					; 0000000aH
	shl	eax, 10					; 0000000aH
	add	eax, OFFSET FLAT:_Col2RGB8

; 1386 : 		dc_destblend = Col2RGB8[bglevel>>10];

	sar	ecx, 10					; 0000000aH
	mov	DWORD PTR _dc_srcblend, eax

; 1387 : 		if (dc_translation == NULL)

	mov	eax, DWORD PTR ?dc_translation@@3PAEA	; dc_translation
	shl	ecx, 10					; 0000000aH
	add	ecx, OFFSET FLAT:_Col2RGB8
	test	eax, eax
	mov	DWORD PTR _dc_destblend, ecx
	jne	SHORT $L10462

; 1405 : 		{
; 1406 : 			colfunc = R_DrawAddClampColumnP_C;
; 1407 : 			hcolfunc_post1 = rt_addclamp1col;
; 1408 : 			hcolfunc_post4 = rt_addclamp4cols;
; 1409 : 		}
; 1410 : 		else
; 1411 : 		{
; 1412 : 			colfunc = R_DrawAddClampTranslatedColumnP_C;
; 1413 : 			hcolfunc_post1 = rt_tlateaddclamp1col;
; 1414 : 			hcolfunc_post4 = rt_tlateaddclamp4cols;
; 1415 : 		}
; 1416 : 	}
; 1417 : 	r_MarkTrans = true;

	mov	al, 1
	mov	DWORD PTR ?colfunc@@3P6AXXZA, OFFSET FLAT:?R_DrawAddColumnP_C@@YAXXZ ; colfunc, R_DrawAddColumnP_C
	mov	DWORD PTR ?hcolfunc_post1@@3P6AXHHHH@ZA, OFFSET FLAT:?rt_add1col@@YAXHHHH@Z ; hcolfunc_post1, rt_add1col
	mov	DWORD PTR ?hcolfunc_post4@@3P6AXHHH@ZA, OFFSET FLAT:?rt_add4cols@@YAXHHH@Z ; hcolfunc_post4, rt_add4cols
	mov	BYTE PTR ?r_MarkTrans@@3_NA, al		; r_MarkTrans

; 1418 : 	return true;
; 1419 : }

	ret	0
$L10456:

; 1378 : 	}
; 1379 : 	if (fglevel == BL_ZERO && bglevel == BL_ONE)

	test	eax, eax
	jne	SHORT $L10460
	cmp	ecx, 65536				; 00010000H
	jne	SHORT $L10460

; 1380 : 	{
; 1381 : 		return false;

	xor	al, al

; 1418 : 	return true;
; 1419 : }

	ret	0
$L10462:

; 1405 : 		{
; 1406 : 			colfunc = R_DrawAddClampColumnP_C;
; 1407 : 			hcolfunc_post1 = rt_addclamp1col;
; 1408 : 			hcolfunc_post4 = rt_addclamp4cols;
; 1409 : 		}
; 1410 : 		else
; 1411 : 		{
; 1412 : 			colfunc = R_DrawAddClampTranslatedColumnP_C;
; 1413 : 			hcolfunc_post1 = rt_tlateaddclamp1col;
; 1414 : 			hcolfunc_post4 = rt_tlateaddclamp4cols;
; 1415 : 		}
; 1416 : 	}
; 1417 : 	r_MarkTrans = true;

	mov	al, 1
	mov	DWORD PTR ?colfunc@@3P6AXXZA, OFFSET FLAT:?R_DrawTlatedAddColumnP_C@@YAXXZ ; colfunc, R_DrawTlatedAddColumnP_C
	mov	DWORD PTR ?hcolfunc_post1@@3P6AXHHHH@ZA, OFFSET FLAT:?rt_tlateadd1col@@YAXHHHH@Z ; hcolfunc_post1, rt_tlateadd1col
	mov	DWORD PTR ?hcolfunc_post4@@3P6AXHHH@ZA, OFFSET FLAT:?rt_tlateadd4cols@@YAXHHH@Z ; hcolfunc_post4, rt_tlateadd4cols
	mov	BYTE PTR ?r_MarkTrans@@3_NA, al		; r_MarkTrans

; 1418 : 	return true;
; 1419 : }

	ret	0
$L10461:

; 1388 : 		{
; 1389 : 			colfunc = R_DrawAddColumnP_C;
; 1390 : 			hcolfunc_post1 = rt_add1col;
; 1391 : 			hcolfunc_post4 = rt_add4cols;
; 1392 : 		}
; 1393 : 		else
; 1394 : 		{
; 1395 : 			colfunc = R_DrawTlatedAddColumnP_C;
; 1396 : 			hcolfunc_post1 = rt_tlateadd1col;
; 1397 : 			hcolfunc_post4 = rt_tlateadd4cols;
; 1398 : 		}
; 1399 : 	}
; 1400 : 	else
; 1401 : 	{ // Colors might overflow when added
; 1402 : 		dc_srcblend = Col2RGB8_LessPrecision[fglevel>>10];

	sar	eax, 10					; 0000000aH

; 1403 : 		dc_destblend = Col2RGB8_LessPrecision[bglevel>>10];

	sar	ecx, 10					; 0000000aH
	mov	eax, DWORD PTR _Col2RGB8_LessPrecision[eax*4]
	mov	ecx, DWORD PTR _Col2RGB8_LessPrecision[ecx*4]
	mov	DWORD PTR _dc_srcblend, eax

; 1404 : 		if (dc_translation == NULL)

	mov	eax, DWORD PTR ?dc_translation@@3PAEA	; dc_translation
	mov	DWORD PTR _dc_destblend, ecx
	test	eax, eax
	jne	SHORT $L10465

; 1405 : 		{
; 1406 : 			colfunc = R_DrawAddClampColumnP_C;
; 1407 : 			hcolfunc_post1 = rt_addclamp1col;
; 1408 : 			hcolfunc_post4 = rt_addclamp4cols;
; 1409 : 		}
; 1410 : 		else
; 1411 : 		{
; 1412 : 			colfunc = R_DrawAddClampTranslatedColumnP_C;
; 1413 : 			hcolfunc_post1 = rt_tlateaddclamp1col;
; 1414 : 			hcolfunc_post4 = rt_tlateaddclamp4cols;
; 1415 : 		}
; 1416 : 	}
; 1417 : 	r_MarkTrans = true;

	mov	al, 1
	mov	DWORD PTR ?colfunc@@3P6AXXZA, OFFSET FLAT:?R_DrawAddClampColumnP_C@@YAXXZ ; colfunc, R_DrawAddClampColumnP_C
	mov	DWORD PTR ?hcolfunc_post1@@3P6AXHHHH@ZA, OFFSET FLAT:?rt_addclamp1col@@YAXHHHH@Z ; hcolfunc_post1, rt_addclamp1col
	mov	DWORD PTR ?hcolfunc_post4@@3P6AXHHH@ZA, OFFSET FLAT:?rt_addclamp4cols@@YAXHHH@Z ; hcolfunc_post4, rt_addclamp4cols
	mov	BYTE PTR ?r_MarkTrans@@3_NA, al		; r_MarkTrans

; 1418 : 	return true;
; 1419 : }

	ret	0
$L10465:

; 1405 : 		{
; 1406 : 			colfunc = R_DrawAddClampColumnP_C;
; 1407 : 			hcolfunc_post1 = rt_addclamp1col;
; 1408 : 			hcolfunc_post4 = rt_addclamp4cols;
; 1409 : 		}
; 1410 : 		else
; 1411 : 		{
; 1412 : 			colfunc = R_DrawAddClampTranslatedColumnP_C;
; 1413 : 			hcolfunc_post1 = rt_tlateaddclamp1col;
; 1414 : 			hcolfunc_post4 = rt_tlateaddclamp4cols;
; 1415 : 		}
; 1416 : 	}
; 1417 : 	r_MarkTrans = true;

	mov	al, 1
	mov	DWORD PTR ?colfunc@@3P6AXXZA, OFFSET FLAT:?R_DrawAddClampTranslatedColumnP_C@@YAXXZ ; colfunc, R_DrawAddClampTranslatedColumnP_C
	mov	DWORD PTR ?hcolfunc_post1@@3P6AXHHHH@ZA, OFFSET FLAT:?rt_tlateaddclamp1col@@YAXHHHH@Z ; hcolfunc_post1, rt_tlateaddclamp1col
	mov	DWORD PTR ?hcolfunc_post4@@3P6AXHHH@ZA, OFFSET FLAT:?rt_tlateaddclamp4cols@@YAXHHH@Z ; hcolfunc_post4, rt_tlateaddclamp4cols
	mov	BYTE PTR ?r_MarkTrans@@3_NA, al		; r_MarkTrans

; 1418 : 	return true;
; 1419 : }

	ret	0
$L10457:

; 1363 : 	{
; 1364 : 		if (dc_translation == NULL)

	mov	eax, DWORD PTR ?dc_translation@@3PAEA	; dc_translation
	test	eax, eax
	jne	SHORT $L10458

; 1365 : 		{
; 1366 : 			colfunc = basecolfunc;
; 1367 : 			hcolfunc_post1 = rt_map1col;
; 1368 : 			hcolfunc_post4 = rt_map4cols;

	mov	eax, DWORD PTR ?rt_map4cols@@3P6AXHHH@ZA ; rt_map4cols
	mov	edx, DWORD PTR ?basecolfunc@@3P6AXXZA	; basecolfunc
	mov	DWORD PTR ?hcolfunc_post4@@3P6AXHHH@ZA, eax ; hcolfunc_post4
	mov	DWORD PTR ?colfunc@@3P6AXXZA, edx	; colfunc
	mov	DWORD PTR ?hcolfunc_post1@@3P6AXHHHH@ZA, OFFSET FLAT:_rt_map1col_asm ; hcolfunc_post1

; 1375 : 		}
; 1376 : 		r_MarkTrans = false;

	mov	BYTE PTR ?r_MarkTrans@@3_NA, 0		; r_MarkTrans

; 1377 : 		return true;

	mov	al, 1

; 1418 : 	return true;
; 1419 : }

	ret	0
$L10458:

; 1369 : 		}
; 1370 : 		else
; 1371 : 		{
; 1372 : 			colfunc = transcolfunc;

	mov	ecx, DWORD PTR ?transcolfunc@@3P6AXXZA	; transcolfunc

; 1373 : 			hcolfunc_post1 = rt_tlate1col;

	mov	DWORD PTR ?hcolfunc_post1@@3P6AXHHHH@ZA, OFFSET FLAT:?rt_tlate1col@@YAXHHHH@Z ; hcolfunc_post1, rt_tlate1col
	mov	DWORD PTR ?colfunc@@3P6AXXZA, ecx	; colfunc

; 1374 : 			hcolfunc_post4 = rt_tlate4cols;

	mov	DWORD PTR ?hcolfunc_post4@@3P6AXHHH@ZA, OFFSET FLAT:?rt_tlate4cols@@YAXHHH@Z ; hcolfunc_post4, rt_tlate4cols

; 1375 : 		}
; 1376 : 		r_MarkTrans = false;

	mov	BYTE PTR ?r_MarkTrans@@3_NA, 0		; r_MarkTrans

; 1377 : 		return true;

	mov	al, 1

; 1418 : 	return true;
; 1419 : }

	ret	0
?R_SetBlendFunc@@YA_NJJ@Z ENDP				; R_SetBlendFunc
_TEXT	ENDS
PUBLIC	?R_FinishSetPatchStyle@@YAXXZ			; R_FinishSetPatchStyle
;	COMDAT ?R_FinishSetPatchStyle@@YAXXZ
_TEXT	SEGMENT
?R_FinishSetPatchStyle@@YAXXZ PROC NEAR			; R_FinishSetPatchStyle, COMDAT

; 1489 : 	basecolormap = basecolormapsave;

	mov	eax, DWORD PTR _basecolormapsave
	mov	DWORD PTR ?basecolormap@@3PAEA, eax	; basecolormap

; 1490 : }

	ret	0
?R_FinishSetPatchStyle@@YAXXZ ENDP			; R_FinishSetPatchStyle
_TEXT	ENDS
END
