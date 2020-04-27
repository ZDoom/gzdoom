//-----------------------------------------------------------------------------
//
// Copyright 2018 Benjamin Berkels
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//

#include "d_player.h"
#include "netcommand.h"
#include "netnode.h"
#include "i_net.h"

extern bool netserver, netclient;

// [BB] Are we measuring outbound traffic?
static	bool	g_MeasuringOutboundTraffic = false;
// [BB] Number of bytes sent by NETWORK_Write* since NETWORK_StartTrafficMeasurement() was called.
static	int		g_OutboundBytesMeasured = 0;

//*****************************************************************************

ByteOutputStream::ByteOutputStream(int size)
{
	SetBuffer(size);
}

ByteOutputStream::ByteOutputStream(void *buffer, int size)
{
	SetBuffer(buffer, size);
}

void ByteOutputStream::SetBuffer(int size)
{
	mBuffer = std::make_shared<DataBuffer>(size);
	SetBuffer(mBuffer->data, mBuffer->size);
}

void ByteOutputStream::SetBuffer(void *buffer, int size)
{
	mData = (uint8_t*)buffer;
	pbStreamEnd = mData + size;
	ResetPos();
}

void ByteOutputStream::ResetPos()
{
	pbStream = mData;
	bitBuffer = nullptr;
	bitShift = -1;
}

void ByteOutputStream::AdvancePointer(const int NumBytes, const bool OutboundTraffic)
{
	pbStream += NumBytes;

	if (g_MeasuringOutboundTraffic && OutboundTraffic)
		g_OutboundBytesMeasured += NumBytes;
}

void ByteOutputStream::WriteByte(int Byte)
{
	if ((pbStream + 1) > pbStreamEnd)
	{
		Printf("ByteOutputStream::WriteByte: Overflow!\n");
		return;
	}

	*pbStream = Byte;

	// Advance the pointer.
	AdvancePointer(1, true);
}

void ByteOutputStream::WriteShort(int Short)
{
	if ((pbStream + 2) > pbStreamEnd)
	{
		Printf("NETWORK_WriteShort: Overflow!\n");
		return;
	}

	pbStream[0] = Short & 0xff;
	pbStream[1] = Short >> 8;

	// Advance the pointer.
	AdvancePointer(2, true);
}

void ByteOutputStream::WriteLong(int Long)
{
	if ((pbStream + 4) > pbStreamEnd)
	{
		Printf("NETWORK_WriteLong: Overflow!\n");
		return;
	}

	pbStream[0] = Long & 0xff;
	pbStream[1] = (Long >> 8) & 0xff;
	pbStream[2] = (Long >> 16) & 0xff;
	pbStream[3] = (Long >> 24);

	// Advance the pointer.
	AdvancePointer(4, true);
}

void ByteOutputStream::WriteFloat(float Float)
{
	union
	{
		float	f;
		int		l;
	} dat;

	dat.f = Float;

	WriteLong(dat.l);
}

void ByteOutputStream::WriteString(const char *pszString)
{
	if ((pszString) && (strlen(pszString) > MAX_NETWORK_STRING))
	{
		Printf("ByteOutputStream::WriteString: String exceeds %d characters!\n", MAX_NETWORK_STRING);
		return;
	}

	if (pszString)
		WriteBuffer("", 1);
	else
		WriteBuffer(pszString, (int)(strlen(pszString)) + 1);
}

void ByteOutputStream::WriteBuffer(const void *pvBuffer, int nLength)
{
	if ((pbStream + nLength) > pbStreamEnd)
	{
		Printf("NETWORK_WriteLBuffer: Overflow!\n");
		return;
	}

	memcpy(pbStream, pvBuffer, nLength);

	// Advance the pointer.
	AdvancePointer(nLength, true);
}

void ByteOutputStream::WriteBit(bool bit)
{
	// Add a bit to this byte
	EnsureBitSpace(1, true);
	if (bit)
		*bitBuffer |= 1 << bitShift;
	++bitShift;
}

void ByteOutputStream::WriteVariable(int value)
{
	int length;

	// Determine how long we need to send this value
	if (value == 0)
		length = 0; // 0 - don't bother sending it at all
	else if ((value <= 0xFF) && (value >= 0))
		length = 1; // Can be sent as a byte
	else if ((value <= 0x7FFF) && (value >= -0x8000))
		length = 2; // Can be sent as a short
	else
		length = 3; // Must be sent as a long

	// Write this length as two bits
	WriteBit(!!(length & 1));
	WriteBit(!!(length & 2));

	// Depending on the required length, write the value.
	switch (length)
	{
	case 1: WriteByte(value); break;
	case 2: WriteShort(value); break;
	case 3: WriteLong(value); break;
	}
}

void ByteOutputStream::WriteShortByte(int value, int bits)
{
	if ((bits < 1) || (bits > 8))
	{
		Printf("NETWORK_WriteShortByte: bits must be within range [1..8], got %d.\n", bits);
		return;
	}

	EnsureBitSpace(bits, true);
	value &= ((1 << bits) - 1); // Form a mask from the bits and trim our value using it.
	value <<= bitShift; // Shift the value to its proper position.
	*bitBuffer |= value; // Add it to the byte.
	bitShift += bits; // Bump the shift value accordingly.
}

void ByteOutputStream::EnsureBitSpace(int bits, bool writing)
{
	if ((bitBuffer == nullptr) || (bitShift < 0) || (bitShift + bits > 8))
	{
		// Not enough bits left in our current byte, we need a new one.
		WriteByte(0);
		bitBuffer = pbStream - 1;

		bitShift = 0;
	}
}

//*****************************************************************************

ByteInputStream::ByteInputStream(const void *buffer, int size)
{
	SetBuffer(buffer, size);
}

void ByteInputStream::SetBuffer(const void *buffer, int size)
{
	mData = (uint8_t*)buffer;
	pbStream = mData;
	pbStreamEnd = pbStream + size;
	bitBuffer = nullptr;
	bitShift = -1;
}

int ByteInputStream::ReadByte()
{
	int	Byte = -1;

	if ((pbStream + 1) <= pbStreamEnd)
		Byte = *pbStream;

	// Advance the pointer.
	pbStream += 1;

	return (Byte);
}

int ByteInputStream::ReadShort()
{
	short Short = -1;

	if ((pbStream + 2) <= pbStreamEnd)
		Short = static_cast<short>(pbStream[0]) | (static_cast<short>(pbStream[1]) << 8);

	// Advance the pointer.
	pbStream += 2;

	return (Short);
}

int ByteInputStream::ReadLong()
{
	int	Long = -1;

	if ((pbStream + 4) <= pbStreamEnd)
	{
		Long = static_cast<int>(pbStream[0]) | (static_cast<int>(pbStream[1]) << 8) | (static_cast<int>(pbStream[2]) << 16) | (static_cast<int>(pbStream[3]) << 24);
	}

	// Advance the pointer.
	pbStream += 4;

	return (Long);
}

float ByteInputStream::ReadFloat()
{
	union
	{
		float	f;
		int		i;
	} dat;

	dat.i = ReadLong();
	return (dat.f);
}

const char *ByteInputStream::ReadString()
{
	int c;
	static char		s_szString[MAX_NETWORK_STRING];

	// Read in characters until we've reached the end of the string.
	unsigned int ulIdx = 0;
	do
	{
		c = ReadByte();
		if (c <= 0)
			break;

		// Place this character into our string.
		// [BB] Even if we don't have enough space in s_szString, we have to fully 
		// parse the received string. Otherwise we can't continue parsing the packet.
		if (ulIdx < MAX_NETWORK_STRING - 1)
			s_szString[ulIdx] = static_cast<char> (c);

		++ulIdx;

	} while (true);

	// [BB] We may have read more chars than we can store.
	const int endIndex = (ulIdx < MAX_NETWORK_STRING) ? ulIdx : MAX_NETWORK_STRING - 1;
	s_szString[endIndex] = '\0';
	return (s_szString);
}

bool ByteInputStream::ReadBit()
{
	EnsureBitSpace(1, false);

	// Use a bit shift to extract a bit from our current byte
	bool result = !!(*bitBuffer & (1 << bitShift));
	bitShift++;
	return result;
}

int ByteInputStream::ReadVariable()
{
	// Read two bits to form an integer 0...3
	int length = ReadBit();
	length |= ReadBit() << 1;

	// Use this length to read in an integer of variable length.
	switch (length)
	{
	default:
	case 0: return 0;
	case 1: return ReadByte();
	case 2: return ReadShort();
	case 3: return ReadLong();
	}
}

int ByteInputStream::ReadShortByte(int bits)
{
	if (bits >= 0 && bits <= 8)
	{
		EnsureBitSpace(bits, false);
		int mask = (1 << bits) - 1; // Create a mask to cover the bits we want.
		mask <<= bitShift; // Shift the mask so that it covers the correct bits.
		int result = *bitBuffer & mask; // Apply the shifted mask on our byte to remove unwanted bits.
		result >>= bitShift; // Shift the result back to start from 0.
		bitShift += bits; // Increase shift to mark these bits as used.
		return result;
	}
	else
	{
		return 0;
	}
}

void ByteInputStream::ReadBuffer(void *buffer, size_t length)
{
	if ((pbStream + length) > pbStreamEnd)
	{
		Printf("ByteInputStream::ReadBuffer: Overflow!\n");
	}
	else
	{
		memcpy(buffer, pbStream, length);
		pbStream += length;
	}
}

bool ByteInputStream::IsAtEnd() const
{
	return (pbStream >= pbStreamEnd);
}

int ByteInputStream::BytesLeft() const
{
	return (int)(ptrdiff_t)(pbStreamEnd - pbStream);
}

ByteInputStream ByteInputStream::ReadSubstream(int length)
{
	if ((pbStream + length) > pbStreamEnd)
	{
		pbStream = pbStreamEnd;
		return {};
	}
	else
	{
		pbStream += length;
		return { pbStream - length, length };
	}
}

void ByteInputStream::EnsureBitSpace(int bits, bool writing)
{
	if ((bitBuffer == nullptr) || (bitShift < 0) || (bitShift + bits > 8))
	{
		// No room for the value in this byte, so we need a new one.
		if (ReadByte() != -1)
		{
			bitBuffer = pbStream - 1;
		}
		else
		{
			// Argh! No bytes left!
			Printf("ByteInputStream::EnsureBitSpace: out of bytes to use\n");
			static uint8_t fallback = 0;
			bitBuffer = &fallback;
		}

		bitShift = 0;
	}
}

//*****************************************************************************

NetCommand::NetCommand(const NetPacketType Header)
{
	// To do: improve memory handling here. 8 kb per command is very wasteful
	mStream.SetBuffer(MAX_UDP_PACKET);

	AddByte(static_cast<int>(Header));
}

void NetCommand::AddInteger(const int IntValue, const int Size)
{
	for (int i = 0; i < Size; ++i)
		mStream.WriteByte((IntValue >> (8 * i)) & 0xff);
}

void NetCommand::AddByte(const int ByteValue)
{
	AddInteger(static_cast<uint8_t> (ByteValue), sizeof(uint8_t));
}

void NetCommand::AddShort(const int ShortValue)
{
	AddInteger(static_cast<int16_t> (ShortValue), sizeof(int16_t));
}

void NetCommand::AddLong(const int32_t LongValue)
{
	AddInteger(LongValue, sizeof(int32_t));
}

void NetCommand::AddFloat(const float FloatValue)
{
	union
	{
		float	f;
		int32_t	l;
	} dat;
	dat.f = FloatValue;
	AddInteger(dat.l, sizeof(int32_t));
}

void NetCommand::AddBit(const bool value)
{
	mStream.WriteBit(value);
}

void NetCommand::AddVariable(const int value)
{
	mStream.WriteVariable(value);
}

void NetCommand::AddShortByte(int value, int bits)
{
	mStream.WriteShortByte(value, bits);
}

void NetCommand::AddString(const char *pszString)
{
	const int len = (pszString != nullptr) ? (int)strlen(pszString) : 0;

	if (len > MAX_NETWORK_STRING)
	{
		Printf("NETWORK_WriteString: String exceeds %d characters! Header: %d\n", MAX_NETWORK_STRING, static_cast<const uint8_t*>(mStream.GetData())[0]);
		return;
	}

	for (int i = 0; i < len; ++i)
		AddByte(pszString[i]);
	AddByte(0);
}

void NetCommand::AddName(FName name)
{
	if (name.IsPredefined())
	{
		AddShort(name.GetIndex());
	}
	else
	{
		AddShort(-1);
		AddString(name.GetChars());
	}
}

void NetCommand::AddBuffer(const void *pvBuffer, int nLength)
{
	mStream.WriteBuffer(pvBuffer, nLength);
}

void NetCommand::WriteToNode(NetNodeOutput& node, bool unreliable) const
{
	node.WriteMessage(mStream.GetData(), mStream.GetSize(), unreliable);
}
