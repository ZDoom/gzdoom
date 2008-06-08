%ifdef VALGRIND_AWARE

%define VG_USERREQ__DISCARD_TRANSLATIONS 0x1002

%macro selfmod 2
		pusha
		sub		esp,6*4
		mov		dword [esp],    VG_USERREQ__DISCARD_TRANSLATIONS
		mov		dword [esp+4],  %1
		mov		dword [esp+8],  %2 - %1
		mov		dword [esp+12], 0
		mov		dword [esp+16], 0
		mov		dword [esp+20], 0
		mov		eax, esp
		; special instruction preamble
		rol		edi, 3
		rol		edi, 13
		rol		edi, 29
		rol		edi, 19
		xchg	ebx, ebx
		; restore state
		add		esp,6*4
		popa
%endmacro

%else

%macro selfmod 2
%endmacro

%endif
