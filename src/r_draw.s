	.file	"r_draw.cpp"
gcc2_compiled.:
___gnu_compiled_cplusplus:
	.def	___terminate;	.scl	2;	.type	32;	.endef
	.def	___sjthrow;	.scl	2;	.type	32;	.endef
.globl _dc_pitch
.data
	.align 4
_dc_pitch:
	.long 305419896
.text
	.align 4
.globl _R_StretchColumnP_C__Fv
	.def	_R_StretchColumnP_C__Fv;	.scl	2;	.type	32;	.endef
_R_StretchColumnP_C__Fv:
	pushl %ebp
	movl _dc_yl,%eax
	pushl %edi
	movl _dc_yh,%ecx
	pushl %esi
	subl %eax,%ecx
	pushl %ebx
	js L1122
	movl _dc_x,%edx
	incl %ecx
	addl _ylookup(,%eax,4),%edx
	movl _dc_iscale,%ebp
	movl _dc_texturefrac,%ebx
	movl _dc_source,%edi
	movl _dc_pitch,%esi
	.align 4
L1127:
	movl %ebx,%eax
	sarl $16,%eax
	addl %ebp,%ebx
	movb (%eax,%edi),%al
	movb %al,(%edx)
	addl %esi,%edx
	decl %ecx
	jnz L1127
L1122:
	popl %ebx
	popl %esi
	popl %edi
	popl %ebp
	ret
	.align 4
.globl _R_FillColumnP__Fv
	.def	_R_FillColumnP__Fv;	.scl	2;	.type	32;	.endef
_R_FillColumnP__Fv:
	movl _dc_yl,%ecx
	pushl %ebx
	movl _dc_yh,%edx
	subl %ecx,%edx
	js L1128
	movl _dc_x,%eax
	incl %edx
	addl _ylookup(,%ecx,4),%eax
	movl _dc_pitch,%ebx
	movb _dc_color,%cl
	.align 4
L1133:
	movb %cl,(%eax)
	addl %ebx,%eax
	decl %edx
	jnz L1133
L1128:
	popl %ebx
	ret
.globl _fuzzpos
.data
	.align 4
_fuzzpos:
	.long 0
.text
	.align 4
.globl _R_InitFuzzTable__Fi
	.def	_R_InitFuzzTable__Fi;	.scl	2;	.type	32;	.endef
_R_InitFuzzTable__Fi:
	pushl %esi
	xorl %edx,%edx
	pushl %ebx
	movl $_fuzzinit,%ecx
	movl 12(%esp),%esi
	movl $_fuzzoffset,%ebx
	.align 4
L1138:
	movsbl (%edx,%ecx),%eax
	imull %esi,%eax
	movl %eax,(%ebx,%edx,4)
	incl %edx
	cmpl $49,%edx
	jle L1138
	popl %ebx
	popl %esi
	ret
	.align 4
.globl _R_DrawAddColumnP_C__Fv
	.def	_R_DrawAddColumnP_C__Fv;	.scl	2;	.type	32;	.endef
_R_DrawAddColumnP_C__Fv:
	subl $44,%esp
	movl _dc_yl,%eax
	pushl %ebp
	pushl %edi
	pushl %esi
	pushl %ebx
	movl _dc_yh,%ebx
	subl %eax,%ebx
	js L1140
	movl _dc_x,%ecx
	incl %ebx
	addl _ylookup(,%eax,4),%ecx
	movl _dc_iscale,%eax
	movl _dc_srcblend,%edx
	movl %eax,44(%esp)
	movl %edx,40(%esp)
	movl _dc_texturefrac,%esi
	movl _dc_pitch,%edi
	movl _dc_destblend,%ebp
	movl _dc_colormap,%eax
	movl %ebp,36(%esp)
	movl _dc_source,%edx
	movl %eax,32(%esp)
	movl %edx,28(%esp)
	.align 4
L1145:
	movl 28(%esp),%ebp
	movl %esi,%eax
	sarl $16,%eax
	movzbl (%eax,%ebp),%eax
	movl 32(%esp),%ebp
	movzbl (%eax,%ebp),%edx
	movzbl (%ecx),%eax
	movl 36(%esp),%ebp
	movl (%ebp,%eax,4),%eax
	movl 40(%esp),%ebp
	movl (%ebp,%edx,4),%edx
	addl %eax,%edx
	orl $32537631,%edx
	movl %edx,%eax
	shrl $15,%eax
	andl %eax,%edx
	movb _RGB32k(%edx),%al
	movb %al,(%ecx)
	addl %edi,%ecx
	addl 44(%esp),%esi
	decl %ebx
	jnz L1145
L1140:
	popl %ebx
	popl %esi
	popl %edi
	popl %ebp
	addl $44,%esp
	ret
	.align 4
.globl _R_DrawTranslatedColumnP_C__Fv
	.def	_R_DrawTranslatedColumnP_C__Fv;	.scl	2;	.type	32;	.endef
_R_DrawTranslatedColumnP_C__Fv:
	subl $44,%esp
	movl _dc_yl,%eax
	pushl %ebp
	movl _dc_yh,%ecx
	pushl %edi
	subl %eax,%ecx
	pushl %esi
	pushl %ebx
	js L1146
	movl _dc_x,%edx
	incl %ecx
	addl _ylookup(,%eax,4),%edx
	movl _dc_iscale,%eax
	movl %eax,44(%esp)
	movl _dc_texturefrac,%ebx
	movl _dc_source,%edi
	movl _dc_pitch,%esi
	movl _dc_colormap,%ebp
	movl _dc_translation,%eax
	movl %ebp,40(%esp)
	movl %eax,28(%esp)
	.align 4
L1151:
	movl %ebx,%eax
	movl 28(%esp),%ebp
	sarl $16,%eax
	movzbl (%eax,%edi),%eax
	movzbl (%eax,%ebp),%eax
	movl 40(%esp),%ebp
	movb (%eax,%ebp),%al
	movb %al,(%edx)
	addl %esi,%edx
	addl 44(%esp),%ebx
	decl %ecx
	jnz L1151
L1146:
	popl %ebx
	popl %esi
	popl %edi
	popl %ebp
	addl $44,%esp
	ret
	.align 4
.globl _R_DrawTlatedAddColumnP_C__Fv
	.def	_R_DrawTlatedAddColumnP_C__Fv;	.scl	2;	.type	32;	.endef
_R_DrawTlatedAddColumnP_C__Fv:
	subl $60,%esp
	movl _dc_yl,%eax
	pushl %ebp
	pushl %edi
	pushl %esi
	pushl %ebx
	movl _dc_yh,%ebx
	subl %eax,%ebx
	js L1152
	movl _dc_x,%ecx
	incl %ebx
	addl _ylookup(,%eax,4),%ecx
	movl _dc_iscale,%eax
	movl _dc_srcblend,%edx
	movl %eax,60(%esp)
	movl _dc_destblend,%ebp
	movl %edx,56(%esp)
	movl %ebp,52(%esp)
	movl _dc_texturefrac,%esi
	movl _dc_pitch,%edi
	movl _dc_translation,%eax
	movl _dc_colormap,%edx
	movl %eax,48(%esp)
	movl _dc_source,%ebp
	movl %edx,44(%esp)
	movl %ebp,28(%esp)
	.align 4
L1157:
	movl 28(%esp),%edx
	movl %esi,%eax
	sarl $16,%eax
	movl 48(%esp),%ebp
	movzbl (%eax,%edx),%eax
	movzbl (%eax,%ebp),%eax
	movl 44(%esp),%ebp
	movzbl (%eax,%ebp),%edx
	movzbl (%ecx),%eax
	movl 52(%esp),%ebp
	movl (%ebp,%eax,4),%eax
	movl 56(%esp),%ebp
	movl (%ebp,%edx,4),%edx
	addl %eax,%edx
	orl $32537631,%edx
	movl %edx,%eax
	shrl $15,%eax
	andl %eax,%edx
	movb _RGB32k(%edx),%al
	movb %al,(%ecx)
	addl %edi,%ecx
	addl 60(%esp),%esi
	decl %ebx
	jnz L1157
L1152:
	popl %ebx
	popl %esi
	popl %edi
	popl %ebp
	addl $60,%esp
	ret
	.align 4
.globl _R_DrawShadedColumnP_C__Fv
	.def	_R_DrawShadedColumnP_C__Fv;	.scl	2;	.type	32;	.endef
_R_DrawShadedColumnP_C__Fv:
	subl $28,%esp
	movl _dc_yl,%eax
	pushl %ebp
	pushl %edi
	pushl %esi
	movl _dc_yh,%esi
	pushl %ebx
	subl %eax,%esi
	js L1158
	movl _dc_x,%ebx
	incl %esi
	addl _ylookup(,%eax,4),%ebx
	movl _dc_iscale,%eax
	movl %eax,28(%esp)
	movl _dc_source,%edx
	movl _dc_colormap,%eax
	movl %edx,24(%esp)
	movl %eax,20(%esp)
	movl _dc_texturefrac,%edi
	movl _dc_color,%eax
	movl _dc_pitch,%edx
	sall $2,%eax
	movl %edx,16(%esp)
	leal _Col2RGB8(%eax),%ebp
	.align 4
L1163:
	movl 24(%esp),%edx
	movl %edi,%eax
	sarl $16,%eax
	movzbl (%eax,%edx),%eax
	movl 20(%esp),%edx
	movzbl (%eax,%edx),%ecx
	movl $64,%eax
	movzbl (%ebx),%edx
	subl %ecx,%eax
	sall $2,%edx
	sall $10,%eax
	sall $8,%ecx
	addl %eax,%edx
	movl _Col2RGB8(%edx),%eax
	addl (%ebp,%ecx,4),%eax
	movl %eax,%ecx
	orl $32537631,%ecx
	movl %ecx,%eax
	shrl $15,%eax
	andl %eax,%ecx
	movb _RGB32k(%ecx),%al
	movb %al,(%ebx)
	addl 16(%esp),%ebx
	addl 28(%esp),%edi
	decl %esi
	jnz L1163
L1158:
	popl %ebx
	popl %esi
	popl %edi
	popl %ebp
	addl $28,%esp
	ret
	.align 4
.globl _R_DrawAddClampColumnP_C__Fv
	.def	_R_DrawAddClampColumnP_C__Fv;	.scl	2;	.type	32;	.endef
_R_DrawAddClampColumnP_C__Fv:
	subl $44,%esp
	movl _dc_yl,%eax
	pushl %ebp
	pushl %edi
	pushl %esi
	movl _dc_yh,%esi
	pushl %ebx
	subl %eax,%esi
	js L1164
	movl _dc_x,%ebx
	incl %esi
	addl _ylookup(,%eax,4),%ebx
	movl _dc_iscale,%eax
	movl %eax,44(%esp)
	movl _dc_colormap,%edx
	movl _dc_source,%eax
	movl %edx,40(%esp)
	movl %eax,36(%esp)
	movl _dc_texturefrac,%edi
	movl _dc_destblend,%ebp
	movl _dc_pitch,%edx
	movl _dc_srcblend,%eax
	movl %edx,32(%esp)
	movl %eax,28(%esp)
	.align 4
L1169:
	movl 36(%esp),%edx
	movl %edi,%eax
	sarl $16,%eax
	movzbl (%eax,%edx),%eax
	movl 40(%esp),%edx
	movzbl (%eax,%edx),%ecx
	movzbl (%ebx),%eax
	movl (%ebp,%eax,4),%edx
	movl 28(%esp),%eax
	addl (%eax,%ecx,4),%edx
	movl %edx,%ecx
	andl $1074791424,%ecx
	orl $32537631,%edx
	movl %ecx,%eax
	andl $1073741823,%edx
	shrl $5,%eax
	subl %eax,%ecx
	orl %ecx,%edx
	movl %edx,%eax
	shrl $15,%eax
	andl %eax,%edx
	movb _RGB32k(%edx),%al
	movb %al,(%ebx)
	addl 32(%esp),%ebx
	addl 44(%esp),%edi
	decl %esi
	jnz L1169
L1164:
	popl %ebx
	popl %esi
	popl %edi
	popl %ebp
	addl $44,%esp
	ret
	.align 4
.globl _R_DrawAddClampTranslatedColumnP_C__Fv
	.def	_R_DrawAddClampTranslatedColumnP_C__Fv;	.scl	2;	.type	32;	.endef
_R_DrawAddClampTranslatedColumnP_C__Fv:
	subl $44,%esp
	movl _dc_yl,%eax
	pushl %ebp
	pushl %edi
	pushl %esi
	movl _dc_yh,%esi
	pushl %ebx
	subl %eax,%esi
	js L1170
	movl _dc_x,%ebx
	incl %esi
	addl _ylookup(,%eax,4),%ebx
	movl _dc_iscale,%eax
	movl _dc_translation,%edx
	movl %eax,44(%esp)
	movl %edx,40(%esp)
	movl _dc_colormap,%eax
	movl _dc_source,%edx
	movl %eax,36(%esp)
	movl %edx,32(%esp)
	movl _dc_texturefrac,%edi
	movl _dc_destblend,%ebp
	movl _dc_pitch,%eax
	movl _dc_srcblend,%edx
	movl %eax,28(%esp)
	movl %edx,24(%esp)
	.align 4
L1175:
	movl 32(%esp),%edx
	movl %edi,%eax
	sarl $16,%eax
	movzbl (%eax,%edx),%eax
	movl 40(%esp),%edx
	movzbl (%eax,%edx),%eax
	movl 36(%esp),%edx
	movzbl (%eax,%edx),%ecx
	movzbl (%ebx),%eax
	movl (%ebp,%eax,4),%edx
	movl 24(%esp),%eax
	addl (%eax,%ecx,4),%edx
	movl %edx,%ecx
	andl $1074791424,%ecx
	orl $32537631,%edx
	movl %ecx,%eax
	andl $1073741823,%edx
	shrl $5,%eax
	subl %eax,%ecx
	orl %ecx,%edx
	movl %edx,%eax
	shrl $15,%eax
	andl %edx,%eax
	movb _RGB32k(%eax),%al
	movb %al,(%ebx)
	addl 28(%esp),%ebx
	addl 44(%esp),%edi
	decl %esi
	jnz L1175
L1170:
	popl %ebx
	popl %esi
	popl %edi
	popl %ebp
	addl $44,%esp
	ret
	.align 4
.globl _R_FillSpan__Fv
	.def	_R_FillSpan__Fv;	.scl	2;	.type	32;	.endef
_R_FillSpan__Fv:
	movl _ds_x1,%edx
	subl $12,%esp
	movl _ds_x2,%eax
	addl $-4,%esp
	subl %edx,%eax
	incl %eax
	pushl %eax
	movl _ds_color,%eax
	pushl %eax
	movl _ds_y,%eax
	sall $2,%eax
	addl _ylookup(%eax),%edx
	pushl %edx
	call _memset
	addl $16,%esp
	addl $12,%esp
	ret
.globl _dovline1
.data
	.align 4
_dovline1:
	.long _vlinec1__Fv
.globl _doprevline1
	.align 4
_doprevline1:
	.long _vlinec1__Fv
.globl _dovline4
	.align 4
_dovline4:
	.long _vlinec4__Fv
.text
	.align 4
.globl _setupvline__Fi
	.def	_setupvline__Fi;	.scl	2;	.type	32;	.endef
_setupvline__Fi:
	subl $12,%esp
	movl 16(%esp),%edx
	cmpl $23,%edx
	jle L1178
	addl $-12,%esp
	pushl %edx
	call _prosetupvlineasm
	movl $_provlineasm4,_dovline4
	addl $16,%esp
	movl $_vlineasm1,_dovline1
	movl $_prevlineasm1,_doprevline1
	jmp L1177
	.align 4
L1178:
	movl $_vlinec1__Fv,%eax
	movl $_vlinec4__Fv,_dovline4
	movl %eax,_doprevline1
	movl %eax,_dovline1
	movl %edx,_vlinebits
L1177:
	addl $12,%esp
	ret
	.align 4
	.def	_vlinec1__Fv;	.scl	3;	.type	32;	.endef
_vlinec1__Fv:
	subl $44,%esp
	movl _dc_iscale,%eax
	pushl %ebp
	movl _dc_colormap,%ecx
	pushl %edi
	movl _dc_texturefrac,%edx
	pushl %esi
	movl _vlinebits,%edi
	pushl %ebx
	movl _dc_pitch,%esi
	movl %eax,44(%esp)
	movl _dc_count,%ebx
	movl %ecx,40(%esp)
	movl _dc_source,%eax
	movl _dc_dest,%ecx
	movl %eax,28(%esp)
	movl %ecx,24(%esp)
	.align 4
L1184:
	movl %edx,%eax
	movl %edi,%ecx
	shrl %cl,%eax
	movl 28(%esp),%ecx
	movzbl (%eax,%ecx),%eax
	movl 40(%esp),%ecx
	movb (%eax,%ecx),%al
	movl 24(%esp),%ecx
	movb %al,(%ecx)
	addl %esi,%ecx
	addl 44(%esp),%edx
	movl %ecx,24(%esp)
	decl %ebx
	jnz L1184
	popl %ebx
	movl %edx,%eax
	popl %esi
	popl %edi
	popl %ebp
	addl $44,%esp
	ret
	.align 4
	.def	_vlinec4__Fv;	.scl	3;	.type	32;	.endef
_vlinec4__Fv:
	pushl %ebp
	pushl %edi
	pushl %esi
	movl _dc_count,%edi
	pushl %ebx
	movl _vlinebits,%esi
	movl _dc_dest,%ebx
	.align 4
L1189:
	movl _vplce,%ebp
	movl %esi,%ecx
	movl _bufplce,%edx
	movl %ebp,%eax
	shrl %cl,%eax
	movzbl (%eax,%edx),%edx
	movl _palookupoffse,%eax
	movb (%edx,%eax),%al
	movb %al,(%ebx)
	addl _vince,%ebp
	movl %ebp,_vplce
	movl _bufplce+4,%edx
	movl _vplce+4,%ebp
	movl %ebp,%eax
	shrl %cl,%eax
	movzbl (%eax,%edx),%edx
	movl _palookupoffse+4,%eax
	movb (%edx,%eax),%al
	movb %al,1(%ebx)
	addl _vince+4,%ebp
	movl %ebp,_vplce+4
	movl _bufplce+8,%edx
	movl _vplce+8,%ebp
	movl %ebp,%eax
	shrl %cl,%eax
	movzbl (%eax,%edx),%edx
	movl _palookupoffse+8,%eax
	movb (%edx,%eax),%al
	movb %al,2(%ebx)
	addl _vince+8,%ebp
	movl %ebp,_vplce+8
	movl _bufplce+12,%edx
	movl _vplce+12,%ebp
	movl %ebp,%eax
	shrl %cl,%eax
	movzbl (%eax,%edx),%edx
	movl _palookupoffse+12,%eax
	movb (%edx,%eax),%al
	movb %al,3(%ebx)
	addl _vince+12,%ebp
	movl %ebp,_vplce+12
	addl _dc_pitch,%ebx
	decl %edi
	jnz L1189
	popl %ebx
	popl %esi
	popl %edi
	popl %ebp
	ret
	.align 4
.globl _R_InitTranslationTables__Fv
	.def	_R_InitTranslationTables__Fv;	.scl	2;	.type	32;	.endef
_R_InitTranslationTables__Fv:
	subl $44,%esp
	pushl %ebp
	pushl %edi
	pushl %esi
	pushl %ebx
	addl $-4,%esp
	pushl $0
	pushl $1
	pushl $136191
	call _Z_Malloc__FUiiPv
	addl $255,%eax
	movl $0,44(%esp)
	xorb %al,%al
	addl $16,%esp
	movl %eax,_translationtables
	.align 4
L1194:
	movl 28(%esp),%ecx
	movb 28(%esp),%dl
	movl _translationtables,%eax
	movb %dl,(%ecx,%eax)
	incl %ecx
	movl %ecx,28(%esp)
	cmpl $255,%ecx
	jle L1194
	movl $1,28(%esp)
	.align 4
L1199:
	movl 28(%esp),%eax
	movl _translationtables,%esi
	sall $8,%eax
	leal (%esi,%eax),%edi
	cld
	movl $64,%ecx
	rep
	movsl
	incl 28(%esp)
	cmpl $18,28(%esp)
	jle L1199
	movl _gameinfo+144,%eax
	cmpl $1,%eax
	jne L1201
	movl $112,28(%esp)
	.align 4
L1205:
	movb 28(%esp),%dl
	andb $15,%dl
	movl _translationtables,%eax
	movb %dl,%bl
	movl 28(%esp),%ecx
	orb $96,%bl
	movb %bl,4096(%eax,%ecx)
	movl _translationtables,%eax
	movb %dl,%bl
	orb $64,%bl
	movb %bl,4352(%eax,%ecx)
	movl _translationtables,%eax
	orb $32,%dl
	movb %dl,4608(%eax,%ecx)
	incl %ecx
	movl %ecx,28(%esp)
	cmpl $127,%ecx
	jle L1205
	jmp L1207
	.align 4
L1201:
	cmpl $2,%eax
	jne L1207
	movl $225,28(%esp)
	.align 4
L1212:
	movl _translationtables,%eax
	movb 28(%esp),%dl
	movl 28(%esp),%ecx
	addb $-111,%dl
	movb %dl,4096(%eax,%ecx)
	movl _translationtables,%eax
	movl 28(%esp),%ebx
	addb $-80,%cl
	movb %cl,4352(%eax,%ebx)
	movl _translationtables,%eax
	movl 28(%esp),%edx
	addb $-35,%bl
	movb %bl,4608(%eax,%edx)
	incl %edx
	movl %edx,28(%esp)
	cmpl $240,%edx
	jle L1212
L1207:
	movl _translationtables,%ebp
	movl $0,28(%esp)
	addl $4864,%ebp
	.align 4
L1217:
	movl $16,%eax
	movl 28(%esp),%ecx
	subl 28(%esp),%eax
	incl %ecx
	sall $3,%eax
	xorl %edx,%edx
	movl %ecx,40(%esp)
	movl %eax,44(%esp)
	.align 4
L1221:
	movl $32,%esi
	leal 256(%ebp),%ebx
	subl %edx,%esi
	movl %ebx,32(%esp)
	imull 44(%esp),%esi
	incl %edx
	xorl %edi,%edi
	movl %edx,36(%esp)
	movl $256,%ebx
	.align 4
L1225:
	movl %ebx,%edx
	movb $64,%cl
	sarl $14,%edx
	movb %dl,%al
	cmpb %al,%cl
	jbe L1228
	movb %dl,%cl
L1228:
	movb %cl,(%edi,%ebp)
	addl %esi,%ebx
	incl %edi
	cmpl $255,%edi
	jle L1225
	movl 32(%esp),%ebp
	movl 36(%esp),%edx
	cmpl $31,%edx
	jle L1221
	movl 40(%esp),%eax
	movl %eax,28(%esp)
	cmpl $15,%eax
	jle L1217
	popl %ebx
	popl %esi
	popl %edi
	popl %ebp
	addl $44,%esp
	ret
	.align 4
LC0:
	.long 0x437f0000
	.align 4
LC1:
	.long 0x3e6b851f
	.align 4
LC2:
	.long 0x3dcccccd
	.align 4
LC3:
	.long 0xbf70ed3e
	.align 4
LC4:
	.long 0x3ed67c28
	.align 4
LC5:
	.long 0x3e70f0d5
	.align 4
LC6:
	.long 0x3fa66666
	.align 4
LC7:
	.long 0x3dc50481
	.align 4
LC8:
	.long 0x3f658106
	.align 4
LC9:
	.long 0x3d484b5e
	.align 4
LC10:
	.long 0x3e573eab
	.align 4
.globl _R_BuildPlayerTranslation__Fii
	.def	_R_BuildPlayerTranslation__Fii;	.scl	2;	.type	32;	.endef
_R_BuildPlayerTranslation__Fii:
	subl $92,%esp
	flds LC0
	pushl %ebp
	pushl %edi
	pushl %esi
	pushl %ebx
	movl 112(%esp),%eax
	movl %eax,%esi
	imull $680,%eax,%eax
	sall $9,%esi
	addl _translationtables,%esi
	movl _players+52(%eax),%eax
	sall $5,%eax
	addl _skins,%eax
	movb 21(%eax),%dl
	movb %dl,63(%esp)
	movb 22(%eax),%al
	movb %al,62(%esp)
	movl 116(%esp),%edx
	movzbl 118(%esp),%eax
	movl %eax,92(%esp)
	movl %edx,%eax
	fildl 92(%esp)
	sarl $8,%eax
	movzbl %dl,%edx
	andl $255,%eax
	movl %eax,92(%esp)
	fdiv %st(1),%st
	movzbl 62(%esp),%eax
	fildl 92(%esp)
	fxch %st(1)
	movl %edx,92(%esp)
	fsts 64(%esp)
	fildl 92(%esp)
	fxch %st(2)
	fdiv %st(3),%st
	fxch %st(2)
	movzbl 63(%esp),%edx
	subl %edx,%eax
	incl %eax
	movl %eax,92(%esp)
	fdivp %st,%st(3)
	fxch %st(1)
	fsts 68(%esp)
	fildl 92(%esp)
	fxch %st(3)
	fsts 72(%esp)
	fxch %st(3)
	fstps 40(%esp)
	fxch %st(2)
	addl $-8,%esp
	leal 92(%esp),%eax
	pushl %eax
	leal 92(%esp),%eax
	pushl %eax
	leal 92(%esp),%eax
	pushl %eax
	subl $4,%esp
	fstps (%esp)
	fxch %st(1)
	subl $4,%esp
	fstps (%esp)
	subl $4,%esp
	fstps (%esp)
	call _RGBtoHSV__FfffPfN23
	flds 112(%esp)
	flds 116(%esp)
	fxch %st(1)
	fstps 88(%esp)
	fstps 84(%esp)
	movl _gameinfo+144,%eax
	addl $32,%esp
	cmpl $1,%eax
	jne L1234
	flds LC1
	flds 56(%esp)
	fsub %st(1),%st
	fxch %st(1)
	movb 63(%esp),%bl
	fdivs 40(%esp)
	fxch %st(1)
	fstps 80(%esp)
	flds 52(%esp)
	fadds LC2
	fxch %st(1)
	fstps 48(%esp)
	fstps 84(%esp)
	flds 40(%esp)
	fdivrs LC3
	fstps 44(%esp)
	cmpb 62(%esp),%bl
	ja L1271
	.align 4
L1238:
	flds 80(%esp)
	fldz
	fcom %st(1)
	fnstsw %ax
	andb $5,%ah
	je L1336
	fstp %st(0)
	fld1
	fcom %st(1)
	fnstsw %ax
	andb $69,%ah
	decb %ah
	cmpb $64,%ah
	jae L1328
L1336:
	fstp %st(1)
	jmp L1245
L1328:
	fstp %st(0)
L1245:
	flds 84(%esp)
	fld1
	fldz
	fcom %st(2)
	fnstsw %ax
	andb $5,%ah
	je L1329
	fstp %st(0)
	fcom %st(1)
	fnstsw %ax
	andb $69,%ah
	decb %ah
	cmpb $64,%ah
	jae L1330
	fstp %st(1)
	jmp L1250
L1329:
	fstp %st(1)
	fstp %st(1)
	jmp L1250
L1330:
	fstp %st(0)
L1250:
	addl $-8,%esp
	subl $4,%esp
	fstps (%esp)
	subl $4,%esp
	flds 92(%esp)
	fxch %st(1)
	fstps (%esp)
	subl $4,%esp
	fstps (%esp)
	leal 92(%esp),%eax
	pushl %eax
	leal 92(%esp),%eax
	pushl %eax
	leal 92(%esp),%eax
	pushl %eax
	call _HSVtoRGB__FPfN20fff
	addl $32,%esp
	flds 72(%esp)
	fmuls LC0
	fnstcw 90(%esp)
	movw 90(%esp),%dx
	orw $3072,%dx
	movw %dx,88(%esp)
	fldcw 88(%esp)
	fistpl 92(%esp)
	movl 92(%esp),%eax
	fldcw 90(%esp)
	testl %eax,%eax
	jle L1258
	movl $255,%edx
	cmpl %eax,%edx
	jle L1260
	movl %eax,%edx
	jmp L1260
	.align 4
L1258:
	xorl %edx,%edx
L1260:
	pushl %edx
	flds 72(%esp)
	fmuls LC0
	fnstcw 94(%esp)
	movw 94(%esp),%dx
	orw $3072,%dx
	movw %dx,92(%esp)
	fldcw 92(%esp)
	fistpl 96(%esp)
	movl 96(%esp),%eax
	fldcw 94(%esp)
	testl %eax,%eax
	jle L1262
	movl $255,%edx
	cmpl %eax,%edx
	jle L1264
	movl %eax,%edx
	jmp L1264
	.align 4
L1262:
	xorl %edx,%edx
L1264:
	pushl %edx
	flds 72(%esp)
	fmuls LC0
	fnstcw 98(%esp)
	movw 98(%esp),%dx
	orw $3072,%dx
	movw %dx,96(%esp)
	fldcw 96(%esp)
	fistpl 100(%esp)
	movl 100(%esp),%eax
	fldcw 98(%esp)
	testl %eax,%eax
	jle L1266
	movl $255,%edx
	cmpl %eax,%edx
	jle L1268
	movl %eax,%edx
	jmp L1268
	.align 4
L1266:
	xorl %edx,%edx
L1268:
	pushl %edx
	pushl $_ColorMatcher
	call _Pick__13FColorMatcheriii
	movb %al,%dl
	movzbl %bl,%eax
	movb %dl,(%eax,%esi)
	incb %bl
	flds 96(%esp)
	fadds 64(%esp)
	fstps 96(%esp)
	flds 100(%esp)
	fadds 60(%esp)
	fstps 100(%esp)
	addl $16,%esp
	cmpb 62(%esp),%bl
	jbe L1238
	jmp L1271
	.align 4
L1234:
	cmpl $2,%eax
	jne L1271
	flds 40(%esp)
	leal 72(%esp),%eax
	fdivrs LC4
	leal 256(%esi),%edx
	movb 63(%esp),%bl
	movl %eax,20(%esp)
	leal 68(%esp),%ebp
	leal 64(%esp),%edi
	movl %edx,24(%esp)
	fstps 36(%esp)
	cmpb 62(%esp),%bl
	ja L1274
	.align 4
L1276:
	movzbl %bl,%eax
	movzbl 63(%esp),%edx
	subl %edx,%eax
	flds 36(%esp)
	movl %eax,92(%esp)
	fildl 92(%esp)
	fmulp %st,%st(1)
	fldz
	fxch %st(1)
	fadds 52(%esp)
	fsubs LC5
	fsts 84(%esp)
	fcom %st(1)
	fnstsw %ax
	andb $69,%ah
	decb %ah
	cmpb $64,%ah
	jb L1331
	fstp %st(1)
	fld1
	fcom %st(1)
	fnstsw %ax
	andb $69,%ah
	decb %ah
	cmpb $64,%ah
	jae L1332
	fstp %st(1)
	jmp L1278
L1331:
L1332:
	fstp %st(0)
L1278:
	fsts 84(%esp)
	addl $-8,%esp
	subl $4,%esp
	fstps (%esp)
	flds 92(%esp)
	subl $4,%esp
	fstps (%esp)
	flds 92(%esp)
	subl $4,%esp
	fstps (%esp)
	movl 40(%esp),%eax
	pushl %eax
	pushl %ebp
	pushl %edi
	call _HSVtoRGB__FPfN20fff
	addl $32,%esp
	flds 72(%esp)
	fmuls LC0
	fnstcw 90(%esp)
	movw 90(%esp),%dx
	orw $3072,%dx
	movw %dx,88(%esp)
	fldcw 88(%esp)
	fistpl 92(%esp)
	movl 92(%esp),%eax
	fldcw 90(%esp)
	testl %eax,%eax
	jle L1282
	movl $255,%edx
	cmpl %eax,%edx
	jle L1284
	movl %eax,%edx
	jmp L1284
	.align 4
L1282:
	xorl %edx,%edx
L1284:
	pushl %edx
	flds 72(%esp)
	fmuls LC0
	fnstcw 94(%esp)
	movw 94(%esp),%dx
	orw $3072,%dx
	movw %dx,92(%esp)
	fldcw 92(%esp)
	fistpl 96(%esp)
	movl 96(%esp),%eax
	fldcw 94(%esp)
	testl %eax,%eax
	jle L1286
	movl $255,%edx
	cmpl %eax,%edx
	jle L1288
	movl %eax,%edx
	jmp L1288
	.align 4
L1286:
	xorl %edx,%edx
L1288:
	pushl %edx
	flds 72(%esp)
	fmuls LC0
	fnstcw 98(%esp)
	movw 98(%esp),%dx
	orw $3072,%dx
	movw %dx,96(%esp)
	fldcw 96(%esp)
	fistpl 100(%esp)
	movl 100(%esp),%eax
	fldcw 98(%esp)
	testl %eax,%eax
	jle L1290
	movl $255,%edx
	cmpl %eax,%edx
	jle L1292
	movl %eax,%edx
	jmp L1292
	.align 4
L1290:
	xorl %edx,%edx
L1292:
	pushl %edx
	pushl $_ColorMatcher
	call _Pick__13FColorMatcheriii
	movb %al,%dl
	addl $16,%esp
	movzbl %bl,%eax
	movb %dl,(%eax,%esi)
	incb %bl
	cmpb 62(%esp),%bl
	jbe L1276
L1274:
	flds LC6
	flds 56(%esp)
	movl $1065353216,32(%esp)
	fmul %st(1),%st
	flds 32(%esp)
	fcom %st(1)
	fnstsw %ax
	andb $69,%ah
	jne L1333
	fxch %st(1)
	fstps 32(%esp)
	jmp L1298
L1333:
	fstp %st(1)
L1298:
	flds 32(%esp)
	fstps 56(%esp)
	flds 52(%esp)
	fmulp %st,%st(2)
	fsts 28(%esp)
	fcomp %st(1)
	fnstsw %ax
	andb $69,%ah
	jne L1334
	fstps 28(%esp)
	jmp L1301
L1334:
	fstp %st(0)
L1301:
	movb $145,%bl
	.align 4
L1307:
	movzbl %bl,%edx
	flds LC8
	leal -161(%edx),%eax
	movl %edx,%esi
	movl %eax,92(%esp)
	fildl 92(%esp)
	fmuls LC7
	fsubrp %st,%st(1)
	fcoms 56(%esp)
	fnstsw %ax
	andb $69,%ah
	jne L1308
	fstp %st(0)
	flds 32(%esp)
L1308:
	leal -144(%esi),%eax
	fsts 80(%esp)
	movl %eax,92(%esp)
	fld1
	fildl 92(%esp)
	fmuls LC9
	fadds LC10
	fmuls 28(%esp)
	fcom %st(1)
	fnstsw %ax
	andb $69,%ah
	jne L1335
	fstp %st(0)
	jmp L1311
L1335:
	fstp %st(1)
L1311:
	fsts 84(%esp)
	addl $-8,%esp
	subl $4,%esp
	fstps (%esp)
	subl $4,%esp
	flds 92(%esp)
	fxch %st(1)
	fstps (%esp)
	subl $4,%esp
	fstps (%esp)
	movl 40(%esp),%edx
	pushl %edx
	pushl %ebp
	pushl %edi
	call _HSVtoRGB__FPfN20fff
	addl $32,%esp
	flds 72(%esp)
	fmuls LC0
	fnstcw 90(%esp)
	movw 90(%esp),%dx
	orw $3072,%dx
	movw %dx,88(%esp)
	fldcw 88(%esp)
	fistpl 92(%esp)
	movl 92(%esp),%eax
	fldcw 90(%esp)
	testl %eax,%eax
	jle L1314
	movl $255,%edx
	cmpl %eax,%edx
	jle L1316
	movl %eax,%edx
	jmp L1316
	.align 4
L1314:
	xorl %edx,%edx
L1316:
	pushl %edx
	flds 72(%esp)
	fmuls LC0
	fnstcw 94(%esp)
	movw 94(%esp),%dx
	orw $3072,%dx
	movw %dx,92(%esp)
	fldcw 92(%esp)
	fistpl 96(%esp)
	movl 96(%esp),%eax
	fldcw 94(%esp)
	testl %eax,%eax
	jle L1318
	movl $255,%edx
	cmpl %eax,%edx
	jle L1320
	movl %eax,%edx
	jmp L1320
	.align 4
L1318:
	xorl %edx,%edx
L1320:
	pushl %edx
	flds 72(%esp)
	fmuls LC0
	fnstcw 98(%esp)
	movw 98(%esp),%dx
	orw $3072,%dx
	movw %dx,96(%esp)
	fldcw 96(%esp)
	fistpl 100(%esp)
	movl 100(%esp),%eax
	fldcw 98(%esp)
	testl %eax,%eax
	jle L1322
	movl $255,%edx
	cmpl %eax,%edx
	jle L1324
	movl %eax,%edx
	jmp L1324
	.align 4
L1322:
	xorl %edx,%edx
L1324:
	pushl %edx
	incb %bl
	pushl $_ColorMatcher
	call _Pick__13FColorMatcheriii
	movl 40(%esp),%edx
	movb %al,(%esi,%edx)
	addl $16,%esp
	cmpb $168,%bl
	jbe L1307
L1271:
	popl %ebx
	popl %esi
	popl %edi
	popl %ebp
	addl $92,%esp
	ret
	.align 4
.globl _R_DrawBorder__Fiiii
	.def	_R_DrawBorder__Fiiii;	.scl	2;	.type	32;	.endef
_R_DrawBorder__Fiiii:
	subl $12,%esp
	pushl %ebp
	pushl %edi
	pushl %esi
	pushl %ebx
	movl 32(%esp),%edi
	movl 44(%esp),%ebp
	addl $-8,%esp
	pushl $2
	pushl $_gameinfo+128
	call _W_CheckNumForName__FPCci
	movl %eax,%edx
	addl $16,%esp
	testl %edx,%edx
	jl L1338
	movl _screen,%eax
	addl $-8,%esp
	addl $-8,%esp
	andl $-64,%edi
	movl 8(%eax),%ebx
	pushl $101
	leal 168(%ebx),%esi
	pushl %edx
	call _W_CacheLumpNum__Fii
	pushl %eax
	pushl %ebp
	movl 72(%esp),%eax
	pushl %eax
	movl 72(%esp),%eax
	pushl %eax
	pushl %edi
	movswl 168(%ebx),%eax
	addl _screen,%eax
	pushl %eax
	movl 4(%esi),%eax
	call *%eax
	addl $48,%esp
	jmp L1339
	.align 4
L1338:
	movl _screen,%edx
	addl $-8,%esp
	movl 8(%edx),%ecx
	pushl $0
	pushl %ebp
	movl 56(%esp),%eax
	pushl %eax
	movl 56(%esp),%eax
	pushl %eax
	pushl %edi
	movswl 176(%ecx),%eax
	addl %eax,%edx
	pushl %edx
	movl 180(%ecx),%eax
	call *%eax
	addl $32,%esp
L1339:
	popl %ebx
	popl %esi
	popl %edi
	popl %ebp
	addl $12,%esp
	ret
	.align 4
.globl _R_DrawViewBorder__Fv
	.def	_R_DrawViewBorder__Fv;	.scl	2;	.type	32;	.endef
_R_DrawViewBorder__Fv:
	subl $12,%esp
	movl _screen,%ecx
	cmpl $320,20(%ecx)
	jle L1341
	movl 8(%ecx),%eax
	addl $-12,%esp
	movswl 336(%eax),%edx
	addl %ecx,%edx
	pushl %edx
	movl 340(%eax),%eax
	call *%eax
	movl %eax,_SB_state
	addl $16,%esp
L1341:
	movl _screen,%edx
	movl _realviewwidth,%eax
	cmpl 20(%edx),%eax
	je L1340
	movl _viewwindowy,%eax
	pushl %eax
	movl 20(%edx),%eax
	pushl %eax
	pushl $0
	pushl $0
	call _R_DrawBorder__Fiiii
	movl _viewwindowy,%edx
	movl %edx,%eax
	addl _realviewheight,%eax
	pushl %eax
	movl _viewwindowx,%eax
	pushl %eax
	pushl %edx
	pushl $0
	call _R_DrawBorder__Fiiii
	movl _viewwindowy,%edx
	addl $32,%esp
	movl %edx,%eax
	addl _realviewheight,%eax
	pushl %eax
	movl _screen,%eax
	movl 20(%eax),%eax
	pushl %eax
	movl _realviewwidth,%eax
	pushl %edx
	addl _viewwindowx,%eax
	pushl %eax
	call _R_DrawBorder__Fiiii
	movl _ST_Y,%eax
	pushl %eax
	movl _screen,%eax
	movl 20(%eax),%eax
	pushl %eax
	movl _realviewheight,%eax
	addl _viewwindowy,%eax
	pushl %eax
	pushl $0
	call _R_DrawBorder__Fiiii
	addl $32,%esp
	movl _realviewheight,%eax
	pushl %eax
	movl _realviewwidth,%eax
	pushl %eax
	movl _viewwindowy,%eax
	pushl %eax
	movl _viewwindowx,%eax
	pushl %eax
	call _M_DrawFrame__Fiiii
	movl _ST_Y,%eax
	pushl %eax
	movl _screen,%eax
	movl 20(%eax),%eax
	pushl %eax
	pushl $0
	pushl $0
	call _V_MarkRect__Fiiii
	addl $32,%esp
L1340:
	addl $12,%esp
	ret
	.align 4
.globl _R_DrawTopBorder__Fv
	.def	_R_DrawTopBorder__Fv;	.scl	2;	.type	32;	.endef
_R_DrawTopBorder__Fv:
	subl $12,%esp
	movl _screen,%edx
	pushl %ebp
	movl _realviewwidth,%eax
	pushl %edi
	pushl %esi
	pushl %ebx
	cmpl 20(%edx),%eax
	je L1349
	pushl $34
	movl 20(%edx),%eax
	pushl %eax
	pushl $0
	pushl $0
	call _R_DrawBorder__Fiiii
	movl _gameinfo+136,%eax
	addl $16,%esp
	movzbl (%eax),%edi
	movzbl 1(%eax),%ebp
	cmpl $34,_viewwindowy
	jg L1349
	movl _viewwindowx,%ebx
	movl %ebx,%eax
	addl _realviewwidth,%eax
	cmpl %eax,%ebx
	jge L1355
	.align 4
L1357:
	addl $-8,%esp
	movl _gameinfo+136,%eax
	pushl $101
	addl $10,%eax
	addl $-12,%esp
	pushl %eax
	call _W_GetNumForName__FPCc
	addl $16,%esp
	pushl %eax
	call _W_CacheLumpNum__Fii
	movl _screen,%edx
	movl %eax,%esi
	movl _viewwindowy,%eax
	addl $16,%esp
	addl $-12,%esp
	subl %edi,%eax
	movl 8(%edx),%ecx
	pushl %eax
	pushl %ebx
	pushl %esi
	addl %ebp,%ebx
	pushl $0
	movswl 224(%ecx),%eax
	addl %eax,%edx
	pushl %edx
	movl 228(%ecx),%eax
	call *%eax
	movl _realviewwidth,%eax
	addl $32,%esp
	addl _viewwindowx,%eax
	cmpl %eax,%ebx
	jl L1357
L1355:
	movl _viewwindowy,%esi
	cmpl $34,%esi
	jg L1361
	.align 4
L1363:
	addl $-8,%esp
	movl _gameinfo+136,%eax
	pushl $101
	addl $26,%eax
	addl $-12,%esp
	pushl %eax
	call _W_GetNumForName__FPCc
	addl $16,%esp
	pushl %eax
	call _W_CacheLumpNum__Fii
	movl _screen,%edx
	movl %eax,%ebx
	addl $16,%esp
	movl _viewwindowx,%eax
	addl $-12,%esp
	subl %edi,%eax
	movl 8(%edx),%ecx
	pushl %esi
	pushl %eax
	pushl %ebx
	pushl $0
	movswl 224(%ecx),%eax
	addl %eax,%edx
	pushl %edx
	movl 228(%ecx),%eax
	call *%eax
	addl $32,%esp
	movl _gameinfo+136,%eax
	addl $-8,%esp
	addl $34,%eax
	pushl $101
	addl $-12,%esp
	pushl %eax
	call _W_GetNumForName__FPCc
	addl $16,%esp
	pushl %eax
	call _W_CacheLumpNum__Fii
	movl _screen,%edx
	movl %eax,%ebx
	addl $16,%esp
	movl _realviewwidth,%eax
	addl $-12,%esp
	addl _viewwindowx,%eax
	movl 8(%edx),%ecx
	pushl %esi
	pushl %eax
	addl %ebp,%esi
	pushl %ebx
	pushl $0
	movswl 224(%ecx),%eax
	addl %eax,%edx
	pushl %edx
	movl 228(%ecx),%eax
	call *%eax
	addl $32,%esp
	cmpl $34,%esi
	jle L1363
L1361:
	addl $-8,%esp
	movl _gameinfo+136,%eax
	pushl $101
	addl $2,%eax
	addl $-12,%esp
	pushl %eax
	call _W_GetNumForName__FPCc
	addl $16,%esp
	pushl %eax
	call _W_CacheLumpNum__Fii
	movl _screen,%edx
	movl %eax,%esi
	movl _viewwindowy,%eax
	addl $16,%esp
	addl $-12,%esp
	movl _viewwindowx,%ebx
	movl 8(%edx),%ecx
	subl %edi,%eax
	pushl %eax
	subl %edi,%ebx
	pushl %ebx
	pushl %esi
	pushl $0
	movswl 224(%ecx),%eax
	addl %eax,%edx
	pushl %edx
	movl 228(%ecx),%eax
	call *%eax
	addl $32,%esp
	movl _gameinfo+136,%eax
	addl $-8,%esp
	addl $18,%eax
	pushl $101
	addl $-12,%esp
	pushl %eax
	call _W_GetNumForName__FPCc
	addl $16,%esp
	pushl %eax
	call _W_CacheLumpNum__Fii
	movl _screen,%edx
	movl %eax,%esi
	movl _viewwindowy,%ebx
	addl $16,%esp
	addl $-12,%esp
	movl _realviewwidth,%eax
	movl 8(%edx),%ecx
	subl %edi,%ebx
	pushl %ebx
	addl _viewwindowx,%eax
	pushl %eax
	pushl %esi
	pushl $0
	movswl 224(%ecx),%eax
	addl %eax,%edx
	pushl %edx
	movl 228(%ecx),%eax
	call *%eax
	addl $32,%esp
L1349:
	popl %ebx
	popl %esi
	popl %edi
	popl %ebp
	addl $12,%esp
	ret
	.align 4
.globl _R_DetailDouble__Fv
	.def	_R_DetailDouble__Fv;	.scl	2;	.type	32;	.endef
_R_DetailDouble__Fv:
	subl $28,%esp
	movl $0,_DetailDoubleCycles
	pushl %ebp
	pushl %edi
	pushl %esi
	pushl %ebx
	cmpl $0,_HaveRDTSC
	je L1370
/APP
	rdtsc
/NO_APP
	jmp L1371
	.align 4
L1370:
	xorl %eax,%eax
L1371:
	subl %eax,_DetailDoubleCycles
	movl _detailxshift,%eax
	addl %eax,%eax
	orl _detailyshift,%eax
	cmpl $2,%eax
	je L1376
	jg L1408
	cmpl $1,%eax
	je L1374
	jmp L1373
	.align 4
L1408:
	cmpl $3,%eax
	je L1391
	jmp L1373
	.align 4
L1374:
	movl _RenderTarget,%eax
	movl 28(%eax),%eax
	pushl %eax
	movl _ylookup,%eax
	pushl %eax
	movl _viewwidth,%eax
	pushl %eax
	movl _viewheight,%eax
	pushl %eax
	call _DoubleVert_ASM
	addl $16,%esp
	jmp L1373
	.align 4
L1376:
	cmpl $0,_UseMMX
	je L1377
	movl _RenderTarget,%eax
	movl _viewwidth,%edx
	movl 28(%eax),%eax
	pushl %eax
	movl %edx,%eax
	addl _ylookup,%eax
	pushl %eax
	pushl %edx
	movl _viewheight,%eax
	pushl %eax
	call _DoubleHoriz_MMX
	addl $16,%esp
	jmp L1373
	.align 4
L1377:
	movl _RenderTarget,%eax
	movl _viewwidth,%ebp
	movl _ylookup,%ebx
	movl 28(%eax),%eax
	movl %eax,28(%esp)
	movl _viewheight,%eax
	testl %eax,%eax
	je L1373
	.align 4
L1384:
	leal -1(%eax),%esi
	movl %ebx,%ecx
	movl 28(%esp),%eax
	xorl %edx,%edx
	subl _viewwidth,%ecx
	leal (%eax,%ebx),%edi
	cmpl %ebp,%edx
	jge L1383
	.align 4
L1388:
	movb (%edx,%ebx),%al
	movb %al,(%ecx,%edx,2)
	movb %al,1(%ecx,%edx,2)
	incl %edx
	cmpl %ebp,%edx
	jl L1388
L1383:
	movl %esi,%eax
	movl %edi,%ebx
	testl %eax,%eax
	jne L1384
	jmp L1373
	.align 4
L1391:
	cmpl $0,_UseMMX
	je L1392
	movl _RenderTarget,%eax
	movl _viewwidth,%edx
	movl 28(%eax),%eax
	pushl %eax
	movl %edx,%eax
	addl _ylookup,%eax
	pushl %eax
	pushl %edx
	movl _viewheight,%eax
	pushl %eax
	call _DoubleHorizVert_MMX
	addl $16,%esp
	jmp L1373
	.align 4
L1392:
	movl _RenderTarget,%eax
	movl _viewwidth,%edx
	movl %edx,24(%esp)
	movl _ylookup,%ebx
	movl 28(%eax),%esi
	leal (%esi,%esi),%eax
	movl _viewheight,%edx
	movl %eax,20(%esp)
	testl %edx,%edx
	je L1373
	.align 4
L1399:
	leal -1(%edx),%ebp
	movl %ebx,%eax
	movl 20(%esp),%edx
	xorl %ecx,%ecx
	subl _viewwidth,%eax
	leal (%edx,%ebx),%edi
	cmpl 24(%esp),%ecx
	jge L1398
	movl %eax,%edx
	.align 4
L1403:
	movb (%ecx,%ebx),%al
	movb %al,(%edx)
	incl %ecx
	movb %al,1(%edx)
	movb %al,(%esi,%edx)
	movb %al,1(%esi,%edx)
	addl $2,%edx
	cmpl 24(%esp),%ecx
	jl L1403
L1398:
	movl %ebp,%edx
	movl %edi,%ebx
	testl %edx,%edx
	jne L1399
L1373:
	cmpl $0,_HaveRDTSC
	je L1409
/APP
	rdtsc
/NO_APP
	jmp L1410
	.align 4
L1409:
	xorl %eax,%eax
L1410:
	popl %ebx
	popl %esi
	popl %edi
	popl %ebp
	addl %eax,_DetailDoubleCycles
	addl $28,%esp
	ret
LC11:
	.ascii "detail\0"
LC13:
	.ascii "doubling = %04.1f ms\0"
	.align 8
LC12:
	.long 0x0,0x408f4000
	.align 4
.globl _GetStats__11Stat_detailPc
	.def	_GetStats__11Stat_detailPc;	.scl	2;	.type	32;	.endef
_GetStats__11Stat_detailPc:
	subl $28,%esp
	movl _DetailDoubleCycles,%eax
	xorl %edx,%edx
	movl %eax,8(%esp)
	movl %edx,12(%esp)
	movl 36(%esp),%ecx
	fildq 8(%esp)
	subl $8,%esp
	fmull LC12
	fmull _SecondsPerCycle
	fstpl (%esp)
	pushl $LC13
	pushl %ecx
	call _sprintf
	addl $16,%esp
	addl $28,%esp
	ret
	.align 4
.globl _R_InitColumnDrawers__Fv
	.def	_R_InitColumnDrawers__Fv;	.scl	2;	.type	32;	.endef
_R_InitColumnDrawers__Fv:
	movl $_R_DrawColumnP_ASM,_R_DrawColumn
	movl $_R_DrawColumnHorizP_ASM,_R_DrawColumnHoriz
	movl $_R_DrawFuzzColumnP_ASM,_R_DrawFuzzColumn
	movl $_R_DrawTranslatedColumnP_C__Fv,_R_DrawTranslatedColumn
	movl $_R_DrawShadedColumnP_C__Fv,_R_DrawShadedColumn
	movl $_R_DrawSpanP_ASM,_R_DrawSpan
	cmpb $5,_CPUFamily
	ja L1426
	movl $_rt_map4cols_asm2,_rt_map4cols
	ret
	.align 4
L1426:
	movl $_rt_map4cols_asm1,_rt_map4cols
	ret
	.align 4
	.def	_R_SetBlendFunc__Fll;	.scl	3;	.type	32;	.endef
_R_SetBlendFunc__Fll:
	movl 4(%esp),%edx
	movl 8(%esp),%ecx
	cmpb $0,_r_drawtrans+20
	je L1430
	cmpl $65536,%edx
	jne L1429
	testl %ecx,%ecx
	jne L1429
L1430:
	cmpl $0,_dc_translation
	jne L1432
	movl _basecolfunc,%eax
	movl $_rt_map1col_asm,_hcolfunc_post1
	movl %eax,_colfunc
	movl _rt_map4cols,%eax
	movl %eax,_hcolfunc_post4
	jmp L1433
	.align 4
L1432:
	movl _transcolfunc,%eax
	movl $_rt_tlate1col__Fiiii,_hcolfunc_post1
	movl %eax,_colfunc
	movl $_rt_tlate4cols__Fiii,_hcolfunc_post4
L1433:
	movb $0,_r_MarkTrans
	movb $1,%al
	ret
	.align 4
L1429:
	testl %edx,%edx
	jne L1434
	cmpl $65536,%ecx
	jne L1434
	movb $0,%al
	ret
	.align 4
L1434:
	leal (%ecx,%edx),%eax
	cmpl $65536,%eax
	jg L1435
	andl $-1024,%edx
	andl $-1024,%ecx
	addl $_Col2RGB8,%edx
	addl $_Col2RGB8,%ecx
	movl %edx,_dc_srcblend
	movl %ecx,_dc_destblend
	cmpl $0,_dc_translation
	jne L1436
	movl $_R_DrawAddColumnP_C__Fv,_colfunc
	movl $_rt_add1col__Fiiii,_hcolfunc_post1
	movl $_rt_add4cols__Fiii,_hcolfunc_post4
	jmp L1438
	.align 4
L1436:
	movl $_R_DrawTlatedAddColumnP_C__Fv,_colfunc
	movl $_rt_tlateadd1col__Fiiii,_hcolfunc_post1
	movl $_rt_tlateadd4cols__Fiii,_hcolfunc_post4
	jmp L1438
	.align 4
L1435:
	movl $_Col2RGB8_LessPrecision,%eax
	sarl $10,%edx
	movl (%eax,%edx,4),%edx
	sarl $10,%ecx
	movl (%eax,%ecx,4),%eax
	movl %edx,_dc_srcblend
	movl %eax,_dc_destblend
	cmpl $0,_dc_translation
	jne L1439
	movl $_R_DrawAddClampColumnP_C__Fv,_colfunc
	movl $_rt_addclamp1col__Fiiii,_hcolfunc_post1
	movl $_rt_addclamp4cols__Fiii,_hcolfunc_post4
	jmp L1438
	.align 4
L1439:
	movl $_R_DrawAddClampTranslatedColumnP_C__Fv,_colfunc
	movl $_rt_tlateaddclamp1col__Fiiii,_hcolfunc_post1
	movl $_rt_tlateaddclamp4cols__Fiii,_hcolfunc_post4
L1438:
	movb $1,_r_MarkTrans
	movb $1,%al
	ret
	.align 4
LC14:
	.long 0x47800000
	.align 4
.globl _R_SetPatchStyle__FilPUcUl
	.def	_R_SetPatchStyle__FilPUcUl;	.scl	2;	.type	32;	.endef
_R_SetPatchStyle__FilPUcUl:
	subl $32,%esp
	pushl %edi
	pushl %esi
	pushl %ebx
	movl 48(%esp),%edx
	movl 52(%esp),%ecx
	movl 60(%esp),%esi
	cmpl $4,%edx
	jne L1443
	cmpb $0,_r_drawfuzz+20
	jne L1446
	cmpb $0,_r_drawtrans+20
	jne L1444
L1446:
	movl $2,%edx
	jmp L1449
	.align 4
L1444:
	movl $64,%edx
	jmp L1449
	.align 4
L1443:
	cmpl $3,%edx
	jne L1449
	flds _transsouls+20
	movl $64,%edx
	fmuls LC14
	fnstcw 30(%esp)
	movw 30(%esp),%ax
	orw $3072,%ax
	movw %ax,28(%esp)
	fldcw 28(%esp)
	fistpl 24(%esp)
	movl 24(%esp),%ecx
	fldcw 30(%esp)
L1449:
	testl %ecx,%ecx
	jle L1456
	movl $65536,%ebx
	cmpl %ecx,%ebx
	jle L1458
	movl %ecx,%ebx
	jmp L1458
	.align 4
L1456:
	xorl %ebx,%ebx
L1458:
	movl 56(%esp),%eax
	movl _basecolormap,%edi
	movl %eax,_dc_translation
	movl %edi,_basecolormapsave
	cmpl $64,%edx
	je L1471
	jg L1475
	cmpl $1,%edx
	je L1470
	cmpl $2,%edx
	je L1461
	jmp L1484
	.align 4
L1475:
	cmpl $65,%edx
	je L1472
	cmpl $66,%edx
	je L1462
	jmp L1484
	.align 4
L1461:
	movl _fuzzcolfunc,%eax
	movb $1,_r_MarkTrans
	movl %eax,_colfunc
	movl $1,%eax
	jmp L1482
	.align 4
L1462:
	movl %ebx,%ecx
	sarl $12,%ecx
	jne L1463
L1484:
	xorl %eax,%eax
	jmp L1482
	.align 4
L1463:
	movl _R_DrawShadedColumn,%eax
	movl $_rt_shaded1col__Fiiii,_hcolfunc_post1
	movl %eax,_colfunc
	movl $_rt_shaded4cols__Fiii,_hcolfunc_post4
	movl _fixedcolormap,%eax
	testl %eax,%eax
	je L1464
	shrl $24,%esi
	movzbl (%esi,%eax),%eax
	jmp L1465
	.align 4
L1464:
	shrl $24,%esi
	movzbl (%esi,%edi),%eax
L1465:
	movl %eax,_dc_color
	movl _fixedlightlev,%edx
	movl $16,%eax
	subl %ecx,%eax
	sall $13,%eax
	orb $19,%ah
	addl _translationtables,%eax
	movl %eax,_basecolormap
	movl %eax,_dc_colormap
	testl %edx,%edx
	je L1466
	addl %edx,%eax
	movl %eax,_dc_colormap
L1466:
	movb $1,_r_MarkTrans
	jmp L1485
	.align 4
L1470:
	movl $65536,%edx
	xorl %eax,%eax
	jmp L1460
	.align 4
L1471:
	movl %ebx,%edx
	movl $65536,%eax
	subl %edx,%eax
	jmp L1460
	.align 4
L1472:
	movl %ebx,%edx
	movl $65536,%eax
L1460:
	addl $-8,%esp
	pushl %eax
	pushl %edx
	call _R_SetBlendFunc__Fll
	addl $16,%esp
	testb %al,%al
	je L1476
L1485:
	movl $1,%eax
	cmpl $0,_r_columnmethod+20
	je L1482
	movl $2,%eax
	jmp L1482
	.align 4
L1476:
	xorl %eax,%eax
L1482:
	popl %ebx
	popl %esi
	popl %edi
	addl $32,%esp
	ret
	.align 4
.globl _R_FinishSetPatchStyle__Fv
	.def	_R_FinishSetPatchStyle__Fv;	.scl	2;	.type	32;	.endef
_R_FinishSetPatchStyle__Fv:
	movl _basecolormapsave,%eax
	movl %eax,_basecolormap
	ret
.globl __vt$11Stat_detail
.section	.data$_vt$11Stat_detail,"w"
	.linkonce same_size
	.align 32
__vt$11Stat_detail:
	.word 0
	.word 0
	.long ___tf11Stat_detail
	.word 0
	.word 0
	.long __$_11Stat_detail
	.word 0
	.word 0
	.long _GetStats__11Stat_detailPc
	.space 8
.lcomm _Istaticstatdetail,16
.globl _r_drawtrans
.bss
	.align 4
_r_drawtrans:
	.space 24
.text
LC15:
	.ascii "r_drawtrans\0"
	.align 4
	.def	___static_initialization_and_destruction_0;	.scl	3;	.type	32;	.endef
___static_initialization_and_destruction_0:
	pushl %ebp
	movl %esp,%ebp
	subl $80,%esp
	pushl %esi
	pushl %ebx
	call ___get_eh_context
	movl %eax,%esi
	movl 12(%ebp),%ebx
	cmpl $65535,%ebx
	jne L1509
	cmpl $0,8(%ebp)
	je L1507
	addl $-8,%esp
	pushl $LC11
	pushl $_Istaticstatdetail
	call ___5FStatPCc
	movl 4(%esi),%eax
	leal 4(%esi),%edx
	movl %eax,-32(%ebp)
	movl $0,-28(%ebp)
	movl %ebp,-24(%ebp)
	movl $L1491,-20(%ebp)
	movl %esp,-16(%ebp)
L1491:
	leal -32(%ebp),%eax
	addl $16,%esp
	movl %eax,(%edx)
	addl $-12,%esp
	movl $__vt$11Stat_detail,_Istaticstatdetail+8
	movl 4(%esi),%eax
	movl (%eax),%eax
	movl %eax,4(%esi)
	pushl $0
	pushl $0
	addl $-2,%esp
	pushw $1
	pushl $LC15
	pushl $_r_drawtrans
	call ___9FBoolCVarPCcbUlPFR9FBoolCVar_v
	addl $32,%esp
	jmp L1509
	.align 4
L1507:
	addl $-8,%esp
	pushl $2
	pushl $_r_drawtrans
	call __$_9FBaseCVar
	addl $16,%esp
	addl $-8,%esp
	pushl $2
	pushl $_Istaticstatdetail
	call __$_5FStat
	addl $16,%esp
L1509:
	leal -88(%ebp),%esp
	popl %ebx
	popl %esi
	movl %ebp,%esp
	popl %ebp
	ret
	.comm	___ti11Stat_detail, 16	 # 12
LC16:
	.ascii "11Stat_detail\0"
.section	.text$_$_11Stat_detail,"x"
	.linkonce discard
	.align 4
.globl __$_11Stat_detail
	.def	__$_11Stat_detail;	.scl	2;	.type	32;	.endef
__$_11Stat_detail:
	subl $12,%esp
	movl 16(%esp),%edx
	movl 20(%esp),%eax
	addl $-8,%esp
	pushl %eax
	pushl %edx
	call __$_5FStat
	addl $16,%esp
	addl $12,%esp
	ret
.section	.text$__tf11Stat_detail,"x"
	.linkonce discard
	.align 4
.globl ___tf11Stat_detail
	.def	___tf11Stat_detail;	.scl	2;	.type	32;	.endef
___tf11Stat_detail:
	subl $24,%esp
	pushl %ebx
	movl $___ti11Stat_detail,%ebx
	cmpl $0,___ti11Stat_detail
	jne L1511
	call ___tf5FStat
	addl $-4,%esp
	pushl $___ti5FStat
	pushl $LC16
	pushl %ebx
	call ___rtti_si
	addl $16,%esp
L1511:
	movl %ebx,%eax
	popl %ebx
	addl $24,%esp
	ret
.section	.text$__11Stat_detail,"x"
	.linkonce discard
	.align 4
.globl ___11Stat_detail
	.def	___11Stat_detail;	.scl	2;	.type	32;	.endef
___11Stat_detail:
	pushl %ebp
	movl %esp,%ebp
	subl $92,%esp
	pushl %edi
	pushl %esi
	pushl %ebx
	call ___get_eh_context
	movl %eax,-68(%ebp)
	addl $-8,%esp
	pushl $LC11
	movl 8(%ebp),%eax
	pushl %eax
	call ___5FStatPCc
	movl -68(%ebp),%ecx
	movl -68(%ebp),%edx
	addl $4,%edx
	movl 4(%ecx),%eax
	movl %eax,-32(%ebp)
	movl $0,-28(%ebp)
	leal -24(%ebp),%eax
	movl %ebp,-24(%ebp)
	movl $L1416,4(%eax)
	movl %esp,8(%eax)
L1416:
	leal -32(%ebp),%eax
	addl $16,%esp
	movl %eax,(%edx)
	leal -104(%ebp),%esp
	movl 8(%ebp),%edx
	movl $__vt$11Stat_detail,8(%edx)
	movl -68(%ebp),%ecx
	movl 4(%ecx),%eax
	movl (%eax),%eax
	movl %eax,4(%ecx)
	popl %ebx
	movl %edx,%eax
	popl %esi
	popl %edi
	movl %ebp,%esp
	popl %ebp
	ret
.globl _viewwindowx
.bss
	.align 4
_viewwindowx:
	.space 4
.globl _viewwindowy
	.align 4
_viewwindowy:
	.space 4
.globl _viewheight
	.align 4
_viewheight:
	.space 4
.globl _viewwidth
	.align 4
_viewwidth:
	.space 4
.globl _halfviewwidth
	.align 4
_halfviewwidth:
	.space 4
.globl _realviewwidth
	.align 4
_realviewwidth:
	.space 4
.globl _realviewheight
	.align 4
_realviewheight:
	.space 4
.globl _detailxshift
	.align 4
_detailxshift:
	.space 4
.globl _detailyshift
	.align 4
_detailyshift:
	.space 4
.globl _BorderNeedRefresh
	.align 4
_BorderNeedRefresh:
	.space 4
.globl _BorderTopRefresh
	.align 4
_BorderTopRefresh:
	.space 4
.globl _ylookup
	.align 32
_ylookup:
	.space 4800
.globl _dc_colormap
	.align 4
_dc_colormap:
	.space 4
.globl _dc_x
	.align 4
_dc_x:
	.space 4
.globl _dc_yl
	.align 4
_dc_yl:
	.space 4
.globl _dc_yh
	.align 4
_dc_yh:
	.space 4
.globl _dc_iscale
	.align 4
_dc_iscale:
	.space 4
.globl _dc_texturemid
	.align 4
_dc_texturemid:
	.space 4
.globl _dc_texturefrac
	.align 4
_dc_texturefrac:
	.space 4
.globl _dc_color
	.align 4
_dc_color:
	.space 4
.globl _dc_srcblend
	.align 4
_dc_srcblend:
	.space 4
.globl _dc_destblend
	.align 4
_dc_destblend:
	.space 4
.globl _dc_source
	.align 4
_dc_source:
	.space 4
.globl _dc_dest
	.align 4
_dc_dest:
	.space 4
.globl _dc_count
	.align 4
_dc_count:
	.space 4
.globl _vplce
	.align 4
_vplce:
	.space 16
.globl _vince
	.align 4
_vince:
	.space 16
.globl _palookupoffse
	.align 4
_palookupoffse:
	.space 16
.globl _bufplce
	.align 4
_bufplce:
	.space 16
.globl _R_DrawColumn
	.align 4
_R_DrawColumn:
	.space 4
.globl _R_DrawFuzzColumn
	.align 4
_R_DrawFuzzColumn:
	.space 4
.globl _R_DrawShadedColumn
	.align 4
_R_DrawShadedColumn:
	.space 4
.globl _R_DrawTranslatedColumn
	.align 4
_R_DrawTranslatedColumn:
	.space 4
.globl _R_DrawSpan
	.align 4
_R_DrawSpan:
	.space 4
.globl _R_DrawColumnHoriz
	.align 4
_R_DrawColumnHoriz:
	.space 4
.globl _rt_map4cols
	.align 4
_rt_map4cols:
	.space 4
.globl _ds_y
	.align 4
_ds_y:
	.space 4
.globl _ds_x1
	.align 4
_ds_x1:
	.space 4
.globl _ds_x2
	.align 4
_ds_x2:
	.space 4
.globl _ds_colormap
	.align 4
_ds_colormap:
	.space 4
.globl _ds_xfrac
	.align 4
_ds_xfrac:
	.space 4
.globl _ds_yfrac
	.align 4
_ds_yfrac:
	.space 4
.globl _ds_xstep
	.align 4
_ds_xstep:
	.space 4
.globl _ds_ystep
	.align 4
_ds_ystep:
	.space 4
.globl _ds_source
	.align 4
_ds_source:
	.space 4
.globl _ds_color
	.align 4
_ds_color:
	.space 4
.globl _translationtables
	.align 4
_translationtables:
	.space 4
.globl _dc_translation
	.align 4
_dc_translation:
	.space 4
.globl _viewimage
	.align 4
_viewimage:
	.space 4
.globl _scaledviewwidth
	.align 4
_scaledviewwidth:
	.space 4
.globl _dccount
	.align 4
_dccount:
	.space 4
.globl _DetailDoubleCycles
	.align 4
_DetailDoubleCycles:
	.space 4
.globl _fuzzoffset
	.align 32
_fuzzoffset:
	.space 204
.text
	.align 32
_fuzzinit:
	.byte 1
	.byte -1
	.byte 1
	.byte -1
	.byte 1
	.byte 1
	.byte -1
	.byte 1
	.byte 1
	.byte -1
	.byte 1
	.byte 1
	.byte 1
	.byte -1
	.byte 1
	.byte 1
	.byte 1
	.byte -1
	.byte -1
	.byte -1
	.byte -1
	.byte 1
	.byte -1
	.byte -1
	.byte 1
	.byte 1
	.byte 1
	.byte 1
	.byte -1
	.byte 1
	.byte -1
	.byte 1
	.byte 1
	.byte -1
	.byte -1
	.byte 1
	.byte 1
	.byte -1
	.byte -1
	.byte -1
	.byte -1
	.byte 1
	.byte 1
	.byte 1
	.byte 1
	.byte -1
	.byte 1
	.byte 1
	.byte -1
	.byte 1
.globl _dscount
.bss
	.align 4
_dscount:
	.space 4
.lcomm _vlinebits,16
.lcomm _basecolormapsave,16
	.comm	___ti5FStat, 16	 # 8
.text
LC17:
	.ascii "5FStat\0"
.section	.text$__tf5FStat,"x"
	.linkonce discard
	.align 4
.globl ___tf5FStat
	.def	___tf5FStat;	.scl	2;	.type	32;	.endef
___tf5FStat:
	subl $24,%esp
	pushl %ebx
	movl $___ti5FStat,%ebx
	cmpl $0,___ti5FStat
	jne L1515
	addl $-8,%esp
	pushl $LC17
	pushl %ebx
	call ___rtti_user
	addl $16,%esp
L1515:
	movl %ebx,%eax
	popl %ebx
	addl $24,%esp
	ret
.text
	.align 4
	.def	__GLOBAL_$I$dc_pitch;	.scl	3;	.type	32;	.endef
__GLOBAL_$I$dc_pitch:
	subl $12,%esp
	addl $-8,%esp
	pushl $65535
	pushl $1
	call ___static_initialization_and_destruction_0
	addl $16,%esp
	addl $12,%esp
	ret
	.section .ctor
	.long	__GLOBAL_$I$dc_pitch
.text
	.align 4
	.def	__GLOBAL_$D$dc_pitch;	.scl	3;	.type	32;	.endef
__GLOBAL_$D$dc_pitch:
	subl $12,%esp
	addl $-8,%esp
	pushl $65535
	pushl $0
	call ___static_initialization_and_destruction_0
	addl $16,%esp
	addl $12,%esp
	ret
	.section .dtor
	.long	__GLOBAL_$D$dc_pitch
	.def	___rtti_user;	.scl	3;	.type	32;	.endef
	.def	___rtti_si;	.scl	3;	.type	32;	.endef
	.def	___tf5FStat;	.scl	3;	.type	32;	.endef
	.def	___get_eh_context;	.scl	3;	.type	32;	.endef
	.def	__$_9FBaseCVar;	.scl	3;	.type	32;	.endef
	.def	___9FBoolCVarPCcbUlPFR9FBoolCVar_v;	.scl	3;	.type	32;	.endef
	.def	___11Stat_detail;	.scl	3;	.type	32;	.endef
	.def	__$_11Stat_detail;	.scl	3;	.type	32;	.endef
	.def	___tf11Stat_detail;	.scl	3;	.type	32;	.endef
	.def	_rt_shaded4cols__Fiii;	.scl	2;	.type	32;	.endef
	.def	_rt_shaded1col__Fiiii;	.scl	2;	.type	32;	.endef
	.def	_rt_tlateaddclamp4cols__Fiii;	.scl	2;	.type	32;	.endef
	.def	_rt_tlateaddclamp1col__Fiiii;	.scl	2;	.type	32;	.endef
	.def	_rt_addclamp4cols__Fiii;	.scl	2;	.type	32;	.endef
	.def	_rt_addclamp1col__Fiiii;	.scl	2;	.type	32;	.endef
	.def	_rt_tlateadd4cols__Fiii;	.scl	2;	.type	32;	.endef
	.def	_rt_tlateadd1col__Fiiii;	.scl	2;	.type	32;	.endef
	.def	_rt_add4cols__Fiii;	.scl	2;	.type	32;	.endef
	.def	_rt_add1col__Fiiii;	.scl	2;	.type	32;	.endef
	.def	_rt_tlate4cols__Fiii;	.scl	2;	.type	32;	.endef
	.def	_rt_tlate1col__Fiiii;	.scl	2;	.type	32;	.endef
	.def	_rt_map1col_asm;	.scl	3;	.type	32;	.endef
	.def	_rt_map4cols_asm1;	.scl	3;	.type	32;	.endef
	.def	_rt_map4cols_asm2;	.scl	3;	.type	32;	.endef
	.def	_R_DrawSpanP_ASM;	.scl	3;	.type	32;	.endef
	.def	_R_DrawFuzzColumnP_ASM;	.scl	3;	.type	32;	.endef
	.def	_R_DrawColumnHorizP_ASM;	.scl	3;	.type	32;	.endef
	.def	_R_DrawColumnP_ASM;	.scl	3;	.type	32;	.endef
	.def	_sprintf;	.scl	3;	.type	32;	.endef
	.def	__$_5FStat;	.scl	3;	.type	32;	.endef
	.def	___5FStatPCc;	.scl	3;	.type	32;	.endef
	.def	_DoubleHorizVert_MMX;	.scl	3;	.type	32;	.endef
	.def	_DoubleHoriz_MMX;	.scl	3;	.type	32;	.endef
	.def	_DoubleVert_ASM;	.scl	3;	.type	32;	.endef
	.def	_V_MarkRect__Fiiii;	.scl	2;	.type	32;	.endef
	.def	_M_DrawFrame__Fiiii;	.scl	2;	.type	32;	.endef
	.def	_W_CacheLumpNum__Fii;	.scl	2;	.type	32;	.endef
	.def	_Pick__13FColorMatcheriii;	.scl	3;	.type	32;	.endef
	.def	_HSVtoRGB__FPfN20fff;	.scl	2;	.type	32;	.endef
	.def	_RGBtoHSV__FfffPfN23;	.scl	2;	.type	32;	.endef
	.def	_Z_Malloc__FUiiPv;	.scl	2;	.type	32;	.endef
	.def	_prevlineasm1;	.scl	3;	.type	32;	.endef
	.def	_vlineasm1;	.scl	3;	.type	32;	.endef
	.def	_provlineasm4;	.scl	3;	.type	32;	.endef
	.def	_prosetupvlineasm;	.scl	3;	.type	32;	.endef
	.def	_memset;	.scl	3;	.type	32;	.endef
	.def	_W_GetNumForName__FPCc;	.scl	2;	.type	32;	.endef
	.def	_W_CheckNumForName__FPCci;	.scl	2;	.type	32;	.endef
