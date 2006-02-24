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
!MESSAGE "FLAC - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "FLAC - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "FLAC - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /ZI /W3 /Od /D "WIN32" /D "_DEBUG" /D "_LIB" /D "FLAC__CPU_IA32" /D "FLAC__HAS_NASM" /D "FLAC__SSE_OS" /D "FLAC__USE_3DNOW" /D "_MBCS" /Gm PRECOMP_VC7_TOBEREMOVED /GZ /c /GX 
# ADD CPP /nologo /MTd /ZI /W3 /Od /D "WIN32" /D "_DEBUG" /D "_LIB" /D "FLAC__CPU_IA32" /D "FLAC__HAS_NASM" /D "FLAC__SSE_OS" /D "FLAC__USE_3DNOW" /D "_MBCS" /Gm PRECOMP_VC7_TOBEREMOVED /GZ /c /GX 
# ADD BASE MTL /nologo /win32 
# ADD MTL /nologo /win32 
# ADD BASE RSC /l 1033 
# ADD RSC /l 1033 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo 
# ADD BSC32 /nologo 
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"Debug\FLAC.lib" 
# ADD LIB32 /nologo /out:"Debug\FLAC.lib" 

!ELSEIF  "$(CFG)" == "FLAC - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /Zi /W3 /O2 /Ob1 /Oy /D "WIN32" /D "NDEBUG" /D "_LIB" /D "FLAC__CPU_IA32" /D "FLAC__HAS_NASM" /D "FLAC__SSE_OS" /D "FLAC__USE_3DNOW" /D "_MBCS" /GF /Gy PRECOMP_VC7_TOBEREMOVED /c /GX 
# ADD CPP /nologo /MT /Zi /W3 /O2 /Ob1 /Oy /D "WIN32" /D "NDEBUG" /D "_LIB" /D "FLAC__CPU_IA32" /D "FLAC__HAS_NASM" /D "FLAC__SSE_OS" /D "FLAC__USE_3DNOW" /D "_MBCS" /GF /Gy PRECOMP_VC7_TOBEREMOVED /c /GX 
# ADD BASE MTL /nologo /win32 
# ADD MTL /nologo /win32 
# ADD BASE RSC /l 1033 
# ADD RSC /l 1033 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo 
# ADD BSC32 /nologo 
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"Release\FLAC.lib" 
# ADD LIB32 /nologo /out:"Release\FLAC.lib" 

!ENDIF

# Begin Target

# Name "FLAC - Win32 Debug"
# Name "FLAC - Win32 Release"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;def;odl;idl;hpj;bat;asm"
# Begin Source File

SOURCE=bitbuffer.c
# End Source File
# Begin Source File

SOURCE=bitmath.c
# End Source File
# Begin Source File

SOURCE=cpu.c
# End Source File
# Begin Source File

SOURCE=crc.c
# End Source File
# Begin Source File

SOURCE=fixed.c
# End Source File
# Begin Source File

SOURCE=format.c
# End Source File
# Begin Source File

SOURCE=lpc.c
# End Source File
# Begin Source File

SOURCE=stream_decoder.c
# End Source File
# Begin Source File

SOURCE=stream_decoder_pp.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl;inc"
# Begin Group "Protected"

# PROP Default_Filter ""
# Begin Source File

SOURCE=protected\stream_decoder.h
# End Source File
# End Group
# Begin Group "Private"

# PROP Default_Filter ""
# Begin Source File

SOURCE=private\bitbuffer.h
# End Source File
# Begin Source File

SOURCE=private\bitmath.h
# End Source File
# Begin Source File

SOURCE=private\cpu.h
# End Source File
# Begin Source File

SOURCE=private\crc.h
# End Source File
# Begin Source File

SOURCE=private\fixed.h
# End Source File
# Begin Source File

SOURCE=private\format.h
# End Source File
# Begin Source File

SOURCE=private\lpc.h
# End Source File
# End Group
# Begin Group "FLAC"

# PROP Default_Filter ""
# Begin Source File

SOURCE=FLAC\assert.h
# End Source File
# Begin Source File

SOURCE=FLAC\export.h
# End Source File
# Begin Source File

SOURCE=FLAC\format.h
# End Source File
# Begin Source File

SOURCE=FLAC\ordinals.h
# End Source File
# Begin Source File

SOURCE=FLAC\stream_decoder.h
# End Source File
# End Group
# Begin Group "FLAC++"

# PROP Default_Filter ""
# Begin Source File

SOURCE=FLAC++\decoder.h
# End Source File
# Begin Source File

SOURCE=FLAC++\export.h
# End Source File
# End Group
# End Group
# Begin Group "IA32 Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=ia32\cpu_asm.nasm

!IF  "$(CFG)" == "FLAC - Win32 Debug"

# PROP Ignore_Default_Tool 1
# Begin Custom Build - Assembling $(InputPath)...
SOURCE="$(InputPath)"

BuildCmds= \
	nasmw -o $(IntDir)\$(InputName).obj -d OBJ_FORMAT_win32 -f win32 $(InputPath) \


"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "FLAC - Win32 Release"

# PROP Ignore_Default_Tool 1
# Begin Custom Build - Assembling $(InputPath)...
SOURCE="$(InputPath)"

BuildCmds= \
	nasmw -o $(IntDir)\$(InputName).obj -d OBJ_FORMAT_win32 -f win32 $(InputPath) \


"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ENDIF

# End Source File
# Begin Source File

SOURCE=ia32\fixed_asm.nasm

!IF  "$(CFG)" == "FLAC - Win32 Debug"

# PROP Ignore_Default_Tool 1
# Begin Custom Build - Assembling $(InputPath)...
SOURCE="$(InputPath)"

BuildCmds= \
	nasmw -o $(IntDir)\$(InputName).obj -d OBJ_FORMAT_win32 -f win32 $(InputPath) \


"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "FLAC - Win32 Release"

# PROP Ignore_Default_Tool 1
# Begin Custom Build - Assembling $(InputPath)...
SOURCE="$(InputPath)"

BuildCmds= \
	nasmw -o $(IntDir)\$(InputName).obj -d OBJ_FORMAT_win32 -f win32 $(InputPath) \


"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ENDIF

# End Source File
# Begin Source File

SOURCE=ia32\lpc_asm.nasm

!IF  "$(CFG)" == "FLAC - Win32 Debug"

# PROP Ignore_Default_Tool 1
# Begin Custom Build - Assembling $(InputPath)...
SOURCE="$(InputPath)"

BuildCmds= \
	nasmw -o $(IntDir)\$(InputName).obj -d OBJ_FORMAT_win32 -f win32 $(InputPath) \


"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "FLAC - Win32 Release"

# PROP Ignore_Default_Tool 1
# Begin Custom Build - Assembling $(InputPath)...
SOURCE="$(InputPath)"

BuildCmds= \
	nasmw -o $(IntDir)\$(InputName).obj -d OBJ_FORMAT_win32 -f win32 $(InputPath) \


"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ENDIF

# End Source File
# Begin Source File

SOURCE=ia32\nasm.h
# End Source File
# End Group
# Begin Source File

SOURCE=ReadMe.txt
# End Source File
# End Target
# End Project

