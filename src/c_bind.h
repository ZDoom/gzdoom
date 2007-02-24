/*
** c_bind.h
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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

#ifndef __C_BINDINGS_H__
#define __C_BINDINGS_H__

#include "doomtype.h"
#include "d_event.h"
#include <stdio.h>

class FConfigFile;

bool C_DoKey (event_t *ev);
void C_ArchiveBindings (FConfigFile *f, bool dodouble, const char *matchcmd=NULL);

// Stuff used by the customize controls menu
int  C_GetKeysForCommand (char *cmd, int *first, int *second);
void C_NameKeys (char *str, int first, int second);
void C_UnbindACommand (char *str);
void C_ChangeBinding (const char *str, int newone);
void C_DoBind (const char *key, const char *bind, bool doublebind);
void C_SetDefaultBindings ();
void C_UnbindAll ();

// Returns string bound to given key (NULL if none)
const char *C_GetBinding (int key);

extern const char *KeyNames[];

#endif //__C_BINDINGS_H__
