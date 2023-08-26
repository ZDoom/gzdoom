@echo off
goto aftercopyright

**
** auto-setup-windows.cmd
** Automatic (easy) setup and build script for Windows
**
** Note that this script assumes you have both 'git' and 'cmake' installed properly and in your PATH!
** This script also assumes you have installed a build system that cmake can automatically detect.
** Such as Visual Studio Community. Requires appropriate SDK installed too!
** Without these items, this script will FAIL! So make sure you have your build environment properly
** set up in order for this script to succeed.
**
** The purpose of this script is to get someone easily going with a full working compile of GZDoom.
** This allows anyone to make simple changes or tweaks to the engine as they see fit and easily
** compile their own copy without having to follow complex instructions to get it working.
** Every build environment is different, and every computer system is different - this should work
** in most typical systems under Windows but it may fail under certain types of systems or conditions.
** Not guaranteed to work and your mileage will vary.
**
**---------------------------------------------------------------------------
** Copyright 2023 Rachael Alexanderson and the GZDoom team
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**

:aftercopyright


setlocal
rem -- Always operate within the build folder
if not exist "%~dp0\build" mkdir "%~dp0\build"
pushd "%~dp0\build"

if exist vcpkg if exist vcpkg\* git -C ./vcpkg pull
if not exist vcpkg git clone https://github.com/microsoft/vcpkg

if exist zmusic if exist vcpkg\* git -C ./zmusic pull
if not exist zmusic git clone https://github.com/zdoom/zmusic

mkdir "%~dp0\build\zmusic\build"
mkdir "%~dp0\build\vcpkg_installed"

cmake -A x64 -S ./zmusic -B ./zmusic/build ^
	-DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake ^
	-DVCPKG_LIBSNDFILE=1 ^
	-DVCPKG_INSTALLLED_DIR=../vcpkg_installed/
cmake --build ./zmusic/build --config Release -- -maxcpucount -verbosity:minimal

cmake -A x64 -S .. -B . ^
	-DCMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake ^
	-DZMUSIC_INCLUDE_DIR=./zmusic/include ^
	-DZMUSIC_LIBRARIES=./zmusic/build/source/Release/zmusic.lib ^
	-DVCPKG_INSTALLLED_DIR=./vcpkg_installed/
cmake --build . --config RelWithDebInfo -- -maxcpucount -verbosity:minimal

rem -- If successful, show the build
if exist RelWithDebInfo\gzdoom.exe explorer.exe RelWithDebInfo

