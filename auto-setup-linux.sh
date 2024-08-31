#!/bin/bash
#**
#** auto-setup-linux.sh
#** Automatic (easy) setup and build script for Linux
#**
#** Note that this script assumes you have both 'git' and 'cmake' installed properly and in your PATH!
#** This script also assumes you have installed a build system that cmake can automatically detect.
#** Such as GCC or Clang. Requires appropriate supporting libraries installed too!
#** Without these items, this script will FAIL! So make sure you have your build environment properly
#** set up in order for this script to succeed.
#**
#** The purpose of this script is to get someone easily going with a full working compile of GZDoom.
#** This allows anyone to make simple changes or tweaks to the engine as they see fit and easily
#** compile their own copy without having to follow complex instructions to get it working.
#** Every build environment is different, and every computer system is different - this should work
#** in most typical systems under Windows but it may fail under certain types of systems or conditions.
#** Not guaranteed to work and your mileage will vary.
#**
#** Prerequisite Packages used in testing (from Linux Mint-XFCE):
#**    nasm autoconf libtool libsystemd-dev clang-15 libx11-dev libsdl2-dev libgtk-3-dev
#**
#**---------------------------------------------------------------------------
#** Copyright 2024 Rachael Alexanderson and the GZDoom team
#** All rights reserved.
#**
#** Redistribution and use in source and binary forms, with or without
#** modification, are permitted provided that the following conditions
#** are met:
#**
#** 1. Redistributions of source code must retain the above copyright
#**    notice, this list of conditions and the following disclaimer.
#** 2. Redistributions in binary form must reproduce the above copyright
#**    notice, this list of conditions and the following disclaimer in the
#**    documentation and/or other materials provided with the distribution.
#** 3. The name of the author may not be used to endorse or promote products
#**    derived from this software without specific prior written permission.
#**
#** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
#** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
#** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
#** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
#** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
#** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
#** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
#** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#**---------------------------------------------------------------------------
#**

# -- Always operate within the build folder
BUILDFOLDER=$(dirname "$0")/build

if [ ! -d "$BUILDFOLDER" ]; then
	mkdir "$BUILDFOLDER"
fi
cd "$BUILDFOLDER"

if [ -d "vcpkg" ]; then
	git -C ./vcpkg pull
fi
if [ ! -d "vcpkg" ]; then
	git clone https://github.com/microsoft/vcpkg
fi
if [ -d "zmusic" ]; then
	git -C ./zmusic fetch
fi
if [ ! -d "zmusic" ]; then
	git clone https://github.com/zdoom/zmusic
fi
if [ -d "zmusic" ]; then
	git -C ./zmusic checkout 1.1.12
fi

if [ ! -d "zmusic/build" ]; then
	mkdir zmusic/build
fi
if [ ! -d "vcpkg_installed" ]; then
	mkdir vcpkg_installed
fi

cmake -S ./zmusic -B ./zmusic/build \
	-DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake \
	-DVCPKG_LIBSNDFILE=1 \
	-DCMAKE_BUILD_TYPE=RelWithDebInfo \
	-DVCPKG_INSTALLLED_DIR=../vcpkg_installed/
pushd ./zmusic/build
make -j $(nproc)
popd

cmake -S .. -B . \
	-DCMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake \
	-DCMAKE_BUILD_TYPE=RelWithDebInfo \
	-DVCPKG_INSTALLLED_DIR=./vcpkg_installed/
make -j $(nproc); rc=$?

# -- If successful, show the build
if [ $rc -eq 0 ]; then
	if [ -f gzdoom ]; then
		xdg-open .
	fi
fi

exit $rc
