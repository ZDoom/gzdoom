@echo off
goto aftercopyright

**
** auto-setup-windows-arm.cmd
** Automatic (easy) setup and build script for Windows ARM (cross compiling on x64) - please have a
** build of x64 in your build folder ready before executing this script!
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
** Copyright 2024 Rachael Alexanderson and the GZDoom team
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
rem -- Always operate within the build-arm folder
if not exist "%~dp0\build-arm" mkdir "%~dp0\build-arm"
pushd "%~dp0\build-arm"

if exist ..\build\vcpkg if exist ..\build\vcpkg\* git -C ../build/vcpkg pull
if not exist ..\build\vcpkg goto :nobuild

if exist zmusic if exist zmusic\* git -C ./zmusic pull
if not exist zmusic git clone https://github.com/zdoom/zmusic

mkdir "%~dp0\build-arm\zmusic\build"
mkdir "%~dp0\build-arm\vcpkg_installed"

cmake -A ARM64 -S ./zmusic -B ./zmusic/build ^
	-DCMAKE_TOOLCHAIN_FILE=../../build/vcpkg/scripts/buildsystems/vcpkg.cmake ^
	-DVCPKG_LIBSNDFILE=1 ^
	-DVCPKG_INSTALLLED_DIR=../vcpkg_installed/ ^
	-DCMAKE_CROSSCOMPILING=TRUE
cmake --build ./zmusic/build --config Release -- -maxcpucount -verbosity:minimal

cmake -A ARM64 -S .. -B . ^
	-DCMAKE_TOOLCHAIN_FILE=../build/vcpkg/scripts/buildsystems/vcpkg.cmake ^
	-DZMUSIC_INCLUDE_DIR=./zmusic/include ^
	-DZMUSIC_LIBRARIES=./zmusic/build/source/Release/zmusic.lib ^
	-DVCPKG_INSTALLLED_DIR=./vcpkg_installed/ ^
	-DFORCE_CROSSCOMPILE=TRUE ^
	-DIMPORT_EXECUTABLES=../build/ImportExecutables.cmake
cmake --build . --config RelWithDebInfo -- -maxcpucount -verbosity:minimal

rem -- If successful, show the build
if exist RelWithDebInfo\gzdoom.exe explorer.exe RelWithDebInfo

goto :eof
:nobuild
echo Please use auto-setup-windows.cmd to create an x64 build first!
