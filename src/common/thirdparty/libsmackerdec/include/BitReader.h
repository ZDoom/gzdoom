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

#ifndef _SmackerBitReader_h_
#define _SmackerBitReader_h_

#include <stdint.h>
#include "FileStream.h"
#include "tarray.h"

namespace SmackerCommon {

class BitReader
{
	public:
		BitReader(const uint8_t *data, uint32_t size) : bitData(data), totalSize(size) { }
		~BitReader() = default;
		uint32_t GetBit();
		uint32_t GetBits(uint32_t n);
		void SkipBits(uint32_t n);

		uint32_t GetSize();
		uint32_t GetPosition();

	private:
		const uint8_t *bitData = nullptr;
		uint32_t totalSize = 0;
		uint32_t currentOffset = 0;
		uint32_t bytesRead = 0;
};

} // close namespace SmackerCommon

#endif
