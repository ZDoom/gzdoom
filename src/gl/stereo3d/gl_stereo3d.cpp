// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2015 Christopher Bruns
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//
/*
** gl_stereo3d.cpp
** Stereoscopic 3D API
**
*/

#include "gl_load/gl_system.h"
#include "gl/stereo3d/gl_stereo3d.h"

namespace s3d {


Stereo3DMode::Stereo3DMode()
{
}

Stereo3DMode::~Stereo3DMode()
{
}

// Avoid static initialization order fiasco by declaring first Mode type (Mono) here in the
// same source file as Stereo3DMode::getCurrentMode()
// https://isocpp.org/wiki/faq/ctors#static-init-order

/* static */
const MonoView& MonoView::getInstance() 
{
	static MonoView instance;
	return instance;
}

} /* namespace s3d */
