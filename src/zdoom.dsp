# Microsoft Developer Studio Project File - Name="_zdoom" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=_zdoom - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "_zdoom.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "_zdoom.mak" CFG="_zdoom - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "_zdoom - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "_zdoom - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\Release"
# PROP BASE Intermediate_Dir ".\Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir ".\Release"
# PROP Intermediate_Dir ".\Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /I "win32" /I "sound" /I "." /I "zlib-1.1.4" /I "g_shared" /I "g_doom" /I "g_raven" /I "g_heretic" /I "g_hexen;flac" /Zd /W3 /Ox /Og /Ob2 /Oi /Ot /Oy /D "NDEBUG" /D "WIN32" /D "_WIN32" /D "_WINDOWS" /D "USEASM" /GF PRECOMP_VC7_TOBEREMOVED /Fo".\Release/" /Fd".\Release/" /Gr /c "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
# ADD CPP /nologo /MT /I "win32" /I "sound" /I "." /I "zlib-1.1.4" /I "g_shared" /I "g_doom" /I "g_raven" /I "g_heretic" /I "g_hexen;flac" /Zd /W3 /Ox /Og /Ob2 /Oi /Ot /Oy /D "NDEBUG" /D "WIN32" /D "_WIN32" /D "_WINDOWS" /D "USEASM" /GF PRECOMP_VC7_TOBEREMOVED /Fo".\Release/" /Fd".\Release/" /Gr /c "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
# ADD BASE MTL /nologo /D"NDEBUG" /mktyplib203 /tlb".\Release\zdoom.tlb" /win32 
# ADD MTL /nologo /D"NDEBUG" /mktyplib203 /tlb".\Release\zdoom.tlb" /win32 
# ADD BASE RSC /l 1033 /d "NDEBUG" 
# ADD RSC /l 1033 /d "NDEBUG" 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo 
# ADD BSC32 /nologo 
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib dxguid.lib dinput8.lib comctl32.lib strmiids.lib wsock32.lib winmm.lib fmodvc.lib /nologo /out:"..\..\zdoom.exe" /incremental:no /debug /pdb:".\Release\zdoom.pdb" /pdbtype:sept /map:".\Release\zdoom.map" /mapinfo:exports /subsystem:windows /stack:0 /opt:ref /opt:icf /MACHINE:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib dxguid.lib dinput8.lib comctl32.lib strmiids.lib wsock32.lib winmm.lib fmodvc.lib /nologo /out:"..\..\zdoom.exe" /incremental:no /debug /pdb:".\Release\zdoom.pdb" /pdbtype:sept /map:".\Release\zdoom.map" /mapinfo:exports /subsystem:windows /stack:0 /opt:ref /opt:icf /MACHINE:I386

!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ".\Debug"
# PROP BASE Intermediate_Dir ".\Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir ".\Debug"
# PROP Intermediate_Dir ".\Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /I "win32" /I "sound" /I "." /I ""zlib-1.1.4"" /I "g_shared" /I "g_doom" /I "g_raven" /I "g_heretic" /I "g_hexen" /I "flac" /ZI /W3 /Od /D "WIN32" /D "_DEBUG" /D "_WIN32" /D "_WINDOWS" /D "USEASM" /D "_CRTDBG_MAP_ALLOC" /Gm /Gy /Fo".\Debug/" /Fd".\Debug/" /GZ /c "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
# ADD CPP /nologo /MTd /I "win32" /I "sound" /I "." /I ""zlib-1.1.4"" /I "g_shared" /I "g_doom" /I "g_raven" /I "g_heretic" /I "g_hexen" /I "flac" /ZI /W3 /Od /D "WIN32" /D "_DEBUG" /D "_WIN32" /D "_WINDOWS" /D "USEASM" /D "_CRTDBG_MAP_ALLOC" /Gm /Gy /Fo".\Debug/" /Fd".\Debug/" /GZ /c "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
# ADD BASE MTL /nologo /D"_DEBUG" /mktyplib203 /tlb".\Debug\zdoom.tlb" /win32 
# ADD MTL /nologo /D"_DEBUG" /mktyplib203 /tlb".\Debug\zdoom.tlb" /win32 
# ADD BASE RSC /l 1033 /d "_DEBUG" 
# ADD RSC /l 1033 /d "_DEBUG" 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo 
# ADD BSC32 /nologo 
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib dxguid.lib dinput8.lib comctl32.lib strmiids.lib wsock32.lib winmm.lib fmodvc.lib /nologo /out:"..\..\zdoomd.exe" /incremental:yes /libpath:"d:\gnu\flac-1.1.0\obj\debug\lib" /nodefaultlib:"libcmt" /nodefaultlib:"msvcrtd" /nodefaultlib:"msvcrt" /debug /pdb:".\Debug\doomdbg.pdb" /pdbtype:sept /subsystem:windows /stack:0 /MACHINE:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib dxguid.lib dinput8.lib comctl32.lib strmiids.lib wsock32.lib winmm.lib fmodvc.lib /nologo /out:"..\..\zdoomd.exe" /incremental:yes /libpath:"d:\gnu\flac-1.1.0\obj\debug\lib" /nodefaultlib:"libcmt" /nodefaultlib:"msvcrtd" /nodefaultlib:"msvcrt" /debug /pdb:".\Debug\doomdbg.pdb" /pdbtype:sept /subsystem:windows /stack:0 /MACHINE:I386

!ENDIF

# Begin Target

# Name "_zdoom - Win32 Release"
# Name "_zdoom - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "c;cpp"
# Begin Source File

SOURCE=.\Am_map.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\autostart.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\autozend.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\b_bot.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\B_func.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\B_game.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\b_move.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\B_think.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\bbannouncer.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\c_bind.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\c_cmds.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\c_console.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\c_cvars.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\c_dispatch.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\c_expr.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\cmdlib.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\colormatcher.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\configfile.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\ct_chat.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\d_dehacked.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\D_main.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\d_net.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\d_netinfo.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\d_protocol.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\decallib.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=decorations.cpp
# End Source File
# Begin Source File

SOURCE=.\dobject.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\doomdef.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\doomstat.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\dsectoreffect.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\dthinker.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\f_finale.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\f_wipe.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\farchive.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\files.cpp
# End Source File
# Begin Source File

SOURCE=.\g_game.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_level.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\gameconfigfile.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\gi.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\hu_scores.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\info.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\infodefaults.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\lumpconfigfile.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\m_alloc.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\m_argv.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\m_bbox.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\m_cheat.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\m_fixed.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\m_menu.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\m_misc.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\m_options.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\m_png.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\m_random.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\mus2midi.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=nodebuild.cpp
# End Source File
# Begin Source File

SOURCE=nodebuild_events.cpp
# End Source File
# Begin Source File

SOURCE=nodebuild_extract.cpp
# End Source File
# Begin Source File

SOURCE=nodebuild_gl.cpp
# End Source File
# Begin Source File

SOURCE=nodebuild_utility.cpp
# End Source File
# Begin Source File

SOURCE=.\p_acs.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\p_buildmap.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\p_ceiling.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\p_doors.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\p_effect.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\p_enemy.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\p_floor.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\p_interaction.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\p_lights.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\p_lnspec.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\p_map.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\p_maputl.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\p_mobj.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\p_pillar.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\p_plats.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\p_pspr.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\p_saveg.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\p_sectors.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\p_setup.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\p_sight.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\p_spec.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\p_switch.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\p_teleport.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\p_terrain.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\p_things.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\p_tick.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\p_trace.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\p_user.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\p_writemap.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\p_xlat.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\po_man.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\s_advsound.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=s_environment.cpp
# End Source File
# Begin Source File

SOURCE=.\s_playlist.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\s_sndseq.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\s_sound.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\sc_man.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\skins.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\st_stuff.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\stats.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\stringtable.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\tables.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\tempfiles.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\v_collection.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\v_draw.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\v_font.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\v_palette.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\v_pfx.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\v_text.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\v_video.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\vectors.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\w_wad.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\wi_stuff.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h"
# Begin Source File

SOURCE=.\actor.h
# End Source File
# Begin Source File

SOURCE=.\am_map.h
# End Source File
# Begin Source File

SOURCE=.\announcer.h
# End Source File
# Begin Source File

SOURCE=.\autosegs.h
# End Source File
# Begin Source File

SOURCE=.\B_BOT.H
# End Source File
# Begin Source File

SOURCE=.\basicinlines.h
# End Source File
# Begin Source File

SOURCE=.\c_bind.h
# End Source File
# Begin Source File

SOURCE=.\c_console.h
# End Source File
# Begin Source File

SOURCE=.\c_cvars.h
# End Source File
# Begin Source File

SOURCE=.\c_dispatch.h
# End Source File
# Begin Source File

SOURCE=.\cmdlib.h
# End Source File
# Begin Source File

SOURCE=.\colormatcher.h
# End Source File
# Begin Source File

SOURCE=.\configfile.h
# End Source File
# Begin Source File

SOURCE=.\d_dehacked.h
# End Source File
# Begin Source File

SOURCE=.\d_dehackedactions.h
# End Source File
# Begin Source File

SOURCE=.\d_event.h
# End Source File
# Begin Source File

SOURCE=.\d_gui.h
# End Source File
# Begin Source File

SOURCE=.\d_main.h
# End Source File
# Begin Source File

SOURCE=.\d_net.h
# End Source File
# Begin Source File

SOURCE=.\d_netinf.h
# End Source File
# Begin Source File

SOURCE=.\d_player.h
# End Source File
# Begin Source File

SOURCE=.\d_protocol.h
# End Source File
# Begin Source File

SOURCE=.\d_textur.h
# End Source File
# Begin Source File

SOURCE=.\d_ticcmd.h
# End Source File
# Begin Source File

SOURCE=.\decallib.h
# End Source File
# Begin Source File

SOURCE=.\dobject.h
# End Source File
# Begin Source File

SOURCE=.\doomdata.h
# End Source File
# Begin Source File

SOURCE=.\doomdef.h
# End Source File
# Begin Source File

SOURCE=doomerrors.h
# End Source File
# Begin Source File

SOURCE=.\doomstat.h
# End Source File
# Begin Source File

SOURCE=.\doomtype.h
# End Source File
# Begin Source File

SOURCE=.\dsectoreffect.h
# End Source File
# Begin Source File

SOURCE=.\dthinker.h
# End Source File
# Begin Source File

SOURCE=.\f_finale.h
# End Source File
# Begin Source File

SOURCE=.\f_wipe.h
# End Source File
# Begin Source File

SOURCE=.\farchive.h
# End Source File
# Begin Source File

SOURCE=.\files.h
# End Source File
# Begin Source File

SOURCE=.\g_game.h
# End Source File
# Begin Source File

SOURCE=.\g_level.h
# End Source File
# Begin Source File

SOURCE=.\gameconfigfile.h
# End Source File
# Begin Source File

SOURCE=.\gccinlines.h
# End Source File
# Begin Source File

SOURCE=.\gi.h
# End Source File
# Begin Source File

SOURCE=.\gstrings.h
# End Source File
# Begin Source File

SOURCE=.\hu_stuff.h
# End Source File
# Begin Source File

SOURCE=.\i_cd.h
# End Source File
# Begin Source File

SOURCE=.\i_movie.h
# End Source File
# Begin Source File

SOURCE=.\Info.h
# End Source File
# Begin Source File

SOURCE=.\infomacros.h
# End Source File
# Begin Source File

SOURCE=.\lists.h
# End Source File
# Begin Source File

SOURCE=.\lumpconfigfile.h
# End Source File
# Begin Source File

SOURCE=.\m_alloc.h
# End Source File
# Begin Source File

SOURCE=.\m_argv.h
# End Source File
# Begin Source File

SOURCE=.\m_bbox.h
# End Source File
# Begin Source File

SOURCE=.\m_cheat.h
# End Source File
# Begin Source File

SOURCE=.\m_crc32.h
# End Source File
# Begin Source File

SOURCE=.\m_fixed.h
# End Source File
# Begin Source File

SOURCE=.\m_menu.h
# End Source File
# Begin Source File

SOURCE=.\m_misc.h
# End Source File
# Begin Source File

SOURCE=.\m_png.h
# End Source File
# Begin Source File

SOURCE=.\m_random.h
# End Source File
# Begin Source File

SOURCE=.\m_swap.h
# End Source File
# Begin Source File

SOURCE=.\mscinlines.h
# End Source File
# Begin Source File

SOURCE=.\mus2midi.h
# End Source File
# Begin Source File

SOURCE=nodebuild.h
# End Source File
# Begin Source File

SOURCE=.\p_acs.h
# End Source File
# Begin Source File

SOURCE=.\p_effect.h
# End Source File
# Begin Source File

SOURCE=.\p_enemy.h
# End Source File
# Begin Source File

SOURCE=.\p_inter.h
# End Source File
# Begin Source File

SOURCE=.\p_lnspec.h
# End Source File
# Begin Source File

SOURCE=.\p_local.h
# End Source File
# Begin Source File

SOURCE=.\p_pspr.h
# End Source File
# Begin Source File

SOURCE=.\p_saveg.h
# End Source File
# Begin Source File

SOURCE=.\p_setup.h
# End Source File
# Begin Source File

SOURCE=.\p_spec.h
# End Source File
# Begin Source File

SOURCE=.\p_terrain.h
# End Source File
# Begin Source File

SOURCE=.\p_tick.h
# End Source File
# Begin Source File

SOURCE=.\p_trace.h
# End Source File
# Begin Source File

SOURCE=.\s_playlist.h
# End Source File
# Begin Source File

SOURCE=.\s_sndseq.h
# End Source File
# Begin Source File

SOURCE=.\s_sound.h
# End Source File
# Begin Source File

SOURCE=.\sc_man.h
# End Source File
# Begin Source File

SOURCE=.\skins.h
# End Source File
# Begin Source File

SOURCE=.\st_stuff.h
# End Source File
# Begin Source File

SOURCE=.\statnums.h
# End Source File
# Begin Source File

SOURCE=.\stats.h
# End Source File
# Begin Source File

SOURCE=.\stringenums.h
# End Source File
# Begin Source File

SOURCE=.\stringtable.h
# End Source File
# Begin Source File

SOURCE=.\Tables.h
# End Source File
# Begin Source File

SOURCE=.\tarray.h
# End Source File
# Begin Source File

SOURCE=.\tempfiles.h
# End Source File
# Begin Source File

SOURCE=.\templates.h
# End Source File
# Begin Source File

SOURCE=.\v_collection.h
# End Source File
# Begin Source File

SOURCE=.\v_font.h
# End Source File
# Begin Source File

SOURCE=.\v_palette.h
# End Source File
# Begin Source File

SOURCE=.\v_pfx.h
# End Source File
# Begin Source File

SOURCE=.\v_text.h
# End Source File
# Begin Source File

SOURCE=.\v_video.h
# End Source File
# Begin Source File

SOURCE=.\vectors.h
# End Source File
# Begin Source File

SOURCE=.\w_wad.h
# End Source File
# Begin Source File

SOURCE=.\weightedlist.h
# End Source File
# Begin Source File

SOURCE=.\wi_stuff.h
# End Source File
# End Group
# Begin Group "Assembly Files"

# PROP Default_Filter "nas; asm"
# Begin Source File

SOURCE=.\a.nas

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# PROP Ignore_Default_Tool 1
# Begin Custom Build - Assembling $(InputPath)...
SOURCE="$(InputPath)"

BuildCmds= \
	nasmw -o $(IntDir)\$(InputName).obj -f win32 $(InputPath) \


"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# PROP Ignore_Default_Tool 1
# Begin Custom Build - Assembling $(InputPath)...
SOURCE="$(InputPath)"

BuildCmds= \
	nasmw -o $(IntDir)\$(InputName).obj -f win32 $(InputPath) \


"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\misc.nas

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# PROP Ignore_Default_Tool 1
# Begin Custom Build - Assembling $(InputPath)...
SOURCE="$(InputPath)"

BuildCmds= \
	nasmw -o $(IntDir)\$(InputName).obj -f win32 $(InputPath) \


"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# PROP Ignore_Default_Tool 1
# Begin Custom Build - Assembling $(InputPath)...
SOURCE="$(InputPath)"

BuildCmds= \
	nasmw -o $(IntDir)\$(InputName).obj -f win32 $(InputPath) \


"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\tmap.nas

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# PROP Ignore_Default_Tool 1
# Begin Custom Build - Assembling $(InputPath)...
SOURCE="$(InputPath)"

BuildCmds= \
	nasmw -o $(IntDir)\$(InputName).obj -f win32 $(InputPath) \


"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# PROP Ignore_Default_Tool 1
# Begin Custom Build - Assembling $(InputPath)...
SOURCE="$(InputPath)"

BuildCmds= \
	nasmw -o $(IntDir)\$(InputName).obj -f win32 $(InputPath) \


"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\tmap2.nas

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# PROP Ignore_Default_Tool 1
# Begin Custom Build - Assembling $(InputPath)...
SOURCE="$(InputPath)"

BuildCmds= \
	nasmw -o $(IntDir)\$(InputName).obj -f win32 $(InputPath) \


"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# PROP Ignore_Default_Tool 1
# Begin Custom Build - Assembling $(InputPath)...
SOURCE="$(InputPath)"

BuildCmds= \
	nasmw -o $(IntDir)\$(InputName).obj -f win32 $(InputPath) \


"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\tmap3.nas

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# PROP Ignore_Default_Tool 1
# Begin Custom Build - Assembling $(InputPath)...
SOURCE="$(InputPath)"

BuildCmds= \
	nasmw -o $(IntDir)\$(InputName).obj -f win32 $(InputPath) \


"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# PROP Ignore_Default_Tool 1
# Begin Custom Build - Assembling $(InputPath)...
SOURCE="$(InputPath)"

BuildCmds= \
	nasmw -o $(IntDir)\$(InputName).obj -f win32 $(InputPath) \


"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ENDIF

# End Source File
# End Group
# Begin Group "Win32 Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\win32\cursor1.cur
# End Source File
# Begin Source File

SOURCE=win32\eaxedit.cpp
# End Source File
# Begin Source File

SOURCE=.\win32\hardware.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\win32\hardware.h
# End Source File
# Begin Source File

SOURCE=.\win32\helperthread.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\win32\helperthread.h
# End Source File
# Begin Source File

SOURCE=win32\i_altsound.cpp
# End Source File
# Begin Source File

SOURCE=.\win32\i_cd.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=win32\i_crash.cpp
# End Source File
# Begin Source File

SOURCE=.\win32\i_input.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\win32\i_input.h
# End Source File
# Begin Source File

SOURCE=.\win32\i_main.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\win32\i_movie.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\win32\i_net.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\win32\i_net.h
# End Source File
# Begin Source File

SOURCE=.\win32\I_system.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\win32\I_system.h
# End Source File
# Begin Source File

SOURCE=.\win32\icon1.ico
# End Source File
# Begin Source File

SOURCE=.\win32\resource.h
# End Source File
# Begin Source File

SOURCE=.\win32\win32iface.h
# End Source File
# Begin Source File

SOURCE=.\win32\win32video.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=win32\zdoom.exe.manifest
# End Source File
# End Group
# Begin Group "Shared Game"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\g_shared\a_action.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_shared\a_action.h
# End Source File
# Begin Source File

SOURCE=.\g_shared\a_artifacts.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_shared\a_artifacts.h
# End Source File
# Begin Source File

SOURCE=.\g_shared\a_bridge.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_shared\a_camera.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_shared\a_debris.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_shared\a_decals.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_shared\a_flashfader.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_shared\a_fountain.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=g_shared\a_hatetarget.cpp
# End Source File
# Begin Source File

SOURCE=.\g_shared\a_lightning.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_shared\a_lightning.h
# End Source File
# Begin Source File

SOURCE=.\g_shared\a_movingcamera.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_shared\a_pickups.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_shared\a_pickups.h
# End Source File
# Begin Source File

SOURCE=.\g_shared\a_quake.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_shared\a_secrettrigger.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_shared\a_sectoraction.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_shared\a_sharedglobal.h
# End Source File
# Begin Source File

SOURCE=.\g_shared\a_sharedmisc.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_shared\a_skies.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=g_shared\a_soundenvironment.cpp
# End Source File
# Begin Source File

SOURCE=.\g_shared\a_spark.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_shared\a_splashes.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_shared\a_waterzone.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_shared\a_weapons.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_shared\hudmessages.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_shared\sbar.h
# End Source File
# Begin Source File

SOURCE=.\g_shared\shared_sbar.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# End Group
# Begin Group "Doom Game"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\g_doom\a_arachnotron.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_doom\a_archvile.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_doom\a_bossbrain.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_doom\a_bruiser.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_doom\a_cacodemon.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_doom\a_cyberdemon.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_doom\a_demon.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_doom\a_doomarmor.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_doom\a_doomartifacts.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_doom\a_doomdecorations.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_doom\a_doomglobal.h
# End Source File
# Begin Source File

SOURCE=.\g_doom\a_doomhealth.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_doom\a_doomimp.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_doom\a_doomkeys.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_doom\a_doommisc.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_doom\a_doomplayer.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_doom\a_doomweaps.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_doom\a_fatso.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_doom\a_keen.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_doom\a_lostsoul.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_doom\a_painelemental.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_doom\a_possessed.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_doom\a_revenant.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_doom\a_scriptedmarine.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_doom\a_spidermaster.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_doom\doom_sbar.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# End Group
# Begin Group "Raven Shared"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\g_raven\a_artiegg.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_raven\a_artitele.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_raven\a_minotaur.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_raven\a_ravenambient.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_raven\a_ravenartifacts.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_raven\a_ravenhealth.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_raven\ravenshared.h
# End Source File
# End Group
# Begin Group "Heretic Game"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\g_heretic\a_beast.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_heretic\a_chicken.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_heretic\a_clink.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_heretic\a_dsparil.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_heretic\a_hereticambience.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_heretic\a_hereticarmor.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_heretic\a_hereticartifacts.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_heretic\a_hereticdecorations.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_heretic\a_hereticglobal.h
# End Source File
# Begin Source File

SOURCE=.\g_heretic\a_hereticimp.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_heretic\a_heretickeys.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_heretic\a_hereticmisc.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_heretic\a_hereticplayer.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_heretic\a_hereticweaps.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_heretic\a_ironlich.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_heretic\a_knight.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_heretic\a_mummy.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_heretic\a_snake.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_heretic\a_wizard.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_heretic\heretic_sbar.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\r_main.h
# End Source File
# End Group
# Begin Group "Hexen Game"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\g_hexen\a_bats.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_bishop.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_blastradius.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_boostarmor.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_boostmana.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_centaur.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_clericboss.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_clericflame.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_clericholy.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_clericmace.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=g_hexen\a_clericplayer.cpp
# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_clericstaff.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_demons.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_dragon.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_ettin.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_fighteraxe.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_fighterboss.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_fighterhammer.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=g_hexen\a_fighterplayer.cpp
# End Source File
# Begin Source File

SOURCE=g_hexen\a_fighterquietus.cpp
# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_firedemon.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_flame.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_flechette.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_fog.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=g_hexen\a_healingradius.cpp
# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_heresiarch.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_hexenarmor.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_hexendecorations.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_hexenglobal.h
# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_hexenkeys.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_hexenspecialdecs.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_iceguy.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_korax.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_mageboss.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_magecone.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_magelightning.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=g_hexen\a_mageplayer.cpp
# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_magestaff.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_magewand.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_mana.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=g_hexen\a_pig.cpp
# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_puzzleitems.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_scriptprojectiles.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_serpent.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_speedboots.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_spike.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_summon.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_teleportother.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=g_hexen\a_weaponpieces.cpp
# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_wraith.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=g_hexen\hexen_sbar.cpp
# End Source File
# End Group
# Begin Group "Strife Game"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\g_strife\a_acolyte.cpp
# End Source File
# Begin Source File

SOURCE=.\g_strife\a_beggars.cpp
# End Source File
# Begin Source File

SOURCE=.\g_strife\a_merchants.cpp
# End Source File
# Begin Source File

SOURCE=.\g_strife\a_peasant.cpp
# End Source File
# Begin Source File

SOURCE=.\g_strife\a_rebels.cpp
# End Source File
# Begin Source File

SOURCE=.\g_strife\a_strifeammo.cpp
# End Source File
# Begin Source File

SOURCE=.\g_strife\a_strifeglobal.h
# End Source File
# Begin Source File

SOURCE=.\g_strife\a_strifeplayer.cpp
# End Source File
# Begin Source File

SOURCE=.\g_strife\a_strifestuff.cpp
# End Source File
# Begin Source File

SOURCE=.\g_strife\a_strifeweapons.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\g_strife\a_zombie.cpp
# End Source File
# Begin Source File

SOURCE=.\g_strife\strife_sbar.cpp
# End Source File
# End Group
# Begin Group "Render Core"

# PROP Default_Filter ""
# Begin Group "Render Sources"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\r_bsp.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\r_data.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\r_draw.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\r_drawt.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\r_main.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\r_plane.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\r_segs.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\r_sky.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=.\r_things.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD CPP /nologo /GZ "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc"  "  /I /fmod/api/inc" /GX 
!ENDIF

# End Source File
# End Group
# Begin Group "Render Headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\r_bsp.h
# End Source File
# Begin Source File

SOURCE=.\r_data.h
# End Source File
# Begin Source File

SOURCE=.\r_defs.h
# End Source File
# Begin Source File

SOURCE=.\r_draw.h
# End Source File
# Begin Source File

SOURCE=.\r_local.h
# End Source File
# Begin Source File

SOURCE=.\r_plane.h
# End Source File
# Begin Source File

SOURCE=.\r_segs.h
# End Source File
# Begin Source File

SOURCE=.\r_sky.h
# End Source File
# Begin Source File

SOURCE=.\r_state.h
# End Source File
# Begin Source File

SOURCE=.\r_things.h
# End Source File
# End Group
# End Group
# Begin Group "Text Files"

# PROP Default_Filter "txt"
# Begin Source File

SOURCE=..\docs\classes.txt
# End Source File
# Begin Source File

SOURCE=..\docs\colors.txt
# End Source File
# Begin Source File

SOURCE=..\docs\commands.txt
# End Source File
# Begin Source File

SOURCE=..\docs\console.html
# End Source File
# Begin Source File

SOURCE=..\docs\history.txt
# End Source File
# Begin Source File

SOURCE=..\docs\notes.txt
# End Source File
# Begin Source File

SOURCE=..\docs\rh-log.txt
# End Source File
# Begin Source File

SOURCE=..\docs\zdoom.txt
# End Source File
# Begin Group "Licenses"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\docs\BUILDLIC.TXT
# End Source File
# Begin Source File

SOURCE=..\docs\doomlic.txt
# End Source File
# End Group
# End Group
# Begin Group "Versioning"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\version.h
# End Source File
# Begin Source File

SOURCE=.\win32\zdoom.rc

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD RSC /l 1033 /i "win32" 
!ELSEIF  "$(CFG)" == "_zdoom - Win32 Debug"

# ADD RSC /l 1033 /i "win32" 
!ENDIF

# End Source File
# End Group
# Begin Group "OPL Synth"

# PROP Default_Filter ""
# Begin Source File

SOURCE=oplsynth\DEFTYPES.H
# End Source File
# Begin Source File

SOURCE=oplsynth\fmopl.cpp

!IF  "$(CFG)" == "_zdoom - Win32 Release"

# ADD CPP /nologo /FAs /Fa"" /GX 
!ENDIF

# End Source File
# Begin Source File

SOURCE=oplsynth\fmopl.h
# End Source File
# Begin Source File

SOURCE=oplsynth\mlkernel.cpp
# End Source File
# Begin Source File

SOURCE=oplsynth\mlopl.cpp
# End Source File
# Begin Source File

SOURCE=oplsynth\mlopl_io.cpp
# End Source File
# Begin Source File

SOURCE=oplsynth\muslib.h
# End Source File
# Begin Source File

SOURCE=oplsynth\opl_mus_player.cpp
# End Source File
# Begin Source File

SOURCE=oplsynth\opl_mus_player.h
# End Source File
# End Group
# Begin Group "Audio Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=sound\i_music.cpp
# End Source File
# Begin Source File

SOURCE=sound\i_music.h
# End Source File
# Begin Source File

SOURCE=sound\i_musicinterns.h
# End Source File
# Begin Source File

SOURCE=sound\i_sound.cpp
# End Source File
# Begin Source File

SOURCE=sound\i_sound.h
# End Source File
# Begin Source File

SOURCE=sound\music_cd.cpp
# End Source File
# Begin Source File

SOURCE=sound\music_flac.cpp
# End Source File
# Begin Source File

SOURCE=sound\music_midi_midiout.cpp
# End Source File
# Begin Source File

SOURCE=sound\music_midi_stream.cpp
# End Source File
# Begin Source File

SOURCE=sound\music_midi_timidity.cpp
# End Source File
# Begin Source File

SOURCE=sound\music_mod.cpp
# End Source File
# Begin Source File

SOURCE=sound\music_mus_midiout.cpp
# End Source File
# Begin Source File

SOURCE=sound\music_mus_opl.cpp
# End Source File
# Begin Source File

SOURCE=sound\music_spc.cpp
# End Source File
# Begin Source File

SOURCE=sound\music_stream.cpp
# End Source File
# Begin Source File

SOURCE=sound\sample_flac.cpp
# End Source File
# Begin Source File

SOURCE=sound\sample_flac.h
# End Source File
# End Group
# Begin Source File

SOURCE=win32\icon2.ico
# End Source File
# End Target
# End Project

