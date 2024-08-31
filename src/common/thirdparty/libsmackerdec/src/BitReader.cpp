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

#include "BitReader.h"
#include <assert.h>

namespace SmackerCommon {

uint32_t BitReader::GetSize()
{
	return totalSize * 8;
}

uint32_t BitReader::GetPosition()
{
	return currentOffset;
}

uint32_t BitReader::GetBit()
{
	uint32_t ret = (bitData[currentOffset>>3]>>(currentOffset&7))&1;
    currentOffset++;
	return ret;
}

uint32_t BitReader::GetBits(uint32_t n)
{
	uint32_t ret = 0;

	int bitsTodo = n;

	uint32_t theShift = 0;

	while (bitsTodo)
	{
		uint32_t bit = GetBit();
		bit <<= theShift;

		theShift++;

		ret |= bit;

		bitsTodo--;
	}

	return ret;
}

void BitReader::SkipBits(uint32_t n)
{
	GetBits(n);
}

} // close namespace SmackerCommon

