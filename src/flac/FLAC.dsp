# Microsoft Developer Studio Project File - Name="FLAC" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=FLAC - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "FLAC.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "FLAC.mak" CFG="FLAC - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "FLAC - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "FLAC - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "FLAC - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "FLAC___Win32_Release"
# PROP BASE Intermediate_Dir "FLAC___Win32_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "FLAC___Win32_Release"
# PROP Intermediate_Dir "FLAC___Win32_Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "FLAC__CPU_IA32" /D "FLAC__HAS_NASM" /D "FLAC__SSE_OS" /D "FLAC__USE_3DNOW" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "FLAC - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "FLAC___Win32_Debug"
# PROP BASE Intermediate_Dir "FLAC___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "FLAC___Win32_Debug"
# PROP Intermediate_Dir "FLAC___Win32_Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "FLAC__CPU_IA32" /D "FLAC__HAS_NASM" /D "FLAC__SSE_OS" /D "FLAC__USE_3DNOW" /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "FLAC - Win32 Release"
# Name "FLAC - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\bitbuffer.c
# End Source File
# Begin Source File

SOURCE=.\bitmath.c
# End Source File
# Begin Source File

SOURCE=.\cpu.c
# End Source File
# Begin Source File

SOURCE=.\crc.c
# End Source File
# Begin Source File

SOURCE=.\fixed.c
# End Source File
# Begin Source File

SOURCE=.\format.c
# End Source File
# Begin Source File

SOURCE=.\lpc.c
# End Source File
# Begin Source File

SOURCE=.\stream_decoder.c
# End Source File
# Begin Source File

SOURCE=.\stream_decoder_pp.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Group "Protected"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\protected\stream_decoder.h
# End Source File
# End Group
# Begin Group "Private"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\private\bitbuffer.h
# End Source File
# Begin Source File

SOURCE=.\private\bitmath.h
# End Source File
# Begin Source File

SOURCE=.\private\cpu.h
# End Source File
# Begin Source File

SOURCE=.\private\crc.h
# End Source File
# Begin Source File

SOURCE=.\private\fixed.h
# End Source File
# Begin Source File

SOURCE=.\private\format.h
# End Source File
# Begin Source File

SOURCE=.\private\lpc.h
# End Source File
# End Group
# Begin Group "FLAC"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\FLAC\assert.h
# End Source File
# Begin Source File

SOURCE=.\FLAC\export.h
# End Source File
# Begin Source File

SOURCE=.\FLAC\format.h
# End Source File
# Begin Source File

SOURCE=.\FLAC\ordinals.h
# End Source File
# Begin Source File

SOURCE=.\FLAC\stream_decoder.h
# End Source File
# End Group
# Begin Group "FLAC++"

# PROP Default_Filter ""
# Begin Source File

SOURCE=".\FLAC++\decoder.h"
# End Source File
# Begin Source File

SOURCE=".\FLAC++\export.h"
# End Source File
# End Group
# End Group
# Begin Group "IA32 Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\ia32\cpu_asm.nasm

!IF  "$(CFG)" == "FLAC - Win32 Release"

# Begin Custom Build
IntDir=.\FLAC___Win32_Release
InputPath=.\ia32\cpu_asm.nasm
InputName=cpu_asm

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nasmw -o $(IntDir)\$(InputName).obj -d OBJ_FORMAT_win32 -f win32 $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "FLAC - Win32 Debug"

# Begin Custom Build
IntDir=.\FLAC___Win32_Debug
InputPath=.\ia32\cpu_asm.nasm
InputName=cpu_asm

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nasmw -o $(IntDir)\$(InputName).obj -d OBJ_FORMAT_win32 -f win32 $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\ia32\fixed_asm.nasm

!IF  "$(CFG)" == "FLAC - Win32 Release"

# Begin Custom Build
IntDir=.\FLAC___Win32_Release
InputPath=.\ia32\fixed_asm.nasm
InputName=fixed_asm

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nasmw -o $(IntDir)\$(InputName).obj -d OBJ_FORMAT_win32 -f win32 $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "FLAC - Win32 Debug"

# Begin Custom Build
IntDir=.\FLAC___Win32_Debug
InputPath=.\ia32\fixed_asm.nasm
InputName=fixed_asm

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nasmw -o $(IntDir)\$(InputName).obj -d OBJ_FORMAT_win32 -f win32 $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\ia32\lpc_asm.nasm

!IF  "$(CFG)" == "FLAC - Win32 Release"

# Begin Custom Build
IntDir=.\FLAC___Win32_Release
InputPath=.\ia32\lpc_asm.nasm
InputName=lpc_asm

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nasmw -o $(IntDir)\$(InputName).obj -d OBJ_FORMAT_win32 -f win32 $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "FLAC - Win32 Debug"

# Begin Custom Build
IntDir=.\FLAC___Win32_Debug
InputPath=.\ia32\lpc_asm.nasm
InputName=lpc_asm

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nasmw -o $(IntDir)\$(InputName).obj -d OBJ_FORMAT_win32 -f win32 $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\ia32\nasm.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\ReadMe.txt
# End Source File
# End Target
# End Project
