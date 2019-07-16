
#pragma once

#include <memory>
#include "vectors.h"
#include "r_data/renderstyle.h"

// Maximum size of the packets sent out by the server.
#define	MAX_UDP_PACKET				8192

// This is the longest possible string we can pass over the network.
#define	MAX_NETWORK_STRING			2048

enum class NetPacketType
{
	ConnectRequest,
	ConnectResponse,
	Disconnect,
	Tic,
	SpawnActor,
	DestroyActor
};

class ByteInputStream
{
public:
	ByteInputStream() = default;
	ByteInputStream(const void *buffer, int size);

	void SetBuffer(const void *buffer, int size);

	int	ReadByte();
	int ReadShort();
	int	ReadLong();
	float ReadFloat();
	const char* ReadString();
	bool ReadBit();
	int ReadVariable();
	int ReadShortByte(int bits);
	void ReadBuffer(void* buffer, size_t length);

	bool IsAtEnd() const;
	int BytesLeft() const;

private:
	void EnsureBitSpace(int bits, bool writing);

	uint8_t *mData = nullptr; // Pointer to our stream of data
	uint8_t *pbStream; // Cursor position for next read.
	uint8_t *pbStreamEnd; // Pointer to the end of the stream. When pbStream >= pbStreamEnd, the entire stream has been read.

	uint8_t *bitBuffer = nullptr;
	int bitShift = -1;
};

class ByteOutputStream
{
public:
	ByteOutputStream() = default;
	ByteOutputStream(int size);
	ByteOutputStream(void *buffer, int size);

	void SetBuffer(int size);
	void SetBuffer(void *buffer, int size);
	void ResetPos();

	void WriteByte(int Byte);
	void WriteShort(int Short);
	void WriteLong(int Long);
	void WriteFloat(float Float);
	void WriteString(const char *pszString);
	void WriteBit(bool bit);
	void WriteVariable(int value);
	void WriteShortByte(int value, int bits);
	void WriteBuffer(const void *pvBuffer, int nLength);

	const void *GetData() const { return mData; }
	int GetSize() const { return (int)(ptrdiff_t)(pbStream - mData); }

private:
	void AdvancePointer(const int NumBytes, const bool OutboundTraffic);
	void EnsureBitSpace(int bits, bool writing);

	uint8_t *mData = nullptr; // Pointer to our stream of data
	uint8_t *pbStream; // Cursor position for next write
	uint8_t *pbStreamEnd; // Pointer to the end of the data buffer

	uint8_t *bitBuffer = nullptr;
	int bitShift = -1;

	struct DataBuffer
	{
		DataBuffer(int size) : data(new uint8_t[size]), size(size) { }
		~DataBuffer() { delete[] data; }

		uint8_t *data;
		int size;
	};
	std::shared_ptr<DataBuffer> mBuffer;
};

/**
 * \author Benjamin Berkels
 */
class NetCommand
{
	ByteOutputStream mStream;
	bool mUnreliable = false;

public:
	NetCommand ( const NetPacketType Header );

	void addInteger( const int IntValue, const int Size );
	void addByte ( const int ByteValue );
	void addShort ( const int ShortValue );
	void addLong ( const int32_t LongValue );
	void addFloat ( const float FloatValue );
	void addString ( const char *pszString );
	void addName ( FName name );
	void addBit ( const bool value );
	void addVariable ( const int value );
	void addShortByte ( int value, int bits );
	void addBuffer ( const void *pvBuffer, int nLength );
	void writeCommandToStream ( ByteOutputStream &stream ) const;
	bool isUnreliable() const { return mUnreliable; }
	void setUnreliable(bool a) { mUnreliable = a; }
	int getSize() const;
};

#if 0
class NetNodeOutputStream
{
public:
	void WriteCommand(std::unique_ptr<NetCommand> command, bool unreliable)
	{
		mMessages.push_back({ std::move(command), mSerial, unreliable });
	}
		
	void Send(doomcom_t *comm)
	{
		NetOutputPacket packet(mNodeIndex);
		packet.stream.WriteByte(0);
		packet.stream.WriteByte(mHeaderFlags);
		packet.stream.WriteShort(mAck);
		packet.stream.WriteShort(mSerial);
		
		auto it = mMessages.begin();
		while (it != mMessages.end())
		{
			const auto &msg = *it;

			packet.stream.WriteShort(msg.serial);
			msg.command.writeCommandToStream(packet.stream);
			
			if (msg.unreliable)
			{
				it = mMessages.erase(it);
			}
			else
			{
				++it;
			}
		}
		
		comm->PacketSend(packet);
		mSerial++;
	}
	
	void AckPacket(uint8_t headerFlags, uint16_t serial, uint16_t ack)
	{
		mAck = ack;
		mHeaderFlags |= 1;
		
		if (headerFlags & 1)
		{
			while (!mMessages.empty() && IsLessEqual(serial, mMessages.front().serial))
			{
				mMessages.erase(mMessages.begin());
			}
		}
	}
	
private:
	bool IsLessEqual(uint16_t serialA, uint16_t serialB)
	{
		if (serialA <= serialB)
			return serialB - serialA < 0x7fff;
		else
			return serialA - serialB > 0x7fff;
	}
	
	struct Entry
	{
		Entry(std::unique_ptr<NetCommand> command, uint16_t serial, bool unreliable) : command(std::move(command)), serial(serial), unreliable(unreliable) { }
		
		std::unique_ptr<NetCommand> command;
		uint16_t serial = 0;
		bool unreliable = false;
	};
	
	int mNodeIndex = 0;
	std::list<Entry> mMessages;
	uint16_t mSerial = 0;
	uint16_t mAck = 0;
	uint8_t mHeaderFlags = 0;
};

class NetNodeInputStream
{
public:
	std::unique_ptr<NetCommand> ReadCommand()
	{
		return nullptr;
	}
	
	void ReceivedPacket(NetInputPacket &packet, NetNodeOutputStream &outputStream)
	{
		packet.stream.ReadByte();
		uint8_t headerFlags = packet.stream.ReadByte();
		uint16_t serial = packet.stream.ReadShort();
		uint16_t ack = packet.stream.ReadShort();
		
		mPackets.push_back({ packet.stream.GetBuffer(), packet.stream.GetSize(), serial });
		
		outputStream.AckPacket(headerFlags, ack, serial);
	}

private:
	struct Entry
	{
		Entry(const void *data, size_t size, uint16_t serial) : buffer((const uint8_t*)data, size), serial(serial) { }
		
		std::vector<uint8_t> buffer;
		uint16_t serial;
	};

	std::list<Entry> mPackets;
	uint16_t mLastSerial = 0;
};
#endif
