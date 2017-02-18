/*
** menuinput.cpp
** The string input code
**
**---------------------------------------------------------------------------
** Copyright 2001-2010 Randy Heit
** Copyright 2010 Christoph Oelckers
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
*/

#include "menu/menu.h"
#include "v_video.h"
#include "c_cvars.h"
#include "d_event.h"
#include "d_gui.h"
#include "v_font.h"
#include "v_palette.h"
#include "cmdlib.h"
// [TP] New #includes
#include "v_text.h"

IMPLEMENT_CLASS(DTextEnterMenu, false, false)

CVAR(Bool, m_showinputgrid, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

DEFINE_FIELD(DTextEnterMenu, mEnterString);
DEFINE_FIELD(DTextEnterMenu, mEnterSize);
DEFINE_FIELD(DTextEnterMenu, mEnterPos);
DEFINE_FIELD(DTextEnterMenu, mSizeMode); // 1: size is length in chars. 2: also check string width
DEFINE_FIELD(DTextEnterMenu, mInputGridOkay);
DEFINE_FIELD(DTextEnterMenu, InputGridX);
DEFINE_FIELD(DTextEnterMenu, InputGridY);
DEFINE_FIELD(DTextEnterMenu, AllowColors);
