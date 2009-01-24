// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id: i_net.h,v 1.1.1.1 1997/12/28 12:59:02 pekangas Exp $
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// DESCRIPTION:
//		System specific network interface stuff.
//
//-----------------------------------------------------------------------------


#ifndef __I_NET_H__
#define __I_NET_H__

// Called by D_DoomMain.
bool I_InitNetwork (void);
void I_NetCmd (void);

#endif
