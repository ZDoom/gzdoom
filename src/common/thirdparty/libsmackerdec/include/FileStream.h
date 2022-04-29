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

#ifndef _SmackerFileStream_h_
#define _SmackerFileStream_h_

#include <stdint.h>
#include "files.h"

namespace SmackerCommon {

class FileStream
{
	public:

		bool Open(const char *fileName);
		bool Is_Open() { return file.isOpen(); }
		void Close() { file.Close(); }

		int32_t ReadBytes(uint8_t *data, uint32_t nBytes)
		{
			return (uint32_t)file.Read(data, static_cast<int32_t>(nBytes));
		}

		uint64_t ReadUint64LE() { return file.ReadUInt64(); }
		uint32_t ReadUint32LE() { return file.ReadUInt32(); }
		uint16_t ReadUint16LE() { return file.ReadUInt16(); }
		uint8_t ReadByte() { return file.ReadInt8(); }


		enum SeekDirection{
			kSeekCurrent = SEEK_CUR,
			kSeekStart   = SEEK_SET,
			kSeekEnd     = SEEK_END
		};

        int32_t Seek(int32_t offset, SeekDirection dir = kSeekStart) { return (int)file.Seek(offset, (FileReader::ESeek) dir); }
        int32_t Skip(int32_t offset) { return (int)Seek(offset, kSeekCurrent); }
		int32_t GetPosition() { return (int)file.Tell(); }

	private:
		FileReader file;
};

} // close namespace SmackerCommon

#endif
