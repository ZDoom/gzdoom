#include "doomtype.h"
#include "r_main.h"
#include "r_draw.h"

// All the assembly routines here were taken
// from doom3's tmap.S file and translated
// into VC++ inline assembly

#ifdef USEASM

int rowbytes;

__declspec(naked) void ASM_PatchRowBytes (int bytes) {
	__asm {
		push	ebp
		mov		ebp,esp				// assure l'"adressabilit‚ du stack"

		mov		edx,[esp+8]			// lire un argument
		mov		rowbytes,edx

		//added:28-01-98:this is really crappy but I want to get it working...
		//				 I'll clean this later

		// 1 * rowbytes
		mov		p1+2,edx

		mov		p5+2,edx

		// 2 * rowbytes
		add		edx,[esp+8]

		mov		p2+2,edx

		mov		p6+2,edx
		mov		p7+2,edx
		mov		p8+2,edx
		mov		p9+2,edx

		// 3 * rowbytes
		add		edx,[esp+8]

		mov		edx,p3+2

		// 4 * rowbytes
		add		edx,[esp+8]

		mov		p4+2,edx

		pop		ebp
		ret
	}
}

//----------------------------------------------------------------------
//
// R_DrawTranslucentColumn
//
// Vertical column texture drawer, with translucency.
//
//----------------------------------------------------------------------

__declspec(naked) void R_DrawTranslucentColumn (void)
{
	__asm {
		push	ebp						// preserve caller's stack frame pointer
		push	esi						// preserve register variables
		push	edi
		push	ebx

//
// dest = ylookup[dc_yl] + columnofs[dc_x];
//
		mov		ebp,dc_yl
		mov		ebx,ebp
		mov		edi,[ylookup+ebx*4]
		mov		ebx,dc_x
		add		edi,[columnofs+ebx*4]	// edi = dest

//
// pixelcount = yh - yl + 1
//
		mov		eax,dc_yh
		inc		eax
		sub		eax,ebp					// pixel count
		mov		pixelcount,eax			// save for final pixel
		jle 	vtdone					// nothing to scale

//
// frac = dc_texturemid - (centery-dc_yl)*fracstep;
//
		mov		ecx,dc_iscale			// fracstep
		mov		eax,centery
		sub		eax,ebp
		imul	eax,ecx
		mov		edx,dc_texturemid
		sub		edx,eax
		mov		ebx,edx
		shr		ebx,FRACBITS			// frac int.
		and		ebx,0x0000007f
		shl		edx,FRACBITS			// y frac up

		mov		ebp,ecx
		shl		ebp,FRACBITS			// fracstep f. up
		shr		ecx,FRACBITS			// fracstep i. ->cl
		and		cl,0x7f

		mov		esi,dc_source

//
// lets rock :) !
//
		mov		eax,pixelcount
		mov		dh,al
		shr		eax,2
		mov		ch,al					// quad count
		mov		eax,TransTable
		test	dh,0x03
		jz		vt4quadloop

//
//	do un-even pixel
//
		test	dh,1
		jz		do2

		mov		ah,[esi+ebx]	 		// fetch texel : colormap number
		add		edx,ebp
		adc		bl,cl
		mov		al,[edi]				// fetch dest  : index into colormap
		and		bl,0x7f
		mov		dl,[eax]
		mov		[edi],edi
		add		edi,SCREENPITCH

//
//	do two non-quad-aligned pixels
//
do2:
		test	dh,2
		jz		do3

		mov		ah,[esi+ebx]	 		// fetch texel : colormap number
		add		edx,ebp
		adc		bl,cl
		mov		al,[edi]				// fetch dest  : index into colormap
		and		bl,0x7f
		mov		dl,[eax]
		mov		[edi],edi
		add		edi,SCREENPITCH

		mov		ah,[esi+ebx]	 		// fetch texel : colormap number
		add		edx,ebp
		adc		bl,cl
		mov		al,[edi]				// fetch dest  : index into colormap
		and		bl,0x7f
		mov		dl,[eax]
		mov		[edi],edi
		add		edi,SCREENPITCH

//
//	test if there was at least 4 pixels
//
do3:
	test	ch,0xFF					// test quad count
	jz		vtdone

//
// ebp : ystep frac. upper 24 bits
// edx : y	   frac. upper 24 bits
// ebx : y	   i.	 lower 7 bits,	masked for index
// ecx : ch = counter, cl = y step i.
// eax : colormap aligned 256
// esi : source texture column
// edi : dest screen
//
vt4quadloop:
		mov		dh,0x7f					// prep mask

		mov		ah,[esi+ebx]			// fetch texel : colormap number
p5:		mov		al,[edi+0x12345678]		// fetch dest  : index into colormap

		mov		tystep,ebp
		mov		ebp,edi
		sub		edi,SCREENPITCH
		jmp		inloop
//	  .align  4
vtquadloop:
		add		edx,tystep
		adc		bl,cl
		and		bl,dh
p6:		add		ebp,2*0x12345678
		mov		dl,[eax]
		mov		ah,[esi+ebx]			// fetch texel : colormap number
		mov		[edi],dl
		mov		al,[ebp]				// fetch dest	: index into colormap
inloop:
		add		edx,tystep
		adc		bl,cl
		and		bl,dh
p7:		add		edi,2*0x12345678
		mov		dl,[eax]
		mov		ah,[esi+ebx]			// fetch texel : colormap number
		mov		[ebp],dl
		mov		al,[edi]				// fetch dest	: index into colormap

		add		edx,tystep
		adc		bl,cl
		and		bl,dh
p8:		add		ebp,2*0x12345678
		mov		dl,[eax]
		mov		ah,[esi+ebx]			// fetch texel : colormap number
		mov		[edi],dl
		mov		al,[ebp]				// fetch dest	: index into colormap

		add		edx,tystep
		adc		bl,cl
		and		bl,dh
p9:		add		edi,2*0x12345678
		mov		dl,[eax]
		mov		ah,[esi+ebx]			// fetch texel : colormap number
		mov		[ebp],dl
		mov		al,[edi]				// fetch dest	: index into colormap

//	  movb	  (%esi,%ebx),%ah		// fetch texel : colormap number
//	   addl    %ebp,%edx
//	  adcb	  %cl,%bl
//	   movb    (%edi),%al			// fetch dest  : index into colormap
//	  andb	  %dh,%bl
//	   movb    (%eax),%dl
//	  movb	  %dl,(%edi)
//	   addl    $SCREENWIDTH,%edi

		dec		ch
		jnz		vtquadloop

vtdone:
		pop		ebx						// restore register variables
		pop		edi
		pop		esi
		pop		ebp						// restore caller's stack frame pointer
		ret
	}
}

#endif