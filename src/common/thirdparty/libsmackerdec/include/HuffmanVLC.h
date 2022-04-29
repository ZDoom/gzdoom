/*
 * libsmackerdec - Smacker video decoder
 * Copyright (C) 2011 Barry Duncan
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _SmackerHuffmanVLC_h_
#define _SmackerHuffmanVLC_h_

#include <stdint.h>
#include "BitReader.h"
#include <vector>

namespace SmackerCommon {

struct VLC
{
    uint32_t symbol;
    uint32_t code;
};

typedef std::vector< std::vector<VLC> > VLCtable;

uint16_t VLC_GetCodeBits(BitReader &bits, VLCtable &table);
void     VLC_InitTable  (VLCtable &table, uint32_t maxLength, uint32_t size, int *lengths, uint32_t *bits);
uint32_t VLC_GetSize    (VLCtable &table);

} // close namespace SmackerCommon

#endif
