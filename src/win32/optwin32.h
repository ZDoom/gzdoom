#pragma once

// Forward declarations for optional Win32 API procedures
// implemented in i_main.cpp

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>

#include "i_module.h"

extern FModule Shell32Module;

namespace OptWin32 {

extern TOptProc<Shell32Module, HRESULT(WINAPI*)(REFKNOWNFOLDERID, DWORD, HANDLE, PWSTR *)> SHGetKnownFolderPath;

} // namespace OptWin32
